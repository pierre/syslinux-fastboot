; -*- fundamental -*- (asm-mode sucks)
; -----------------------------------------------------------------------
;
;   Copyright 2006-2008 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Boston MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; head.inc
;
; Common header includes
;

%ifndef _HEAD_INC
%define _HEAD_INC

%include "macros.inc"
%include "config.inc"
%include "layout.inc"
%include "kernel.inc"
%include "bios.inc"
%include "tracers.inc"
%include "stack.inc"

%endif ; _HEAD_INC
