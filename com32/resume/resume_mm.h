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

#ifndef _RESUME_MM_H
#define _RESUME_MM_H

struct mm {
	int num_nodes;				/* Number of nodes (MAX_NUMNODES) */
	int num_zones;				/* Number of zones (MAX_NR_ZONES) */
	unsigned long* zones_start_pfn;		/* First pfn for each zone */
	unsigned long* zones_max_offset;	/* Size of each zone */
};

#endif /* _RESUME_MM_H */
