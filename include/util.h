/*
 * $Id: util.h,v 1.2 1996/05/20 22:10:11 kilian Exp $
 *
 * Utility functions for midilib.
 *
 * $Log: util.h,v $
 * Revision 1.2  1996/05/20 22:10:11  kilian
 * Added compressNoteOff.
 * Fixed and improved pairNotes (now uses alloca to store NoteOns).
 *
 * Revision 1.1  1996/05/20 04:29:46  kilian
 * Initial revision
 *
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include "track.h"

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
int pairNotes(Track *t);


/*
 * Counterpart to `pairNotes'. For each combined Note event, the
 * corresponding NoteOff event is created and the duration and release
 * velocity fields are reset to 0.
 * This function doesn't adjust possible EOT events!
 * Returns the number of converted events.
 */
int unpairNotes(Track *t);


/*
 * If `force' is nonzero, unconditionally replace all NoteOff events by
 * NoteOn events with velocity zero.
 * If `force' is zero, the replacement takes place if all NoteOff events
 * have the same velocity.
 */
void compressNoteOff(Track *t, int force);

#endif /* __UTIL_H__ */
