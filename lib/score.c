/*
 * $Id: score.c,v 1.3 1996/04/02 23:25:23 kilian Exp $
 *
 * Reading and writing of complete scores.
 *
 * $Log: score.c,v $
 * Revision 1.3  1996/04/02 23:25:23  kilian
 * Field `nev' removed. The tracks field now contains the number of events
 * in each track.
 *
 * Revision 1.2  1996/04/02  10:19:43  kilian
 * Changed score structure.
 *
 * Revision 1.1  1996/04/01  19:11:06  kilian
 * Initial revision
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "print.h"
#include "chunk.h"
#include "score.h"


/*
 * Read an event list from the buffer and append it to the event list
 * `es' that already contains `*n' events.
 * Returns a pointer to the events, which are terminated by an `End Of
 * Track' event. `*n' is updated accordingly.
 * If an error occurs (e.g. out of memory), a NULL pointer is returned.
 * If an `End Of Track' event comes before the end of the buffer, an
 * error message is issued and the remainder of the buffer is ignored.
 * If very last event of the buffer is no `End Of Track' event, an error
 * message is issued and an `End Of Track' event is automatically
 * appended to the list.
 */
static MFEvent *read_events(MBUF *b, MFEvent *es, long *n)
{
  char running = 0;
  MFEvent e;

  while(mbuf_rem(b) > 0 && read_event(b, &e, &running))
    {
      if(!(es = realloc(es, sizeof(*es) * (*n+1))))
        {
          midiprint(MPFatal, "%s", strerror(errno));
          return NULL;
        }

      es[(*n)++] = e;
      if(e.msg.generic.cmd == ENDOFTRACK)
        break;
    }


  if(!es)
    return NULL;

  if(es[*n-1].msg.generic.cmd != ENDOFTRACK)
    {
      midiprint(MPError, "inserting missing End Of Track");
      if(!(es = realloc(es, sizeof(*es) * (*n+1))))
        {
          midiprint(MPFatal, "%s", strerror(errno));
          return NULL;
        }

      es[*n].time = 0;
      es[(*n)++].msg.generic.cmd = ENDOFTRACK;
    }
  else if(mbuf_rem(b) > 0)
    midiprint(MPError, "ignoring events after End Of Track");

  return es;
}


/*
 * Read the score header (if existing) and the first track header.
 * The header data is filled into the score structure and the size field
 * of the track header is returned.
 * If the score header is missing, an error message is issued and the
 * score fmt field is set to -1.
 * If there is no track header, an error message is issued and -1 is
 * returned.
 * If there is garbage in front of the headers, error messages will be
 * issued.
 */
static long read_header(MBUF *b, Score *s)
{
  CHUNK chunk;
  long skip;
  long pos;

  s->fmt = -1;
  s->ntrk = 0;
  s->div = 0;

  pos = mbuf_pos(b);

  /* Search the first chunk. */
  if(mbuf_rem(b) > 0)
    skip = search_chunk(b, &chunk, 0);
  else
    skip = -1;

  if(skip < 0)
    return -1;

  if(skip > 0)
    midiprint(MPError, "%ld bytes skipped", skip);

  if(chunk.type == MThd)
    {
      if(chunk.hdr.mthd.xsize > 0)
        midiprint(MPWarn, "large score header (%ld extra bytes)", chunk.hdr.mthd.xsize);

      s->fmt = chunk.hdr.mthd.fmt;
      s->ntrk = chunk.hdr.mthd.ntrk;
      s->div = chunk.hdr.mthd.div;

      pos = mbuf_pos(b);

      /* We need the second chunk, too. */
      if(mbuf_rem(b) > 0)
        skip = search_chunk(b, &chunk, 0);
      else
        skip = -1;

      if(skip < 0)
        {
          midiprint(MPError, "no tracks");
          return -1;
        }

      if(skip > 0)
        midiprint(MPError, "%ld bytes skipped", skip);
    }

  /* At this point, we should have got a track header. */
  if(chunk.type != MTrk)
    {
      midiprint(MPError, "no tracks");
      /* Restore the position. */
      mbuf_set(b, pos);
      return -1;
    }

  return chunk.hdr.mtrk.size;
}


/*
 * Read the next track header and return it's size.
 * If there are no more chunks, or the next chunk is not a track
 * header, the old buffer position is restored and -1 is returned.
 */
static long read_track(MBUF *b)
{
  CHUNK chunk;
  long skip;
  long pos;

  pos = mbuf_pos(b);

  if(mbuf_rem(b) > 0)
    skip = search_chunk(b, &chunk, 0);
  else
    skip = -1;

  if(skip < 0)
    return -1;

  if(skip > 0)
    midiprint(MPError, "%ld bytes skipped", skip);

  if(chunk.type != MTrk)
    {
      mbuf_set(b, pos);
      return -1;
    }

  if(skip > 0)
    midiprint(MPError, "%ld bytes skipped", skip);

  return chunk.hdr.mtrk.size;
}



/*
 * Read the next score from a buffer (there may be multiple scores
 * within one buffer).
 * If the score header is missing, default values are assumed.
 * Returns 1 on success and 0 on error.
 */
int read_score(MBUF *b, Score *s)
{
  long size;
  long n = 0;
  int ntrk = 0;

  s->events = NULL;
  s->tracks = NULL;

  if((size = read_header(b, s)) < 0)
    return 0;

  while(size >= 0)
    {
      MBUF t;

      t.b = b->b + b->i;
      t.i = 0;
      t.n = size;

      /* May be that this should be an error. */
      if(!size)
        midiprint(MPWarn, "empty track");

      if(!(s->events = read_events(&t, s->events, &n)))
        {
          if(s->tracks) free(s->tracks);
          return 0;
        }

      b->i += t.i;

      ntrk++;

      /* This may leave dead allocations in memory! */
      if(!(s->tracks = realloc(s->tracks, sizeof(*s->tracks) * ntrk)))
        {
          midiprint(MPFatal, "%s", strerror(errno));
          return 0;
        }

      /*
       * We first only store the event indices rather than counts.
       */
      s->tracks[ntrk-1] = n;

      size = read_track(b);
    }

  /*
   * If there was no score header, fill in defaults. Otherwise check the
   * number of tracks and correct it if necessary.
   */
  if(s->fmt < 0)
    {
      s->fmt = 0;
      s->ntrk = ntrk;
      s->div = 240;
    }
  else if(s->ntrk > ntrk)
    {
      midiprint(MPError, "%ld tracks missing", s->ntrk - ntrk);
      s->ntrk = ntrk;
    }
  else if(s->ntrk < ntrk)
    {
      midiprint(MPError, "%ld extraneous tracks", ntrk - s->ntrk);
      s->ntrk = ntrk;
    }

  if(!s->ntrk)
    midiprint(MPWarn, "empty score");

  /*
   * Change the event indices into event counts.
   */
  for(ntrk = s->ntrk - 1; ntrk > 0; ntrk--)
    s->tracks[ntrk] -= s->tracks[ntrk-1];

  return 1;
}


/*
 * Free all allocated data.
 */
void clear_score(Score *s)
{
  MFEvent *es;
  long n, t;

  for(es = s->events, t = 0; t < s->ntrk; t++)
    for(n = 0; n < s->tracks[t]; n++)
      clear_message(&(es++)->msg);

  free(s->events);
  free(s->tracks);
}
