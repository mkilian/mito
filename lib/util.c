/*
 * $Id: util.c,v 1.2 1996/05/20 17:46:07 kilian Exp $
 *
 * Utility functions for midilib.
 *
 * $Log: util.c,v $
 * Revision 1.2  1996/05/20 17:46:07  kilian
 * Changes due to new track structure/functions.
 *
 * Revision 1.1  1996/05/20 04:29:46  kilian
 * Initial revision
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include "util.h"


/*
 * From the current position, do a backwards search a NoteOn event with
 * matching channel and note. The position will be left untouched.
 * Returns the found event or NULL, if no matching event is found.
 */
static MFEvent *findNoteOn(Track *t, unsigned char channel, unsigned char note)
{
  MFEvent *e;
  TrackPos p;

  p = track_getpos(t);

  /* Search the matching event. */
  while((e = track_step(t, 1)) != NULL &&
        !(e->msg.noteon.chn == channel &&
          e->msg.noteon.note == note &&
          e->msg.noteon.velocity != 0 &&
          e->msg.noteon.duration == 0))
    ; /* SKIP */

  /* Restore the old position. */
  track_setpos(t, p);

  if(e->msg.noteon.chn != channel ||
     e->msg.noteon.note != note ||
     e->msg.noteon.velocity == 0 ||
     e->msg.noteon.duration != 0)
    return NULL;
  else
    return e;
}


/*
 * Convert NoteOn/NoteOff pairs into combined Note Events. For each NoteOff
 * event, the last corresponding NoteOn event will get the release
 * velocity and duration fields filled in and the NoteOff event will be
 * deleted. Thus, if two notes overlap, the shorter one will allways be
 * completely within the larger one for example:
 *   100 NoteOn ch=1, n=60
 *   110 NoteOn ch=1, n=60
 *   120 NoteOff ch=1, n=60
 *   130 NoteOff ch=1, n=60
 * will become
 *   100 Note ch=1, n=60, dur=30
 *   110 Note ch=1, n=60, dur=10
 *
 * This function returns the number of unmatched events (NoteOn *and*
 * NoteOff).
 */
int pairNotes(Track *t)
{
  MFEvent *e, *n;
  int non = 0, noff = 0;

  track_rewind(t);
  while((e = track_step(t, 0)) != NULL)
    switch(e->msg.generic.cmd & 0xf0)
      {
        case NOTEON:
          if(e->msg.noteon.duration != 0 || e->msg.noteon.velocity != 0)
            {
              non++;
              break;
            }
          /* NoteOn events with vel. 0 fall through the MFNoteOff case. */
        case NOTEOFF:
          if(!non)
            /* NoteOff without any NoteOn, i.e. unmatched NoteOff */
            noff++;
          else if(!(n = findNoteOn(t, e->msg.noteon.chn, e->msg.noteon.note)))
            /* Unmatched NoteOff */
            noff++;
          else
            {
              n->msg.noteon.duration = e->time - n->time;
              n->msg.noteon.release = e->msg.noteon.velocity;
              track_delete(t);
              track_step(t, 1);
              non--;
            }
          break;
        default:
          break;
      }

  return non + noff;
}


/*
 * Counterpart to `pairNotes'. For each combined Note event, the
 * corresponding NoteOff event is created and the duration and release
 * velocity fields are reset to 0.
 * This function doesn't adjust possible EOT events!
 * Returns the number of converted events.
 */
int unpairNotes(Track *t)
{
  Track *tt;
  MFEvent *e;
  long n = 0;

  /* The temporary track tt holds all NoteOff events to be inserted. */
  if(!(tt = track_new()))
    {
      perror("unpair");
      exit(EXIT_FAILURE);
    }

  track_rewind(t);

  while((e = track_step(t, 0)) != NULL)
    if((e->msg.generic.cmd & 0xf0) == NOTEON &&
       e->msg.noteon.duration != 0)
      {
        MFEvent _o, *o = &_o;
        o->time = e->time + e->msg.noteon.duration;
        o->msg.generic.cmd = NOTEOFF;
        o->msg.noteoff.chn = e->msg.noteon.chn;
        o->msg.noteoff.note = e->msg.noteon.note;
        o->msg.noteoff.velocity = e->msg.noteon.release;
        e->msg.noteon.duration = 0;
        e->msg.noteon.release = 0;
        if(!track_insert(tt, o))
          {
            perror("unpair");
            exit(EXIT_FAILURE);
          }
        n++;
      }

  track_rewind(tt);
  while((e = track_step(tt, 0)) != NULL)
    if(!track_insert(t, e))
      {
        perror("unpair");
        exit(EXIT_FAILURE);
      }

  track_clear(tt);

  return n;
}
