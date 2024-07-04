/*
 * Get header and track chunks of standard midi files.
 */

#ifndef __CHUNK_H__
#define __CHUNK_H__

#include "buffer.h"


/*
 * Supported chunk types.
 */
extern const unsigned long MThd;
extern const unsigned long MTrk;


/*
 * Structure for the chunk headers.
 */
typedef struct {
  int fmt;        /* Midi file format. */
  int ntrk;       /* No. of tracks. */
  int div;        /* Time division. */
  long xsize;     /* Extra size of header (for unusually long headers).  */
} MTHD;


typedef struct {
  unsigned long size; /* Size of track. */
} MTRK;


typedef struct {
  unsigned long type;   /* MThd or MTrk. */
  union {
    MTHD mthd;
    MTRK mtrk;
  } hdr;
} CHUNK;


/*
 * Search the given buffer of size `size' until a header or track chunk
 * is found.  The name is used in warning messages. If it is a NULL
 * pointer, no messages will be written.
 *
 * There are some simple consistency checks on header chunks:
 *   - The header must be at least 14 bytes large (including the name
 *     and the size field.
 *   - The format must be 0, 1 or 2.
 *   - The number of tracks must not be negative. However, a zero track
 *     number is accepted.
 *   - The division must not be zero.
 *
 * For track chunks, the size must not be negative.
 *
 * Only if the above conditions are met, the function returns success.
 * Corrupted headers and tracks are skipped after issuing a warning
 * message (if name is set).
 *
 * The functions returns -1 if no valid chunk is found, otherwise the
 * number of bytes skipped before the chunk (normally 0).
 */
long search_chunk(MBUF *b, CHUNK *chunk);


/*
 * Write a header chunk with the given fields.
 * Return 1 on succes, 0 on error.
 */
int write_MThd(MBUF *b, int fmt, int ntrk, int div);


/*
 * Write a track chunk with the given size.
 */
int write_MTrk(MBUF *b, long size);


#endif /* __CHUNK_H__ */
