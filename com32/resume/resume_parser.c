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

#include <disk/swsusp.h>

#include "resume.h"
#include "resume_debug.h"
#include "resume_tuxonice.h"
#include "resume_mm.h"
#include "resume_bitmaps.h"

/**
 * toi_read_metadata - read configuration and bitmaps from disk
 *
 * Side effects:
 *	mm is populated
 *
 * Return:
 *	0 if everything went well, -1 otherwise.
 **/
static int toi_file_read_metadata()
{
	struct toi_header* toi_header;
	struct hibernate_extent_iterate_saved_state toi_writer_posn_save[4];
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

	/*
	 * Setup some values we are going to need to load the bitmaps.
	 * These are kernel dependent (configuration dependent) so we
	 * need to store them in the image.
	 * XXX: In the static pageflags case, we assume only one zone.
	 * I do not believe it supports NUMA.
	 */
	mm.num_nodes = resume_info.toi_file_header.num_nodes;
	mm.num_zones = resume_info.toi_file_header.num_zones;

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

	dump_toi_file_header(&resume_info.toi_file_header);

	/* Get extents info */
	resume_info.image_buffer_posn = PAGE_SIZE;

	memcpy(&toi_writer_posn_save,
	       resume_info.image_buffer + resume_info.image_buffer_posn,
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

	if (!resume_info.toi_file_header.devinfo_sz)
		/* Backward compatibility */
		resume_info.toi_file_header.devinfo_sz = 16;
	/* Skip dev_info */
	MOVE_FORWARD_BUFFER_POINTER(resume_info.toi_file_header.devinfo_sz);

	/* Load chains */
	// XXX This assumes only one chain (one device).
	// FIXME (easy fix)
	READ_BUFFER(chain, struct hibernate_extent_chain*);
	MOVE_FORWARD_BUFFER_POINTER(2 * sizeof(int));

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
		resume_info.toi_file_header.devinfo_sz +
		sizeof(struct hibernate_extent_chain) +
		2 * sizeof(unsigned long) * chain->num_extents;
	resume_info.image_buffer_posn = header_offset;

	READ_BUFFER(toi_header, struct toi_header*);
	MOVE_FORWARD_BUFFER_POINTER(sizeof(struct toi_header));
	resume_info.toi_pagedir1_size = toi_header->pagedir.size;
	resume_info.toi_pagedir2_size = toi_header->pageset_2_size;
	dump_toi_header(toi_header);

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

// XXX Move the memcmp code to gpllib
static int try_tuxonice()
{
	if (resume_info.file_size) {
		memcpy(&resume_info.toi_file_header, resume_info.file, sizeof resume_info.toi_file_header);
		/*
		 * We only attempt to resume if the magic binary signature is found and
		 * if an image is marked as present.
		 */
		if (memcmp(tuxonice_signature, resume_info.toi_file_header.sig,
			   sizeof tuxonice_signature) ||
		    !resume_info.toi_file_header.have_image)
			return 0;
		else
			return 1;
	} else if (resume_info.drive_info.disk) {
		return 0; //toi_check(&resume_info.driveinfo, ptab, NULL);
	} else
		return 0;
}

/**
 * swsusp_read_metadata - read configuration and bitmaps from disk
 *
 * Side effects:
 *	mm is populated
 *
 * Return:
 *	0 if everything went well, -1 otherwise.
 **/
static int swsusp_file_read_metadata()
{
	return 0;
}

static int try_swsusp()
{
	if (resume_info.file_size) {
		memcpy(&resume_info.sw_header, resume_info.file, sizeof resume_info.sw_header);
		return swsusp_check_signature(&resume_info.sw_header);
	} else if (resume_info.drive_info.disk) {
		return swsusp_check(&resume_info.drive_info, &resume_info.ptab, NULL);
	} else
		return 0;
}

/**
 * read_metadata - parse image header
 *
 * Detect the resume method (swsusp vs TuxOnIce) and bootstrap
 * the restore code.
 **/
int read_metadata(void)
{
	resume_info.method = NONE;

	if (try_tuxonice()) {
		if (resume_info.drive_info.disk) {
			printf("TuxOnIce image on swap device found.\n");
			resume_info.method = TOI_SWAP_PARTITION;
			return 1;	/* TODO */
		} else if (resume_info.file_size) {
			printf("TuxOnIce file image found.\n");
			resume_info.method = TOI_FILE;
			// XXX TOI_SWAP_FILE?
			/* Bootstrap the restore code */
			return toi_file_read_metadata();
		}
	} else if (try_swsusp()) {
		if (resume_info.drive_info.disk) {
			printf("Swsusp image on swap device found.\n");
			resume_info.method = SWSUSP_SWAP_PARTITION;
		} else if (resume_info.file_size) {
			printf("Swsusp file image found.\n");
			resume_info.method = SWSUSP_SWAP_FILE;
		}
		return 0;
	}

	/* Invalid file or device */
	return -1;
}
