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
#include <disk/geom.h>
#include <disk/partition.h>
#include <disk/swsusp.h>

#include "resume_tuxonice.h"

#ifndef TESTING
#include <syslinux/bootrm.h>
#else
typedef uint32_t addr_t;
#endif /* !TESTING */

#include "resume_mm.h"
#include "resume_linux.h"

static __inline__ void error(const char *msg)
{
	fputs(msg, stderr);
}

/*
 * Global definitions
 */

enum {
	NONE,
	SWSUSP_SWAP_PARTITION,
	SWSUSP_SWAP_FILE,
	TOI_SWAP_PARTITION,
	TOI_SWAP_FILE,
	TOI_FILE,
};

struct resume_params {
	int method;				/* Which resume method? See above */
	/* Resume from partition */
	struct driveinfo drive_info;
	int partition;
	struct part_entry ptab;
	/* Resume from file */
	char* file;
	size_t file_size;
	char* image_buffer;			/* Beginning of the image file */
	unsigned long image_buffer_posn;	/* Offset in the file */
	/* swsusp only */
	struct swsusp_header sw_header;
	/* TuxOnIce only */
	struct toi_file_header toi_file_header;
	long toi_pagedir1_size;
	long toi_pagedir2_size;
} resume_info;

/* Location of book kernel data struct in kernel being resumed */
unsigned long* boot_kernel_data_buffer;

/* Memory information */
struct mm mm;

#define MOVE_TO_NEXT_PAGE \
resume_info.image_buffer_posn = (PAGE_SIZE * (resume_info.image_buffer_posn / PAGE_SIZE + 1));
#define MOVE_FORWARD_BUFFER_POINTER(a) (resume_info.image_buffer_posn += (a))
#define READ_BUFFER(a, b) (a = (b) (resume_info.image_buffer+resume_info.image_buffer_posn))

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
