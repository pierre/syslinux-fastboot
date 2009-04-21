#include <com32.h>
#include <stdlib.h>
#include <string.h>
#include <disk/read.h>
#include <disk/write.h>
#include <disk/common.h>
#include <disk/util.h>

/**
 * write_sectors - write several sectors from disk
 * @drive_info:		driveinfo struct describing the disk
 * @lba:		Position to write
 * @data:		Buffer to write
 * @size:		Size of the buffer (number of sectors)
 * @error:		Return the error code on failure
 **/
int write_sectors(const struct driveinfo* drive_info, const unsigned int lba,
		  const void *data, const int size, int *error)
{
	com32sys_t inreg, outreg;
	struct ebios_dapa *dapa = __com32.cs_bounce;
	void *buf = (char *)__com32.cs_bounce + size;

	memcpy(buf, data, size);
	memset(&inreg, 0, sizeof inreg);

	if ( drive_info->ebios ) {
		dapa->len = sizeof(*dapa);
		dapa->count = size;
		dapa->off = OFFS(buf);
		dapa->seg = SEG(buf);
		dapa->lba = lba;

		inreg.esi.w[0] = OFFS(dapa);
		inreg.ds       = SEG(dapa);
		inreg.edx.b[0] = drive_info->disk;
		inreg.eax.w[0] = 0x4300;	/* Extended write */
	} else {
		unsigned int c, h, s;

		if ( !drive_info->cbios ) {
		  /* We failed to get the geometry */

		  if ( lba )
		    return -1;		/* Can only write MBR */

		  s = 1;  h = 0;  c = 0;
		} else
		    lba_to_chs(drive_info, lba, &s, &h, &c);

		if ( s > 63 || h > 256 || c > 1023 )
		  return -1;

		inreg.eax.w[0] = 0x0301;	/* Write one sector */
		inreg.ecx.b[1] = c & 0xff;
		inreg.ecx.b[0] = s + (c >> 6);
		inreg.edx.b[1] = h;
		inreg.edx.b[0] = drive_info->disk;
		inreg.ebx.w[0] = OFFS(buf);
		inreg.es       = SEG(buf);
	}

	/* Perform the write */
	if (int13_retry(&inreg, &outreg)) {
		if (error)
			*error = outreg.eax.b[1];
		return -1;	/* Give up */
	} else {
		if (error)
			*error = 0;
		return 0;
	}
}

/**
 * write_verify_sectors - write several sectors from disk
 * @drive_info:		driveinfo struct describing the disk
 * @lba:		Position to write
 * @data:		Buffer to write
 **/
int write_verify_sector(struct driveinfo* drive_info,
			const unsigned int lba,
			const void *data, int *error)
{
	return write_verify_sectors(drive_info, lba, data, SECTOR, error);
}

/**
 * write_verify_sectors - write several sectors from disk
 * @drive_info:		driveinfo struct describing the disk
 * @lba:		Position to write
 * @data:		Buffer to write
 * @size:		Size of the buffer (number of sectors)
 **/
int write_verify_sectors(struct driveinfo* drive_info,
			 const unsigned int lba,
			 const void *data, const int size, int* error)
{
	char *rb;
	int rv;

	rv = write_sectors(drive_info, lba, data, size, error);
	if (rv)
		return rv;		/* Write failure */

	rb = read_sectors(drive_info, lba, size, error);
	if (!rb)
		return -1;		/* Readback failure */

	rv = memcmp(data, rb, SECTOR);
	free(rb);
	return rv ? -1 : 0;
}
