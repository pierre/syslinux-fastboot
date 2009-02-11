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
#include "resume_linux.h"
#include "resume_tuxonice.h"
#include "resume_mm.h"
#include "resume_bitops.h"

#include "resume_bitmaps.h"

/**
 * load_bitmap - load the pagemap from the image
 *
 * The memory will be allocated when constructing the bitmap.
 *
 * Return:
 *	Zero if all went well. Non-zero value otherwise.
 **/
int load_bitmap(void)
{
	int zone_id;
	int* number_zones;

	dprintf("Memory bitmaps:\n");

	READ_BUFFER(number_zones, int*);
	MOVE_FORWARD_BUFFER_POINTER(sizeof(int));

	dprintf("\tNumber of zones\t\t%d\n", *number_zones);

	pagemap.bitmap = malloc(*number_zones * sizeof(unsigned long*));

	for (zone_id = 0; zone_id < *number_zones; zone_id++) {
		unsigned long* pfn;

		READ_BUFFER(pfn, unsigned long*);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(unsigned long));
		dprintf("\tZone %d: start pfn\t0x%08lx\n", zone_id, *pfn);

		/* Sanity checking */
		//if(mm.zones_start_pfn[zone_id] != *pfn) {
		//	dprintf("Zone %d: invalid start pfn\t0x%08lx\n",
		//		 zone_id, *pfn);
		//	dprintf("Zone %d: expected start pfn\t0x%08lx\n",
		//		 zone_id, mm.zones_start_pfn[zone_id]);
		//	goto bail;
		//}

		//READ_BUFFER(pfn, unsigned long*);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(unsigned long));
		dprintf("\tZone %d: end pfn\t\t0x%08lx\n", zone_id, *pfn);

		/* Sanity checking */
		//if(mm.zones_start_pfn[zone_id] + mm.zones_max_offset[zone_id] !=
		//	*pfn) {
		//	dprintf("Zone %d: invalid end pfn\t0x%08lx\n",
		//		 zone_id, *pfn);
		//	dprintf("Zone %d: expected end pfn\t0x%08lx\n",
		//		 zone_id, mm.zones_start_pfn[zone_id] +
		//		 mm.zones_max_offset[zone_id]);
		//	goto bail;
		//}

		/* Load the bitmap itself */
		pagemap.bitmap[zone_id] = malloc(PAGE_SIZE);
		memcpy(pagemap.bitmap[zone_id],
		       toi_image_buffer + toi_image_buffer_posn,
		       PAGE_SIZE);
		MOVE_FORWARD_BUFFER_POINTER(PAGE_SIZE);
	}

	return 0;

bail:
	// TODO free_bitmap()!
	return -1;
}

/**
 * check_number_of_pfn - verify if the bitmap loaded is correct
 * @toi_header:	header of the image (containing the number of pfn expected).
 *
 * Count all bits in the bitmap loaded from disk. The total number of "1"
 * should be equal to toi_file_header->pagedir.size.
 *
 * Return:
 *	total_pfn-toi_header->pagedir.size
 **/
int check_number_of_pfn(struct toi_header* toi_header) {
	int i;
	long pfn = 0, total_pfn = 0;

	printf("Number of pages to load (pagedir1): %lu\n",
		toi_header->pagedir.size);
	for (i = 0; i < mm.num_zones; i++) {
		int num_pfn = 0;
		pfn = -1;
		do {
			pfn = bitmap_get_next_bit_on(pfn, i);
			if(pfn !=  -1)
				num_pfn++;
		} while (pfn != -1);
		printf("\tZone %d: %d pfns\n", i, num_pfn);
		total_pfn += num_pfn;
	}

	if (total_pfn != toi_header->pagedir.size)
		printf("BUG: %lu != %lu\n",
			total_pfn,
			toi_header->pagedir.size);
	else
		printf("Number of pages to load: %lu\n",
			toi_header->pagedir.size);

	return total_pfn-toi_header->pagedir.size;
}

/**
 * bitmap_get_next_bit_on - get the next bit set in a bitmap
 * @previous_pfn:	The previous pfn. We always return a value > this.
 * @zone_id:		The current zone searched.
 *
 * Given a pfn (possibly -1), find the next pfn in the bitmap that
 * is set. If there are no more bit set, return -1.
 **/
unsigned long bitmap_get_next_bit_on(long previous_pfn, int zone_id)
{
	unsigned long *ul = NULL;
	unsigned long prev_pfn;
	int zone_offset, pagebit, first;

	first = (previous_pfn == -1);

	if (first)
		prev_pfn = mm.zones_start_pfn[zone_id];
	else
		prev_pfn = previous_pfn;

	zone_offset = prev_pfn - mm.zones_start_pfn[zone_id];

	if (first) {
		prev_pfn--;
		zone_offset--;
	} else
		ul = (pagemap.bitmap[zone_id][PAGENUMBER(zone_offset)+2]) +
		      PAGEINDEX(zone_offset);

	do {
		prev_pfn++;
		zone_offset++;

		if (prev_pfn >= (unsigned long) (mm.zones_start_pfn[zone_id] +
						 mm.zones_max_offset[zone_id])) {
			return -1;
		}

		/*
		 * This could be optimised, but there are more
		 * important things and the code is simple at
		 * the moment 
		 */
		pagebit = PAGEBIT(zone_offset);

		if (!pagebit)
			ul = (pagemap.bitmap[zone_id][PAGENUMBER(zone_offset) + 2]) +
			PAGEINDEX(zone_offset);

		if(!ul)
			continue;

	} while(!test_bit(pagebit, (u32 *) ul));

	return prev_pfn;
}
