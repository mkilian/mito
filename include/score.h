/*
 * $Id: score.h,v 1.2 1996/04/02 10:18:12 kilian Exp $
 *
 * Reading and writing of complete scores.
 *
 * $Log: score.h,v $
 * Revision 1.2  1996/04/02 10:18:12  kilian
 * Changed score structure.
 *
 * Revision 1.1  1996/04/01  19:10:57  kilian
 * Initial revision
 *
 */

#ifndef __SCORE_H__
#define __SCORE_H__

#include "buffer.h"
#include "event.h"


/*
 * This contains the score header data and the tracks.
 * The events field points to a list containing all events of the score.
 * The nev field contains the total number of events, i.e. the size (in
 * events) of the events field.
 * The tracks field points to a list that contains the index into the
 * event list to the first event of each track.
 * Thus, to get the event `n' of the track `m' of score `s', use
 * `s.events[s.tracks[m] + n]'.
 * The fact that all events of all tracks are stored within *one* list
 * allows to merge all tracks into one by just sorting the complete
 * event list by the (absolute) time fields. Although this invalidates
 * the indices of the tracks field, this can be useful, for example, if
 * when playing the score on the midi port.
 */
typedef struct {
  int fmt;
  int ntrk;
  int div;
  long nev;
  MFEvent *events;
  long *tracks;
} Score;


/*
 * Read the next score from a buffer (there may be multiple scores
 * within one buffer).
 * If the score header is missing, default values are assumed.
 * Returns 1 on success and 0 on error.
 */
int read_score(MBUF *b, Score *s);


/*
 * Write a score into a buffer.
 */
int write_score(MBUF *b, Score *s);


/*
 * Free all allocated data.
 */
void clear_score(Score *s);


#endif /* __SCORE_H__ */
