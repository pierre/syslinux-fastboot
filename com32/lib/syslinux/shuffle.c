/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2009 H. Peter Anvin - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * shuffle.c
 *
 * Common code for "shuffle and boot" operation; generates a shuffle list
 * and puts it in the bounce buffer.  Returns the number of shuffle
 * descriptors.
 */

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <com32.h>
#include <minmax.h>
#include <syslinux/movebits.h>
#include <klibc/compiler.h>

#ifndef DEBUG
# define DEBUG 0
#endif

#define dprintf(f, ...) ((void)0)
#define dprintf2(f, ...) ((void)0)

#if DEBUG
# include <stdio.h>
# undef dprintf
# define dprintf printf
# if DEBUG > 1
#  undef dprintf2
#  define dprintf2 printf
# endif
#endif

struct shuffle_descriptor {
  uint32_t dst, src, len;
};

static int desc_block_size;

static void __constructor __syslinux_get_desc_block_size(void)
{
  static com32sys_t reg;

  reg.eax.w[0] = 0x0011;
  __intcall(0x22, &reg, &reg);

  desc_block_size = (reg.eflags.l & EFLAGS_CF) ? 64 : reg.ecx.w[0];
}

/* Allocate descriptor memory in these chunks */
#define DESC_BLOCK_SIZE	desc_block_size

int syslinux_prepare_shuffle(struct syslinux_movelist *fraglist,
			     struct syslinux_memmap *memmap)
{
  struct syslinux_movelist *moves = NULL, *mp;
  struct syslinux_memmap *rxmap = NULL, *ml;
  struct shuffle_descriptor *dp, *dbuf;
  int np, nb, nl, rv = -1;
  int desc_blocks, need_blocks;
  int need_ptrs;
  addr_t desczone, descfree, descaddr, descoffs;
  int nmoves, nzero;
  struct shuffle_descriptor primaries[2];

  /* Count the number of zero operations */
  nzero = 0;
  for (ml = memmap; ml->type != SMT_END; ml = ml->next) {
    if (ml->type == SMT_ZERO)
      nzero++;
  }

  /* Find the largest contiguous region unused by input *and* output;
     this is where we put the move descriptor list */

  rxmap = syslinux_dup_memmap(memmap);
  if (!rxmap)
    goto bail;
  for (mp = fraglist; mp; mp = mp->next) {
    if (syslinux_add_memmap(&rxmap, mp->src, mp->len, SMT_ALLOC) ||
	syslinux_add_memmap(&rxmap, mp->dst, mp->len, SMT_ALLOC))
      goto bail;
  }
  if (syslinux_memmap_largest(rxmap, SMT_FREE, &desczone, &descfree))
    goto bail;

  syslinux_free_memmap(rxmap);

  dprintf("desczone = 0x%08x, descfree = 0x%08x\n", desczone, descfree);

  rxmap = syslinux_dup_memmap(memmap);
  if (!rxmap)
    goto bail;

  desc_blocks = (nzero+DESC_BLOCK_SIZE-1)/(DESC_BLOCK_SIZE-1);
  for (;;) {
    addr_t descmem = desc_blocks*
      sizeof(struct shuffle_descriptor)*DESC_BLOCK_SIZE;

    if (descfree < descmem)
      goto bail;		/* No memory block large enough */

    /* Mark memory used by shuffle descriptors as reserved */
    descaddr = desczone + descfree - descmem;
    if (syslinux_add_memmap(&rxmap, descaddr, descmem, SMT_RESERVED))
      goto bail;

#if DEBUG > 1
    syslinux_dump_movelist(stdout, fraglist);
#endif

    if (syslinux_compute_movelist(&moves, fraglist, rxmap))
      goto bail;

    nmoves = 0;
    for (mp = moves; mp; mp = mp->next)
      nmoves++;

    need_blocks = (nmoves+nzero+DESC_BLOCK_SIZE-1)/(DESC_BLOCK_SIZE-1);

    if (desc_blocks >= need_blocks)
      break;			/* Sufficient memory, yay */

    desc_blocks = need_blocks;	/* Try again... */
  }

#if DEBUG > 1
  dprintf("Final movelist:\n");
  syslinux_dump_movelist(stdout, moves);
#endif

  syslinux_free_memmap(rxmap);
  rxmap = NULL;

  need_ptrs = nmoves+nzero+desc_blocks-1;
  dbuf = malloc(need_ptrs*sizeof(struct shuffle_descriptor));
  if (!dbuf)
    goto bail;

  descoffs = descaddr - (addr_t)dbuf;

#if DEBUG
  dprintf("nmoves = %d, nzero = %d, dbuf = %p, offs = 0x%08x\n",
	  nmoves, nzero, dbuf, descoffs);
#endif

  /* Copy the move sequence into the descriptor buffer */
  np = 0;
  nb = 0;
  nl = nmoves+nzero;
  dp = dbuf;
  for (mp = moves; mp; mp = mp->next) {
    if (nb == DESC_BLOCK_SIZE-1) {
      dp->dst = -1;		/* Load new descriptors */
      dp->src = (addr_t)(dp+1) + descoffs;
      dp->len = sizeof(*dp)*min(nl, DESC_BLOCK_SIZE);
      dprintf("[ %08x %08x %08x ]\n", dp->dst, dp->src, dp->len);
      dp++; np++;
      nb = 0;
    }

    dp->dst = mp->dst;
    dp->src = mp->src;
    dp->len = mp->len;
    dprintf2("[ %08x %08x %08x ]\n", dp->dst, dp->src, dp->len);
    dp++; np++; nb++; nl--;
  }

  /* Copy bzero operations into the descriptor buffer */
  for (ml = memmap; ml->type != SMT_END; ml = ml->next) {
    if (ml->type == SMT_ZERO) {
      if (nb == DESC_BLOCK_SIZE-1) {
	dp->dst = (addr_t)-1;	/* Load new descriptors */
	dp->src = (addr_t)(dp+1) + descoffs;
	dp->len = sizeof(*dp)*min(nl, DESC_BLOCK_SIZE);
	dprintf("[ %08x %08x %08x ]\n", dp->dst, dp->src, dp->len);
	dp++; np++;
	nb = 0;
      }

      dp->dst = ml->start;
      dp->src = (addr_t)-1;	/* bzero region */
      dp->len = ml->next->start - ml->start;
      dprintf2("[ %08x %08x %08x ]\n", dp->dst, dp->src, dp->len);
      dp++; np++; nb++; nl--;
    }
  }

  if (np != need_ptrs) {
    dprintf("!!! np = %d : nmoves = %d, nzero = %d, desc_blocks = %d\n",
	    np, nmoves, nzero, desc_blocks);
  }

  /* Set up the primary descriptors in the bounce buffer.
     The first one moves the descriptor list into its designated safe
     zone, the second one loads the first descriptor block. */
  dp = primaries;

  dp->dst = descaddr;
  dp->src = (addr_t)dbuf;
  dp->len = np*sizeof(*dp);
  dprintf("< %08x %08x %08x >\n", dp->dst, dp->src, dp->len);
  dp++;

  dp->dst = (addr_t)-1;
  dp->src = descaddr;
  dp->len = sizeof(*dp)*min(np, DESC_BLOCK_SIZE);
  dprintf("< %08x %08x %08x >\n", dp->dst, dp->src, dp->len);
  dp++;

  memcpy(__com32.cs_bounce, primaries, 2*sizeof(*dp));

  rv = 2;			/* Always two primaries */

 bail:
  /* This is safe only because free() doesn't use the bounce buffer!!!! */
  if (moves)
    syslinux_free_movelist(moves);
  if (rxmap)
    syslinux_free_memmap(rxmap);

  return rv;
}
