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
#include <syslinux/bootpm.h>
#include "resume_trampoline.h"
#endif /* !TESTING */

#include "resume.h"
#include "resume_debug.h"
#include "resume_restore.h"
#include "resume_bitmaps.h"
#include "resume_lzf.h"
#include "resume_symbols.h"

#ifndef TESTING
/* Syslinux variables */
struct syslinux_memmap *mmap = NULL, *amap = NULL;
struct syslinux_pm_regs regs;
struct syslinux_movelist *ml = NULL;
extern void (*trampoline) (void);

extern unsigned long __nosave_begin;

/**
 * memory_map_add - relocate a range of pfns
 * @start_range_pfn:		minimum pfn to add
 * @end_range_pfn:		maximum pfn to add
 * @data_addr:			address of data to save
 * @syslinux_reserved:		number of pfns skipped so far (occupied by
 *				SYSLINUX)
 * @highmem_unreachable:	number of pfns skipped so far (> 4GB)
 * @mapped:			number of pfns successfully mapped so far
 *
 * The specified range of pfns is added to the SYSLINUX memory map.
 * We expect absolute pfns, not zone offsets.
 *
 * data_addr points to a big chunk af data. Potentially not all of it will be
 * relocated.
 *
 * Side effects:
 *	syslinux_reserved, highmem_unreachable and mapped are modified,
 *	if needed.
 **/
static int memory_map_add(unsigned long start_range_pfn,
			  unsigned long end_range_pfn,
			  addr_t data_addr,
			  unsigned int* syslinux_reserved,
			  unsigned int* highmem_unreachable,
			  unsigned int* mapped)
{
	unsigned int nb_of_pfns, i;
	unsigned long final_start_range_pfn = start_range_pfn;
	unsigned long final_end_range_pfn = end_range_pfn;
	addr_t data_location = data_addr;

	/* Destination for data after relocation */
	addr_t final_load_addr;
	/* Address of the highest most page after relocation */
	addr_t final_upper_addr;
	/* Size of the data to shuffle */
	addr_t dsize;

	/*
	 * This portion is not available (used by syslinux)
	 * We can ask the kernel to reserve the first 64K of
	 * the memory (technically for BIOS bugs). See:
	 *
	 *	X86_RESERVE_LOW_64K
	 *	git: fc38151947477596aa27df6c4306ad6008dc6711
	 *
	 * You need a 2.6.28 kernel or more recent.
	 * XXX Take into account the trampoline!
	 */
	final_load_addr = __pfn_to_phys(final_start_range_pfn);
	while (final_load_addr < 0x7c00) {
		/* Debug info */
		(*syslinux_reserved)++;

		/* Skip this pfn */
		final_start_range_pfn++;
		final_load_addr = __pfn_to_phys(final_start_range_pfn);
	}

	/*
	 * SYSLINUX cannot access memory > 4GB.
	 * This patch will work with:
	 *
	 *	CONFIG_HIGHMEM4G
	 *
	 * but _NOT_ with
	 *
	 *	CONFIG_HIGHMEM64G
	 *
	 * Note: this is always false since upper_addr is
	 * uint32_t. But I kept it not to forget...
	 */
	final_upper_addr = __pfn_to_phys(final_end_range_pfn);
	while (final_upper_addr > 0x100000000) {
		/* Debug info */
		(*highmem_unreachable)++;

		/* Skip this pfn */
		final_end_range_pfn--;
		final_upper_addr = __pfn_to_phys(final_end_range_pfn);
	}

	/*
	 * Free [start_range_pfn...final_start_range_pfn[
	 * This memory is located between:
	 *
	 *	[data_addr...data_location[
	 */
	for (i = start_range_pfn; i < final_start_range_pfn; i++)
		continue; //TODO

	/* Free [start_range_pfn...final_start_range_pfn[ */
	for (i = end_range_pfn + 1; i <= final_end_range_pfn; i++)
		continue; //TODO

	/* Keep track of the number of pages actually mapped */
	nb_of_pfns = (final_end_range_pfn - final_start_range_pfn + 1);
	(*mapped) += nb_of_pfns;

	dsize = PAGE_SIZE * nb_of_pfns;
	// XXX Check if right (char*?)
	data_location = data_addr + PAGE_SIZE *
				(final_start_range_pfn - start_range_pfn);

#ifdef METADATA_DEBUG
	/*
	 * Noisy. In a typical file, there will be ~15K of pages!
	 * This output is similar to syslinux_dump_movelist() (in reverse order
	 * though).
	 */
	dprintf("0x%08x 0x%08x 0x%08x %lu-%lu\n",
		final_load_addr, data_location, dsize,
		final_start_range_pfn, final_end_range_pfn);
#endif /* METADATA_DEBUG */

	/* Memory region available? */
	if (syslinux_memmap_type(amap, final_load_addr,
				 dsize) != SMT_FREE) {
		/*
		 * See include/syslinux/movebits.h
		 * If the memory region has no e820 type (error 0 -
		 * SMT_UNDEFINED), this is probably because there is not enough
		 * RAM. Typical mistake in a VM with only 256M.
		 */
		printf("BUG: Memory segment at 0x%08x (len %d) is "
		       "unavailable: error=%d\n",
		       final_load_addr, dsize,
		       syslinux_memmap_type(amap, final_load_addr, dsize));
		goto bail;
	}

	/* Mark this region as allocated in the available map */
	if (syslinux_add_memmap(&amap,
				final_load_addr, dsize,
				SMT_ALLOC))
		goto bail;

	/* Data present region. Create a move entry for it. */
	if (syslinux_add_movelist(&ml, final_load_addr,
				  data_location,
				  dsize))
		goto bail;

	return 0;

bail:
	syslinux_free_memmap(amap);
	syslinux_free_memmap(mmap);
	syslinux_free_movelist(ml);
	return 1;
}
#endif /* !TESTING */

/**
 * add_entry_list - add an entry to a data_buffers_list
 * @list:		Pointer to the beginning of the list.
 * @cur:		Pointer to the end of the list.
 * @data_buffer:	Pointer to the data corresponding to @pfn.
 * @pfn:		pfn to add to the list.
 *
 * A data_buffers_list describes a contiguous range of pfns to be restored.
 * Each entry contains an entry to the corresponding data (uncompressed) that
 * will need to be restored.
 *
 * list always points to the first element (containing start_range_pfn).
 * cur always point to the last element of the range (containing
 * prev_dest_pfn).
 * When adding the first entry, both are NULL pointers. In that case, the list
 * will be bootstrapped.
 **/
static void add_entry_list(struct data_buffers_list** list,
			   struct data_buffers_list** cur,
			   addr_t* data_buffer, unsigned long pfn)
{
	struct data_buffers_list* new_element =
				malloc(sizeof(struct data_buffers_list));

	/* Create new element */
	new_element->data = malloc(PAGE_SIZE);
	new_element->next = NULL;
	new_element->pfn = pfn;
	memmove(new_element->data, data_buffer, PAGE_SIZE);

	/* First element in a range? */
	if (!*list) {
		new_element->prev = NULL;
		*list = new_element;
		*cur = new_element;
	} else {
		new_element->prev = *cur;
		(*cur)->next = new_element;
		*cur = new_element;
	}
}

/**
 * extract_data_from_list - extract all data from a given list
 * @orig_list:			List of data_buffers_list to consider.
 * @shuffling_load_addr:	Beginning of the chunk that will contain the
 *				data.
 *
 * Given a list of contiguous pfn, move all data from the list to a contiguous
 * chunk of memory.
 * This chunk will be relocated to its final destination by SYSLINUX.
 * The memory needs to be allocated before calling this routine.
 **/
static void extract_data_from_list(struct data_buffers_list** orig_list,
				   char* shuffling_load_addr)
{
	int i = 0;
	struct data_buffers_list* list = *orig_list;

	while (list != NULL) {
		if (list->next) {
			list = list->next;
			memmove(shuffling_load_addr + i, list->prev->data,
				PAGE_SIZE);

#ifdef METADATA_DEBUG
			dprintf("PFN %lu: staged data from %p to %p\n",
				list->prev->pfn,
				list->prev->data,
				shuffling_load_addr + i);
#endif /* METADATA_DEBUG */

			/* Free the previous element */
			free(list->prev->data);
			free(list->prev);

			i += PAGE_SIZE;
		} else { /* Last element in the list - or only element */
			memmove(shuffling_load_addr + i, list->data, PAGE_SIZE);

#ifdef METADATA_DEBUG
			dprintf("PFN %lu: staged data from %p at %p\n",
				list->pfn,
				list->data,
				shuffling_load_addr + i);
#endif /* METADATA_DEBUG */

			/* Free the last element */
			free(list->data);
			free(list);
			*orig_list = NULL;

			return;
		}
	}
}

/**
 * skip_pagedir2 - move the image pointer after pagedir2
 * @pagedir2_size:	Number of pfns in pagedir2.
 *
 * pagedir2 is saved first in the image - and reloaded last by TuxOnIce.
 * We don't care about this data - so we just skip it.
 *
 * Side Effects:
 *	toi_image_buffer_posn is moved.
 *	dest_pfn is changed.
 **/
static int skip_pagedir2(long pagedir2_size)
{
	long pfns_found = 0;
	unsigned long* dest_pfn = NULL;
	unsigned int* data_buffer_size;

	/* pagedir2 is PAGE_SIZE aligned */
	do {
		MOVE_TO_NEXT_PAGE
		READ_BUFFER(dest_pfn, unsigned long*);
	} while (!*dest_pfn);
	goto read_buf_size_pagedir2;

	while (pfns_found < pagedir2_size) {
		READ_BUFFER(dest_pfn, unsigned long*);

read_buf_size_pagedir2:
		/* Read the size of the data */
		data_buffer_size = (unsigned int*) (toi_image_buffer +
						    toi_image_buffer_posn +
						    sizeof(unsigned long));
		if (!*data_buffer_size || *data_buffer_size > PAGE_SIZE) {
			toi_image_buffer_posn += 1;
			/* This is not a bug */
			continue;
		} else if (!*dest_pfn) {
			/* This is a bug */
			printf("BUG: end of pagedir2 reached but still "
			       "pages to skip.\n");
			printf("PFN %lu/%lu: %lu [%d]\n",
			       pfns_found,
			       pagedir2_size,
			       *dest_pfn,
			       *data_buffer_size);
			goto bail;
		} else {
			pfns_found++;
			toi_image_buffer_posn += *data_buffer_size +
						 sizeof(unsigned int) +
						 sizeof(unsigned long);

			/*
			 * This is noisy: ~15K pfn.
			 * But useful in development: add the same printk
			 * in toi_bio_write_page() and check both outputs.
			 */
#ifdef METADATA_DEBUG
			dprintf("%lu [%d]\n",
				 *dest_pfn,
				 *data_buffer_size);
#endif
		}
	}
	return 0;

bail:
	return -1;
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
		    unsigned long pagedir1_size,
		    unsigned long pagedir2_size)
{
	void* data_buffer = malloc(PAGE_SIZE);
	u8* data_load_addr = NULL;
	addr_t* shuffling_load_addr;

	unsigned int* data_buffer_size = NULL;
	unsigned long* dest_pfn = NULL;

	unsigned int pfn_read = 0;
	unsigned long start_range_pfn = 0;
	unsigned long prev_dest_pfn = 0;

	/* lzf support */
	u8* uncompr_tmp = malloc(PAGE_SIZE);

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
	unsigned int mapped = 0;
	unsigned int syslinux_reserved = 0;
	unsigned int highmem_unreachable = 0;

	/* Setup the syslinux memory map */
	mmap = syslinux_memory_map();
	amap = syslinux_dup_memmap(mmap);
	if (!mmap || !amap)
		goto bail;

	if (get_missing_symbols_from_saved_kernel()) {
		printf("BUG: error while loading symbols.\n");
		goto bail;
	}

	/*
	 * The trampoline must be setup first to reserve the area.
	 * XXX return size see above
	 */
	if (setup_trampoline_blob()) {
		printf("BUG: error when setting up the trampoline.\n");
		goto bail;
	}
#endif /* !TESTING */

	/*
	 * Restoring program caches is handled via TuxOnIce, in the resume
	 * code path from the saved kernel.
	 */
	if (skip_pagedir2(pagedir2_size)) {
		printf("Error while skipping pagedir2.\n");
		goto bail;
	}

	/* pagedir1 is PAGE_SIZE aligned */
	do {
		MOVE_TO_NEXT_PAGE
		READ_BUFFER(dest_pfn, unsigned long*);
	} while (!*dest_pfn);
	goto read_buf_size_pagedir1;

	/*
	 * Read all pages back - this is the main loop to bring back the pages
	 * in memory
	 */
	while (toi_image_buffer_posn < data_len && pfn_read < pagedir1_size) {
		READ_BUFFER(dest_pfn, unsigned long*);

read_buf_size_pagedir1:
		/* Read the size of the data */
		READ_BUFFER(data_buffer_size, unsigned int*);

		/* Read the size of the data */
		data_buffer_size = (unsigned int*) (toi_image_buffer +
						     toi_image_buffer_posn +
						     sizeof(unsigned long));
		if (!*data_buffer_size || *data_buffer_size > PAGE_SIZE) {
			/* This is a bug: wrong buffer size */
			printf("BUG: Wrong buffer size while reading pagedir1.\n");
			printf("\tPFN %lu: buffer_size=%d\n",
			       *dest_pfn,
			       *data_buffer_size);
			goto bail;
		}

		/*
		 * The buffer size seem valid. Advance the pointer and
		 * check if the pfn is in the bitmap.
		 */
		toi_image_buffer_posn += sizeof(unsigned int) +
					 sizeof(unsigned long);
		// XXX BROKEN
		//if (!memory_bm_test_pfn(pageset1, *dest_pfn)) {
		//	/* That shouldn't happen */
		//	dprintf("Skipping pfn %lu\n", *dest_pfn);
		//	continue;
		//}

		/* Ok, it IS valid */
		pfn_read++;

#ifdef METADATA_DEBUG
		/*
		 * This is useful to debug the parser code. Add a prink() in
		 * do_rw_loop() and check if the pfns match.
		 * Debugging the code related to pageset1 is tough. There is no
		 * (easy) way to check the values since it is always saved last
		 * and reloaded first (hence always vanishes).
		 * One solution is to hibernate in a Virtual Machine and connect
		 * a virtual serial port to a pipe.
		 */
		dprintf("%lu [%d]\n",
			 *dest_pfn,
			 *data_buffer_size);
#endif /* METADATA_DEBUG */

		/* Read the data */
		READ_BUFFER(data_load_addr, u8*);
		MOVE_FORWARD_BUFFER_POINTER(*data_buffer_size);

		/* Compressed data? */
		if (*data_buffer_size < PAGE_SIZE) {
			unsigned int uncompr_len;
			int error;

			error = lzf_decompress(data_load_addr,
					       *data_buffer_size,
					       uncompr_tmp,
					       &uncompr_len);
			if (uncompr_len != PAGE_SIZE) {
				printf("BUG: error when uncompressing data.\n");
				if (uncompr_len)
					printf("PFN %lu: uncompressed size is "
					       "%d != %d\n",
					       *dest_pfn, uncompr_len,
					       PAGE_SIZE);
				else
					printf("PFN %lu: error=%d.\n",
					       *dest_pfn, error);
				goto bail;
			} else
				memmove(data_buffer, uncompr_tmp, PAGE_SIZE);
		} else /* *data_buffer_size == PAGE_SIZE */
			memmove(data_buffer, data_load_addr, *data_buffer_size);

		/*
		 * At that point, data_buffer points to a page of data. It has
		 * been decompressed if needed.
		 * The buffer pointer is already in the right position for the
		 * next read.
		 * Below, we only process the data for SYSLINUX.
		 */

		/*
		 * We bring back to memory continuous chunks of pfns: this is to
		 * optimize the shuffle code.
		 * For a regular file with ~10K pages, this optimization reduces
		 * the number of chunks to shuffle to ~600.
		 */
		if (!start_range_pfn || prev_dest_pfn == *dest_pfn - 1) {
save_dest_pfn:
			/* We have found another contiguous pfn, carry on */
			add_entry_list(&data_buffers_list,
				       &data_buffers_list_cur,
				       data_buffer, *dest_pfn);

			/* This happens only when creating a new range */
			if (!start_range_pfn)
				start_range_pfn = *dest_pfn;

			prev_dest_pfn = *dest_pfn;

			/*
			 * This happens only if the last pfn is alone (not part
			 * of a range (see below).
			 */
			if (pfn_read == pagedir1_size) {
				pfn_read++;
				goto extract_restore_list;
			}
		} else {
extract_restore_list:
			/*
			 * This last pfn is not contiguous with the previous
			 * ones. We need to load into memory the contiguous chunk
			 *	start_range_pfn..prev_dest_pfn
			 * and restart to read_dest_pfn.
			 */
#ifdef METADATA_DEBUG
			dprintf("\nPFNs restore range %lu..%lu found. New one "
				"will start at: %lu\n",
				start_range_pfn, prev_dest_pfn, *dest_pfn);
			dump_restore_list(data_buffers_list);
#endif /* METADATA_DEBUG */

			/*
			 * Create a big chunk of data for later shuffling and
			 * free the list of data
			 * This will also free the buffer list.
			 */
			shuffling_load_addr = malloc((prev_dest_pfn -
						      start_range_pfn + 1) *
						      PAGE_SIZE);
			extract_data_from_list(&data_buffers_list,
					       (char *) shuffling_load_addr);

#ifndef TESTING
			/* XXX Check it is absolute pfns and not zone offsets */
			if(memory_map_add(start_range_pfn,
					  prev_dest_pfn,
					  (addr_t) shuffling_load_addr,
					  &syslinux_reserved,
					  &highmem_unreachable,
					  &mapped))
				goto bail;
#endif /* !TESTING */

			/*
			 * This only happens if the last pfn wasn't part of a
			 * range (see above).
			 */
			if (pfn_read > pagedir1_size) {
				/*
				 * We added one in pfn_read to detect it was
				 * the last entry. We need to remove it for the
				 * sanity check below
				 */
				pfn_read -= 1;
				break;
			}
			else { /* very likely */
				/*
				 * Start looking for a new range after saving the
				 * current dest_pfn.
				 */
				start_range_pfn = 0;
				goto save_dest_pfn;
			}
		}
	}

	/* Sanity check: have we read all data? */
	if (pfn_read != pagedir1_size) {
		dprintf("BUG: pfn_read=%d but pagedir1_size=%lu\n",
				pfn_read,
				pagedir1_size);
		/* This is really bad - we cannot resume */
		goto bail;
	} else
		dprintf("All data have been read.\n");

	/* Cleanups */
	free(toi_image_buffer);
	free(data_buffer);
	free(uncompr_tmp);

#ifndef TESTING
	dprintf("%d pages mapped, %d reserved by SYSLINUX, %d unreachable\n",
		mapped, syslinux_reserved, highmem_unreachable);

	/* Set up registers */
	memset(&regs, 0, sizeof regs);
	regs.eip = __nosave_begin;
	regs.ebx = __nosave_begin;

#ifdef METADATA_DEBUG
	dprintf("Final memory map:\n");
	syslinux_dump_memmap(stdout, mmap);

	dprintf("Final available map:\n");
	syslinux_dump_memmap(stdout, amap);

	dprintf("Final movelist:\n");
	syslinux_dump_movelist(stdout, ml);
#endif /* METADATA_DEBUG */

	/*
	 * The memory map is ready. We now jump into protected mode. We will
	 * then restore the registers and page tables before jumping into the
	 * kernel.
	 */
	fputs("Setting up protected mode...\n", stdout);
	syslinux_shuffle_boot_pm(ml, mmap, 0, &regs);

	/* If here, not in PM, give up. */
	asm volatile ("int $0x19");
#endif /* !TESTING */

bail:
	return -1;
}
