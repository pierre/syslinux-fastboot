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
#define IDT_ENTRIES			256
#define __PAGE_OFFSET			0xC0000000

/* CPU definitions. See arch/x86/include/asm/processor-flags.h */
#define X86_CR4_PAE	0x00000020 /* enable physical address extensions */
#define X86_CR4_PGE	0x00000080 /* enable global pages */

#define __SYSLINUX_CS			0x20

/* Physical address */
#define pa2(X) ((X)-trampoline_start+0x1473000)
// XXX
#define pa(X) ((X)-boot_data_start+0x8000)

.global trampoline_start
.global trampoline_end
.global saved_context_esp
.global saved_context_ebp
.global saved_context_ebx
.global saved_context_esi
.global saved_context_edi
.global saved_context_eflags
.global saved_swapper_pg_dir
.global saved_context_idt
.global __nosave_begin
.global __nosave_end
.global boot_data_start
.global boot_data_end

.text

/*
 * Start of our text segment (it will be relocated at __nosave_begin).
 *
 * WARNING!
 *
 * All symbols compiled in the com32 modules are put in a random place. Since
 * we relocate the trampoline, when accessing these symbols
 * you need to use the pa (physical address) macro to access data and pa2 to
 * jump inside the trampoline.
 *
 * XXX Try to do something clever to get rid of pa*. ld script?
 */
trampoline_start:
	cli
	cld

	/*
	 * Our first job is to turn on paging, but before, we need to
	 * initialize our page tables.
	 *
	 * Begin at pg0 (page 0) and init all pages to 007 (PRESENT+RW+USER).
	 *
	 * Format of a page table entry:
	 *
	 * 				<-- 4bytes -->
	 *  31                                  12 11    9 8 7 6 5 4 3 2 1 0
	 * +----------------------------------------------------------------+
	 * |                                      |       |   | | |   |U|R| |
	 * |      PAGE FRAME ADDRESS 31..12       | AVAIL |0 0|D|A|0 0|/|/|P|
	 * |                                      |       |   | | |   |S|W| |
	 * +----------------------------------------------------------------+
	 *
	 * Page tables are stored starting at pg0, at 0x9000.
	 * The PDE are allocated statically (see the data section below).
	 */

	/*
	 * Map 0x8000 ... 0xc000
	 * This is used by the trampoline to access page directories/tables
	 * and misc. data.
	 */
	movl $pa(pg0),%edi
	movl $007,%eax				# 0x007 = PRESENT+RW+USER

2:	stosl					# Write one entry:
						# write %eax in %edi and
						# advance edi
	addl $0x1000,%eax			# Next page address (4K)
	cmp $0xb000,%edi			# Write only one page table
	jne 2b

	/*
	 * Map __nosave_begin ... __nosave_end
	 * This is used to access the code itself of the trampoline.
	 */
	movl $pa(pg1),%edi
	movl $007,%eax				# 0x007 = PRESENT+RW+USER
	addl $0x1400000,%eax			# Align to cover __nosave_begin
2:	stosl					# Write one entry:
						# write %eax in %edi and
						# advance edi
	addl $0x1000,%eax			# Next page address (4K)
	cmp $pa(empty_zero_page),%edi		# Write only on page table
	jne 2b

	/* Load CR3 with the address of the page directory */
	movl $pa(boot_swapper_pg_dir),%eax
	movl %eax,%cr3

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
	/* PAGING enabled */

	/* We need to relocate ourselves in the kernel address space */
	ljmp $(__SYSLINUX_CS),$(pa2(1f)+__PAGE_OFFSET)

1:
	// XXX lldt? Linux doesn't care about the LDT. Wine apps do though.

	/*
	 * We are still using the SYSLINUX idt/gdt. Now is a good time to overwrite
	 * it with the Linux' ones.
	 *
	 * XXX Is there a way to reload the gdt from the image instead of having
	 * to hardcode it here?
	 */

	/* We need to add __PAGE_OFFSET to the saved idt address */
	movl pa2(idt_reloc)+__PAGE_OFFSET, %eax
	movl (%eax),%ebx
	addl $__PAGE_OFFSET,%ebx
	movl %ebx, pa2(idt_reloc)+__PAGE_OFFSET
	lidt pa2(idt_descr_reloc)

	lgdt pa2(gdt_descr_reloc)

	ljmp $(__KERNEL_CS),$pa2(2f)+__PAGE_OFFSET

2:
	movl $__KERNEL_DS, %eax
	movl %eax, %ss

	movl $(__USER_DS), %ecx
	movl %ecx, %ds
	movl %ecx, %es

	xorl %eax, %eax
	movl %eax, %fs
	movl %eax, %gs

	/*
	 * All the work before was to bootstrap the system. Now, we are ready
	 * to jump into the old kernel.
	 *
	 * See arch/x86/power/hibernate_asm_*.S
	 *
	 * The first thing to do is to restore the page directory to its
	 * proper value. This will give us access to the saved_context_* data.
	 *
	 * XXX SMP: turn on paging options (PSE, PAE, ...) - reload cr4?
	 */
	movl pa(saved_swapper_pg_dir),%ecx
	movl %ecx,%cr3
	ljmp $(__KERNEL_CS),$(pa2(3f)+__PAGE_OFFSET)

3:
	/* Flush TLB, including "global" things (vmalloc) */
	movl pa2(mmu_cr4_features)+__PAGE_OFFSET, %ecx
	jecxz 1f			# cr4 Pentium and higher, skip if zero
	movl %ecx, %edx
	andl $~(X86_CR4_PGE),%edx
	movl %edx,%cr4			# turn off PGE
1:
	/*
	 * Hardware automatically invalidates (implicitly flushing) TLB
	 * entries when the contents of the cr3 register changes,
	 * since all the TLB stored entries become invalid.
	 */
	movl %cr3,%eax			# flush TLB
	movl %eax,%cr3
	jecxz 1f			# cr4 Pentium and higher, skip if zero
	movl %ecx,%cr4			# turn PGE back on
1:

	// XXX Wakeup other CPUs?

	/*
	 * Restore stack information
	 * It looks like:
	 *	ebp: 0x...EC0 |
	 *	       ...    |
	 *	esp: 0x...E9C V
	 * (36 bytes)
	 */
	movl pa2(saved_context_esp)+__PAGE_OFFSET, %eax
	addl $__PAGE_OFFSET,%eax
	movl (%eax),%esp
	movl pa2(saved_context_ebp)+__PAGE_OFFSET, %eax
	addl $__PAGE_OFFSET,%eax
	movl (%eax),%ebp

	/* Restore general purpose registers (likely to be zeroed) */
	movl pa2(saved_context_ebx)+__PAGE_OFFSET, %eax
	addl $__PAGE_OFFSET,%eax
	movl (%eax),%ebx

	/* Restore index registers (likely to be zeroed) */
	movl pa2(saved_context_esi)+__PAGE_OFFSET, %eax
	addl $__PAGE_OFFSET,%eax
	movl (%eax),%esi
	movl pa2(saved_context_edi)+__PAGE_OFFSET, %eax
	addl $__PAGE_OFFSET,%eax
	movl (%eax),%edi

	/*
	 * Restore the flags
	 * Looks like 0x200256:
	 * 0000 0000 0010 0000 0000 0010 0101 0110
	 *                            |   | |  |
	 *                            IE ZF AF PF
	 */
	movl pa2(saved_context_eflags)+__PAGE_OFFSET, %eax
	addl $__PAGE_OFFSET,%eax
	hlt // XXX BROKEN
	pushl (%eax)
	popfl

	/* Cleanup */
	xorl %eax, %eax
	xorl %ecx, %ecx
	xorl %edx, %edx

	/* pop to get EIP (value of %esp) */
	ret

/* Data that needs to be accessed after loading swapper_pg_dir */
saved_context_idt:
	.long	0
saved_context_esp:
	.long	0
saved_context_ebp:
	.long	0
saved_context_ebx:
	.long	0
saved_context_esi:
	.long	0
saved_context_edi:
	.long	0
saved_context_eflags:
	.long	0

#if !defined(CONFIG_X86_PAE) || defined(CONFIG_X86_64)
mmu_cr4_features:
	.long	0
#else
mmu_cr4_features:
	.long	X86_CR4_PAE
#endif

idt_descr_reloc:
	.word	IDT_ENTRIES*8-1
idt_reloc:
	.long	pa2(saved_context_idt)+__PAGE_OFFSET	# idt address
gdt_descr_reloc:
	.word	0xf8
gdt_reloc:
	.long	pa2(gdt_table_reloc)+__PAGE_OFFSET	# gdt address
gdt_table_reloc:
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

/*
 * Start of our data section (will be reloaded at 0x8000).
 *
 * Note: the trampoline will be restored at __nosave_begin. It needs to fit
 * between __nosave_begin and __nosave_end! (4K). There is no room for our
 * boot page tables. This is why the bulk of data is restored lower in
 * memory, in a safer, larger place.
 */

/* Needed to honor the .org */
.data

boot_data_start:
saved_swapper_pg_dir:
	.long	0
__nosave_begin:
	.long	0
__nosave_end:
	.long	0

/*
 * We need to do a double mapping of
 *		0x8000 -- 0xe000
 *		__nosave_begin+__PAGE_OFFSET -- __nosave_end+__PAGE_OFFSET
 *
 * When restoring swapper_pg_dir, EIP needs to be in the 0xCxxxxx address
 * space. This means that, when we enable paging, we need to add a kernel
 * space mapping as well as the identity one.
 *
 * boot_swapper_pg_dir is at 0x9000.
 *
 * XXX This needs to be dynamic. __nosave_begin will not always be at 0xc146xxx
 */
.org 0x1000
boot_swapper_pg_dir:
	.long 0x0000a007	# pfns 0 --> 1023 (0x0-0x3FF000)
	.fill 4,4,0
	.long 0x0000b007	# pfns 5120 --> 6143 (1400000-17FF000)
	.fill 766,4,0
	.long 0x0000b007	# pfns 791552 --> 792575 (0xC1400000-0xC17FF000)
	.fill 251,4,0x0000b007  # XXX?? I screwed something up,
				# I shouldn't need that. Check the math.

/*
 * These are provisional page tables - the final page tables will be
 * restored from the image.
 *
 * pg0 is at 0xa000.
 */
.org 0x2000
pg0:

/* For info only, not really used per se */
.org 0x3000
pg1:

/*
 * empty_zero_page must immediately follow the page tables! (The
 * initialization loop counts until empty_zero_page)
 */
.org 0x4000
empty_zero_page:
boot_data_end:
