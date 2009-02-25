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

#ifndef _RESUME_SYMBOLS_H
#define _RESUME_SYMBOLS_H

#include "resume_linux.h"

struct swsusp_symbl_info {
	char		symbl_name[50];
	unsigned long	*value;
};

/* See arch/x86/include/asm/desc_defs.h */
struct desc_ptr {
	unsigned short size;
	unsigned long address;
} __attribute__((packed)) ;

/*
 * Image of the saved processor state
 * See arch/x86/include/asm/suspend_32.h
 */
struct saved_context {
	u16 es, fs, gs, ss;
	unsigned long cr0, cr2, cr3, cr4;
	struct desc_ptr gdt;
	struct desc_ptr idt;
	u16 ldt;
	u16 tss;
	unsigned long tr;
	unsigned long safety;
	unsigned long return_address;
} __attribute__((packed));

unsigned long saved_context_ebx;
unsigned long saved_context_esp;
unsigned long saved_context_ebp;
unsigned long saved_context_esi;
unsigned long saved_context_edi;
unsigned long saved_context_state;
unsigned long saved_context_eflags;
unsigned long saved_context_nosave_begin;
unsigned long saved_context_nosave_end;

struct swsusp_symbl_info sym_info[10];

int get_missing_symbols_from_saved_kernel(void);

#endif /* _RESUME_SYMBOLS_H */
