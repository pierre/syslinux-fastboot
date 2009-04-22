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

		READ_BUFFER(pfn, unsigned long*);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(unsigned long));
		bb->end_pfn = *pfn;

		/* Load the bitmap itself */
		bb->data = malloc(PAGE_SIZE);
		memcpy(bb->data,
		       resume_info.image_buffer + resume_info.image_buffer_posn,
		       PAGE_SIZE);
		MOVE_FORWARD_BUFFER_POINTER(PAGE_SIZE);

		list_add(&bb->hook, &pageset1->blocks);
	}

	return 0;
}

/**
 * check_number_of_pfn - verify if the bitmap loaded is correct
 * @toi_header:	Header of the image (containing the number of pfn expected).
 *
 * Count all pfns in the bitmap loaded from disk. The total number
 * should be equal to toi_file_header->pagedir.size.
 *
 * Return:
 *	total_pfn - toi_header->pagedir.size
 **/
int check_number_of_pfn(struct toi_header* toi_header) {
	unsigned long pfn = 0;
	long total_pfn = 0;

	/* Go through all set bits */
	memory_bm_position_reset(pageset1);
	for (pfn = memory_bm_next_pfn(pageset1); pfn != BM_END_OF_MAP;
		pfn = memory_bm_next_pfn(pageset1)) {
		total_pfn++;
	}

	dprintf("Number of pages to reload (pagedir1): %lu\n",
		toi_header->pagedir.size);

	if (total_pfn != toi_header->pagedir.size)
		printf("BUG: %lu != %lu\n",
			total_pfn,
			toi_header->pagedir.size);

	return total_pfn - toi_header->pagedir.size;
}

/*
 * Most of the code below comes from the Linux kernel.
 */

/**
 * memory_bm_position_reset - reset the cur and iter positions in @bm
 * @bm:	Bitmap to reset.
 *
 * This is imported from kernel/power/snapshot.c
 **/
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
 *
 * This is imported from kernel/power/snapshot.c
 **/
unsigned long memory_bm_next_pfn(struct memory_bitmap *bm)
{
	struct bm_block *bb;
	unsigned int bit;

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

/**
 * find_next_bit - find the next set bit in a memory region
 *
 * This is imported from kernel/power/snapshot.c
 **/
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

/**
 * memory_bm_find_bit - find the bit in the bitmap @bm that corresponds
 *                      to given pfn
 * @bm:		Bitmap to look.
 * @pfn:	pfn to look for.
 * @addr:	Pointer to the data block containing the bit for @pfn.
 * @bit_nr:	Bit corresponding to pfn @pfn in @addr.
 *
 * Side effects:
 *	The cur_zone_bm member of @bm and the cur_block member
 *	of @bm->cur_zone_bm are updated.
 *
 * This is imported from kernel/power/snapshot.c
 **/
int memory_bm_find_bit(struct memory_bitmap *bm, unsigned long pfn,
				void **addr, unsigned int *bit_nr)
{
	struct bm_block *bb;

	/*
	 * Check if the pfn corresponds to the current bitmap block and find
	 * the block where it fits if this is not the case.
	 */
	bb = bm->cur.block;
	if (pfn < bb->start_pfn)
		list_for_each_entry_continue_reverse(bb, &bm->blocks, hook)
			if (pfn >= bb->start_pfn)
				break;

	if (pfn >= bb->end_pfn)
		list_for_each_entry_continue(bb, &bm->blocks, hook)
			if (pfn >= bb->start_pfn && pfn < bb->end_pfn)
				break;

	if (&bb->hook == &bm->blocks)
		return -1;

	/* The block has been found */
	bm->cur.block = bb;
	pfn -= bb->start_pfn;
	bm->cur.bit = pfn + 1;
	*bit_nr = pfn;
	*addr = bb->data;
	return 0;
}

/**
 * memory_bm_test_pfn - test if a pfn is in a bitmap
 * @bm:		Bitmap to look.
 * @pfn:	pfn to look for.
 *
 * XXX This routine is used in resume_restore to check if the retrieved pfn is
 * valid. This is not optimized since the iterator is reset each time. It would
 * be better to copy the pageset1 (e.g. io_map) and try to unset the bit in the
 * io_map.
 *
 * Side Effects:
 *	cur and iter blocks are reset in @bm.
 **/
int memory_bm_test_pfn(struct memory_bitmap *bm, unsigned long pfn)
{
	void *addr;
	unsigned int bit;

	memory_bm_position_reset(bm);
	return memory_bm_find_bit(bm, pfn, &addr, &bit);
}

/**
 * memory_bm_test_bit - test if the bit corresponding to a pfn is set
 * @bm:		Bitmap to look.
 * @pfn:	pfn to look for.
 *
 * This is imported from kernel/power/snapshot.c
 **/
int memory_bm_test_bit(struct memory_bitmap *bm, unsigned long pfn)
{
	void *addr;
	unsigned int bit;
	int error;

	error = memory_bm_find_bit(bm, pfn, &addr, &bit);
	if (!error)
		return test_bit(bit, addr);
	else
		return 0;

}

/**
 * memory_bm_set_bit - set the bit corresponding to a pfn
 * @bm:		Bitmap to look.
 * @pfn:	pfn to look for.
 *
 * This is imported from kernel/power/snapshot.c
 **/
void memory_bm_set_bit(struct memory_bitmap *bm, unsigned long pfn)
{
	void *addr;
	unsigned int bit;
	int error;

	error = memory_bm_find_bit(bm, pfn, &addr, &bit);
	if (!error)
		set_bit(bit, addr);
}

/**
 * memory_bm_clear_bit - unset the bit corresponding to a pfn
 * @bm:		Bitmap to look.
 * @pfn:	pfn to look for.
 *
 * This is imported from kernel/power/snapshot.c
 **/
void memory_bm_clear_bit(struct memory_bitmap *bm, unsigned long pfn)
{
	void *addr;
	unsigned int bit;
	int error;

	error = memory_bm_find_bit(bm, pfn, &addr, &bit);
	if (!error)
		clear_bit(bit, addr);
}
