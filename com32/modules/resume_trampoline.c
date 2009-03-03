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
#include "resume_linux.h"
#include "resume_debug.h"
#include "resume_symbols.h"
#include "resume_trampoline.h"

extern struct syslinux_memmap *mmap, *amap;
extern struct syslinux_movelist *ml;

/* Defined in resume_trampoline_asm.S */
extern int trampoline_start(void);
extern const void trampoline_end;

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
int setup_trampoline_blob(void)
{
	size_t trampoline_size = (void *)&trampoline_end -
				 (void *)&trampoline_start;

	if (syslinux_memmap_type(amap, 0x8000, trampoline_size) != SMT_FREE)
		return -1;

	if (syslinux_add_memmap(&amap, 0x8000, trampoline_size, SMT_ALLOC))
		return -1;

	if (syslinux_add_movelist(&ml, 0x8000, (addr_t) trampoline_start,
							trampoline_size))
		return -1;

	dprintf("Trampoline: 0x%08x (size %#8.8x)\n", (addr_t) trampoline_start,
							   trampoline_size);
	return 0;
}
