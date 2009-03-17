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
#include "resume_mm.h"
#include "resume_tuxonice.h"
#include "resume_bitops.h"

#include "resume_dyn_pageflags.h"

extern void error(const char*);

__inline__ int load_bitmap(void)
{
	return load_dyn_pageflags();
}

/**
 * load_dyn_pageflags - load the pagemap from the image
 *
 * The memory will be allocated when constructing the bitmap.
 * This is used for loading (depreacted) dynamyc pageflags.
 *
 * Return:
 *	Zero if all went well. Non-zero value otherwise.
 **/
int load_dyn_pageflags(void)
{
	int i, zone_id, node_id;
	int *zone_check = 0, *size = 0;

	pagemap.bitmap = malloc(mm.num_nodes * sizeof(unsigned long*));

	for(node_id = 0; node_id < mm.num_nodes; node_id++) {
		pagemap.bitmap[node_id] =
			malloc(mm.num_nodes * sizeof(unsigned long*));

		for (zone_id = 0; zone_id < mm.num_zones; zone_id++) {
			/*
			 * SANITY CHECKS
			 *
			 * The bitmap is stored in order wrt nodes and zones.
			 * The node and zone always precede a bitmap.
			 */

			/* Same node? */
			READ_BUFFER(zone_check, int *);
			MOVE_FORWARD_BUFFER_POINTER(sizeof(int));

			/*
			 * We can't detect enabled zones as they were in the
			 * saved kernel.
			 * Stop if we the end data marker
			 */
			if (*zone_check < 0 || *zone_check >= mm.num_zones) {
				pagemap.bitmap[node_id][zone_id] = NULL;
				dprintf("Zone %d empty.\n", zone_id);
				continue;
			} else if (*zone_check != node_id) {
				dprintf("Node read (%d) != node "
						"(%d).\n",
						*zone_check, node_id);
				error("BUG: node mismatch. Invalid bitmap.");
				goto bail;
			}

			/* Same zone? */
			READ_BUFFER(zone_check, int *);
			MOVE_FORWARD_BUFFER_POINTER(sizeof(int));
			if (*zone_check != zone_id) {
				dprintf("Zone read (%d) != zone "
						"(%d).\n",
						*zone_check, zone_id);
				error("BUG: zone mismatch. Invalid bitmap.");
				goto bail;
			}

			/*
			 * READ BITMAPS
			 *
			 * Do the actual reading.
			 */

			/* Note: the size is the number of pages */
			READ_BUFFER(size, int *);
			MOVE_FORWARD_BUFFER_POINTER(sizeof(int));
			dprintf("Zone %d: size\t0x%08x\n", zone_id, *size);

			pagemap.bitmap[node_id][zone_id] =
			       malloc((*size+2)*sizeof(unsigned long*));

			/*
			 * In theory:
			 *	pagemap.bitmap[node_id][zone_id][0] ==
			 *				zone_start_pfn
			 *
			 * We don't store it as we already have the info.
			 * TODO: get rid of that.
			 */
			pagemap.bitmap[node_id][zone_id][1] =
					malloc(sizeof(unsigned long));
			*pagemap.bitmap[node_id][zone_id][1] = *size;

			/* Map the pages
			 * zone_offset represents a pfn in a zone
			 * PAGEINDEX is used to get a page
			 */
			for (i=2; i<*size+2; i++) {
				pagemap.bitmap[node_id][zone_id][i] =
					malloc(PAGE_SIZE);
				memcpy(pagemap.bitmap[node_id][zone_id][i],
				       toi_image_buffer+toi_image_buffer_posn,
				       PAGE_SIZE);
				MOVE_FORWARD_BUFFER_POINTER(PAGE_SIZE);
			}
		}
		node_id++;
	}

	if (*zone_check != -1) {
		READ_BUFFER(zone_check, int *);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(int));
		if (*zone_check != -1) {
			error("BUG: end of bitmap not reached.\n");
			goto bail;
		}
	}

	return 0;

bail:
	// TODO free_bitmap()!
	return -1;
}

/**
 * bitmap_get_next_bit_on - get the next bit set in a bitmap
 * @previous_pfn:	The previous pfn. We always return a value > this.
 * @node_id:		The current node searched.
 * @zone_id:		The current zone searched.
 *
 * Given a pfn (possibly -1), find the next pfn in the bitmap that
 * is set. If there are no more bit set, return -1.
 **/
unsigned long bitmap_get_next_bit_on(long previous_pfn,
				     int node_id,
				     int zone_id)
{
	unsigned long *ul = NULL;
	unsigned long prev_pfn;
	int zone_offset, pagebit, first;

	first = (previous_pfn == -1);

	if (first)
		prev_pfn = ZONE_START(node_id, zone_id);
	else
		prev_pfn = previous_pfn;

	zone_offset = prev_pfn - ZONE_START(node_id, zone_id);

	if (first) {
		prev_pfn--;
		zone_offset--;
	} else
		ul = (pagemap.bitmap[node_id][zone_id][PAGENUMBER(zone_offset)+2]) +
		     PAGEINDEX(zone_offset);

	do {
		prev_pfn++;
		zone_offset++;

		if (prev_pfn >= (unsigned long) MAX_PFN(node_id, zone_id)) {
			return -1;
		}

		/*
		 * This could be optimised, but there are more
		 * important things and the code is simple at
		 * the moment 
		 */
		pagebit = PAGEBIT(zone_offset);

		if (!pagebit)
			ul = (pagemap.bitmap[node_id][zone_id][PAGENUMBER(zone_offset) + 2]) +
			PAGEINDEX(zone_offset);

		if(!ul)
			continue;

	} while(!test_bit(pagebit, (u32 *) ul));

	return prev_pfn;
}

/**
 * bitmap_test_all - test a page in the entire bitmap
 * @pfn:	pfn to check.
 *
 * Test whether the bit is on for the pfn in the bitmap, without knowing its
 * location (node and/or zone).
 * XXX FIXME This is really no optimized.
 **/
int bitmap_test_all(unsigned long pfn)
{
	int node_id, zone_id;

	for(node_id = 0; node_id < mm.num_nodes; node_id++) {
		for(zone_id = 0; zone_id < mm.num_zones; zone_id++) {
			if(bitmap_test(node_id, zone_id, pfn))
				return 1;
		}
	}
	return 0;
}

/**
 * bitmap_test - test a page in a bitmap
 * @node_id:	The node containing the pfn.
 * @zone_id:	The zone containing the pfn.
 * @pfn:	pfn to check.
 *
 * Test whether the bit is on for the pfn in the bitmap: in that case, the page
 * is present in the image.
 * XXX FIXME: there is no need to know the zone of the pfn, since we are
 * passing an absolute pfn.
 **/
int bitmap_test(int node_id, int zone_id, unsigned long pfn)
{
	GET_BIT_AND_UL(node_id, zone_id, pfn);
	return ul ? test_bit(bit, (u32 *) ul) : 0;
}

/**
 * bitmap_unset - unmark a page in the bitmap
 * @pfn:	pfn of the page to unset.
 *
 * Given a pfn, clear the corresponding bit in pagemap.
 * XXX FIXME This is really no optimized.
 *
 * Return:
 *	1 if the page couldn't be found, 0 otherwise.
 **/
int bitmap_unset(unsigned long pfn)
{
	int node_id, zone_id;

	for(node_id = 0; node_id < mm.num_nodes; node_id++) {
		for(zone_id = 0; zone_id < mm.num_zones; zone_id++) {
			if(bitmap_test(node_id, zone_id, pfn)) {
				GET_BIT_AND_UL(node_id, zone_id, pfn);
#ifdef DEBUG
				//dprintf("Before removing bit %d\n", bit);
				//showbits(ul);
#endif
				clear_bit(bit, (u32 *) ul);
#ifdef DEBUG
				//dprintf("After removing bit %d\n", bit);
				//showbits(ul);
#endif
				return 0;
			}
		}
	}
	return 1;
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
	int i, j;
	long pfn = 0, total_pfn = 0;

	printf("Number of pages to load: %lu\n", toi_header->pagedir.size);
	for (i=0; i < mm.num_nodes; i++) {
		dprintf("Node %d\n", i);
		for (j=0; j < mm.num_zones; j++) {
			int num_pfn = 0;
			pfn = -1;
			do {
				pfn = bitmap_get_next_bit_on(pfn, i, j);
				//dprintf("\tZone %d\tpfn %lu\n", j, pfn);
				if(pfn !=  -1)
					num_pfn++;
			} while(pfn != -1);
			dprintf("\tZone %d: %d pfns\n", j, num_pfn);
			total_pfn += num_pfn;
		}
	}

	if(total_pfn != toi_header->pagedir.size)
		printf("BUG: %lu != %lu\n", total_pfn, toi_header->pagedir.size);
	else
		printf("Number of pages to load: %lu\n", toi_header->pagedir.size);

	return total_pfn-toi_header->pagedir.size;
}
