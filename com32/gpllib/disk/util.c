#include <com32.h>
#include <stdlib.h>
#include <string.h>

#include <disk/geom.h>
#include <disk/read.h>
#include <disk/partition.h>

#define MAX_NB_RETRIES 6

/**
 * find_logical_partition - look for an extended partition
 *
 * Search for a logical partition.  Logical partitions are actually implemented
 * as recursive partition tables; theoretically they're supposed to form a
 * linked list, but other structures have been seen.

 * To make things extra confusing: data partition offsets are relative to where
 * the data partition record is stored, whereas extended partition offsets
 * are relative to the beginning of the extended partition all the way back
 * at the MBR... but still not absolute!
 **/
static struct part_entry *
find_logical_partition(int whichpart, char *table, struct part_entry *self,
		       struct part_entry *root, int *nextpart, struct driveinfo * drive_info)
{
	static struct part_entry ltab_entry;
	struct part_entry *ptab = (struct part_entry *)(table + PARTITION_TABLES_OFFSET);
	struct part_entry *found;
	char *sector;
	int i;

	if ( *(uint16_t *)(table + 0x1fe) != 0xaa55 )
		return NULL;		/* Signature missing */

	/*
	 * We are assumed to already having enumerated all the data partitions
	 * in this table if this is the MBR.  For MBR, self == NULL.
	 */

	if ( self ) {
		/* Scan the data partitions. */
		for ( i = 0 ; i < 4 ; i++ ) {
			if ( ptab[i].ostype == 0x00 || ptab[i].ostype == 0x05 ||
			     ptab[i].ostype == 0x0f || ptab[i].ostype == 0x85 )
				continue;		/* Skip empty or extended partitions */

			if ( !ptab[i].length )
				continue;

			/* Adjust the offset to account for the extended partition itself */
			ptab[i].start_lba += self->start_lba;

			/*
			 * Sanity check entry: must not extend outside the extended partition.
			 * This is necessary since some OSes put crap in some entries.
			 */
			if ( ptab[i].start_lba + ptab[i].length <= self->start_lba ||
			     ptab[i].start_lba >= self->start_lba + self->length )
				continue;

			/* OK, it's a data partition.  Is it the one we're looking for? */
			if ( (*nextpart)++ == whichpart ) {
				memcpy(&ltab_entry, &ptab[i], sizeof ltab_entry);
				return &ltab_entry;
			}
		}
	}

	/* Scan the extended partitions. */
	for ( i = 0 ; i < 4 ; i++ ) {
		if ( ptab[i].ostype != 0x05 &&
		     ptab[i].ostype != 0x0f && ptab[i].ostype != 0x85 )
			continue;		/* Skip empty or data partitions */

		if ( !ptab[i].length )
			continue;

		/* Adjust the offset to account for the extended partition itself */
		if ( root )
			ptab[i].start_lba += root->start_lba;

		/*
		 * Sanity check entry: must not extend outside the extended partition.
		 * This is necessary since some OSes put crap in some entries.
		 */
		if ( root )
		  if ( ptab[i].start_lba + ptab[i].length <= root->start_lba ||
		       ptab[i].start_lba >= root->start_lba + root->length )
			continue;

		/* Process this partition */
		if ( !(sector = read_sectors(drive_info, ptab[i].start_lba, 1, NULL)) )
		  continue;			/* Read error, must be invalid */

		found = find_logical_partition(whichpart, sector, &ptab[i],
					       root ? root : &ptab[i], nextpart, drive_info);
		free(sector);
		if (found)
			return found;
	}

	/* If we get here, there ain't nothing... */
	return NULL;
}

/**
 * get_pentry - retrieve a partition entry from a disk
 * @d:		Disk where the partition is to be found
 * @ptab:	Will store the entry, if found (filled with 0 otherwise)
 * @i:		Partition number to find
 * @error:	Will store I/O error code, on failure
 *
 * Given a disk and a partition number, retrieve the associated partition entry.
 * Partition 1->4 are assumed to be primary, 5+ to be extended.
 **/
void get_pentry(struct driveinfo *d, struct part_entry * ptab, int i, int *error)
{
	struct part_entry *partinfo;
	char *mbr = read_mbr(d->disk, error);
	if (mbr) {
		if (i < 5)
			partinfo = (struct part_entry *) (mbr + PARTITION_TABLES_OFFSET +
								i * sizeof(struct part_entry));
		else {
			int nextpart = 5;
			partinfo = find_logical_partition(i, mbr, NULL, NULL, &nextpart, d);
		}
		memcpy(ptab, partinfo, sizeof (struct part_entry));
	} else
		memset(ptab, 0, sizeof ptab);
}

/**
 * int13_retry - int13h with error handling
 * @inreg:	int13h function parameters
 * @outreg:	output registers
 *
 * Call int 13h, but with retry on failure.  Especially floppies need this.
 **/
int int13_retry(const com32sys_t *inreg, com32sys_t *outreg)
{
	int retry = MAX_NB_RETRIES;		/* Number of retries */
	com32sys_t tmpregs;

	if ( !outreg ) outreg = &tmpregs;

	while ( retry-- ) {
		__intcall(0x13, inreg, outreg);
		if ( !(outreg->eflags.l & EFLAGS_CF) )
			return 0;			/* CF=0 => OK */
	}

	/* If we get here: error */
	return -1;
}
