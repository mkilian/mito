/*
 * $Id: buffer.c,v 1.1 1996/04/01 19:11:06 kilian Exp $
 *
 * In-memory buffer.
 *
 * $Log: buffer.c,v $
 * Revision 1.1  1996/04/01 19:11:06  kilian
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "buffer.h"

/*
 * Read the file into the buffer.
 * Returns 0 on success, else -1.
 */
int read_mbuf(MBUF *b, FILE *f)
{
  char buf[1024];
  long size = 0;
  b->i = b->n = 0;
  b->b = NULL;

  while((size = fread(buf, 1, 1024, f)) > 0)
    {
      b->n += size;
      if(!(b->b = realloc(b->b, b->n)))
        return -1;
      memcpy(b->b + b->i, buf, size);
      b->i += size;
    }

  if(ferror(f))
    {
      free(b->b);
      return -1;
    }

  b->i = 0;

  return 0;
}


/*
 * Write the buffer to the file.
 * Returns -1 on error and 0 on success.
 */
int write_mbuf(MBUF *b, FILE *f)
{
  if(b->n > 0 && fwrite(b->b, b->n, 1, f) != 1)
    return -1;
  else
    return 0;
}


/*
 * Get the current position of a buffer.
 */
unsigned long mbuf_pos(MBUF *b)
{
  return b->i;
}


/*
 * Set the position of a buffer.
 * If the position is negative, set relative to the end of buffer.
 * Returns the new position which may be different from `pos' if `pos'
 * is out of range.
 */
unsigned long mbuf_set(MBUF *b, unsigned long pos)
{
  if(pos < 0)
    pos = b->n + pos;

  if(pos < 0)
    b->i = 0;
  else if(pos < b->n)
    b->i = pos;

  return b->i;
}


/*
 * Get the remaining size of the buffer (from the current position to
 * the end).
 */
unsigned long mbuf_rem(MBUF *b)
{
  if(b->i < b->n)
    return b->n - b->i;
  else
    return 0;
}


/*
 * Get the address of the byte at the current position.
 * This allows to directly manipulate buffer. It should be used with
 * care, especially when writing data.
 */
void *mbuf_adr(MBUF *b)
{
  return b->b + b->i;
}


/*
 * Get the character at the current position of the buffer and advance
 * the position.
 * Returns the character or EOF if the end of the buffer is reached.
 */
int mbuf_get(MBUF *b)
{
  if(b->i >= b->n)
    return EOF;
  else
    return (b->b[b->i++]) & 0xff;
}


/*
 * Put a character at the current position in the buffer and advance the
 * position. If the current position is a the end of the buffer, the
 * buffer is automatically enlarged.
 * Returns the stored character or EOF on errors.
 */
int mbuf_put(MBUF *b, int ch)
{
  ch &= 0xff;

  if(b->i >= b->n && !(b->b = realloc(b->b, ++b->n)))
    return EOF;

  return (b->b[b->i++] = ch) & 0xff;
}


/*
 * Free the data of `b'.
 */
void mbuf_free(MBUF *b)
{
  free(b->b);
}


/*
 * Insert buffer `b2' at the current position of `b1'.
 * Returns 0 on success, else -1. In the latter case, `b1' may be
 * invalid.
 */
int mbuf_insert(MBUF *b1, MBUF *b2)
{
  if(!b2->n)  /* b2 empty */
    return 0;

  if(!(b1->b = realloc(b1->b, b1->n + b2->n)))
    return -1;

  if(b1->i < b1->n)
    memmove(b1->b + b1->i + b2->n, b1->b + b1->i, b2->n);

  memcpy(b1->b + b1->i, b2->b, b2->n);
  b1->n += b2->n;
  b2->i += b2->n;
  return 0;
}
