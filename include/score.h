/* Reading and writing of complete scores. */

#ifndef __SCORE_H__
#define __SCORE_H__

#include "buffer.h"
#include "event.h"
#include "track.h"

/* This contains the score header data and the tracks. */
typedef struct {
	int fmt;
	int ntrk;
	int div;
	Track **tracks;
} Score;

/* Create a new score. */
Score *score_new(void);

/* Add an empty track to a score.
 * Returns 1 on success, else 0.
 */
int score_add(Score *s);

/* Read the next score from a buffer (there may be multiple scores
 * within one buffer).
 * If the score header is missing, default values are assumed.
 */
Score *score_read(MBUF *b);

/* Write a score into a buffer. */
int score_write(MBUF *b, Score *s);

/* Free all allocated data. */
void score_clear(Score *s);

#endif /* __SCORE_H__ */
