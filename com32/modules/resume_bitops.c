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
 * test_bit - test a bit in an address
 * @bit:	Bit to test.
 * @addr:	Address to test.
 **/
__inline__ int test_bit(int bit, const u32 *addr)
{
	return ((1UL << (bit & 31)) & (addr[bit >> 5])) != 0;
}

/**
 * clear_bit - unset a bit in an address
 * @bit:	Bit to clear.
 * @addr:	Address to modify.
 *
 * XXX This is not endiannes-portable.
 **/
__inline__ void clear_bit(int bit, u32 *addr)
{
	*addr = *addr & (~( 1 << (bit - 1) ));
}

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
