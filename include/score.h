/*
 * $Id: score.h,v 1.3 1996/04/02 23:24:16 kilian Exp $
 *
 * Reading and writing of complete scores.
 *
 * $Log: score.h,v $
 * Revision 1.3  1996/04/02 23:24:16  kilian
 * Field `nev' removed. The tracks field now contains the number of events
 * in each track.
 *
 * Revision 1.2  1996/04/02  10:18:12  kilian
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
 * The tracks field points to a list that contains the number of events
 * within each track. Thus, to get the fifth event of the third track of
 * a score `s', use `s.events[s.tracks[0] + s.tracks[1] + 4]'.
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
