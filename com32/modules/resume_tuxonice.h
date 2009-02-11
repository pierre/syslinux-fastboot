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

#ifndef _RESUME_TUXONICE_H
#define _RESUME_TUXONICE_H

#include <string.h>

#include "resume_linux.h"

/*
 * resume_tuxonice.h
 *
 * TuxOnIce header definitions
 *
 * See kernel/power/tuxonice_*.{h,c}
 */

struct pagedir {
	int id;
	long size;
#ifdef CONFIG_HIGHMEM
	long size_high;
#endif
};

#ifdef DYN_PAGEFLAGS
/**
 * dyn_pageflags - store bitmaps
 **/
struct dyn_pageflags {
	unsigned long ****bitmap; /* [pg_dat][zone][page_num] */
	int sparse, initialised;
	struct list_head list;
	spinlock_t struct_lock;
} pagemap;
#else
struct pageflags {
	unsigned long ****bitmap; /* [pg_dat][zone][page_num] */
} pagemap;
#endif /* !DYN_PAGEFLAGS */

/*
 * The following definitions are specific to the File Allocator.
 * See kernel/power/tuxonice_file.{h,c}
 */

/* Old signatures */
static char HaveImage[] = "HaveImage\n";
#define sig_size (sizeof(HaveImage) + 1)
/* Note: static char NoImage[] =   "TuxOnIce\n"; */

/* Binary signature */
static char *tuxonice_signature = "\xed\xc3\x02\xe9\x98\x56\xe5\x0c";

/**
 * toi_file_header - file header
 *
 * This is not the pristine one from TuxOnIce. The fields:
 *	+ devinfo_sz
 *	+ nodes_num
 *	+ zones_num
 * have been added.
 * And the following misses zones_start_pfn and zones_max_offset;
 **/
struct toi_file_header {
	char sig[sig_size];			/* TuxOnIce signature */
	int resumed_before;			/* Unused */
	unsigned long first_header_block;	/* Unused */
	int have_image;				/* Unused */
	int devinfo_sz;				/* Typically 16 */
	int num_nodes;				/* MAX_NUMNODES */
	int num_zones;				/* MAX_NR_ZONES */
};

/**
 * parse_signature - read the signature from the saved image
 * @header:	signature header
 *
 * Return true iff the signature header contains TuxOnIce binary
 * signature, meaning an image is present.
 **/
static __inline__ int parse_signature(struct toi_file_header *header)
{
	return !memcmp(tuxonice_signature, header->sig,
			sizeof tuxonice_signature);
}

/**
 * toi_header - header proper
 *
 * Non-module data saved in our image header
 **/
struct toi_header {
	/*
	 * Mirror struct swsusp_info, but without
	 * the page aligned attribute
	 */
	struct new_utsname uts;
	u32 version_code;
	unsigned long num_physpages;
	int cpus;
	unsigned long image_pages;
	unsigned long pages;
	unsigned long size;

	/* Our own data */
	unsigned long orig_mem_free;
	int page_size;
	int pageset_2_size;
	int param0;
	int param1;
	int param2;
	int param3;
	int progress0;
	int progress1;
	int progress2;
	int progress3;
	int io_time[2][2];
	struct pagedir pagedir;
	unsigned long root_fs; /* dev_t */
	unsigned long bkd; /* Boot kernel data locn */
};

/*
 * Extents are used to load the image (TuxOnIce doesn't have access to a file
 * system). No need for us since SYSLINUX is smart enough to understand files.
 */

struct hibernate_extent_chain {
	int size; /* size of the chain ie sum (max-min+1) */
	int num_extents;
	struct hibernate_extent *first, *last_touched;
};

struct hibernate_extent {
	unsigned long start, end;
	int lost_offset;
	struct hibernate_extent *next;
};

struct hibernate_extent_iterate_saved_state {
	int chain_num;
	int extent_num;
	unsigned long offset;
};

/* This is the maximum size we store in the image header for a module name */
#define TOI_MAX_MODULE_NAME_LENGTH 30

/**
 * toi_module_header - per-module metadata
 **/
struct toi_module_header {
	char name[TOI_MAX_MODULE_NAME_LENGTH];
	int enabled;
	int type;
	int index;
	int data_length;
	unsigned long signature;
};

#define COMMAND_LINE_SIZE	0

/**
 * toi_boot_kernel_data - misc kernel data
 **/
struct toi_boot_kernel_data {
	int version;
	int size;
	unsigned long toi_action;
	unsigned long toi_debug_state;
	int toi_default_console_level;
	int toi_io_time[2][2];
	char toi_nosave_commandline[COMMAND_LINE_SIZE];
};

#endif /* _RESUME_TUXONICE_H */
