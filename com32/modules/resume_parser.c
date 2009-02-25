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
 * toi_load_extent_chain - load a chain of extents
 * @chain:	Chain to populate.
 *
 * Read the extent chain back from disk. This is not needed as we have
 * filesystem drivers.
 **/
static int toi_load_extent_chain(struct hibernate_extent_chain* chain)
{
	struct hibernate_extent *this, *last = NULL;
	int i;

	for (i = 0; i < chain->num_extents; i++) {
		this = malloc(sizeof(struct hibernate_extent));
		if (!this) {
			error("Failed to allocate a new extent.\n");
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

/**
 * read_metadata - read configuration and bitmaps from disk
 * @pagedir1_size:	Size of pagedir1 (found in the header).
 * @pagedir2_size:	Size of pagedir2 (found in the header).
 *
 * Side effects:
 *	mm is populated
 *
 * Return:
 *	0 if everything went well, -1 otherwise.
 **/
int read_metadata(unsigned long* pagedir1_size, unsigned long* pagedir2_size)
{
	struct toi_header* toi_header;
	struct hibernate_extent_iterate_saved_state toi_writer_posn_save[4];
	struct toi_file_header* toi_file_header;
	struct toi_module_header* toi_module_header;
	struct hibernate_extent_chain* chain;

	int* module_extra_info;
	int header_offset;
#ifdef METADATA_DEBUG
	int i;
#endif /* METADATA_DEBUG */
#ifdef DYN_PAGEFLAGS
	int size_bitmap;
#endif /* DYN_PAGEFLAGS */

	READ_BUFFER(toi_file_header, struct toi_file_header*);
	/* We only attempt to resume if the magic binary signature is found */
	if (!parse_signature(toi_file_header)) {
		error("Invalid TuxOnIce file.\n");
		goto bail;
	}

	dprintf("TuxOnIce: binary signature found.\n");

	/*
	 * Setup some values we are going to need to load the bitmaps.
	 * These are kernel dependent (configuration dependent) so we
	 * need to store them in the image.
	 * XXX: In the static pageflags case, we assume only one zone.
	 * I do not believe it supports NUMA.
	 */
	mm.num_nodes = toi_file_header->num_nodes;
	mm.num_zones = toi_file_header->num_zones;

	/*
	 * The original header is:
	 *
	 * struct toi_file_header {
	 *	char sig[sig_size];
	 *	int resumed_before;
	 *	unsigned long first_header_block;
	 *	int have_image;
	 *	int devinfo_sz;
	 * };
	 *
	 * The header needs:
	 *	unsigned long zones_start_pfn[MAX_NUMNODES][MAX_NR_ZONES];
	 *	unsigned long zones_max_offset[MAX_NUMNODES][MAX_NR_ZONES];
	 *
	 * to use dyn_pageflags.
	 */
#ifdef DYN_PAGEFLAGS
	size_bitmap = mm.num_nodes * mm.num_zones * sizeof(unsigned long);
	MOVE_FORWARD_BUFFER_POINTER(sizeof(struct toi_file_header));

	READ_BUFFER(mm.zones_start_pfn, unsigned long*);
	MOVE_FORWARD_BUFFER_POINTER(size_bitmap);

	READ_BUFFER(mm.zones_max_offset, unsigned long*);
	MOVE_FORWARD_BUFFER_POINTER(size_bitmap);
#endif /* DYN_PAGEFLAGS */

	dump_toi_file_header(toi_file_header);

	/* Get extents info */
	toi_image_buffer_posn = PAGE_SIZE;

	memcpy(&toi_writer_posn_save,
	       toi_image_buffer + toi_image_buffer_posn,
	       sizeof(toi_writer_posn_save));
	MOVE_FORWARD_BUFFER_POINTER(sizeof(toi_writer_posn_save));

#ifdef METADATA_DEBUG
	/* We really don't need those as we have filesystem drivers */
	dprintf("Disk extents offsets:\n");
	for (i = 0; i < 4; i++)
		dprintf("\tPosn %d: Chain %d, extent %d, offset %lu.\n",
				i, toi_writer_posn_save[i].chain_num,
				toi_writer_posn_save[i].extent_num,
				toi_writer_posn_save[i].offset);
#endif /* METADATA_DEBUG */

	if (!toi_file_header->devinfo_sz)
		/* Backward compatibility */
		toi_file_header->devinfo_sz = 16;
	/* Skip dev_info */
	MOVE_FORWARD_BUFFER_POINTER(toi_file_header->devinfo_sz);

	/* Load chains */
	// XXX This assumes only one chain (one device).
	// FIXME (easy fix)
	READ_BUFFER(chain, struct hibernate_extent_chain*);
	MOVE_FORWARD_BUFFER_POINTER(2 * sizeof(int));

#ifdef METADATA_DEBUG
	dump_extent_chain(chain);

	/* This is not needed, as we have filesystem drivers */
	//toi_load_extent_chain(chain);
	//dump_block_chains(chain);
#endif /* METADATA_DEBUG */

	/*
	 * The header is located after:
	 *	+ the toi_file_header
	 *	+ the devinfo struct (we pass it in the image as we
	 *	  don't want to compute its size here - too many complex
	 *	  dependencies)
	 *	+ the chain of extents
	 */
	header_offset = PAGE_SIZE +
		sizeof(struct toi_file_header) +
		sizeof(unsigned long) +
		toi_file_header->devinfo_sz +
		sizeof(struct hibernate_extent_chain) +
		2 * sizeof(unsigned long) * chain->num_extents;
	toi_image_buffer_posn = header_offset;

	/* Read metadata */
	READ_BUFFER(toi_header, struct toi_header*);
	MOVE_FORWARD_BUFFER_POINTER(sizeof(struct toi_header));
	dump_toi_header(toi_header);
	*pagedir1_size = toi_header->pagedir.size;
	*pagedir2_size = toi_header->pageset_2_size;

	MOVE_FORWARD_BUFFER_POINTER(4); /* FIXME: where does that come from? */

	/* Skip TuxOnIce modules info - we don't care */
	READ_BUFFER(toi_module_header, struct toi_module_header*);
	MOVE_FORWARD_BUFFER_POINTER(sizeof(struct toi_module_header));
	while (toi_module_header->name[0]) {
#ifdef METADATA_DEBUG
		dump_toi_module_header(toi_module_header);
#endif /* METADATA_DEBUG */

		/* Skip extra info, if needed */
		READ_BUFFER(module_extra_info, int*);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(int));
		if (*module_extra_info) {
#ifdef METADATA_DEBUG
			dprintf("\textra_info\t%d\n",
				*module_extra_info);
#endif /* METADATA_DEBUG */
			MOVE_FORWARD_BUFFER_POINTER(*module_extra_info);
		}

		/* Get the next module */
		READ_BUFFER(toi_module_header, struct toi_module_header *);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(struct toi_module_header));
	}

	/* Load the bitmap from disk */
	load_bitmap();
	dump_pagemap();

	/*
	 * The following performs some sanity checks.
	 * Let's avoid it in non-debug environments as we are looking for
	 * performance.
	 */
#ifdef DEBUG
	if(check_number_of_pfn(toi_header))
		goto bail;
#endif
	return 0;

bail:
	return -1;
}
