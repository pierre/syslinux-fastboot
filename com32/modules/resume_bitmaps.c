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

	pageset1 = malloc(sizeof(struct memory_bitmap));
	pageset1->blocks.prev = &pageset1->blocks;
	pageset1->blocks.next = &pageset1->blocks;

	READ_BUFFER(number_zones, int*);
	MOVE_FORWARD_BUFFER_POINTER(sizeof(int));

	dprintf("Number of memory zones\t\t%d\n", *number_zones);

	for (zone_id = 0; zone_id < *number_zones; zone_id++) {
		struct bm_block *bb = malloc(sizeof(struct bm_block));
		unsigned long* pfn;

		READ_BUFFER(pfn, unsigned long*);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(unsigned long));
		bb->start_pfn = *pfn;
		//dprintf("\tZone %d: start pfn\t0x%08lx\n", zone_id,
		//					   bb->start_pfn);

		READ_BUFFER(pfn, unsigned long*);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(unsigned long));
		bb->end_pfn = *pfn;
		//dprintf("\tZone %d: end pfn\t\t0x%08lx\n", zone_id,
		//					   bb->end_pfn);

		/* Load the bitmap itself */
		bb->data = malloc(PAGE_SIZE);
		memcpy(bb->data,
		       toi_image_buffer + toi_image_buffer_posn,
		       PAGE_SIZE);
		MOVE_FORWARD_BUFFER_POINTER(PAGE_SIZE);

		list_add(&bb->hook, &pageset1->blocks);
	}

	return 0;
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
	long pfn = 0, total_pfn = 0;

	printf("Number of pages to load (pagedir1): %lu\n",
		toi_header->pagedir.size);

	memory_bm_position_reset(pageset1);
	for (pfn = memory_bm_next_pfn(pageset1); pfn != BM_END_OF_MAP;
		pfn = memory_bm_next_pfn(pageset1)) {
		//printf("\tpfn: %lu\n", pfn);
		total_pfn++;
	}

	if (total_pfn != toi_header->pagedir.size)
		printf("BUG: %lu != %lu\n",
			total_pfn,
			toi_header->pagedir.size);

	return total_pfn-toi_header->pagedir.size;
}

void memory_bm_position_reset(struct memory_bitmap *bm)
{
	bm->cur.block = list_entry(bm->blocks.next, struct bm_block, hook);
	bm->cur.bit = 0;

	bm->iter.block = list_entry(bm->blocks.next, struct bm_block, hook);
	bm->iter.bit = 0;
}

/**
 * bitmap_get_next_bit_on - get the next bit set in a bitmap
 * @bm:	bitmap to parse
 *
 * Find the pfn that corresponds to the next set bit in the bitmap. If the
 * pfn cannot be found, BM_END_OF_MAP is returned.
 **/
unsigned long memory_bm_next_pfn(struct memory_bitmap *bm)
{
	struct bm_block *bb;
	int bit;

	bb = bm->iter.block;
	do {
		bit = bm->iter.bit;
		bit = find_next_bit(bb->data, bm_block_bits(bb), bit);
		if (bit < bm_block_bits(bb))
			goto Return_pfn;

		bb = list_entry(bb->hook.next, struct bm_block, hook);
		bm->iter.block = bb;
		bm->iter.bit = 0;
	} while (&bb->hook != &bm->blocks);

	memory_bm_position_reset(bm);
	return BM_END_OF_MAP;

 Return_pfn:
	bm->iter.bit = bit + 1;
	return bb->start_pfn + bit;
}

/*
 * Find the next set bit in a memory region.
 */
unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
			    unsigned long offset)
{
	const unsigned long *p = addr + BITOP_WORD(offset);
	unsigned long result = offset & ~(BITS_PER_LONG-1);
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset %= BITS_PER_LONG;
	if (offset) {
		tmp = *(p++);
		tmp &= (~0UL << offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	} 
	while (size & ~(BITS_PER_LONG-1)) {
		if ((tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp &= (~0UL >> (BITS_PER_LONG - size));
	if (tmp == 0UL)		/* Are any bits set? */
		return result + size;	/* Nope. */
found_middle:
	return result + __ffs(tmp);
}
