/*
 * $Id: score.c,v 1.1 1996/04/01 19:11:06 kilian Exp $
 *
 * Reading and writing of complete scores.
 *
 * $Log: score.c,v $
 * Revision 1.1  1996/04/01 19:11:06  kilian
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
 * Read an event list from the buffer.
 * Returns a pointer to the events, which are terminated by an `End Of
 * Track' event. If an error occurs (e.g. out of memory), a NULL pointer
 * is returned.
 * If an `End Of Track' event comes before the end of the buffer, an
 * error message is issued and the remainder of the buffer is ignored.
 * If very last event of the buffer is no `End Of Track' event, an error
 * message is issued and an `End Of Track' event is automatically
 * appended to the list.
 */
static MFEvent *read_events(MBUF *b)
{
  char running = 0;
  long i = 0;
  MFEvent e;
  MFEvent *es = NULL;

  while(mbuf_rem(b) > 0 && read_event(b, &e, &running))
    {
      if(!(es = realloc(es, sizeof(*es) * (i+1))))
        {
          midierror("%s", strerror(errno));
          return NULL;
        }

      es[i++] = e;
      if(e.msg.generic.cmd == ENDOFTRACK)
        break;
    }

  if(es[i-1].msg.generic.cmd != ENDOFTRACK)
    {
      midierror("inserting missing End Of Track");
      if(!(es = realloc(es, sizeof(*es) * (i+1))))
        {
          midierror("%s", strerror(errno));
          return NULL;
        }

      es[i].time = 0;
      es[i++].msg.generic.cmd = ENDOFTRACK;
    }
  else if(mbuf_rem(b) > 0)
    midierror("ignoring events after End Of Track");

  return es;
}


/*
 * Read the score header (if existing) and the first track header.
 * The header data is filled into the score structure and the length
 * from the track header is returned.
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
    midierror("%ld bytes skipped", skip);

  if(chunk.type == MThd)
    {
      if(chunk.hdr.mthd.xsize > 0)
        midierror("large score header (%ld extra bytes)", chunk.hdr.mthd.xsize);

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
          midierror("no tracks");
          return -1;
        }

      if(skip > 0)
        midierror("%ld bytes skipped", skip);
    }

  /* At this point, we should have got a track header. */
  if(chunk.type != MTrk)
    {
      midierror("no tracks");
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

  if(chunk.type != MTrk)
    {
      mbuf_set(b, pos);
      return -1;
    }

  if(skip > 0)
    midierror("%ld bytes skipped", skip);

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
  MBUF t;
  MFEvent *es;
  long size;
  int ntrk = 0;

  if((size = read_header(b, s)) < 0)
    return 0;

  while(size >= 0)
    {
      if(!size)
        midiwarn("empty track");

      t.b = b->b + b->i;
      t.i = 0;
      t.n = size;

      if(!(es = read_events(&t)))
        {
          while(ntrk-- > 0)
            free(s->tracks[ntrk]);

          free(s->tracks);
          return -1;
        }

      ntrk++;
      /* This may leave dead allocations in memory! */
      if(!(s->tracks = realloc(s->tracks, sizeof(*s->tracks) * ntrk)))
        {
          midierror("%s", strerror(errno));
          return -1;
        }

      s->tracks[ntrk-1] = es;

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
      midierror("%ld tracks missing", s->ntrk - ntrk);
      s->ntrk = ntrk;
    }
  else if(s->ntrk < ntrk)
    {
      midierror("%ld extraneous tracks", ntrk - s->ntrk);
      s->ntrk = ntrk;
    }

  if(!s->ntrk)
    midiwarn("empty score");

  return 0;
}
