/* Managing tracks, i.e. sequences of events. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "track.h"

/*
 * Memory statistics:
 * Greatest used track size and greatest allocated track size (in
 * elements, i.e. event structures), the difference giving the wasted
 * space in the worst track.
 */
unsigned long maxused = 0;
unsigned long maxallocated = 0;

/*
 * Build a new track.
 * The function returns a pointer to the new (empty) track or NULL on
 * errors.
 */
Track *track_new(void) {
	Track *t;

	if (!(t = malloc(sizeof(*t))))
		return NULL;

	t->events = NULL;
	t->current = t->nempty = t->nevents = 0;
	t->inserting = 0;
	return t;
}

/*
 * Enlarge a track by one entry.
 * Since realloc() may be expensive, we use exponentially growing
 * blocksizes starting at 1K, i.e. 1024, 2048, 4096, ...
 * The nevents field is updated and the address of the new (last) event
 * is returned. If an error occurs (out of memory), a NULL pointer is
 * returned.
 */
static MFEvent *enlarge(Track *t) {
	unsigned long n;
	MFEvent *new;

	n = t->nevents ? t->nevents : 512;

	/* This tricky expression tests wether `n' is a power of two. */
	if ((n ^ (n-1)) == 2 * n - 1) {
		n *= 2;
		if (!(new = realloc(t->events, n * sizeof(*new))))
			return NULL;

		if (maxallocated < n)
			maxallocated = n;

		t->events = new;
	}

	t->nevents++;
	if (maxused < t->nevents)
		maxused = t->nevents;

	return &(t->events[t->nevents-1]);
}


/*
 * Shrink the track by `n' events. This means that the last `n' events
 * of the track become invalid.
 */
static void shrink(Track *t, unsigned long n) {
	unsigned long x = 1024;

	/* Get the new blocksize. */
	while (x < t->nevents - n)
		x += x;

	/* Only realloc if we did skip a block boundary. */
	if (t->nevents >= 2 * x)
		t->events = realloc(t->events, x * sizeof(*(t->events)));

	t->nevents -= n;
	if (t->current > t->nevents)
		t->current = t->nevents;
}

/*
 * Pack a track, i.e. fill in all gaps (empty events) by shifting events
 * down.
 */
static void pack(Track *t) {
	unsigned long from, to;

	from = to = 0;
	while (from < t->nevents)
		if (t->events[from].msg.generic.cmd == EMPTY)
			from++;
		else if (to < from) {
			if (t->current == from)
				t->current = to;
			t->events[to++] = t->events[from++];
		} else
			to++, from++;

	shrink(t, t->nempty);
	t->nempty = 0;
}

/* Start or continue insertion of events. */
static void start_insertion(Track *t) {
	if (!t || t->inserting)
		return;

	pack(t);
	t->inserting = 1;
}

/*
 * Comparision function for sorting of events.
 * For equal-timed events, the following partial order holds:
 *   Any event           < End of track
 *   Other meta event    < Voice event
 *   Voice event ch=x    < Voice event ch=y, if x < y
 *   Program change      < Other voice event
 *   Control change      < Other voice event
 *   Note off            < Note on
 * In all other cases, this function is order-preserving, i.e.
 * the addresses are compared.
 */

#define isVoice(e)    ((e)->msg.generic.cmd != 0xff &&\
                      ((e)->msg.generic.cmd & 0xf0))
#define isMeta(e)     (!isVoice(e))
#define isProgram(e)  (((e)->msg.generic.cmd & 0xf0) == PROGRAMCHANGE)
#define isControl(e)  (((e)->msg.generic.cmd & 0xf0) == CONTROLCHANGE)
#define isNoteOn(e)   (((e)->msg.generic.cmd & 0xf0) == NOTEON &&\
                        (e)->msg.noteon.velocity != 0)
#define isNoteOff(e)  (((e)->msg.generic.cmd & 0xf0) == NOTEOFF ||\
                       ((e)->msg.generic.cmd & 0xf0) == NOTEON &&\
                        (e)->msg.noteon.velocity == 0)

static int _ecmp(const void *_e1, const void *_e2) {
	const MFEvent *e1 = _e1;
	const MFEvent *e2 = _e2;

	if (e1->time < e2->time)
		return -1;
	else if (e1->time > e2->time)
		return 1;
	else if (e2->msg.generic.cmd == ENDOFTRACK)
		return -1;
	else if (e1->msg.generic.cmd == ENDOFTRACK)
		return 1;
	else if (isMeta(e1) && isVoice(e2))
		return -1;
	else if (isMeta(e2) && isVoice(e1))
		return 1;
	else if (isVoice(e1) && isVoice(e2) && e1->msg.noteon.chn < e2->msg.noteon.chn)
		return -1;
	else if (isVoice(e1) && isVoice(e2) && e2->msg.noteon.chn < e1->msg.noteon.chn)
		return 1;
	else if (isProgram(e1) && !isProgram(e2))
		return -1;
	else if (isProgram(e2) && !isProgram(e1))
		return 1;
	else if (isControl(e1) && !isControl(e2))
		return -1;
	else if (isControl(e2) && !isControl(e1))
		return 1;
	else if (isNoteOff(e1) && isNoteOn(e2))
		return -1;
	else if (isNoteOff(e2) && isNoteOn(e1))
		return 1;
	else if (e1 < e2)
		return -1;
	else if (e1 > e2)
		return 1;
	else
		return 0;
}

/* If in insertion mode, end insertion. */
static void stop_insertion(Track *t) {
	if (!t || !t->inserting)
		return;

	t->inserting = 0;
	qsort(t->events, t->nevents, sizeof(*(t->events)), _ecmp);
}

/*
 * This check for EOT (end of tape), which is the position directly
 * after the last event as well as the position directly before the
 * first event. Thus, EOT is like a special mark in a circular
 * structure.
 * Passing a NULL pointer allways returns true.
 */
int track_eot(Track *t) {
	return !t || t->current >= t->nevents;
}

/* Get the number of events in the track. */
unsigned long track_nevents(Track *t) {
	return t ? t->nevents - t->nempty : 0;
}

/* Rewind the track position. If `t' is NULL, do nothing. */
void track_rewind(Track *t) {
	stop_insertion(t);
	if (t)
		t->current = t->nevents;
}

/*
 * Retrieve the current position.
 * A retrieved position becomes invalid after an event has been deleted
 * or inserted.
 */
TrackPos track_getpos(Track *t) {
	stop_insertion(t);
	return t ? t->current : 0;
}

/* Restore a position retrieved with `track_getpos'. */
void track_setpos(Track *t, TrackPos p) {
	if (t)
		t->current = p;
}

/*
 * Step to the next or, if `rew' is true, the previous  event and update
 * the position.
 * Returns the address of the event or NULL, if EOT is reached. Note
 * that `EOT' means `end of track' in both directions.
 * If `t' is NULL, NULL is returned.
 */
static MFEvent *_track_step(Track *t, int rew) {
	if (!t || !t->nevents)
		return NULL;

	if (t->current >= t->nevents)
		t->current = rew ? t->nevents - 1 : 0;
	else if (rew && !t->current)
		t->current = t->nevents;
	else if (rew)
		t->current--;
	else
		t->current++;

	return t->current < t->nevents ? &(t->events[t->current]) : NULL;
}

MFEvent *track_step(Track *t, int rew) {
	MFEvent *e;

	while ((e = _track_step(t, rew)) != NULL && e->msg.generic.cmd == EMPTY)
		; /* SKIP */

	return e;
}

/*
 * Search the first event with a time field equal to or greater than
 * `time'. Returns the found event, or NULL, if EOT is reached.
 * In both cases, the position will be updated, i.e. will be either the
 * position of the found event or EOT.
 */
MFEvent *_track_find(Track *t, long time) {
	unsigned long size, pos;

	if (!t || !t->nevents)
		return NULL;

	pos = 0;
	size = t->nevents / 2;
	while (size > 0)
		if (t->events[pos + size].time < time) {
			pos += size;
			size = (t->nevents - pos) / 2;
		} else
			size /= 2;

	t->current = pos + 1;

	return &(t->events[t->current]);
}

MFEvent *track_find(Track *t, long time) {
	MFEvent *e;

	stop_insertion(t);
	e = _track_find(t, time);
	if (e && e->msg.generic.cmd == EMPTY)
		e = track_step(t, 0);

	return e;
}

/* Completely delete a track. */
void track_clear(Track *t) {
	unsigned long pos;
	if (!t)
		return;

	for (pos = 0; pos < t->nevents; pos++)
		clear_message(&(t->events[pos].msg));

	if (t->events)
		free(t->events);

	t->events = NULL;
	t->nevents = t->current = t->nempty = 0;
	t->inserting = 0;

	free(t);
}

/*
 * Delete the event at the current position and increase the position,
 * i.e. set the position to the next element.
 * If the current position is EOT, or the track is empty at all, return
 * 0, else 1. In other words, this function returns the number of
 * deleted events.
 */
int track_delete(Track *t) {
	stop_insertion(t);
	if (!t || track_eot(t))
		return 0;

	clear_message(&(t->events[t->current].msg));
	t->events[t->current].msg.generic.cmd = EMPTY;

	  if (t->current + 1 < t->nevents) {
		t->events[t->current].time = t->events[t->current + 1].time;
		t->nempty++;
		track_step(t, 0);
	} else
		t->events[t->current].time = -1;

	if (t->nevents < 2 * t->nempty)
		pack(t);

	return 1;
}

/*
 * Insert the given event `e' into `t'.
 * If there are already events at the time of `e' within `t', `e' will
 * be the last event with this time. It is not possible to insert events
 * in front of a track that already contains events of time 0.
 * The position will be undefined.
 * This function returns 1 on succes, else 0.
 */
int track_insert(Track *t, MFEvent *e) {
	MFEvent *new;

	start_insertion(t);

	if (!(new = enlarge(t)))
		return 0;

	*new = *e;
	return 1;
}
