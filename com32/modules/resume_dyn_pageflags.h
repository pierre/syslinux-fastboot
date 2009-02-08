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

#ifndef RESUME_DYN_PAGEFLAGS_H
#define RESUME_DYN_PAGEFLAGS_H

extern struct mm mm;
extern struct dyn_pageflags pagemap;

/*
 *  PAGENUMBER gives the index of the page within the zone.
 *  PAGEINDEX gives the index of the unsigned long within that page.
 *  PAGEBIT gives the index of the bit within the unsigned long.
 */
#define PAGENUMBER(zone_offset) ((int) ((zone_offset) >> (PAGE_SHIFT + 3)))
#define PAGEINDEX(zone_offset) ((int) (((zone_offset) & UL_NUM_MASK) >> UL_SHIFT))
#define PAGEBIT(zone_offset) ((int) ((zone_offset) & BIT_NUM_MASK))

#define SPANNED_PAGES(node_id, zone_id) \
	(pagemap.bitmap[node_id] && \
	 pagemap.bitmap[node_id][zone_id] ? \
	 *pagemap.bitmap[node_id][zone_id][1] : 0)

#define ZONE_START(node_id, zone_id) \
	mm.zones_start_pfn[(node_id) * mm.num_zones + (zone_id)]

#define MAX_PFN(node_id, zone_id) ZONE_START((node_id), (zone_id)) + \
	mm.zones_max_offset[(zone_id)]

#define GET_ZONE_ARRAY(node_id, zone_id) \
	(pagemap.bitmap && \
	pagemap.bitmap[(node_id)] && \
	pagemap.bitmap[(node_id)][(zone_id)]) ? \
	pagemap.bitmap[(node_id)][(zone_id)] : NULL; \

#define GET_UL(zone_array, pagenum, page_offset) \
	(zone_array && \
	(unsigned long) *zone_array[1] > (unsigned int) (pagenum-2) && \
	zone_array[pagenum]) ? zone_array[pagenum] + page_offset : \
	  NULL; \

#define GET_BIT_AND_UL(node_id, zone_id, pfn) \
	unsigned long zone_pfn = (pfn) - ZONE_START((node_id), (zone_id)); \
	int pagenum = PAGENUMBER(zone_pfn) + 2; \
	int page_offset = PAGEINDEX(zone_pfn); \
	unsigned long **zone_array = GET_ZONE_ARRAY(node_id, zone_id) \
	unsigned long *ul = GET_UL(zone_array, pagenum, page_offset) \
	int bit = PAGEBIT(zone_pfn);

__inline__ int load_bitmap(void) __attribute__((always_inline));
int load_dyn_pageflags(void);
unsigned long bitmap_get_next_bit_on(long, int, int);
int bitmap_test_all(unsigned long);
__inline__ int bitmap_test(int, int, unsigned long) __attribute__((always_inline));
int bitmap_unset(unsigned long);

int check_number_of_pfn(struct toi_header*);

#endif /* RESUME_DYN_PAGEFLAGS_H */
