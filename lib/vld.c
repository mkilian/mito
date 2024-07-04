/*
 * Read variable sized quantities and data.
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
  unsigned long p = mbuf_pos(b);
  int n = 0;
  unsigned char c = 0;

  while(mbuf_request(b, 1) && n++ < 4 && (c = mbuf_get(b)) & 0x80)
  	vlq = vlq << 7 | c & 0x7f;

  if(n < 1)
    {
      midiprint(MPError, "reading vlq: end of input");
      mbuf_set(b, p);
      return -1;
    }

  if(c & 0x80)
    {
      midiprint(MPError, "reading vlq: out of range");
      mbuf_set(b, p);
      return -1;
    }

  vlq = vlq << 7 | c;

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
  unsigned long p = mbuf_pos(b);
  long length;
  void *data;
  char *ptr;

  if((length = read_vlq(b)) < 0)
    return NULL;

  if(!mbuf_request(b, length))
    {
      midiprint(MPError, "reading vld: end of input");
      mbuf_set(b, p);
      return NULL;
    }

  if(!(data = malloc(sizeof(long) + length)))
    {
      midiprint(MPFatal, "%s", strerror(errno));
      mbuf_set(b, p);
      return NULL;
    }

  *(long*)data = length;
  ptr = ((char*)data) + sizeof(long);
  while(length--)
  	*ptr++ = mbuf_get(b);

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
