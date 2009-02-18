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
 * See resume_lzf.c for Copyright information.
 */

#ifndef _RESUME_LZF_H
#define _RESUME_LZF_H

#include "resume_linux.h"

int lzf_decompress(const u8*, unsigned int, u8*, unsigned int*);

#endif /* _RESUME_LZF_H */
