/*
 * $Id: vld.h,v 1.2 1996/04/03 14:24:31 kilian Exp $
 *
 * Read variable sized quantities and data.
 *
 * $Log: vld.h,v $
 * Revision 1.2  1996/04/03 14:24:31  kilian
 * Made argument to vld_size and vld_data const.
 *
 * Revision 1.1  1996/04/01  19:10:57  kilian
 * Initial revision
 *
 */

#ifndef __VLD_H__
#define __VLD_H__

#include "buffer.h"

/*
 * Read a variable length quantity (e.g. delta time) from the buffer.
 * If an error occurs (too large value), -1 is returned and the buffer
 * is left untouched. Otherwise, the vlq is returned and the buffer
 * pointer is advanced to the byte after the vlq.
 */
long read_vlq(MBUF *b);


/*
 * Write `vlq' as variable length quantity. `vlq' must be positive and
 * not greater than 0x0fffffff (28 bit).
 * Return 0 on error (in this case `b' may be invalid), else the number
 * of bytes written.
 */
int write_vlq(MBUF *b, long vlq);


/*
 * Read variable length data, i.e. a vlq and following data bytes.
 * Returns the data pointer, or NULL on error.
 */
void *read_vld(MBUF *b);


/*
 * Write variable length data, i.e. a vlq and following data bytes.
 * The number of bytes written, or 0 on error.
 */
long write_vld(MBUF *b, const void *vld);


/*
 * Get the size of vld.
 */
long vld_size(const void *vld);


/*
 * Get the data of vld.
 */
const void *vld_data(const void *vld);

#endif /* __VLD_H__ */
