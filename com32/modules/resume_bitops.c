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

#include "resume_linux.h"
#include "resume_bitops.h"

/**
 * showbits - print the bits in an address
 * @addr:	Address to decompose in bits.
 **/
void showbits(unsigned long *addr)
{
	int i, k, mask;

	for (i = 8 * sizeof(*addr)-1; i >= 0; i--) {
		mask = 1 << i;
		k = *addr & mask;
		if (k == 0)
			putchar('0');
		else
			putchar('1');
		if (i % 4 == 0)
			putchar(' ');
	}
	putchar('\n');
}
