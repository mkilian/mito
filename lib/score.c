/*
 * $Id: score.c,v 1.5 1996/04/07 16:45:15 kilian Exp $
 *
 * Reading and writing of complete scores.
 *
 * $Log: score.c,v $
 * Revision 1.5  1996/04/07 16:45:15  kilian
 * Added maxdescs and maxempty parameters to score_new and score_read.
 *
 * Revision 1.4  1996/04/06  23:02:42  kilian
 * Changes due to the new track structure.
 *
 * Revision 1.3  1996/04/02  23:25:23  kilian
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
 * Create a new score.
 * If `maxdesc' is nonzero, the track will be compressed whenever the
 * number of link-events reaches `maxdescs / 10000' times the number of
 * total events within the track. The same holds for `maxempty', which
 * specifies the maximum relative number of empty (deleted) events
 * before auto-compression. A reasonable value for both parameters may
 * be 1000, i.e. 10% of total events.
 */
Score *score_new(unsigned long maxdescs, unsigned long maxempty)
{
  Score *s;

  if(!(s = malloc(sizeof *s)))
    return NULL;

  s->fmt = 0;
  s->ntrk = 0;
  s->div = 120;
  s->tracks = NULL;
  s->maxdescs = maxdescs;
  s->maxempty = maxempty;

  return s;
}


/*
 * Add an empty track to a score.
 * Returns 1 on success, else 0.
 */
int score_add(Score *s)
{
  Track **nt;

  if(!(nt = realloc(s->tracks, (s->ntrk + 1) * sizeof(*nt))))
    return 0;

  s->tracks = nt;
  if(!(s->tracks[s->ntrk] = track_new(s->maxdescs, s->maxempty)))
    return 0;

  s->ntrk++;
  return 1;
}




/*
 * Read an event list from the buffer into the track `t'.
 */
static int read_events(MBUF *b, Track *t)
{
  long time = 0;
  char running = 0;
  MFEvent e;

  e.time = 0;
  e.msg.empty.type = EMPTY;

  while(mbuf_rem(b) > 0 && read_event(b, &e, &running) &&
        e.msg.endoftrack.type != ENDOFTRACK)
    {
      time += e.time;
      e.time = time;

      if(!track_insert(t, &e))
        return 0;
    }

  if(e.msg.endoftrack.type != ENDOFTRACK)
    {
      midiprint(MPWarn, "inserting missing `End Of Track'");
      e.time = time;
      e.msg.endoftrack.type = ENDOFTRACK;
    }
  else
    e.time += time;

  if(!track_insert(t, &e))
    return 0;

  if(mbuf_rem(b) > 0)
    midiprint(MPWarn, "ignoring events after `End Of Track'");

  return 1;
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
 * The meaning of `maxdescs' and `maxempty' is the same as in `score_new'.
 * If the score header is missing, default values are assumed.
 */
Score *score_read(MBUF *b, unsigned long maxdescs, unsigned long maxempty)
{
  long size;
  int ntrk = 0;
  Score *s;

  if(!(s = score_new(maxdescs, maxempty)))
    return NULL;

  if((size = read_header(b, s)) < 0)
    {
      score_clear(s);
      return NULL;
    }

  ntrk = s->ntrk;
  s->ntrk = 0;

  while(size >= 0)
    {
      MBUF t;

      t.b = b->b + b->i;
      t.i = 0;
      t.n = size;

      /* May be that this should be an error. */
      if(!size)
        midiprint(MPWarn, "empty track");

      if(!score_add(s))
        {
          score_clear(s);
          return NULL;
        }

      if(!(read_events(&t, s->tracks[s->ntrk - 1])))
        {
          score_clear(s);
          return NULL;
        }

      b->i += t.i;

      size = read_track(b);
    }

  /*
   * Check the number of tracks.
   */
  if(s->ntrk < ntrk)
    midiprint(MPError, "%ld tracks missing", ntrk - s->ntrk);
  else if(s->ntrk > ntrk)
    midiprint(MPError, "%ld extraneous tracks", s->ntrk - ntrk);

  if(!s->ntrk)
    midiprint(MPWarn, "empty score");

  return s;
}


/*
 * Free all allocated data.
 */
void score_clear(Score *s)
{
  long t;

  for(t = 0; t < s->ntrk; t++)
    track_clear(s->tracks[t]);

  if(s->tracks)
    free(s->tracks);

  free(s);
}
