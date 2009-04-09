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

#include <stdlib.h>
#include <string.h>

#include <syslinux/movebits.h>
#include <syslinux/bootrm.h>

#include "resume.h"
#include "resume_asm.h"
#include "resume_linux.h"
#include "resume_tuxonice.h"
#include "resume_debug.h"
#include "resume_symbols.h"
#include "resume_trampoline.h"

extern struct syslinux_memmap *mmap, *amap;
extern struct syslinux_movelist *ml;

/* Defined in resume_trampoline_asm.S */
extern unsigned long trampoline_start(void);
extern unsigned long trampoline_end;

extern unsigned long boot_data_start(void);
extern unsigned long boot_data_end;

extern const unsigned long __nosave_begin;
extern const unsigned long __nosave_end;
extern const unsigned long toi_bkd_address;
extern const unsigned long toi_in_hibernate_address;
extern const unsigned long in_suspend_address;

struct toi_boot_kernel_data toi_bkd = {
	.version = MY_BOOT_KERNEL_DATA_VERSION,
	.size = 0,
	.toi_action = (1 << TOI_REPLACE_SWSUSP) |
		      (1 << TOI_NO_FLUSHER_THREAD) |
		      (1 << TOI_PAGESET2_FULL) | (1 << TOI_LATE_CPU_HOTPLUG),
};

/**
 * setup_trampoline_blob - prepare the code to reload the CPU saved state
 *
 * From the SYSLINUX doc:
 *
 *	Protected mode is entered with all data segments set up as a
 *	flat 32-bit read/write segment and the code segment a flat 32-bit
 *	read/execute segment.  Interrupts and paging is off, CPL=0, DF=0;
 *	however, GDT, LDT and IDT are undefined, so it is up to the
 *	invoked code to set new descriptor tables to its liking.
 *
 * Hence, some registers and segment registers can be set via
 *
 *	struct syslinux_pm_regs;
 *
 * These registers are cs (code segment), ds (data segment), and ss (stack
 * segment).
 *
 * But we need to jump into a temporary code to setup GDT, IDT, and cr3
 * (restore them from the image).
 *
 * To do this, a blob of code malloc'ed is mapped to 0x7c00.
 **/
size_t trampoline_size, boot_data_size;
int toi_in_hibernate = 1;
int in_suspend = 1;
int setup_trampoline_blob(void)
{
	trampoline_size = (void *)&trampoline_end -
			  (void *)&trampoline_start;

	if (syslinux_memmap_type(amap, TRAMPOLINE_ADDR,
				 trampoline_size) != SMT_FREE)
		return -1;

	if (syslinux_add_memmap(&amap, TRAMPOLINE_ADDR,
				trampoline_size, SMT_ALLOC))
		return -1;

	if (syslinux_add_movelist(&ml, TRAMPOLINE_ADDR,
				  (addr_t) trampoline_start, trampoline_size))
		return -1;

	dprintf("Trampoline       (size %#8.8x): [0x%08x .. 0x%08lx] (%lu -> %lu)\n",
		 trampoline_size, (addr_t) TRAMPOLINE_ADDR,
		 TRAMPOLINE_ADDR + trampoline_size,
		 __phys_to_pfn((addr_t) TRAMPOLINE_ADDR),
		 __phys_to_pfn(TRAMPOLINE_ADDR + trampoline_size));

	boot_data_size = (void *)&boot_data_end -
			 (void *)&boot_data_start;

	if (syslinux_memmap_type(amap, BOOT_DATA_ADDR,
				 boot_data_size) != SMT_FREE)
		return -1;

	if (syslinux_add_memmap(&amap, BOOT_DATA_ADDR,
				boot_data_size, SMT_ALLOC))
		return -1;

	if (syslinux_add_movelist(&ml, BOOT_DATA_ADDR,
				  (addr_t) boot_data_start, boot_data_size))
		return -1;

	dprintf("Boot data        (size %#8.8x): [0x%08x .. 0x%08x] (%lu -> %lu)\n",
		 boot_data_size, BOOT_DATA_ADDR, BOOT_DATA_ADDR + boot_data_size,
		 __phys_to_pfn(BOOT_DATA_ADDR),
		 __phys_to_pfn(BOOT_DATA_ADDR + boot_data_size));

	/* Setup data needed by TuxOnIce */
	if (syslinux_memmap_type(amap, toi_bkd_address,
				 sizeof(toi_bkd)) != SMT_FREE)
		return -1;

	if (syslinux_add_memmap(&amap, toi_bkd_address,
				sizeof(toi_bkd), SMT_ALLOC))
		return -1;

	if (syslinux_add_movelist(&ml, toi_bkd_address,
				  (addr_t) &toi_bkd, sizeof(toi_bkd)))
		return -1;

	dprintf("toi_bkd          (size %#8.8x): [0x%08x .. 0x%08x] (%lu -> %lu)\n",
		 sizeof(toi_bkd), toi_bkd_address, toi_bkd_address + sizeof(toi_bkd),
		 __phys_to_pfn(toi_bkd_address),
		 __phys_to_pfn(toi_bkd_address + sizeof(toi_bkd)));

	if (syslinux_memmap_type(amap, toi_in_hibernate_address,
				 sizeof(toi_in_hibernate)) != SMT_FREE)
		return -1;

	if (syslinux_add_memmap(&amap, toi_in_hibernate_address,
				sizeof(toi_in_hibernate), SMT_ALLOC))
		return -1;

	if (syslinux_add_movelist(&ml, toi_in_hibernate_address,
				  (addr_t) &toi_in_hibernate, sizeof(toi_in_hibernate)))
		return -1;

	dprintf("toi_in_hibernate (size %#8.8x): [0x%08x .. 0x%08x] (%lu -> %lu)\n",
		 sizeof(toi_in_hibernate), toi_in_hibernate_address,
		 toi_in_hibernate_address + sizeof(toi_in_hibernate),
		 __phys_to_pfn(toi_in_hibernate_address),
		 __phys_to_pfn(toi_in_hibernate_address + sizeof(toi_in_hibernate)));

	/* Needed by swsusp */
	if (syslinux_memmap_type(amap, in_suspend_address,
				 sizeof(in_suspend)) != SMT_FREE)
		return -1;

	if (syslinux_add_memmap(&amap, in_suspend_address,
				sizeof(in_suspend), SMT_ALLOC))
		return -1;

	if (syslinux_add_movelist(&ml, in_suspend_address,
				  (addr_t) &in_suspend, sizeof(in_suspend)))
		return -1;

	dprintf("in_suspend       (size %#8.8x): [0x%08x .. 0x%08x] (%lu -> %lu)\n",
		 sizeof(in_suspend), in_suspend_address,
		 in_suspend_address + sizeof(in_suspend),
		 __phys_to_pfn(in_suspend_address),
		 __phys_to_pfn(in_suspend_address + sizeof(in_suspend)));

	return 0;
}
