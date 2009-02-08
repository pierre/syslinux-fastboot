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
 * resume.c
 *
 * Fast resume.
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "resume.h"
#include "resume_debug.h"
#include "resume_mm.h"
#include "resume_tuxonice.h"
#include "resume_parser.h"
#include "resume_restore.h"

/* Define DYN_PAGEFLAGS for TuxOnIce < 3-rc8 */
#ifdef DYN_PAGEFLAGS
#include "resume_dyn_pageflags.h"
#else
#include "resume_bitmaps.h"
#endif /* !DYN_PAGEFLAGS */

/* Do not attempt to relocate code in testing mode */
#ifdef TESTING
# define setup_trampoline_blob() ((void)1)
#else
#include <minmax.h>
#include <console.h>
#include <syslinux/loadfile.h>
#include <syslinux/movebits.h>
#include <syslinux/bootrm.h>
#include <syslinux/io.h>
#include <syslinux/zio.h>

#include "resume_trampoline.h"
#endif /* !TESTING */

__inline__ void error(const char *msg)
{
	fputs(msg, stderr);
}

/**
 * boot_image - main logic to bring back an image from disk
 * @ptr:	Pointer to the file.
 * @len:	Length of the file.
 **/
static void boot_image(int len)
{
	int nb_of_pfns;

	read_metadata(&nb_of_pfns);
	load_memory_map(len, nb_of_pfns); /* Will return in protected mode */
}

/**
 * main - entry point
 *
 * This is called when
 *
 *	resume.c[32] image
 *
 * is executed.
 **/
int main(int argc, char *argv[])
{
	void *data;
#ifndef TESTING
	size_t data_len;
#else
	FILE *stream;
	size_t data_len = BUFFER_SIZE;
	char buffer[data_len];
#endif

	openconsole(&dev_null_r, &dev_stdcon_w);

	if (argc != 2) {
		error("Usage: resume.c32 resume_file\n");
		return 1;
	}

	fputs("Loading ", stdout);
	fputs(argv[1], stdout);
	fputs("... ", stdout);
#ifndef TESTING
	if (zloadfile(argv[1], &data, &data_len)) {
		error("failed!\n");
		return 1;
	}
#else
	stream = fopen(argv[1],"r");
	fread(buffer, sizeof(char), BUFFER_SIZE, stream);
	data = (void *) buffer;
#endif
	fputs("ok\n", stdout);

	/* Initialize the global data pointer */
	toi_image_buffer = (char *) data;
	toi_image_buffer_posn = 0;

	boot_image(data_len);

	/* We shouldn't get here */
	error("Unable to resume.\n");
	return 1;
}
