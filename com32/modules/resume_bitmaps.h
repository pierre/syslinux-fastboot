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

#ifndef RESUME_BITMAPS_H
#define RESUME_BITMAPS_H

/*
 *  PAGENUMBER gives the index of the page within the zone.
 *  PAGEINDEX gives the index of the unsigned long within that page.
 *  PAGEBIT gives the index of the bit within the unsigned long.
 */
#define PAGENUMBER(zone_offset) ((int) ((zone_offset) >> (PAGE_SHIFT + 3)))
#define PAGEINDEX(zone_offset) ((int) (((zone_offset) & UL_NUM_MASK) >> UL_SHIFT))
#define PAGEBIT(zone_offset) ((int) ((zone_offset) & BIT_NUM_MASK))

int load_bitmap(void);
int check_number_of_pfn(struct toi_header*);
unsigned long bitmap_get_next_bit_on(long, int);

#endif /* RESUME_BITMAPS_H */
