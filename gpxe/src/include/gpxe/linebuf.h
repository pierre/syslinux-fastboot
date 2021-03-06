#ifndef _GPXE_LINEBUF_H
#define _GPXE_LINEBUF_H

/** @file
 *
 * Line buffering
 *
 */

#include <stdint.h>
#include <stddef.h>

/** A line buffer */
struct line_buffer {
	/** Current string in the buffer */
	char *data;
	/** Length of current string, excluding the terminating NUL */
	size_t len;
	/** String is ready to read */
	int ready;
};

extern char * buffered_line ( struct line_buffer *linebuf );
extern ssize_t line_buffer ( struct line_buffer *linebuf,
			     const char *data, size_t len );
extern void empty_line_buffer ( struct line_buffer *linebuf );

#endif /* _GPXE_LINEBUF_H */
