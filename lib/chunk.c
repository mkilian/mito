/*
 * $Id: chunk.c,v 1.6 1996/05/21 11:48:00 kilian Exp $
 *
 * Get header and track chunks of standard midi files.
 *
 * $Log: chunk.c,v $
 * Revision 1.6  1996/05/21 11:48:00  kilian
 * The buffer structure has been hidden. This may allow reading and writing
 * files directly in future versions.
 *
 * Revision 1.5  1996/04/06 23:02:19  kilian
 * Fix in write_MThd.
 *
 * Revision 1.4  1996/04/03  14:23:54  kilian
 * Added write_MThd and write_MTrk.
 *
 * Revision 1.3  1996/04/02  23:26:30  kilian
 * Restore buffer position if no chunk was found.
 * Treat `This can't happen' as fatal error.
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
  unsigned long p = mbuf_pos(b);

  if(!mbuf_request(b, 8) ||
     mbuf_get(b) != 'M' ||
     mbuf_get(b) != 'T' ||
     mbuf_get(b) != 'h' ||
     mbuf_get(b) != 'd')
    {
      mbuf_set(b, p);
      return 0;
    }

  size = mbuf_get(b) << 24 | mbuf_get(b) << 16 | mbuf_get(b) << 8 | mbuf_get(b);

  if(size < 6)
    {
      midiprint(MPError, "skipping header: size too short");
      mbuf_set(b, p);
      return 0;
    }
  if(size > 6)
    midiprint(MPWarn, "unusual long header: %ld bytes", size);
  if(!mbuf_request(b, 6))
    {
      midiprint(MPError, "skipping header: truncated header at end of file");
      mbuf_set(b, p);
      return 0;
    }
  if(!mbuf_request(b, size))
    midiprint(MPWarn, "truncated but usable header at end of file");

  fmt = mbuf_get(b) << 8 | mbuf_get(b);
  ntrk = mbuf_get(b) << 8 | mbuf_get(b);
  div = mbuf_get(b) << 8 | mbuf_get(b);
  if(fmt < 0 || fmt > 2)
    {
      midiprint(MPError, "skipping header: illegal format %d", fmt);
      mbuf_set(b, p);
      return 0;
    }
  if(ntrk < 0)
    {
      midiprint(MPError, "skipping header: number of tracks %d", ntrk);
      mbuf_set(b, p);
      return 0;
    }
  if(!div)
    {
      midiprint(MPError, "skipping header: division is 0");
      mbuf_set(b, p);
      return 0;
    }

  c->type = MThd;
  c->hdr.mthd.fmt = fmt;
  c->hdr.mthd.ntrk = ntrk;
  c->hdr.mthd.div = div;
  c->hdr.mthd.xsize = size - 6;

  return 1;
}


/*
 * As above, but for tracks.
 */
static int tryMTrk(MBUF *b, CHUNK *c)
{
  long size;
  unsigned long p = mbuf_pos(b);

  if(!mbuf_request(b, 8) ||
     mbuf_get(b) != 'M' ||
     mbuf_get(b) != 'T' ||
     mbuf_get(b) != 'r' ||
     mbuf_get(b) != 'k')
    {
      mbuf_set(b, p);
      return 0;
    }

  size = mbuf_get(b) << 24 | mbuf_get(b) << 16 | mbuf_get(b) << 8 | mbuf_get(b);

  if(size < 0)
    {
      midiprint(MPError, "skipping track: negative size %ld", size);
      mbuf_set(b, p);
      return 0;
    }

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
 * For track chunks, the size must not be negative.
 *
 * Only if the above conditions are met, the function returns success.
 * Corrupted headers and tracks are skipped after issuing a warning
 * message (if name is set).
 *
 * The functions returns -1 if no valid chunk is found, otherwise the
 * number of bytes skipped before the chunk (normally 0).
 */
long search_chunk(MBUF *b, CHUNK *chunk)
{
  unsigned long p = mbuf_pos(b);
  unsigned long i = 0;

  /*
   * Search the header including all fields.
   * Corrupted headers are skipped after issuing a warning message.
   */
  while(mbuf_request(b, 8) && !tryMThd(b, chunk) && !tryMTrk(b, chunk))
    {
      (void)mbuf_get(b);
      i++;
    }

  if(!mbuf_request(b,1))
    return -1;

  if(chunk->type == MThd || chunk->type == MTrk)
    return i;
  else
    {
      midiprint(MPFatal, "no type; this can't happen");
      mbuf_set(b, p);
      return -1;
    }
}


/*
 * Write a header chunk with the given fields.
 * Return 1 on succes, 0 on error.
 */
int write_MThd(MBUF *b, int fmt, int ntrk, int div)
{
  char hdr[14] = "MThd\0\0\0\6";
  int i;

  hdr[8] = (fmt >> 8) & 0xff;
  hdr[9] = fmt & 0xff;
  hdr[10] = (ntrk >> 8) & 0xff;
  hdr[11] = ntrk & 0xff;
  hdr[12] = (div >> 8) & 0xff;
  hdr[13] = div & 0xff;

  for(i = 0; i < 14; i++)
    if(mbuf_put(b, hdr[i]) == EOF)
      return 0;

  return 1;
}


/*
 * Write a track chunk with the given size.
 */
int write_MTrk(MBUF *b, long size)
{
  char hdr[8] = "MTrk";
  int i;

  hdr[4] = (size >> 24)  & 0xff;
  hdr[5] = (size >> 16)  & 0xff;
  hdr[6] = (size >> 8)  & 0xff;
  hdr[7] = size  & 0xff;

  for(i = 0; i < 8; i++)
    if(mbuf_put(b, hdr[i]) == EOF)
      return 0;

  return 1;
}
