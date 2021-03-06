/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
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
 * pxe_get_cached.c
 *
 * PXE call "get cached info"
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <com32.h>

#include <syslinux/pxe.h>

/* Returns the status code from PXE (0 on success),
   or -1 on invocation failure */
int pxe_get_cached_info(int level, void **buf, size_t *len)
{
  com32sys_t regs;
  t_PXENV_GET_CACHED_INFO *gci = __com32.cs_bounce;
  void *bbuf, *nbuf;

  memset(&regs, 0, sizeof regs);
  regs.eax.w[0] = 0x0009;
  regs.ebx.w[0] = PXENV_GET_CACHED_INFO;
  regs.es       = SEG(gci);
  regs.edi.w[0] = OFFS(gci);

  bbuf = &gci[1];

  gci->Status = PXENV_STATUS_FAILURE;
  gci->PacketType = level;
  gci->BufferSize = gci->BufferLimit = 65536-sizeof(*gci);
  gci->Buffer.seg = SEG(bbuf);
  gci->Buffer.offs = OFFS(bbuf);

  __intcall(0x22, &regs, &regs);

  if (regs.eflags.l & EFLAGS_CF)
    return -1;

  if (gci->Status)
    return gci->Status;

  nbuf = malloc(gci->BufferSize); /* malloc() does not use the bounce buffer */
  if (!nbuf)
    return -1;

  memcpy(nbuf, bbuf, gci->BufferSize);

  *buf = nbuf;
  *len = gci->BufferSize;

  return 0;
}
