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
#include <stdlib.h>
#include <string.h>
#include <syslinux/loadfile.h>

#include "resume_symbols.h"
#include "resume_debug.h"

/* Pointer to the beginning of the image file */
char* symbols_table_buffer;

/* Current offset */
unsigned long symbols_table_posn;

/* Size of System.map */
size_t data_len;

struct swsusp_symbl_info {
	char		symbl_name[50];
	unsigned long	*value;
};

unsigned long saved_context_ebx;
unsigned long saved_context_esp;
unsigned long saved_context_ebp;
unsigned long saved_context_esi;
unsigned long saved_context_edi;
unsigned long saved_context_context;
unsigned long saved_context_eflags;
unsigned long saved_context_nosave_begin;
unsigned long saved_context_nosave_end;

static struct swsusp_symbl_info sym_info[] =
{
    {"saved_context_ebx",       &saved_context_ebx},
    {"saved_context_esp",       &saved_context_esp},
    {"saved_context_ebp",       &saved_context_ebp},
    {"saved_context_esi",       &saved_context_esi},
    {"saved_context_edi",       &saved_context_edi},
    {"saved_context",           &saved_context_context},
    {"saved_context_eflags",    &saved_context_eflags},
    {"__nosave_begin",          &saved_context_nosave_begin},
    {"__nosave_end",            &saved_context_nosave_end},
    {"\0", 0},
};

/**
 * load_symbols_table - read System.map file
 **/
static int load_symbols_table(void)
{
	void *data;

	fputs("Loading System.map...", stdout);
	if (zloadfile("System.map", &data, &data_len)) {
		printf("failed!\n");
		return 1;
	}
	fputs("ok\n", stdout);

	/* Initialize the global data pointer */
	symbols_table_buffer = (char *) data;
	symbols_table_posn = 0;

	return 0;
}

/**
 * get_kernel_symbl - read one line in System.map
 *
 * A typical entry looks like:
 *
 *	00100000 A phys_startup_32
 *
 * We care about only 9 of them (see above).
 *
 * XXX: Really quick-and-dirty...
 **/
static int get_kernel_symbl(void)
{
	int address_size = 8 * sizeof(char);
	char *s_address = malloc(address_size + 1);
	unsigned long address = 0;
	char symbol_found_name[50] = "";
	int i = 0;

	/* Get adress value */
	memcpy(s_address, symbols_table_buffer, address_size);
	s_address[address_size] = '\0';
	symbols_table_buffer += address_size;
	symbols_table_posn += address_size;
	address = strtol(s_address, NULL, 16);
#ifdef METADATA_DEBUG
	dprintf("%s", s_address);
#endif /* METADATA_DEBUG */
	free(s_address);

	/* Skip symbol type */
	symbols_table_buffer += 3 * sizeof(char);
	symbols_table_posn += 3 * sizeof(char);
#ifdef METADATA_DEBUG
	dprintf(" ");
#endif /* METADATA_DEBUG */

	/* Get symbol name */
	// XXX Optimize me... The symbols are often at the very end.
	while (*symbols_table_buffer != '\n') {
		symbol_found_name[i++] = *symbols_table_buffer;
		symbols_table_buffer++;
		symbols_table_posn++;
		if (i >= 49) {
			// PCI related stuff.
			//printf("BUG: Symbol name is longer than 49 chars.\n");
			while (*symbols_table_buffer != '\n') {
				symbols_table_buffer++;
				symbols_table_posn++;
			}
			return 0;
		}
	}

	symbol_found_name[i] = '\0';
#ifdef METADATA_DEBUG
	dprintf("%s\n", symbol_found_name);
#endif /* METADATA_DEBUG */
	symbols_table_buffer++;
	symbols_table_posn++;
	i = 0;
	while (sym_info[i].symbl_name[0] != 0){
		if (strcmp(sym_info[i].symbl_name, symbol_found_name) == 0) {
			*(sym_info[i].value) = address;
			dprintf("Symbol %s is located at 0x%08x.\n",
				symbol_found_name, address);
			return 1;
		}
		i++;
	}

	return 0;
}

/**
 * get_missing_symbols_from_saved_kernel - load missing symbols offsets
 *
 * We need to retrieve the state of the processor. It is saved in the
 * saved_context_* symbols. Their offset is found in the System.map file.
 **/
int get_missing_symbols_from_saved_kernel(void)
{
	int symbols_to_match = 9;

	if (load_symbols_table())
		return 1;

	while (symbols_table_posn < data_len && symbols_to_match) {
		if (get_kernel_symbl())
			symbols_to_match--;
	}

	/* 
	 * Don't bother free'ing the memory, we are about to rewrite the Memory
	 * Map anyway.
	 */
	return symbols_to_match;
}
