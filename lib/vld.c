/*
 * $Id: vld.c,v 1.4 1996/04/03 14:24:31 kilian Exp $
 *
 * Read variable sized quantities and data.
 *
 * $Log: vld.c,v $
 * Revision 1.4  1996/04/03 14:24:31  kilian
 * Fixed some bugs when using mbuf_get.
 * Made argument to vld_size and vld_data const.
 *
 * Revision 1.3  1996/04/02  23:27:48  kilian
 * Treat writing of out-of-range vlq's and failing allocations as fatal errors.
 *
 * Revision 1.2  1996/04/02  10:19:57  kilian
 * Adapted changes of the print functions.
 *
 * Revision 1.1  1996/04/01  19:11:06  kilian
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "print.h"
#include "vld.h"


/*
 * Read a variable length quantity (e.g. delta time) from the buffer.
 * If an error occurs (too large value), -1 is returned and the buffer
 * is left untouched. Otherwise, the vlq is returned and the buffer
 * pointer is advanced to the byte after the vlq.
 */
long read_vlq(MBUF *b)
{
  long vlq = 0;
  long i, n = b->n - b->i;
  unsigned char *ptr = b->b + b->i;

  if(n > 4) n = 4;

  i = 0;
  while(ptr[i] & 0x80 && n-- > 1)
    vlq = vlq << 7 | ptr[i++] & 0x7f;

  if(n < 1)
    {
      midiprint(MPError, "reading vlq: end of input");
      return -1;
    }

  vlq = vlq << 7 | ptr[i++];

  b->i += i;

  return vlq;
}


/*
 * Write `vlq' as variable length quantity. `vlq' must be positive and
 * not greater than 0x0fffffff (28 bit).
 * Return 0 on error (in this case `b' may be invalid), else the number
 * of bytes written.
 */
int write_vlq(MBUF *b, long vlq)
{
  char buf[4];
  int i, result;

  if(vlq < 0 || vlq > 0x0fffffff)
    {
      midiprint(MPFatal, "writing vlq: out of range");
      return 0;
    }

  result = 0;
  buf[result++] = vlq & 0x7f;
  while(vlq > 0x7f)
    {
      vlq >>= 7;
      buf[result++] = 0x80 | vlq & 0x7f;
    }

  i = result;
  while(i-- > 0)
    if(mbuf_put(b, buf[i]) == EOF)
      return 0;

  return result;
}


/*
 * Read variable length data, i.e. a vlq and following data bytes.
 * Returns the data pointer, or NULL on error.
 */
void *read_vld(MBUF *b)
{
  long length;
  long i = b->i;
  void *data;

  if((length = read_vlq(b)) < 0)
    return NULL;

  if(b->i + length > b->n)
    {
      midiprint(MPError, "reading vld: end of input");
      b->i = i;
      return NULL;
    }

  if(!(data = malloc(sizeof(long) + length)))
    {
      midiprint(MPFatal, "%s", strerror(errno));
      b->i = i;
      return NULL;
    }

  *(long*)data = length;
  memcpy(data + sizeof(long), b->b + b->i, length);
  b->i += length;

  return data;
}


/*
 * Get the size of vld.
 */
long vld_size(const void *vld)
{
  const long *size = vld;
  return *size;
}


/*
 * Get the data of vld.
 */
const void *vld_data(const void *vld)
{
  return vld + sizeof(long);
}


/*
 * Write variable length data, i.e. a vlq and following data bytes.
 * The number of bytes written, or 0 on error.
 */
long write_vld(MBUF *b, const void *vld)
{
  long length = vld_size(vld);
  const char *data = vld_data(vld);
  long result;

  if(!(result = write_vlq(b, length)))
    return 0;

  result += length;
  while(length-- > 0)
    if(mbuf_put(b, *data++) == EOF)
      return 0;

  return result;
}
