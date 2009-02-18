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

/*
 * resume_debug.h
 */

#ifndef _RESUME_DEBUG_H
#define _RESUME_DEBUG_H

#ifdef DEBUG

#include "resume_tuxonice.h"
#include "resume_restore.h"

#define dprintf printf
void dump_toi_file_header(struct toi_file_header*);
void dump_toi_header(struct toi_header*);
void dump_toi_module_header(struct toi_module_header*);
void dump_pagemap();
void dump_extent_chain(struct hibernate_extent_chain*);
void dump_block_chains(struct hibernate_extent_chain*);
void dump_restore_list(struct data_buffers_list*);
#else /* DEBUG */
# define dprintf(f, ...) ((void)0)
# define dump_toi_file_header(a, ...) ((void)0)
# define dump_toi_header(a, ...) ((void)0)
# define dump_toi_module_header(a, ...) ((void)0)
# define dump_pagemap(a, ...) ((void)0)
# define dump_extent_chain(a, ...) ((void)0)
# define dump_block_chains(a, ...) ((void)0)
# define dump_restore_list(a, ...) ((void)0)
#endif /* !DEBUG */

#define DUMP_PNTR dprintf("toi_image_buffer_posn=%lu\n", toi_image_buffer_posn);

#endif /* _RESUME_DEBUG_H */
