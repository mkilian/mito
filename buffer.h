/*
 * In-memory buffer.
 */

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stdio.h>

/*
 * Buffer structure containing the midifile.
 */
typedef struct { void *dummy; } MBUF;

/*
 * Create an mbuf.
 */
MBUF *mbuf_new(void);

/*
 * Read the file into the buffer.
 * Returns 0 on success, else -1.
 */
int read_mbuf(MBUF *b, FILE *f);

/*
 * Write the buffer to the file.
 * Returns -1 on error and 0 on success.
 */
int write_mbuf(MBUF *b, FILE *f);

/*
 * Get the current position of a buffer.
 */
unsigned long mbuf_pos(MBUF *b);

/*
 * Set the position of a buffer.
 * If the position is negative, set relative to the end of buffer.
 * Returns the new position which may be different from `pos' if `pos'
 * is out of range.
 */
unsigned long mbuf_set(MBUF *b, long pos);

/*
 * Returns nonzero if the buffer contains at least n bytes from the
 * current position to the end.
 */
int mbuf_request(MBUF *b, unsigned long n);

/*
 * Get the character at the current position of the buffer and advance
 * the position.
 * Returns the character or EOF if the end of the buffer is reached.
 */
int mbuf_get(MBUF *b);

/*
 * Put a character at the current position in the buffer and advance the
 * position. If the current position is a the end of the buffer, the
 * buffer is automatically enlarged.
 * Returns the stored character or EOF on errors.
 */
int mbuf_put(MBUF *b, int ch);

/*
 * Free the data of `b'.
 */
void mbuf_free(MBUF *b);

/*
 * Insert buffer `b2' at the current position of `b1'.
 * Returns 0 on success, else -1. In the latter case, `b1' may be
 * invalid.
 */
int mbuf_insert(MBUF *b1, MBUF *b2);

#endif /* __BUFFER_H__ */
