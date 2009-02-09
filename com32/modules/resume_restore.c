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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TESTING
#include <syslinux/movebits.h>
#include <syslinux/bootrm.h>
#endif /* !TESTING */

#include "resume.h"
#include "resume_debug.h"
#include "resume_restore.h"

static void add_entry_list(struct data_buffers_list* list,
			   struct data_buffers_list* cur,
			   addr_t* data_buffer, unsigned long pfn, int first)
{
	struct data_buffers_list* new_element = malloc(sizeof(struct data_buffers_list));

	/* Create new element */
	new_element->data = malloc(PAGE_SIZE);
	new_element->next = NULL;
	new_element->pfn = pfn;
	memmove(new_element->data, data_buffer, PAGE_SIZE);

	/* First element in a range? */
	if (first) {
		new_element->prev = NULL;
		list = new_element;
		cur = list;
	} else {
		new_element->prev = list;
		cur->next = new_element;
		cur = new_element;
	}

	dprintf("\tAdded restore entry: prev = 0x%p, next = 0x%p,"
		"data = 0x%p\n",
		 cur->prev,
		 cur->next,
		 cur->data);
}

static void extract_data_from_list(struct data_buffers_list* cur,
				   addr_t* shuffling_load_addr)
{
	int i = 0;

	while (cur != NULL) {
		if (cur->next) {
			cur = cur->next;
			memmove(shuffling_load_addr + i, cur->prev->data, PAGE_SIZE);

			dprintf("PFN %lu: staged data from 0x%p at "
				"0x%p\n",
				cur->prev->pfn,
				cur->prev->data,
				shuffling_load_addr + i);

			/* Free the previous element */
			free(cur->prev->data);
			free(cur->prev);

			i += PAGE_SIZE;
		} else { /* Last element in the list */
			memmove(shuffling_load_addr + i, cur->data, PAGE_SIZE);

			dprintf("PFN %lu: staged data from %p at %p\n",
				cur->pfn,
				cur->data,
				shuffling_load_addr + i);

			/* Free the last element */
			free(cur->data);
			free(cur);

			return;
		}
	}
}

/**
 * skip_pagedir2 - move the image pointer after pagedir2
 * @pagedir2_size:	number of pfns in pagedir2
 *
 * pagedir2 is saved first in the image - and reloaded last by TuxOnIce.
 * We don't care about this data - so we just skip it.
 *
 * Side Effects:
 *	toi_image_buffer_posn is moved.
 *	dest_pfn is changed.
 **/
static void skip_pagedir2(long pagedir2_size)
{
	/* Number of pfns found */
	long pfns_found = 0;
	unsigned long* dest_pfn = NULL;
	unsigned long* data_buffer_size;

	/* pagedir2 is PAGE_SIZE aligned */
	do {
		MOVE_TO_NEXT_PAGE
		READ_BUFFER(dest_pfn, unsigned long*);
	} while (!*dest_pfn);
	goto read_buf_size_pagedir2;

	while (pfns_found <= pagedir2_size) {
		READ_BUFFER(dest_pfn, unsigned long*);

read_buf_size_pagedir2:
		/* Read the size of the data */
		data_buffer_size = (unsigned long*) (toi_image_buffer +
						     toi_image_buffer_posn +
						     sizeof(unsigned long));
		if (!*data_buffer_size || *data_buffer_size > PAGE_SIZE) {
			toi_image_buffer_posn += 1;
			/* This is not a bug */
			continue;
		} else {
			pfns_found++;
			/*
			 * This is noisy: ~15K pfn.
			 * But useful in development: add the same printk
			 * in toi_bio_write_page() and check both outputs.
			 */
			dprintf("%lu [%lu]\n",
				 *dest_pfn,
				 *data_buffer_size);
			toi_image_buffer_posn += *data_buffer_size +
						 2 * sizeof(unsigned long);
		}
	}
}
/**
 * load_memory_map - iterate through the bitmaps to setup SYSLINUX memory map
 * @data_len:	Size of the file.
 * @nb_of_pfns:	Total size of both pagedirs.
 *
 * We bring back the data in the right pfns, setup SYSLINUX memory maps, shuffle
 * the memory and setup protected mode. The execute will jump to the trampoline
 * to restore registers and page tables.
 **/
int load_memory_map(unsigned long data_len,
		    long pagedir1_size,
		    long pagedir2_size)
{
	addr_t* data_buffer = malloc(PAGE_SIZE);
	addr_t* data_load_addr = NULL;

	unsigned long* data_buffer_size = NULL;
	unsigned long* dest_pfn = NULL;

	unsigned long* pfn_read = malloc(sizeof(unsigned long));
	unsigned long* start_range_pfn = malloc(sizeof(unsigned long));
	unsigned long* prev_dest_pfn = malloc(sizeof(unsigned long));
	*pfn_read = 0;
	*start_range_pfn = 0;
	*prev_dest_pfn = 0;

	/*
	 * Used to move around chunks of data PAGE_SIZE aligned.
	 * At any given time, we are reading a chunk of data located at
	 * data_buffer:
	 *
	 * -----------------------------------------------------------------
	 * | dest_pfn | data_buffer_size | data_load_addr | dest_pfn | ... |
	 * -----------------------------------------------------------------
	 *
	 * The data at data_load_addr will be stored in data_buffer (and
	 * uncompressed if needed).
	 * data_buffer always contains PAGE_SIZE bytes of data.
	 *
	 * In order to load chunks of pfn ranges, we uncompress data_buffer
	 * and store it in a linked list of data_buffers_list (its size will
	 * be the number of pfns in the range.
	 *
	 * I would love to override my own memory in order save space but the
	 * fact that data_buffer may be compressed (maximum size is PAGE_SIZE)
	 * makes things difficult.
	 *
	 * Note that we assume that pfns are stored in order. If this is not
	 * true, this function adds overhead, instead of optimizing the
	 * restore code path.
	 */
	struct data_buffers_list* data_buffers_list = NULL;
	struct data_buffers_list* data_buffers_list_cur = NULL;

	/* In testing mode, you cannot shuffle the memory :) */
#ifndef TESTING
	int* mapped = 0, syslinux_reserved = 0, highmem_unreachable = 0;
	int err;
	struct toi_boot_kernel_data *bkd;
	struct syslinux_rm_regs regs;

	/* Setup the syslinux memory map */
	mmap = syslinux_memory_map();
	amap = syslinux_dup_memmap(mmap);
	if (!mmap || !amap)
		goto bail;
#endif /* !TESTING */

	/*
	 * Restoring program caches is handled via TuxOnIce, in the resume
	 * code path from the saved kernel.
	 */
	skip_pagedir2(pagedir2_size);

	/* Read all pages back */
	while (toi_image_buffer_posn < data_len && *pfn_read < 9900) {
read_dest_pfn:
		READ_BUFFER(dest_pfn, unsigned long*);
		MOVE_FORWARD_BUFFER_POINTER(sizeof(unsigned long));

		//if (!*prev_dest_pfn)
		//	*start_range_pfn = *dest_pfn;

		/* Read the size of the data */
		READ_BUFFER(data_buffer_size, unsigned long*);

		/*
		 * Valid data?
		 *
		 * A good indicator to check if what we are currently reading
		 * valid is the buffer size. It must be less or equal to
		 * PAGE_SIZE.
		 *
		 * This is also used to jump from pagedir2 to pagedir1.
		 */
		if (!*dest_pfn || *data_buffer_size > PAGE_SIZE
			       || *data_buffer_size <= 0)
			continue;

		if (*dest_pfn > 800059) {
			toi_image_buffer_posn -= sizeof(unsigned long);
			DUMP_PNTR
			break;
		}
		/*
		 * The buffer size seem valid. Let's check the pfn in the bitmap
		 * to be sure.
		 *
		 * XXX IMPROVE ME: Test if the pfn is valid and unset me! This
		 * is to avoid fetching twice the same pfn (shouldn't happen).
		 */
		//if (!bitmap_test_all(*dest_pfn))
		//	//if (*prev_dest_pfn) {
		//	//	printf("PFN %lu not valid. Saving previous range and starting over.\n", dest_pfn);
		//	//	goto save_range;
		//	//}
		//	//else
		//		continue;

		/* Ok, it IS valid */
		MOVE_FORWARD_BUFFER_POINTER(sizeof(unsigned long));

		(*pfn_read)++;
		dprintf("%lu [%lu] (%lu) = %lu\n",
			 toi_image_buffer_posn - 2 * sizeof(unsigned long),
			 *data_buffer_size,
			 *pfn_read,
			 *dest_pfn);

		/* Read the data */
		READ_BUFFER(data_load_addr, addr_t*);
		MOVE_FORWARD_BUFFER_POINTER(*data_buffer_size);

		/* Compressed data? */
		if (*data_buffer_size < PAGE_SIZE) {
			// TODO
			//err = uncompress(uncompr_temp, &uncompr_len,
			//		   data_buffer, *data_buffer_size);
		} else
			memcpy(data_buffer, data_load_addr, *data_buffer_size);

		/*
		 * At that point, data_buffer points to a page of data. It has
		 * been decompressed if needed.
		 */
		continue; //XXX

		/* We bring back to memory continuous chunks of pfns */
		if (*start_range_pfn == *dest_pfn || *prev_dest_pfn == *dest_pfn - 1) {
			/* We have found another contiguous pfn, carry on */
			add_entry_list(data_buffers_list, data_buffers_list_cur, data_buffer,
				       *dest_pfn, *dest_pfn == *start_range_pfn);

			*prev_dest_pfn = *dest_pfn;
			goto read_dest_pfn;
		} else {
			/*
			 * This last pfn is not contiguous with the previous
			 * ones. We need to load into memory the contiguous chunk
			 *    *start_range_pfn..*prev_dest_pfn
			 * and restart to read_dest_pfn.
			 */
save_range:
			dprintf("PFNs restore range %lu..%lu found.\n",
				*start_range_pfn, *prev_dest_pfn);

			/* Create a big chunk of data for later shuffling and free the list of data */
			addr_t* shuffling_load_addr = malloc((*prev_dest_pfn - *start_range_pfn + 1) * PAGE_SIZE);
			extract_data_from_list(data_buffers_list,
					       shuffling_load_addr);

#ifndef TESTING
			/* XXX Check it is absolute pfns and not zone offsets */
			if(memory_map_add(*start_range_pfn,
					  *prev_dest_pfn,
					  shuffling_load_addr,
					  pfn_read,
					  syslinux_reserved,
					  highmem_unreachable,
					  mapped))
				goto bail;
#endif /* !TESTING */

			/* Start looking for a new range */
			*start_range_pfn = *dest_pfn;
			*prev_dest_pfn = 0;
			goto read_dest_pfn;
		}
	}

	/*
	 * Sanity check: have we read all data?
	 */
	if (*pfn_read != pagedir1_size) {
		dprintf("BUG: pfn_read=%lu but pagedir1_size=%lu\n",
				*pfn_read,
				pagedir1_size);
		goto bail; // XXX Is it really a bug?
	}

#ifndef TESTING
	dprintf("%d pages mapped, %d reserved by SYSLINUX, %d unrechable\n",
		*mapped, *syslinux_reserved, *highmem_unreachable);
	//dprintf("Final movelist:\n");
	//syslinux_dump_movelist(stdout, ml);

	if (setup_trampoline_blob())
		goto bail;

	/* Set up registers */
	memset(&regs, 0, sizeof regs);
	regs.ip    = 0x7c00;
	regs.esp.l = 0x7c00;

// XXX Restore other registers? Not here probably.
	/*
	 * The memory map is ready. We now jump into protected mode. We will
	 * then restore the registers and page tables before jumping into the
	 * kernel.
	 */
	fputs("Setting up protected mode...\n", stdout);
	syslinux_shuffle_boot_rm(ml, mmap, 0, &regs);
#endif

bail:
	return -1;
}

/**
 * memory_map_add - relocate a range of pfns
 * @start_range_pfn:		minimum pfn to add
 * @end_range_pfn:		maximum pfn to add
 * @data_addr:			address of data to save
 * @pfn_read:			number of valid pfns read so far
 * @syslinux_reserved:		number of pfns skipped so far (occupied by SYSLINUX)
 * @highmem_unreachable:	number of pfns skipped so far (> 4GB)
 * @mapped:			number of pfns successfully mapped so far
 *
 * The specified range of pfns is added to the SYSLINUX memory map.
 * We expect absolute pfns, not zone offsets.
 *
 * data_addr points to a big chunk af data. Potentially not all of it will be relocated.
 *
 * Side effects:
 * 	pfn_read, syslinux_reserved, highmem_unreachable and mapped are modified, if needed.
 **/
int memory_map_add(unsigned long start_range_pfn,
		   unsigned long end_range_pfn,
		   addr_t* data_addr,
		   int* pfn_read,
		   int* syslinux_reserved,
		   int* highmem_unreachable,
		   int* mapped)
{
#ifndef TESTING
	int nb_of_pfns;
	/* Destination for data after relocation */
	addr_t* load_addr = NULL;
	/* Address of the highest most page */
	addr_t* upper_addr = NULL;
	/* Size of the data to shuffle */
	addr_t dsize = NULL;

	addr_t* final_data_addr = data_addr;
#endif

	/*
	 * Given the range of pfns, we check that they are really marked as
	 * occupied in the bitmap.
	 * If all of them are marked, we unmark them all and proceed. If (at
	 * least) one is not, we bail out.
	 *
	 * XXX: Is that what we want? Maybe too strict.
	 */
	//if (bitmap_test_and_unset(start_range_pfn, end_range_pfn)) {
	if (1) {
		/* Keep track of the number of pages found */
		*pfn_read += (end_range_pfn - start_range_pfn + 1);

		/*
		 * Note: in a typical (small) file, there will be ~16K of
		 * pages!
		 */

#ifndef TESTING
		/*
		 * This portion is not available (used by syslinux)
		 * We can ask the kernel to reserve the first 64K of
		 * the memory (technically for BIOS bugs). See:
		 *	X86_RESERVE_LOW_64K
		 *	git: fc38151947477596aa27df6c4306ad6008dc6711
		 * You need a 2.6.28 kernel or more recent.
		 */
		load_addr = __pfn_to_phys(start_range_pfn);
		while (load_addr < 0x7c00) {
			(*syslinux_reserved)++;
			start_range_pfn++; /* Skip this pfn */

			final_data_addr += PAGE_SIZE;
//XXX Is that right? mega mix data_addr and load_addr.... Find better names.
		}
		load_addr = __pfn_to_phys(start_range_pfn);

		/*
		 * SYSLINUX cannot access memory > 4GB.
		 * This patch will work with:
		 *	CONFIG_HIGHMEM4G
		 * but _NOT_ with CONFIG_HIGHMEM64G
		 */
		upper_addr = __pfn_to_phys(end_range_pfn);
		while (upper_addr > 0x100000000) {
			(*highmem_unreachable)++;
			end_range_pfn--; /* Skip this pfn */
			/*
			 * Note: this is always false since load_addr is
			 * uint32_t. But I kept it not to forget...
			 */
		}
		upper_addr = __pfn_to_phys(end_range_pfn);

		/* XXX Free data_addr...final_data_addr and end_range...END */

		/* Keep track of the number of pages actually mapped */
		nb_of_pfns = (end_range_pfn - start_range_pfn + 1);
		mapped += nb_of_pfns;

		dsize = PAGE_SIZE * nb_of_pfns;
		//dprintf("Segment at 0x%08x data 0x%08x len 0x%08x\n",
		//	  load_addr, data_buffer, dsize);

		/* Memory region available? */
		if (syslinux_memmap_type(amap, load_addr,
					 dsize) != SMT_FREE) {
			//printf("Memory segment at 0x%08x (len"
			//	 "0x%08x) is unavailable\n",
			//	 load_addr, size_to_reserve);
			goto bail;
		}

		/*
		 * Mark this region as allocated in the
		 * available map
		 */
		if (syslinux_add_memmap(&amap,
					load_addr, dsize,
					SMT_ALLOC))
			goto bail;

		/*
		 * Data present region. Create a move entry
		 * for it.
		 */
		if (syslinux_add_movelist(&ml, load_addr,
					  (addr_t)data_addr,
					  dsize))
			goto bail;

#endif /* !TESTING */
	} else
		goto bail;

	return 0;

bail:
#ifndef TESTING
	syslinux_free_memmap(amap);
	syslinux_free_memmap(mmap);
	syslinux_free_movelist(ml);
#endif /* !TESTING */
	return -1;
}
