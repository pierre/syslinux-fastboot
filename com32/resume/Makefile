## -----------------------------------------------------------------------
##
##   Copyright 2001-2008 H. Peter Anvin - All Rights Reserved
##   Copyright 2009 Pierre-Alexandre Meyer <pierre@mouraf.org>
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## resume Makefile
##

topdir		:= ../..
include $(topdir)/MCONFIG.embedded
com32		:= $(topdir)/com32

MODULES		:= resume.c32

INCLUDES	:= -I$(com32)/include

LIBGCC		:= $(shell $(CC) $(GCCOPT) --print-libgcc)
LIB		:= liboldcom32.a

LIBS		:= $(LIB) $(LIBGCC) \
		   $(com32)/libutil/libutil_com.a \
		   $(com32)/lib/libcom32.a

LDFLAGS		:= -m elf_i386 -T $(com32)/lib/com32.ld

all: $(MODULES) $(LIB)

resume.elf: resume_restore.o resume_parser.o resume_debug.o \
	    resume_trampoline.o resume_bitops.o resume_bitmaps.o \
	    resume.o resume_lzf.o resume_symbols.o resume_trampoline_asm.o \
	    $(LIBS)
	@echo "  LD      $@"
	@$(LD) $(LDFLAGS) -o $@ $^

resume-test:
	$(CC) -funit-at-a-time -g resume.c resume_restore.c resume_parser.c \
				   resume_debug.c resume_bitops.c \
				   resume_bitmaps.c resume_lzf.c \
				   -o resume -DDEBUG -DTESTING -Wall

.PRECIOUS: %.o
%.o: %.S
	@echo "  CC      $@"
	@$(CC) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c
	@echo "  CC      $@"
	@$(CC) $(CFLAGS) -std=gnu99  -D__COM32__ -c -o $@ $<

%.c32: %.elf
	@echo "  OBJCOPY $@"
	@$(OBJCOPY) -O binary $< $@

$(LIB):
	@rm -f $@
	@echo "  AR      $@ $^"
	@$(AR) cq $@ $^
	@echo "  RANLIB  $@ $^"
	@$(RANLIB) $@

tidy dist:
	@rm -f *.o *.a *.lst *.elf

install: all

clean: tidy
	@echo "  CLEAN   *.a *.lst *.elf *.o *.c32 *.c~ *.h~ Makefile~"
	@rm -f *.o *.c32 *.c~ *.h~ Makefile~

spotless: clean
