SUBDIRS = lib gpllib libutil modules menu samples rosh cmenu hdt
all tidy dist clean spotless install:
	set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
