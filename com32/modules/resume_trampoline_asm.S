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

.text

/* Linux definitions. See arch/x86/include/asm/segment.h */
#define GDT_ENTRY_BOOT_CS		2
#define GDT_ENTRY_BOOT_DS		(GDT_ENTRY_BOOT_CS + 1)
#define __BOOT_DS			(GDT_ENTRY_BOOT_DS * 8)
#define __BOOT_CS			(GDT_ENTRY_BOOT_CS * 8)
#define GDT_ENTRY_KERNEL_BASE		12
#define GDT_ENTRY_KERNEL_CS		(GDT_ENTRY_KERNEL_BASE + 0)
#define GDT_ENTRY_KERNEL_DS		(GDT_ENTRY_KERNEL_BASE + 1)
#define GDT_ENTRY_DEFAULT_USER_DS	15
#define __KERNEL_CS			(GDT_ENTRY_KERNEL_CS * 8)
#define __KERNEL_DS			(GDT_ENTRY_KERNEL_DS * 8)
#define __USER_DS			(GDT_ENTRY_DEFAULT_USER_DS* 8 + 3)
#define __USER_CS			(GDT_ENTRY_DEFAULT_USER_CS* 8 + 3)
#define __PAGE_OFFSET			0xC0000000
#define INIT_MAP_BEYOND_END		(1*1024*1024)		/* 1MB */
#define STACK_SIZE			(8 * 1024)

/* Physical address */
#define pa(X) ((X)-trampoline_start+0x8000)

.global trampoline_start
.global trampoline_end
.global saved_gdt_address
.global saved_context_esp
.global saved_context_ebp
.global saved_context_ebx
.global saved_context_esi
.global saved_context_edi
.global saved_context_eflags

	.align 32
	.code32

/*
 * WARNING!
 *
 * All symbols compiled in the com32 modules are put in a random place. Since
 * we relocate the trampoline starting at 0x8000, when accessing these symbols
 * you need to use the pa (physical address) macro.
 */
reloc_start:
trampoline_start:
	movw $0x1f01, 0xb8000 /* :) */
	cli
	cld

/*
 * Set segments to known values.
 */
	lidt pa(idt_descr)		# load idt with 0, 0
	lgdt pa(gdt_descr)		# load gdt with whatever is appropriate

	ljmp $(__KERNEL_CS), $pa(startup_32)

startup_32:
	movl $(__KERNEL_DS),%eax
	movl %eax, %ds
	movl %eax, %es
	movl %eax, %fs
	movl %eax, %gs
	movl %eax, %ss

	// XXX SMP: turn on paging options (PSE,PAE,..) - cr4

/*
 * Initialize page tables.
 *
 * Begin at pg0 (page 0) and init all pages to 007 (PRESENT + RW + USER).
 *
 * Format of a page table entry:
 *
 * 				<-- 4bytes -->
 *      31                                  12 11    9 8 7 6 5 4 3 2 1 0
 *     +----------------------------------------------------------------+
 *     |                                      |       |   | | |   |U|R| |
 *     |      PAGE FRAME ADDRESS 31..12       | AVAIL |0 0|D|A|0 0|/|/|P|
 *     |                                      |       |   | | |   |S|W| |
 *     +----------------------------------------------------------------+
 *
 * The PDE are allocated statically.
 */
	/*
	 * Page tables are stored starting at pg0, at 0x9c00:
	 * pa(swapper_pg_dir) + 0x1000 (1024 * 4 = 4K)
	 */
	movl $pa(pg0),%edi
	movl $007,%eax				# 0x007 = PRESENT+RW+USER

	/* Fill the first page table: 4M is large enough for the trampoline */
2:	stosl					# Write one entry:
						# write %eax in %edi and
						# advance edi
	addl $0x1000,%eax			# Next page address (4K)
	cmp $pa(empty_zero_page),%edi
	jne 2b

	/*
	 * %edi now points to the first safe page (end of our page tables),
	 * i.e. 0x8000 + 0x4000 = 0xc000
	 */

	/* Load CR3 with the address of the page directory */
	movl $pa(swapper_pg_dir),%eax
	movl %eax,%cr3

	/* Set the paging bit of CR0 */
	movl %cr0,%eax
	orl $0x80000000,%eax
	/*
	 * Up until here, we were using physical addresses. The next instruction
	 * turns paging on. eip will refer to the next instruction (physical
	 * address) but this instruction will go through the page tables (and
	 * be translated). The previous page tables need to map the code up to
	 * here (identity: pa <-> va). The jmp instructions will then normalize
	 * eip.
	 */
	movl %eax,%cr0
	ljmp $(__KERNEL_CS), $pa(1f)
1:
	/* Set up the stack pointer */
	movl pa(stack_start),%esp

	// XXX Wakeup other CPUs
/*
 * All the work before was to bootstrap the system. Now, we are ready to jump
 * into the old kernel
 */
	/*
	 * Now restore the descriptor tables to their proper values
	 * This will recover CS
	 */
	// XXX lidt?
	lgdt (saved_gdt_address)

	ljmp $(__KERNEL_CS), $pa(jmp_gdt)

jmp_gdt:
	/*
	 * Reload the segment registers (gdt has been changed)
	 */
	movl $(__KERNEL_DS), %eax
	movl %eax, %ss

	movl $(__USER_DS), %ecx
	movl %ecx, %ds
	movl %ecx, %es

	xorl %eax, %eax
	movl %eax, %fs
	movl %eax, %gs

	// XXX Why is the state of es and ss saved? This is fixed!
	//movw $(state->es), %es
	//movw $(state->ss), %ss
	//movw $(state->fs), %fs
	//movw $(state->gs), %gs

	// XXX Linux doesn't care about the LDT. Wine apps do though
	//lldt $(state->ldt)

recover_cr3:
	/*
	 * Control registers
	 * XXX What about state->cr0 and state->cr2?
	 */
	movl $(saved_cr4), %eax
	movl %eax, %cr4

	movl $(saved_cr3), %eax
	movl %eax, %cr3

	ljmp $(__KERNEL_CS), $pa(jmp_cr3)

jmp_cr3:
	/*
	 * Flush TLB, including "global" things (vmalloc)
	 */
	movl %cr4, %eax
	movl %eax, %edx
	andl $~(1<<7), %edx			# PGE bit in cr4
	movl %edx, %cr4				# Turn off PGE
	/*
	 * Hardware automatically invalidates (implicitly flushing) TLB
	 * entries when the contents of the cr3 register changes,
	 * since all the TLB stored entries become invalid.
	 */
	movl %cr3, %ecx				# Flush TLB
	movl %ecx, %cr3
	movl %eax, %cr4				# Turn PGE back on

/*
 * Restore the other general registers
 */
recover_general:
	/*
	 * Stack pointer
	 */
	movl $(saved_context_esp), %ecx
	movl (%ecx), %esp
	movl $(saved_context_ebp), %ecx
	movl (%ecx), %ebp

	/*
	 * General purpose registers
	 */
	movl $(saved_context_ebx), %ecx
	movl (%ecx), %ebx

	/*
	 * Index registers
	 */
	movl $(saved_context_esi), %ecx
	movl (%ecx), %esi
	movl $(saved_context_edi), %ecx
	movl (%ecx), %edi

	/*
	 * Restore the flags
	 */
	movl $(saved_context_eflags), %ecx
	pushl (%ecx)
	popfl

	/*
	 * Cleanup
	 */
	xorl %eax, %eax
	ret

/*
 * Misc. definitions
 */

stack_start:
	.long INIT_MAP_BEYOND_END+8192
	.long __KERNEL_DS

	.align 0
	.word	0
idt_descr:
	.word	0				# idt limit = 0
idt:
	.long	0				# idt base = 0L

	.word 0
gdt_descr:
	.word	0xf8				# gdt limit
gdt:
	.long	pa(gdt_table)			# gdt address

/*
 * This is initialized to create an identity-mapping at 0-4M (for bootup
 * purposes).
 *
 * swapper_pg_dir is at 0x9000.
 */
.org 0x1000
swapper_pg_dir:
	.long 0x0000a007			# We only map 1024 pages
	.fill 1023,4,0

/*
 * The page tables are initialized to only 4MB here - the final page
 * tables will be restored from the image.
 *
 * pg0 is at 0xa000.
 */
.org 0x2000
pg0:

/*
 * empty_zero_page must immediately follow the page tables ! (The
 * initialization loop counts until empty_zero_page)
 */
.org 0x3000
empty_zero_page:

.org 0x4000

/*
 * This starts the data section. Note that the above is all
 * in the text section because it has alignment requirements
 * that we cannot fulfill any other way.
 */
.data

.align 0
/*
 * GDT for cpu#0
 * See arch/x86/kernel/cpu/common.c
 */
gdt_table:
	.quad 0x0000000000000000	/* NULL descriptor */
	.quad 0x0000000000000000	/* 0x0b reserved */
	.quad 0x0000000000000000	/* 0x13 reserved */
	.quad 0x0000000000000000	/* 0x1b reserved */
	.quad 0x0000000000000000	/* 0x20 unused */
	.quad 0x0000000000000000	/* 0x28 unused */
	.quad 0x0000000000000000	/* 0x33 TLS entry 1 */
	.quad 0x0000000000000000	/* 0x3b TLS entry 2 */
	.quad 0x0000000000000000	/* 0x43 TLS entry 3 */
	.quad 0x0000000000000000	/* 0x4b reserved */
	.quad 0x0000000000000000	/* 0x53 reserved */
	.quad 0x0000000000000000	/* 0x5b reserved */

	.quad 0x00cf9a000000ffff	/* 0x60 kernel 4GB code at 0x00000000 */
	.quad 0x00cf92000000ffff	/* 0x68 kernel 4GB data at 0x00000000 */
	.quad 0x00cffa000000ffff	/* 0x73 user 4GB code at 0x00000000 */
	.quad 0x00cff2000000ffff	/* 0x7b user 4GB data at 0x00000000 */

	.quad 0x0000000000000000	/* 0x80 TSS descriptor */
	.quad 0x0000000000000000	/* 0x88 LDT descriptor */

	/*
	 * Segments used for calling PnP BIOS have byte granularity.
	 * They code segments and data segments have fixed 64k limits,
	 * the transfer segment sizes are set at run time.
	 */
	.quad 0x00409a000000ffff	/* 0x90 32-bit code */
	.quad 0x00009a000000ffff	/* 0x98 16-bit code */
	.quad 0x000092000000ffff	/* 0xa0 16-bit data */
	.quad 0x0000920000000000	/* 0xa8 16-bit data */
	.quad 0x0000920000000000	/* 0xb0 16-bit data */

	/*
	 * The APM segments have byte granularity and their bases
	 * are set at run time.  All have 64k limits.
	 */
	.quad 0x00409a000000ffff	/* 0xb8 APM CS    code */
	.quad 0x00009a000000ffff	/* 0xc0 APM CS 16 code (16 bit) */
	.quad 0x004092000000ffff	/* 0xc8 APM DS    data */

	.quad 0x00c0920000000000	/* 0xd0 - ESPFIX SS */
	.quad 0x00cf92000000ffff	/* 0xd8 - PDA */
	.quad 0x0000000000000000	/* 0xe0 - unused */
	.quad 0x0000000000000000	/* 0xe8 - unused */
	.quad 0x0000000000000000	/* 0xf0 - unused */
	.quad 0x0000000000000000	/* 0xf8 - GDT entry 31: double-fault TSS */

trampoline_end:
