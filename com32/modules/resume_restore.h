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

#ifndef _RESUME_RESTORE_H
#define _RESUME_RESTORE_H

int load_memory_map(unsigned long, unsigned long);

int memory_map_add(unsigned long, unsigned long, addr_t*, int*, int*, int*, int*);

struct data_buffers_list {
	addr_t pfn;
	addr_t* data;
	struct data_buffers_list *next, *prev;
};

#endif /* _RESUME_RESTORE_H */
