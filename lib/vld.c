/*
 * $Id: vld.c,v 1.1 1996/04/01 19:11:06 kilian Exp $
 *
 * Read variable sized quantities and data.
 *
 * $Log: vld.c,v $
 * Revision 1.1  1996/04/01 19:11:06  kilian
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
      midierror("reading vlq: end of input");
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
  int result;

  if(vlq < 0 || vlq > 0x0fffffff)
    {
      midierror("writing vlq: out of range");
      return 0;
    }

  if(vlq >= 0x80 &&
     (result = write_vlq(b, vlq >> 7)) >= 0 &&
     mbuf_put(b, vlq & 0x7f))
    return result + 1;
  else if(vlq < 0x80)
    return mbuf_put(b, vlq) ? 1 : 0;
  else
    return 0;
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
      midierror("reading vld: end of input");
      b->i = i;
      return NULL;
    }

  if(!(data = malloc(sizeof(long) + length)))
    {
      midierror("%s", strerror(errno));
      b->i = i;
      return NULL;
    }

  *(long*)data = length;
  memcpy(data + sizeof(long), b->b + b->i, length);
  b->i += length;

  return data;
}


/*
 * Write variable length data, i.e. a vlq and following data bytes.
 * The number of bytes written, or 0 on error.
 */
long write_vld(MBUF *b, void *vld)
{
  long length = vld_size(vld);
  char *data = vld_data(vld);
  long result;

  if(!(result = write_vlq(b, length)))
    return 0;

  result += length;
  while(length-- > 0)
    if(!mbuf_put(b, *data++))
      return 0;

  return result;
}


/*
 * Get the size of vld.
 */
long vld_size(void *vld)
{
  long *size = vld;
  return *size;
}


/*
 * Get the data of vld.
 */
void *vld_data(void *vld)
{
  return vld + sizeof(long);
}
