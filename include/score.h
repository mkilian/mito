/*
 * $Id: score.h,v 1.6 1996/05/20 17:46:07 kilian Exp $
 *
 * Reading and writing of complete scores.
 *
 * $Log: score.h,v $
 * Revision 1.6  1996/05/20 17:46:07  kilian
 * Changes due to new track structure/functions.
 *
 * Revision 1.5  1996/04/07 16:45:15  kilian
 * Added maxdescs and maxempty parameters to score_new and score_read.
 *
 * Revision 1.4  1996/04/06  23:00:10  kilian
 * Changes due to the new track structure.
 *
 * Revision 1.3  1996/04/02  23:24:16  kilian
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
#include "track.h"


/*
 * This contains the score header data and the tracks.
 */
typedef struct {
  int fmt;
  int ntrk;
  int div;
  Track **tracks;
} Score;


/*
 * Create a new score.
 */
Score *score_new(void);


/*
 * Add an empty track to a score.
 * Returns 1 on success, else 0.
 */
int score_add(Score *s);


/*
 * Read the next score from a buffer (there may be multiple scores
 * within one buffer).
 * If the score header is missing, default values are assumed.
 */
Score *score_read(MBUF *b);


/*
 * Write a score into a buffer.
 */
int score_write(MBUF *b, Score *s);


/*
 * Free all allocated data.
 */
void score_clear(Score *s);


#endif /* __SCORE_H__ */
