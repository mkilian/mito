/*
 * $Id: score.h,v 1.1 1996/04/01 19:10:57 kilian Exp $
 *
 * Reading and writing of complete scores.
 *
 * $Log: score.h,v $
 * Revision 1.1  1996/04/01 19:10:57  kilian
 * Initial revision
 *
 */

#ifndef __SCORE_H__
#define __SCORE_H__

#include "buffer.h"
#include "event.h"


/*
 * This contains the score header data and the tracks.
 */
typedef struct {
  int fmt;
  int ntrk;
  int div;
  MFEvent **tracks;
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


#endif /* __SCORE_H__ */
