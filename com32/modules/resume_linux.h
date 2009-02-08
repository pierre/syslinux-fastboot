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

#ifndef RESUME_LINUX_H
#define RESUME_LINUX_H

#define CONFIG_X86_32

#define PAGE_SHIFT 12
#define PAGE_SIZE 4096 /* 1<<12 */
#define BIT_NUM_MASK ((sizeof(unsigned long) << 3) - 1)
#define PAGE_NUM_MASK (~((1 << (PAGE_SHIFT + 3)) - 1))
#define UL_NUM_MASK (~(BIT_NUM_MASK | PAGE_NUM_MASK))

#ifdef CONFIG_X86_32
# define BITS_PER_LONG 32
#else
# define BITS_PER_LONG 64
#endif

#if BITS_PER_LONG == 32
#define UL_SHIFT 5
#else
#if BITS_PER_LONG == 64
#define UL_SHIFT 6
#else
#error Bits per long not 32 or 64?
#endif
#endif

typedef unsigned int u32;

struct list_head {
	struct list_head *next, *prev;
};

typedef int spinlock_t;

struct new_utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

/*
 *  Convert a physical address to a Page Frame Number and back
 */
#define	__phys_to_pfn(paddr)	((paddr) >> PAGE_SHIFT)
#define	__pfn_to_phys(pfn)	((pfn) << PAGE_SHIFT)

#endif /* RESUME_LINUX_H */
