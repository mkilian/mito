/* Utility functions for midilib. */

#include <stdio.h>
#include <stdlib.h>

#include "util.h"

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
int pairNotes(Track *t) {
	MFEvent *e;
	int non = 0, noff = 0;

	struct _n {
		MFEvent *e;
		struct _n *n;
	} *notes = NULL, *nn;

	track_rewind(t);
	while ((e = track_step(t, 0)) != NULL)
		switch (e->msg.cmd & 0xf0) {
		case NOTEON:
			if (e->msg.noteon.velocity != 0) {
				if (e->msg.noteon.duration == 0) {
					nn = alloca(sizeof(*nn));
					nn->n = notes;
					nn->e = e;
					notes = nn;
					non++;
				}
				break;
			}
			/* NoteOn events with vel. 0 fall through the MFNoteOff case. */
		case NOTEOFF:
			if (!non)
				/* NoteOff without any NoteOn, i.e. unmatched NoteOff */
				noff++;
			else {
				nn = notes;
				while (nn && (nn->e == NULL ||
				    CHN(nn->e->msg) != CHN(e->msg) ||
				    CHN(nn->e->msg) != CHN(e->msg)))
					nn = nn->n;

				if (!nn)
					/* Unmatched NoteOff */
					noff++;
				else {
					nn->e->msg.noteon.duration = e->time - nn->e->time;
					nn->e->msg.noteon.release = e->msg.noteon.velocity;
					nn->e = NULL;
					track_delete(t);
					track_step(t, 1);
					non--;
				}
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
int unpairNotes(Track *t) {
	Track *tt;
	MFEvent *e;
	long n = 0;

	/* The temporary track tt holds all NoteOff events to be inserted. */
	if (!(tt = track_new())) {
		perror("unpair");
		exit(EXIT_FAILURE);
	}

	track_rewind(t);

	while ((e = track_step(t, 0)) != NULL)
		if ((e->msg.cmd & 0xf0) == NOTEON &&
			e->msg.noteon.duration != 0) {
		MFEvent _o, *o = &_o;
		o->time = e->time + e->msg.noteon.duration;
		o->msg.cmd = NOTEOFF | CHN(e->msg);
		o->msg.noteoff.note = e->msg.noteon.note;
		o->msg.noteoff.velocity = e->msg.noteon.release;
		e->msg.noteon.duration = 0;
		e->msg.noteon.release = 0;
		if (!track_insert(tt, o)) {
			perror("unpair");
			exit(EXIT_FAILURE);
		}
		n++;
	}

	track_rewind(tt);
	while ((e = track_step(tt, 0)) != NULL)
		if (!track_insert(t, e)) {
			perror("unpair");
			exit(EXIT_FAILURE);
		}

	track_clear(tt);

	return n;
}

/*
 * If `force' is nonzero, unconditionally replace all NoteOff events by
 * NoteOn events with velocity zero.
 * If `force' is zero, the replacement takes place if all NoteOff events
 * have the same velocity.
 */
void compressNoteOff(Track *t, int force) {
	MFEvent *e;
	TrackPos p = track_getpos(t);

	if (!force) {
		force = 1;
		track_rewind(t);
		while ((e = track_step(t, 0)) != NULL &&
		    (e->msg.cmd & 0xf0) != NOTEOFF)
			; /* SKIP */

		if (e != NULL) {
			int vel = e->msg.noteoff.velocity;

			while (force && (e = track_step(t, 0)) != NULL)
				if ((e->msg.cmd & 0xf0) == NOTEOFF &&
				    e->msg.noteoff.velocity != vel)
					force = 0;
		}
	}

	if (force) {
		track_rewind(t);
	while ((e = track_step(t, 0)) != NULL)
		if ((e->msg.cmd & 0xf0) == NOTEOFF) {
			e->msg.cmd &= 0x0f;
			e->msg.cmd |= NOTEON;
			e->msg.noteon.velocity = 0;
			e->msg.noteon.duration = 0;
		}
	}

	track_setpos(t, p);
}
