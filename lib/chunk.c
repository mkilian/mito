/*
 * $Id: chunk.c,v 1.1 1996/04/01 19:11:06 kilian Exp $
 *
 * Get header and track chunks of standard midi files.
 *
 * $Log: chunk.c,v $
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
#include "chunk.h"


/*
 * Supported chunk types.
 */
const unsigned long MThd = 'M' << 24 | 'T' << 16 | 'h' << 8 | 'd';
const unsigned long MTrk = 'M' << 24 | 'T' << 16 | 'r' << 8 | 'k';


/*
 * Check if there is a header chunk at the current position of `b'.  If
 * so, fill in the given chunk position, update the buffer position and
 * return 1 else return 0.
 * If there is an invalid header chunk, a warning is written.
 */
static int tryMThd(MBUF *b, CHUNK *c)
{
  long size;
  int fmt, ntrk, div;
  unsigned char *ptr = b->b + b->i;
  unsigned long i = b->i + 8;
  unsigned long n = b->n;

  if(i >= n || ptr[0] != 'M' || ptr[1] != 'T' || ptr[2] != 'h' || ptr[3] != 'd')
    return 0;

  size = ptr[4] << 24 | ptr[5] << 16 | ptr[6] << 8 | ptr[7];

  if(size < 6)
    {
      midierror("skipping header: size too short");
      return 0;
    }
  if(size > 6)
    midiwarn("unusual long header: %ld bytes", size);
  if(i + 6 >= n)
    {
      midierror("skipping header: truncated header at end of file");
      return 0;
    }
  if(i + size >= n)
    midiwarn("truncated but usable header at end of file");

  fmt = ptr[8] << 8 | ptr[9];
  ntrk = ptr[10] << 8 | ptr[11];
  div = ptr[12] << 8 | ptr[13];
  if(fmt < 0 || fmt > 2)
    {
      midierror("skipping header: illegal format %d", fmt);
      return 0;
    }
  if(ntrk < 0)
    {
      midierror("skipping header: number of tracks %d", ntrk);
      return 0;
    }
  if(!div)
    {
      midierror("skipping header: division is 0");
      return 0;
    }

  b->i += 14;
  c->type = MThd;
  c->hdr.mthd.fmt = fmt;
  c->hdr.mthd.ntrk = ntrk;
  c->hdr.mthd.div = div;
  c->hdr.mthd.xsize = size - 6;

  return 1;
}


/*
 * As above, but for tracks.
 * `mtl', if nonzero, is the maximum allowed track length.
 */
static int tryMTrk(MBUF *b, CHUNK *c, long mtl)
{
  long size;
  unsigned char *ptr = b->b + b->i;
  unsigned long i = b->i + 8;
  unsigned long n = b->n;

  if(i >= n || ptr[0] != 'M' || ptr[1] != 'T' || ptr[2] != 'r' || ptr[3] != 'k')
    return 0;

  size = ptr[4] << 24 | ptr[5] << 16 | ptr[6] << 8 | ptr[7];

  if(size < 0)
    {
      midierror("skipping track: negative size %ld", size);
      return 0;
    }
  if(mtl > 0 && size > mtl)
    midierror("skipping track: track of %ld bytes too large", size);

  b->i += 8;
  c->type = MTrk;
  c->hdr.mtrk.size = size;

  return 1;
}


/*
 * Search the given buffer of size `size' until a header or track chunk
 * is found.  The name is used in warning messages. If it is a NULL
 * pointer, no messages will be written.
 *
 * There are some simple consistency checks on header chunks:
 *   - The header must be at least 14 bytes large (including the name
 *     and the size field.
 *   - The format must be 0, 1 or 2.
 *   - The number of tracks must not be negative. However, a zero track
 *     number is accepted.
 *   - The division must not be zero.
 *
 * For track chunks, the size must not be negative and not greater than
 * `mtl', if this argument is positive.
 *
 * Only if the above conditions are met, the function returns success.
 * Corrupted headers and tracks are skipped after issuing a warning
 * message (if name is set).
 *
 * The functions returns -1 if no valid chunk is found, otherwise the
 * number of bytes skipped before the chunk (normally 0).
 */
long search_chunk(MBUF *b, CHUNK *chunk, unsigned long mtl)
{
  long i = b->i;

  /*
   * Search the header including all fields.
   * Corrupted headers are skipped after issuing a warning message.
   */
  while(b->i < b->n && !tryMThd(b, chunk) && !tryMTrk(b, chunk, mtl))
    b->i++;

  if(b->i >= b->n)
    return -1;

  if(chunk->type == MThd)
    return b->i - i - 14;
  else if(chunk->type == MTrk)
    return b->i - i - 8;
  else
    {
      midierror("no type; this can't happen");
      return -1;
    }
}
