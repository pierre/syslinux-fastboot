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
 * Most of these definitions come from the Linux kernel. See its source for
 * Copyright information.
 */

#ifndef RESUME_LINUX_H
#define RESUME_LINUX_H

#define CONFIG_X86_32

#define PAGE_SHIFT 12
#define PAGE_SIZE 4096 /* 1<<12 */
#define BIT_MASK(nr)	(1UL << ((nr) % BITS_PER_LONG))
#define BIT_NUM_MASK ((sizeof(unsigned long) << 3) - 1)
#define PAGE_NUM_MASK (~((1 << (PAGE_SHIFT + 3)) - 1))
#define UL_NUM_MASK (~(BIT_NUM_MASK | PAGE_NUM_MASK))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)

#ifdef CONFIG_X86_32
# define BITS_PER_LONG 32
#else
# define BITS_PER_LONG 64
#endif

#if BITS_PER_LONG == 32
#define UL_SHIFT 5
#else
#if BITS_PER_LONG == 64
#define UL_SHIFT 6
#else
#error Bits per long not 32 or 64?
#endif
#endif

#define BITOP_WORD(nr)		((nr) / BITS_PER_LONG)

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	The pointer to the member.
 * @type:	The type of the container struct this is embedded in.
 * @member:	The name of the member within the struct.
 **/
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

struct list_head {
	struct list_head *next, *prev;
};

/**
 * list_for_each_entry_continue_reverse - iterate backwards from the given point
 * @pos:	The type * to use as a loop cursor.
 * @head:	The head for your list.
 * @member:	The name of the list_struct within the struct.
 *
 * Start to iterate over list of given type backwards, continuing after
 * the current position.
 **/
#define list_for_each_entry_continue_reverse(pos, head, member)		\
	for (pos = list_entry(pos->member.prev, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.prev, typeof(*pos), member))

/**
 * list_for_each_entry_continue - continue iteration over list of given type
 * @pos:	The type * to use as a loop cursor.
 * @head:	The head for your list.
 * @member:	The name of the list_struct within the struct.
 *
 * Continue to iterate over list of given type, continuing after
 * the current position.
 **/
#define list_for_each_entry_continue(pos, head, member)			\
	for (pos = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each - iterate over a list
 * @pos:	The &struct list_head to use as a loop cursor.
 * @head:	The head for your list.
 **/
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); \
		pos = pos->next)

/**
 * list_for_each_entry - iterate over list of given type
 * @pos:	The type * to use as a loop cursor.
 * @head:	The head for your list.
 * @member:	The name of the list_struct within the struct.
 **/
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 **/
static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/**
 * list_add - add a new entry
 * @new:	New entry to be added
 * @head:	List head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 **/
static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

/**
 * list_entry - get the struct for this entry
 * @ptr:	The &struct list_head pointer.
 * @type:	The type of the struct this is embedded in.
 * @member:	The name of the list_struct within the struct.
 **/
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

typedef int spinlock_t;

struct new_utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

/*
 * Convert a physical address to a Page Frame Number and back
 */
#define	__pfn_to_phys(pfn)	((pfn) << PAGE_SHIFT)

#endif /* RESUME_LINUX_H */
