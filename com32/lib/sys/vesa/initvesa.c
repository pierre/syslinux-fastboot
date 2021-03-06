/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1999-2008 H. Peter Anvin - All Rights Reserved
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
 * initvesa.c
 *
 * Query the VESA BIOS and select a 640x480x32 mode with local mapping
 * support, if one exists.
 */

#include <inttypes.h>
#include <com32.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fpu.h>
#include "vesa.h"
#include "video.h"
#include "fill.h"
#include "debug.h"

struct vesa_info __vesa_info;

struct vesa_char *__vesacon_text_display;

int __vesacon_font_height, __vesacon_text_rows;
enum vesa_pixel_format __vesacon_pixel_format = PXF_NONE;
unsigned int __vesacon_bytes_per_pixel;
uint8_t __vesacon_graphics_font[FONT_MAX_CHARS][FONT_MAX_HEIGHT];

uint32_t __vesacon_background[VIDEO_Y_SIZE][VIDEO_X_SIZE];

static void unpack_font(uint8_t *dst, uint8_t *src, int height)
{
  int i;

  for (i = 0; i < FONT_MAX_CHARS; i++) {
    memcpy(dst, src, height);
    memset(dst+height, 0, FONT_MAX_HEIGHT-height);

    dst += FONT_MAX_HEIGHT;
    src += height;
  }
}

static int __constfunc is_power_of_2(unsigned int x)
{
  return x && !(x & (x-1));
}

static int vesacon_paged_mode_ok(const struct vesa_mode_info *mi)
{
  int i;

  if (!is_power_of_2(mi->win_size) ||
      !is_power_of_2(mi->win_grain) ||
      mi->win_grain > mi->win_size)
    return 0;			/* Impossible... */

  for (i = 0; i < 2; i++) {
    if ((mi->win_attr[i] & 0x05) == 0x05 && mi->win_seg[i])
      return 1;			/* Usable window */
  }

  return 0;			/* Nope... */
}

static int vesacon_set_mode(void)
{
  com32sys_t rm;
  uint8_t *rom_font;
  uint16_t mode, bestmode, *mode_ptr;
  struct vesa_general_info *gi;
  struct vesa_mode_info *mi;
  enum vesa_pixel_format pxf, bestpxf;

  /* Allocate space in the bounce buffer for these structures */
  gi = &((struct vesa_info *)__com32.cs_bounce)->gi;
  mi = &((struct vesa_info *)__com32.cs_bounce)->mi;

  debug("Hello, World!\r\n");

  memset(&rm, 0, sizeof rm);
  memset(gi, 0, sizeof *gi);

  gi->signature = VBE2_MAGIC;	/* Get VBE2 extended data */
  rm.eax.w[0] = 0x4F00;		/* Get SVGA general information */
  rm.edi.w[0] = OFFS(gi);
  rm.es      = SEG(gi);
  __intcall(0x10, &rm, &rm);

  if ( rm.eax.w[0] != 0x004F )
    return 1;			/* Function call failed */
  if ( gi->signature != VESA_MAGIC )
    return 2;			/* No magic */
  if ( gi->version < 0x0102 )
    return 3;			/* VESA 1.2+ required */

  /* Copy general info */
  memcpy(&__vesa_info.gi, gi, sizeof *gi);

  /* Search for a 640x480 mode with a suitable color and memory model... */

  mode_ptr = GET_PTR(gi->video_mode_ptr);
  bestmode = 0;
  bestpxf  = PXF_NONE;

  while ((mode = *mode_ptr++) != 0xFFFF) {
    mode &= 0x1FF;		/* The rest are attributes of sorts */

    debug("Found mode: 0x%04x\r\n", mode);

    memset(mi, 0, sizeof *mi);
    rm.eax.w[0] = 0x4F01;	/* Get SVGA mode information */
    rm.ecx.w[0] = mode;
    rm.edi.w[0] = OFFS(mi);
    rm.es  = SEG(mi);
    __intcall(0x10, &rm, &rm);

    /* Must be a supported mode */
    if ( rm.eax.w[0] != 0x004f )
      continue;

    debug("mode_attr 0x%04x, h_res = %4d, v_res = %4d, bpp = %2d, layout = %d (%d,%d,%d)\r\n",
	  mi->mode_attr, mi->h_res, mi->v_res, mi->bpp, mi->memory_layout,
	  mi->rpos, mi->gpos, mi->bpos);

    /* Must be an LFB color graphics mode supported by the hardware.

      The bits tested are:
       4 - graphics mode
       3 - color mode
       1 - mode information available (mandatory in VBE 1.2+)
       0 - mode supported by hardware
    */
    if ( (mi->mode_attr & 0x001b) != 0x001b )
      continue;

    /* Must be 640x480 */
    if ( mi->h_res != VIDEO_X_SIZE || mi->v_res != VIDEO_Y_SIZE )
      continue;

    /* We don't support multibank (interlaced memory) modes */
    /*
     *  Note: The Bochs VESA BIOS (vbe.c 1.58 2006/08/19) violates the
     * specification which states that banks == 1 for unbanked modes;
     * fortunately it does report bank_size == 0 for those.
     */
    if ( mi->banks > 1 && mi->bank_size ) {
      debug("bad: banks = %d, banksize = %d, pages = %d\r\n",
	    mi->banks, mi->bank_size, mi->image_pages);
      continue;
    }

    /* Must be either a flat-framebuffer mode, or be an acceptable
       paged mode */
    if ( !(mi->mode_attr & 0x0080) && !vesacon_paged_mode_ok(mi) ) {
      debug("bad: invalid paged mode\r\n");
      continue;
    }

    /* Must either be a packed-pixel mode or a direct color mode
       (depending on VESA version ); must be a supported pixel format */
    pxf = PXF_NONE;		/* Not usable */

    if (mi->bpp == 32 &&
	(mi->memory_layout == 4 ||
	 (mi->memory_layout == 6 && mi->rpos == 16 && mi->gpos == 8 &&
	  mi->bpos == 0)))
      pxf = PXF_BGRA32;
    else if (mi->bpp == 24 &&
	     (mi->memory_layout == 4 ||
	      (mi->memory_layout == 6 && mi->rpos == 16 && mi->gpos == 8 &&
	       mi->bpos == 0)))
      pxf = PXF_BGR24;
    else if (mi->bpp == 16 &&
	     (mi->memory_layout == 4 ||
	      (mi->memory_layout == 6 && mi->rpos == 11 && mi->gpos == 5 &&
	       mi->bpos == 0)))
      pxf = PXF_LE_RGB16_565;
    else if (mi->bpp == 15 &&
	     (mi->memory_layout == 4 ||
	      (mi->memory_layout == 6 && mi->rpos == 10 && mi->gpos == 5 &&
	       mi->bpos == 0)))
      pxf = PXF_LE_RGB15_555;

    if (pxf < bestpxf) {
      debug("Best mode so far, pxf = %d\r\n", pxf);

      /* Best mode so far... */
      bestmode = mode;
      bestpxf  = pxf;

      /* Copy mode info */
      memcpy(&__vesa_info.mi, mi, sizeof *mi);
    }
  }

  if (bestpxf == PXF_NONE)
    return 4;			/* No mode found */

  mi = &__vesa_info.mi;
  mode = bestmode;
  __vesacon_bytes_per_pixel = (mi->bpp+7) >> 3;
  __vesacon_format_pixels = __vesacon_format_pixels_list[bestpxf];

  /* Download the SYSLINUX- or BIOS-provided font */
  rm.eax.w[0] = 0x0018;		/* Query custom font */
  __intcall(0x22, &rm, &rm);
  if (!(rm.eflags.l & EFLAGS_CF) && rm.eax.b[0]) {
    __vesacon_font_height = rm.eax.b[0];
    rom_font = MK_PTR(rm.es, rm.ebx.w[0]);
  } else {
    rm.eax.w[0] = 0x1130;		/* Get Font Information */
    rm.ebx.w[0] = 0x0600;		/* Get 8x16 ROM font */
    __intcall(0x10, &rm, &rm);
    rom_font = MK_PTR(rm.es, rm.ebp.w[0]);
    __vesacon_font_height = 16;
  }
  unpack_font((uint8_t *)__vesacon_graphics_font, rom_font,
	      __vesacon_font_height);
  __vesacon_text_rows = (VIDEO_Y_SIZE-2*VIDEO_BORDER)/__vesacon_font_height;
  __vesacon_init_cursor(__vesacon_font_height);

  /* Now set video mode */
  rm.eax.w[0] = 0x4F02;		/* Set SVGA video mode */
  if (mi->mode_attr & 0x0080)
    mode |= 0x4000;		/* Request linear framebuffer if supported */
  rm.ebx.w[0] = mode;
  __intcall(0x10, &rm, &rm);
  if ( rm.eax.w[0] != 0x004F )
    return 9;			/* Failed to set mode */

  __vesacon_init_copy_to_screen();

  /* Tell syslinux we changed video mode */
  rm.eax.w[0] = 0x0017;		/* Report video mode change */
  /* In theory this should be:

     rm.ebx.w[0] = (mi->mode_attr & 4) ? 0x0007 : 0x000f;

     However, that would assume all systems that claim to handle text
     output in VESA modes actually do that... */
  rm.ebx.w[0] = 0x000f;
  rm.ecx.w[0] = VIDEO_X_SIZE;
  rm.edx.w[0] = VIDEO_Y_SIZE;
  __intcall(0x22, &rm, NULL);

  __vesacon_pixel_format = bestpxf;

  return 0;
}


static int init_text_display(void)
{
  size_t nchars;
  struct vesa_char *ptr;
  struct vesa_char def_char = {
    .ch = ' ',
    .attr = 0,
  };

  nchars = (TEXT_PIXEL_ROWS/__vesacon_font_height+2)*
    (TEXT_PIXEL_COLS/FONT_WIDTH+2);

  __vesacon_text_display = ptr = malloc(nchars*sizeof(struct vesa_char));

  if (!ptr)
    return -1;


  vesacon_fill(ptr, def_char, nchars);

  return 0;
}

int __vesacon_init(void)
{
  int rv;

  /* We need the FPU for graphics, at least libpng et al will need it... */
  if (x86_init_fpu())
    return 10;

  rv = vesacon_set_mode();
  if (rv)
    return rv;

  init_text_display();

  debug("Mode set, now drawing at %#p\r\n", __vesa_info.mi.lfb_ptr);

  __vesacon_init_background();

  debug("Ready!\r\n");
  return 0;
}
