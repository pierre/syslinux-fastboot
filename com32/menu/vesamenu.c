/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * vesamenu.c
 *
 * Simple menu system which displays a list and allows the user to select
 * a command line and/or edit it.
 *
 * VESA graphics version.
 */

#include <stdio.h>
#include <console.h>
#include <syslinux/vesacon.h>

#include "menu.h"

void console_prepare(void)
{
  fputs("\033[0m\033[25l", stdout);
}

void console_cleanup(void)
{
  /* For the serial console, be nice and clean up */
  fputs("\033[0m", stdout);
}

int draw_background(const char *what)
{
  if (!what)
    return vesacon_default_background();
  else if (what[0] == '#')
    return vesacon_set_background(parse_argb((char **)&what));
  else
    return vesacon_load_background(what);
}

int main(int argc, char *argv[])
{
  openconsole(&dev_rawcon_r, &dev_vesaserial_w);
  return menu_main(argc, argv);
}
