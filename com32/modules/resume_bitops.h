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

#ifndef RESUME_BITOPS_H
#define RESUME_BITOPS_H

int test_bit(int, const u32*);
void clear_bit(int, u32*);
void showbits(unsigned long *);
inline unsigned long __ffs(unsigned long);

#endif /* RESUME_BITOPS_H */
