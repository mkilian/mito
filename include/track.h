/*
 * $Id: track.h,v 1.4 1996/05/20 17:46:48 kilian Exp $
 *
 * Managing tracks, i.e. sequences of events.
 *
 * $Log: track.h,v $
 * Revision 1.4  1996/05/20 17:46:48  kilian
 * Events are no longer stored in a tree but in a flat array.
 * Insertion will now be somewhat slower and tracks will use slightly more
 * memory due to exponentially growing blocks (about 20% more).
 * On the other hand, the track functions are much less error prone than the
 * old functions using the tree-like structure.
 *
 * Revision 1.3  1996/04/07 22:17:24  kilian
 * User-controllable debugging level.
 *
 * Revision 1.2  1996/04/07  16:46:20  kilian
 * Changed return value of track_compress to int.
 *
 * Revision 1.1  1996/04/06  23:00:10  kilian
 * Initial revision
 *
 */

#ifndef __TRACK_H__
#define __TRACK_H__

#include "event.h"



/*
 * The track structure itself.
 */
typedef struct _Track {
  MFEvent       *events;    /* List of events. */
  unsigned long nevents;    /* # of total events. */
  unsigned long current;    /* Index to event in this track. */
  unsigned long nempty;     /* # of deleted events. */
  char          inserting;  /* Indicates insertion mode. */
} Track;


/*
 * Track positions:
 */
typedef unsigned long TrackPos;


/*
 * Build a new track.
 * The function returns a pointer to the new (empty) track or NULL on
 * errors.
 */
Track *track_new(void);


/*
 * Get the number of events in the track.
 */
unsigned long track_nevents(Track *t);


/*
 * This check for EOT (end of track), which is the position directly
 * after the last event as well as the position directly before the
 * first event. Thus, EOT is like a special mark in a circular
 * structure.
 * Passing a NULL pointer allways returns true.
 */
int track_eot(Track *t);


/*
 * Rewind the track position. If `t' is NULL, do nothing.
 */
void track_rewind(Track *t);


/*
 * Retrieve the current position.
 * A retrieved position becomes invalid after an event has been deleted
 * or inserted.
 */
TrackPos track_getpos(Track *t);


/*
 * Restore a position retrieved with `track_getpos'.
 */
void track_setpos(Track *t, TrackPos p);


/*
 * Step to the next or, if `rew' is true, the previous  event and update
 * the position.
 * Returns the address of the event or NULL, if EOT is reached. Note
 * that `EOT' means `end of track' in both directions.
 * If `t' is NULL, NULL is returned.
 */
MFEvent *track_step(Track *t, int rew);


/*
 * Search the first event with a time field equal to or greater than
 * `time'. Returns the found event, or NULL, if EOT is reached.
 * In both cases, the position will be updated, i.e. will be either the
 * position of the found event or EOT.
 */
MFEvent *track_find(Track *t, long time);


/*
 * Completely delete a track.
 */
void track_clear(Track *t);


/*
 * Delete the event at the current position and increase the position,
 * i.e. set the position to the next element.
 * If the current position is EOT, or the track is empty at all, return
 * 0, else 1. In other words, this function returns the number of
 * deleted events.
 */
int track_delete(Track *t);


/*
 * Insert the given event `e' into `t'.
 * If there are already events at the time of `e' within `t', `e' will
 * be the last event with this time. It is not possible to insert events
 * in front of a track that already contains events of time 0.
 * The position will be undefined.
 * This function returns 1 on success, else 0.
 */
int track_insert(Track *t, MFEvent *e);

#endif /*  __TRACK_H__ */
