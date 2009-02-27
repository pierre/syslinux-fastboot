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

/* See arch/x86/include/asm/segment.h */
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

.text

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

trampoline_start:
	movw $0x1f01, 0xb8000 /* :) */

	/*
	 * All the code starts at 0xC0000000
	 * (See System.map)
	 */
	add  $(__PAGE_OFFSET), %ebx
	xor %eax, %eax

	/*
	 * First restore the segment registers, so we can access our data
	 * properly
	 */
	movl $(__KERNEL_DS), %eax
	movl %eax, %ss

	movl $(__USER_DS),%eax
	movl %eax, %ds
	movl %eax, %es

	xorl %eax, %eax
	movl %eax, %fs
	movl %eax, %gs

	// XXX TODO?!
	//lidt $(state_idt_address)

recover_gdt:
	/*
	 * Now restore the descriptor tables to their proper values
	 * This will recover CS
	 */
	movl $(saved_gdt_address), %eax
	add %ebx, %eax
	lgdt (%eax)

	mov $(jmp_gdt), %ecx
	add %ebx, %ecx;
	ljmp *(%ecx)

jmp_gdt:
	/*
	 * Reload the segment registers (gdt has been changed)
	 */
	movl $(__KERNEL_DS), %eax
	movl $(__USER_DS), %ecx
	movl %eax, %ss
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
	//lldt $(state->ldt)

recover_cr3:
	/*
	 * Control registers
	 * XXX What about state->cr0 and state->cr2?
	 */
	movl $(saved_cr4), %ecx
	movl (%ecx), %eax
	add %ebx, %ecx
	movl %eax, %cr4

	movl $(saved_cr3), %ecx
	add %ebx, %ecx
	movl (%ecx), %eax
	movl %eax, %cr3

	mov (jmp_cr3), %ecx
	add %ebx, %ecx;
	ljmp *(%ecx)

jmp_cr3:
	/*
	 * Flush TLB, including "global" things (vmalloc)
	 */
	movl %cr4, %eax
	movl %eax, %edx
	andl $~(1<<7), %edx // PGE bit in cr4
	movl %edx, %cr4     // Turn off PGE
	movl %cr3, %ecx     // Flush TLB
	movl %ecx, %cr3
	movl %eax, %cr4     // Turn PGE back on

recover_general:
	/*
	 * Restore the other general registers
	 */

	mov %ebx, %edx

	/*
	 * Stack pointer
	 */
	movl $(saved_context_esp), %ecx
	add %edx, %ecx
	movl (%ecx), %esp
	movl $(saved_context_ebp), %ecx
	add %edx, %ecx
	movl (%ecx), %ebp

	/*
	 * General purpose registers
	 */
	movl $(saved_context_ebx), %ecx
	add %edx, %ecx
	movl (%ecx), %ebx

	/*
	 * Index registers
	 */
	movl $(saved_context_esi), %ecx
	add %edx, %ecx
	movl (%ecx), %esi
	movl $(saved_context_edi), %ecx
	add %edx, %ecx
	movl (%ecx), %edi

	/*
	 * Restore the flags
	 */
	movl $(saved_context_eflags), %ecx
	add %edx, %ecx
	pushl (%ecx)
	popfl

	/*
	 * Cleanup
	 */
	xorl %eax, %eax
	ret

trampoline_end:
