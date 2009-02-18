/* ----------------------------------------------------------------------- *
 *
 *   Copyright (C) 2008, VMware, Inc.
 *   Author : Pierre-Alexandre Meyer <pierre@vmware.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef _RESUME_H
#define _RESUME_H

#include <stdint.h>

#ifndef TESTING
#include <syslinux/bootrm.h>
#else
typedef uint32_t addr_t;
#endif /* !TESTING */

#include "resume_mm.h"
#include "resume_linux.h"

/*
 * Global definitions
 */

/* Pointer to the beginning of the image file */
char* toi_image_buffer;

/* Current offset */
unsigned long toi_image_buffer_posn;

/* Location of book kernel data struct in kernel being resumed */
unsigned long* boot_kernel_data_buffer;

/* Memory information */
struct mm mm;

#define MOVE_TO_NEXT_PAGE \
toi_image_buffer_posn = (PAGE_SIZE * (toi_image_buffer_posn / PAGE_SIZE + 1));
#define MOVE_FORWARD_BUFFER_POINTER(a) (toi_image_buffer_posn += (a))
#define READ_BUFFER(a, b) (a = (b) (toi_image_buffer+toi_image_buffer_posn))

/* Don't be too verbose */
#define LIGHT 1

/*
 * TESTING functions
 *
 * Define TESTING to execute localy the binary
 */

#ifdef TESTING
/* You want ulimit -s unlimited in testing mode */
# define BUFFER_SIZE 104857609
# define openconsole(a, ...) ((void)0)
#endif /* TESTING */

#endif /* _RESUME_H */
