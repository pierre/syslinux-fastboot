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

#include "resume_tuxonice.h"
#include "resume_mm.h"
#include "resume_debug.h"

extern struct mm mm;

#ifdef DEBUG
void dump_toi_file_header(struct toi_file_header* toi_file_header)
{
	int i, j;

	printf("File header:\n");
	printf("\tresumed_before\t\t%d\n", toi_file_header->resumed_before);
	printf("\tdevinfo_sz\t\t%d\n", toi_file_header->devinfo_sz);
	printf("\theader_offset\t\t%d\n", toi_file_header->header_offset);
	printf("\tfirst_header_block\t%lu\n",
		toi_file_header->first_header_block);
	printf("\thave_image\t\t%d\n", toi_file_header->have_image);
	printf("\textents_num\t\t%d\n", toi_file_header->extents_num);
	printf("\tnodes_num\t\t%d\n", toi_file_header->nodes_num);
	printf("\tzones_num\t\t%d\n", toi_file_header->zones_num);

	for (i=0; i<toi_file_header->nodes_num; i++) {
		for (j=0; j<toi_file_header->zones_num; j++) {
			printf("\tzones_start_pfn[%d][%d]\t0x%08lx\n",
				i, j,
				(unsigned long) mm.zones_start_pfn[i * toi_file_header->zones_num + j]);

			printf("\tzones_max_offset[%d][%d]\t0x%08lx\n",
				i, j,
				(unsigned long) mm.zones_max_offset[i * toi_file_header->zones_num + j]);
		}
	}
}

void dump_toi_header(struct toi_header* toi_header)
{
	printf("Header proper:\n");
	printf("\tuts_sysname\t%s\n", toi_header->uts.sysname);
	printf("\tuts_nodename\t%s\n", toi_header->uts.nodename);
	printf("\tuts_release\t%s\n", toi_header->uts.release);
	printf("\tuts_version\t%s\n", toi_header->uts.version);
	printf("\tuts_machine\t%s\n", toi_header->uts.machine);
#ifdef METADATA_DEBUG
	printf("\tuts_domainname\t%s\n", toi_header->uts.domainname);
	printf("\tversion_code\t%d\n", toi_header->version_code);
	printf("\tnum_physpages\t%lu\n", toi_header->num_physpages);
	printf("\tcpus\t\t%d\n", toi_header->cpus);
	printf("\timage_pages\t%lu\n", toi_header->image_pages);
	printf("\tpages\t\t%lu\n", toi_header->pages);
	printf("\tsize\t\t%lu\n", toi_header->size);
	printf("\torig_mem_free\t%lu\n", toi_header->orig_mem_free);
	printf("\tpage_size\t%d\n", toi_header->page_size);
	printf("\tpageset_2_size\t%d\n", toi_header->pageset_2_size);
	printf("\tparam0\t\t%d\n", toi_header->param0);
	printf("\tparam1\t\t%d\n", toi_header->param1);
	printf("\tparam2\t\t%d\n", toi_header->param2);
	printf("\tparam3\t\t%d\n", toi_header->param3);
	printf("\tprogress0\t%d\n", toi_header->progress0);
	printf("\tprogress1\t%d\n", toi_header->progress1);
	printf("\tprogress2\t%d\n", toi_header->progress2);
	printf("\tprogress3\t%d\n", toi_header->progress3);
	printf("\troot_fs\t\t%lu\n", toi_header->root_fs);
	printf("\tbkd\t\t%lu\n", toi_header->bkd);
#endif /* METADATA_DEBUG */
}

void dump_toi_module_header(struct toi_module_header * toi_module_header)
{
	printf("Found module info:\n");
	printf("\tname\t\t%s\n",toi_module_header->name);
#ifdef METADATA_DEBUG
	printf("\tenabled\t\t%d\n",toi_module_header->enabled);
	printf("\ttype\t\t%d\n",toi_module_header->type);
	printf("\tindex\t\t%d\n",toi_module_header->index);
	printf("\tdata_length\t%d\n",toi_module_header->data_length);
	printf("\tsignature\t%lu\n",toi_module_header->signature);
#endif /* METADATA_DEBUG */
}

void dump_pagemap(int light)
{
	int node_id, zone_nr = 0;
	unsigned int i = 0;
	unsigned long ****bitmap = pagemap.bitmap;

	printf(" --- Dump bitmap ---\n");

	if (!bitmap)
		goto out;

	for (node_id = 0; node_id < mm.num_nodes; node_id++) {
		printf("%p: Node %d => %p\n",
			&bitmap[node_id], node_id,
			bitmap[node_id]);

		if (!bitmap[node_id])
			continue;

		for (zone_nr = 0; zone_nr < mm.num_zones; zone_nr++) {
			printf("%p:   Zone %d => %p%s\n",
				&bitmap[node_id][zone_nr], zone_nr,
				bitmap[node_id][zone_nr],
				bitmap[node_id][zone_nr] ? "" :
					" (empty)");

			if (!bitmap[node_id][zone_nr])
				continue;

			/*
			 * printf("%p:     Zone start pfn  = 0x%08lx\n",
			 *	&bitmap[node_id][zone_nr][0],
			 *	bitmap[node_id][zone_nr][0]);
			 */
			printf("%p:     Number of pages = 0x%08lx\n",
				&bitmap[node_id][zone_nr][1],
				*bitmap[node_id][zone_nr][1]);
			if (!light) {
				for (i = 2; i < (unsigned long) *bitmap[node_id]
						[zone_nr][1] + 2; i++)
					if (bitmap[node_id][zone_nr][i])
						printf("%p:     Page %2d         = %p\n",
							&bitmap[node_id][zone_nr][i],
							i - 2,
							bitmap[node_id][zone_nr][i]);
			}
		}
	}
out:
	printf(" --- Dump of bitmap finishes ---\n");
}

/**
 * dump_block_chains: Print the contents of the bdev info array.
 */
void dump_block_chains(struct hibernate_extent_chain* chain)
{
#ifdef METADATA_DEBUG
	int i;

	// XXX 1 chain FIXME
	for (i = 0; i < 1; i++) {
		struct hibernate_extent *this;

		this = chain->first;

		if (!this)
			continue;

		printf("Chain %d:", i);

		while (this) {
			printf(" [%lu-%lu]%s", this->start,
					this->end, this->next ? "," : "");
			this = this->next;
		}

		printf("\n");
	}
#endif /* METADATA_DEBUG */
}
#else /* DEBUG */
# define dump_toi_file_header(a, ...) ((void)0)
# define dump_toi_header(a, ...) ((void)0)
# define dump_toi_module_header(a, ...) ((void)0)
# define dump_pagemap(a, ...) ((void)0)
#endif /* !DEBUG */
