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

#include <disk/partition.h>
#include <disk/util.h>

#include "resume.h"
#include "resume_debug.h"
#include "resume_tuxonice.h"
#include "resume_parser.h"

/* Do not attempt to relocate code in testing mode */
#ifndef TESTING
#include <console.h>
#include <syslinux/loadfile.h>
#include <syslinux/zio.h>
#endif /* !TESTING */

/**
 * boot_image - main logic to bring back an image from disk
 **/
static void boot_image(void)
{
	if (read_metadata()) {
		error("Unable to parse metadata.\n");
		return;
	}

	if (resume_info.method == TOI_FILE)
		load_memory_map();
	else
		error("Not yet implemented.\n");
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
	char *partition_p;
#ifdef TESTING
	FILE *stream;
	char buffer[resume_info.file_size];
	resume_info.file_size = BUFFER_SIZE;
#endif

	openconsole(&dev_null_r, &dev_stdcon_w);

	if (argc != 2) {
error:
		error("Usage: resume.c32 (hd#,partition|file=resume_file)\n");
		return 1;
	}

	resume_info.method = NONE;

	if (!strncmp(argv[1], "file=", 5)) {
		resume_info.file = argv[1] + 5;
		fputs("Loading ", stdout);
		fputs(resume_info.file, stdout);
		fputs("... ", stdout);
#ifndef TESTING
		if (zloadfile(resume_info.file, &data, &resume_info.file_size)) {
			error("failed!\n");
			return 1;
		}
#else
		stream = fopen(resume_info.toi_file,"r");
		fread(buffer, sizeof(char), BUFFER_SIZE, stream);
		data = (void *) buffer;
#endif
		fputs("ok\n", stdout);

		/* Initialize the global data pointer */
		resume_info.image_buffer = (char *) data;
		resume_info.image_buffer_posn = 0;

		/* For sanity checks */
		resume_info.drive_info.disk = 0;
		resume_info.partition = 0;
	} else if (argv[1][0] == 'h' && argv[1][1] == 'd') {
		 /* e.g. 0x80 or 0x81 */
		resume_info.drive_info.disk = 0x80 + strtoul(argv[1] + 2, NULL, 10);
		partition_p = strchr(argv[1], ',');
		if (partition_p) {
			resume_info.partition = strtoul(partition_p + 1, NULL, 10);
			get_ptab(&resume_info.drive_info, &resume_info.ptab,
				 resume_info.partition, NULL);

			/* Check if the partition is valid */
			if (resume_info.ptab.length == 0) {
				error("The partition doesn't exist.\n");
				return -1;
			} else if (resume_info.ptab.ostype != 0x82) {
				char *parttype;
				get_label(resume_info.ptab.ostype, &parttype);
				printf("The partition doesn't look like swap space but %s (Id=0x%X)\n",
				       parttype, resume_info.ptab.ostype);
				free(parttype);
				return -1;
			}
		}

		/* For sanity checks */
		resume_info.file = NULL;
		resume_info.file_size = 0;
		resume_info.image_buffer = NULL;
		resume_info.image_buffer_posn = 0;
	} else
		goto error;

	boot_image();

	/* We shouldn't get here */
	if (resume_info.drive_info.disk)
		printf("Unable to resume from device 0x%X:%d\n", resume_info.drive_info.disk,
								 resume_info.partition);
	else
		printf("Unable to resume from file %s\n", resume_info.file);

	return 1;
}
