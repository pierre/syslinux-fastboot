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

#ifndef _RESUME_ASM_H
#define _RESUME_ASM_H

/* Give some room to restore low 1M */
#define BOOT_DATA_ADDR	0x100000
/* Nuke the first pfn, the kernel shouldn't need it anyway (startup stuff) */
#define TRAMPOLINE_ADDR	0x1473000

#endif /* _RESUME_ASM_H */
