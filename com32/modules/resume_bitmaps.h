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

#ifndef RESUME_BITMAPS_H
#define RESUME_BITMAPS_H

/* Define struct toi_header */
#include "resume_tuxonice.h"

#define BM_END_OF_MAP	(~0UL)

/* struct linked_page is used to build chains of pages */
#define LINKED_PAGE_DATA_SIZE	(PAGE_SIZE - sizeof(void *))

struct linked_page {
	struct linked_page *next;
	char data[LINKED_PAGE_DATA_SIZE];
} __attribute__((packed));

struct bm_block {
	struct list_head hook;		/* hook into a list of bitmap blocks */
	unsigned long start_pfn;	/* pfn represented by the first bit */
	unsigned long end_pfn;		/* pfn represented by the last bit + 1 */
	unsigned long *data;		/* bitmap representing pages */
};

/* struct bm_position is used for browsing memory bitmaps */
struct bm_position {
	struct bm_block *block;
	int bit;
};

struct memory_bitmap {
	struct list_head blocks;	/* list of bitmap blocks */
	struct linked_page *p_list;	/* list of pages used to store zone
					 * bitmap objects and bitmap block
					 * objects
					 */
	struct bm_position cur;		/* most recently used bit position */
	struct bm_position iter;	/* most recently used bit position
					 * when iterating over a bitmap.
					 */
};

static inline unsigned long bm_block_bits(struct bm_block *bb)
{
	return bb->end_pfn - bb->start_pfn;
}

struct memory_bitmap *pageset1;

int load_bitmap(void);
int check_number_of_pfn(struct toi_header*);
void memory_bm_position_reset(struct memory_bitmap*);
unsigned long memory_bm_next_pfn(struct memory_bitmap*);
unsigned long find_next_bit(const unsigned long*, unsigned long,
			    unsigned long);
int memory_bm_test_pfn(struct memory_bitmap*, unsigned long);
int memory_bm_find_bit(struct memory_bitmap*, unsigned long,
				void **, unsigned int*);
int memory_bm_test_bit(struct memory_bitmap*, unsigned long);
void memory_bm_set_bit(struct memory_bitmap*, unsigned long);
void memory_bm_clear_bit(struct memory_bitmap*, unsigned long);

#endif /* RESUME_BITMAPS_H */
