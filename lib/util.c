/*
 * $Id: util.c,v 1.1 1996/05/20 04:29:46 kilian Exp $
 *
 * Utility functions for midilib.
 *
 * $Log: util.c,v $
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
  long n = 1;

  /* Not changing the current position is not possible, so we count the
   * steps we go backwards and step forward such many times after we
   * have found the matching event or have reached EOT. Maybe I should
   * implement something like iterators for tracks or something like a
   * position data type...
   */
  while((e = track_step(t, 1)) != NULL &&
        !(e->msg.noteon.chn == channel &&
          e->msg.noteon.note == note &&
          e->msg.noteon.velocity != 0 &&
          e->msg.noteon.duration == 0))
    n++;

  /* Restore the old position. */
  while(n--)
    track_step(t, 0);

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
    switch(e->msg.generic.cmd)
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
  MFEvent *e, _o, *o = &_o;
  long n = 0;

  /* Since insertion of events changes the current position, we do the
   * conversion backwards.
   * Another reason to introduce a position type!
   */
  track_rewind(t);

  while((e = track_step(t, 1)) != NULL)
    if(e->msg.generic.cmd == NOTEON &&
       e->msg.noteon.duration != 0)
      {
        o->time = e->time + e->msg.noteon.duration;
        o->msg.noteoff.cmd = NOTEOFF;
        o->msg.noteoff.chn = e->msg.noteon.chn;
        o->msg.noteoff.note = e->msg.noteon.note;
        o->msg.noteoff.velocity = e->msg.noteon.release;
        if(!track_insert(t, o))
          {
            perror("unpair");
            exit(EXIT_FAILURE);
          }
        e->msg.noteon.duration = 0;
        e->msg.noteon.release = 0;
        n++;
      }

  return n;
}
