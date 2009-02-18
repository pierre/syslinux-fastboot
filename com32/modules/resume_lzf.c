/*
 * Cryptoapi LZF compression module.
 *
 * based on the lzf.c file:
 *
 * Copyright (c) 2004-2008 Nigel Cunningham <nigel at tuxonice net>
 *
 * based on the deflate.c file:
 *
 * Copyright (c) 2003 James Morris <jmorris@intercode.com.au>
 *
 * and upon the LZF compression module donated to the TuxOnIce project with
 * the following copyright:
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * Copyright (c) 2000-2003 Marc Alexander Lehmann <pcg@goof.com>
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *   3.  The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License version 2 (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of the above. If you wish to
 * allow the use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * BSD license, indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL. If
 * you do not delete the provisions above, a recipient may use your version
 * of this file under either the BSD or the GPL.
 */

#include <string.h>

#include "resume_linux.h"

/**
 * @src:	Pointer to conmpressed data.
 * @slen:	Length of compressed data.
 * @dst:	Where to decompress.
 * @dlen:	Size decompressed.
 **/
int lzf_decompress(const u8 *src, unsigned int slen, u8 *dst,
		   unsigned int *dlen)
{
	u8 const *ip = src;
	u8 *op = dst;
	u8 const *const in_end = ip + slen;
	u8 *const out_end = op + *dlen;

	*dlen = PAGE_SIZE;
	do {
		unsigned int ctrl = *ip++;

		if (ctrl < (1 << 5)) {
			/* literal run */
			ctrl++;

			if (op + ctrl > out_end)
				return 0;
			memcpy(op, ip, ctrl);
			op += ctrl;
			ip += ctrl;
		} else {	/* back reference */

			unsigned int len = ctrl >> 5;

			u8 *ref = op - ((ctrl & 0x1f) << 8) - 1;

			if (len == 7)
				len += *ip++;

			ref -= *ip++;
			len += 2;

			if (op + len > out_end || ref < (u8 *) dst)
				return 0;

			do {
				*op++ = *ref++;
			} while (--len);
		}
	} while (op < out_end && ip < in_end);

	*dlen = op - (u8 *) dst;
	return 0;
}
