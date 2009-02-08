/* ----------------------------------------------------------------------- *
 *
 *   Copyright (C) 2008, VMware, Inc.
 *   Author: Pierre-Alexandre Meyer <pierre@vmware.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>

#include "resume.h"
#include "resume_debug.h"
#include "resume_tuxonice.h"
#include "resume_mm.h"
#include "resume_bitmaps.h"
#include "resume_parser.h"

/**
 * read_metadata - read configuration and bitmaps from disk
 * @nb_of_pfns:	Size of pagedir (found in the header)
 *
 * Side effects:
 *	mm is populated
 *
 * Return:
 *	0 if everything went well, -1 otherwise.
 **/
int read_metadata(int* nb_of_pfns)
{
	struct toi_header* toi_header;
	struct hibernate_extent_iterate_saved_state toi_writer_posn_save[4];
	struct toi_file_header* toi_file_header;
	struct toi_module_header* toi_module_header;
	struct hibernate_extent_chain* chain;

	int* module_extra_info;
	int size_bitmap, header_offset;

	int i;

	READ_BUFFER(toi_file_header, struct toi_file_header*);

	/* We only attempt to resume if the magic binary signature is found */
	if (!parse_signature(toi_file_header)) {
		//error("Invalid TuxOnIce file.\n");
		goto bail;
	} else {
		dprintf("TuxOnIce: binary signature found.\n");

		/*
		 * Setup some values we are going to need to load the bitmaps.
		 * These are kernel dependent (configuration dependent) so we
		 * need to store them in the image.
		 * XXX: In the static pageflags case, we assume only one zone.
		 * I do not believe it supports NUMA.
		 */
		mm.num_nodes = toi_file_header->nodes_num;
		mm.num_zones = toi_file_header->zones_num;

		/*
		 * The original header is:
		 *
		 * struct toi_file_header {
		 *	char sig[sig_size];
		 *	int resumed_before;
		 *	int devinfo_sz;
		 *	unsigned long first_header_block;
		 *	int have_image;
		 *	int extents_num;
		 *	int nodes_num;
		 *	int zones_num;
		 *	unsigned long zones_start_pfn[MAX_NUMNODES][MAX_NR_ZONES];
		 *	unsigned long zones_max_offset[MAX_NUMNODES][MAX_NR_ZONES];
		 * };
		 *
		 * Our copy of the header is without:
		 *	unsigned long zones_start_pfn[MAX_NUMNODES][MAX_NR_ZONES];
		 *	unsigned long zones_max_offset[MAX_NUMNODES][MAX_NR_ZONES];
		 *
		 */
		size_bitmap = mm.num_nodes * mm.num_zones * sizeof(unsigned long);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(struct toi_file_header));

		READ_BUFFER(mm.zones_start_pfn, unsigned long*);
		MOVE_FORWARD_BUFFER_POINTER(size_bitmap);

		READ_BUFFER(mm.zones_max_offset, unsigned long*);
		MOVE_FORWARD_BUFFER_POINTER(size_bitmap);

		dump_toi_file_header(toi_file_header);

		/*
		 * Get extents info
		 *
		 * Note: pageset 2 is saved BEFORE pageset 1!
		 */
		toi_image_buffer_posn = PAGE_SIZE;

		memcpy(&toi_writer_posn_save,
		       toi_image_buffer+toi_image_buffer_posn,
		       sizeof(toi_writer_posn_save));
		MOVE_FORWARD_BUFFER_POINTER(sizeof(toi_writer_posn_save));

		for (i = 0; i < 4; i++)
			dprintf("Posn %d: Chain %d, extent %d, offset %lu.\n",
					i, toi_writer_posn_save[i].chain_num,
					toi_writer_posn_save[i].extent_num,
					toi_writer_posn_save[i].offset);

		/* Skip dev_info */
		MOVE_FORWARD_BUFFER_POINTER(toi_file_header->devinfo_sz);

		/* Load chains */
		// XXX This assumes only one chain (one device).
		//FIXME (easy fix)
		READ_BUFFER(chain, char*);
		MOVE_FORWARD_BUFFER_POINTER(2 * sizeof(int));

		toi_load_extent_chain(chain);
		dump_block_chains(chain);

		/*
		 * The header is located after:
		 *	+ the toi_file_header
		 *	+ the devinfo struct (we pass it in the image as we don't
		 *	  want to compute its size here - too many complex
		 *	  dependencies)
		 *	+ the chain of extents
		 *
		 * In theory, if we get to the hibernate_extent_chain struct,
		 * we can access the number of extents. But this is tricky as
		 * we don't have the size of devinfo. This is why we pass it in
		 * the image as well.
		 */
		header_offset = PAGE_SIZE +
			sizeof(struct toi_file_header) -
			sizeof(unsigned long) +
			toi_file_header->devinfo_sz +
			sizeof(struct hibernate_extent_chain) +
			2 * sizeof(unsigned long) * toi_file_header->extents_num;
		toi_image_buffer_posn = header_offset;
	}

	/* Read metadata */
	READ_BUFFER(toi_header, struct toi_header*);
	MOVE_FORWARD_BUFFER_POINTER(sizeof(struct toi_header));
	dump_toi_header(toi_header);
	*nb_of_pfns = toi_header->pagedir.size;

	MOVE_FORWARD_BUFFER_POINTER(4); /* FIXME: where does that come from? */

	/* Skip TuxOnIce modules info - we don't care */
	READ_BUFFER(toi_module_header, struct toi_module_header*);
	MOVE_FORWARD_BUFFER_POINTER(sizeof(struct toi_module_header));
	while (toi_module_header->name[0]) {
		dump_toi_module_header(toi_module_header);

		/* Skip extra info, if needed */
		READ_BUFFER(module_extra_info, int*);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(int));
		if (*module_extra_info) {
			dprintf("\tmodule_extra_info\t\t%d\n",
				*module_extra_info);
			MOVE_FORWARD_BUFFER_POINTER(*module_extra_info);
		}

		/* Get the next module */
		READ_BUFFER(toi_module_header, struct toi_module_header *);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(struct toi_module_header));
	}

	/* Load the bitmap from disk */
	//load_bitmap();
	//dump_pagemap(LIGHT);

	/*
	 * The following performs some sanity checks.
	 * Let's avoid it in non-debug environments as we are looking for
	 * performance.
	 */
#ifdef DEBUG
	//if(check_number_of_pfn(toi_header))
	//	goto bail;
#endif
	return 0;

bail:
	return -1;
}

static int toi_load_extent_chain(struct hibernate_extent_chain* chain)
{
	struct hibernate_extent *this, *last = NULL;
	int i;

	for (i = 0; i < chain->num_extents; i++) {
		this = malloc(sizeof(struct hibernate_extent));
		if (!this) {
			printf("Failed to allocate a new extent.\n");
			return -1;
		}
		this->next = NULL;

		memcpy(this, toi_image_buffer + toi_image_buffer_posn,
			2 * sizeof(unsigned long));
		MOVE_FORWARD_BUFFER_POINTER(2 * sizeof(unsigned long));

		if (last)
			last->next = this;
		else
			chain->first = this;
		last = this;
	}

	return 0;
}
