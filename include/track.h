/*
 * $Id: track.h,v 1.3 1996/04/07 22:17:24 kilian Exp $
 *
 * Managing tracks, i.e. sequences of events.
 *
 * $Log: track.h,v $
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
 * Track structure:
 * ================
 * Although a track appears as a flat sequential structure to the user,
 * a tree-like structure is used for fast insertions of events in the
 * middle of the track. This is done as following:
 *
 * When appending an event to the end of a track, the track will usually
 * remain flat, thus the track will be realloc'ed and the event will be
 * stored in the last location.
 *
 * When inserting an event `e' in the middle of a track, a new subtrack
 * is created. The first element of this subtrack will be `e', the
 * second element will be the event directly after the location at which
 * `e_n' normally would have been inserted, say `e_next'. `e_next' (in
 * the parent track) will then be overwritten by a link-event with the
 * time of `e'.
 *
 * Each subtrack contains the number of direct events (including
 * links), a pointer to it's parent track and the index to it's own link
 * event within the parent track. The root track's parent pointer is
 * NULL.
 *
 * If the event directly before the location of a newly inserted
 * event is a link, the new event will be inserted in the subtrack
 * this link points to. This ensures that tracks and subtracks
 * will never overlap, which simplifies traversing and flattening of
 * tracks.
 *
 * Deletion of events:
 * ===================
 * When deleting an event, it is replaced by an empty-event. This avoids
 * reallocing and/or restructuring the whole track on each event
 * deletion. Empty events are removed when flattening the track.
 *
 * Automatic cleanup:
 * ==================
 * Each (sub-) track structure contains the number of all descendants,
 * i.e. the sum of it's own link-events and the link-events of all of
 * it's children. Similar, the number of empty events for the track and
 * all of it's children and the number of all events at all are stored.
 * Whenever the maximum number of descendants resp. empty events is
 * exceeded, the while track (starting from the root) will be flattened.
 * Setting the maximum number of descendants or empty events to zero
 * will disable the respective limit check. Otherwise, the maxima are
 * interpreted as 1/10000th of the total number of events within the
 * track.
 *
 * Traversing a track:
 * ===================
 * Stepping event-wise through a track is done using the `current' field
 * of the track structure. This is an index into a (sub-) track's event
 * list. If it points to a link-event, the `current' of this subtrack
 * has to be used, and so on.
 *
 * Searching of specific time points:
 * ==================================
 * A simple binary search, probably entering subtracks.
 */


/*
 *
 */
typedef struct _Track {
  /* Structure: */
  unsigned long nevents;  /* # of events in this (sub-) track */
  MFEvent       *events;  /* Pointer to `nevents' events. */
  struct _Track *parent;  /* Pointer to the paren track; NULL for root */
  unsigned long link;     /* Index to the link event of parent. */

  /* Statistics */
  unsigned long descs;    /* Total # of descendants. */
  unsigned long empty;    /* Total # of empty events. */
  unsigned long total;    /* Total # of events. */

  /* Auto-Cleanup. */
  unsigned long maxdescs; /* Maximum amount of descendants. */
  unsigned long maxempty; /* Maximum amount of empty events. */

  /* Travering the track. */
  unsigned long current;  /* Current event in this (sub-) track. */
} Track;


/*
 * Debugging control.
 * This defaults to 0.
 */
extern int track_debug;

#define TRACK_CHECK 0x01  /* Full consistency checks */
#define TRACK_ACV   0x02  /* Verbose auto-cleanup */


/*
 * Build a new track.
 * `maxdesc' is the maximum number of descendants before auto-cleanup.
 * `maxempty' is the maximum number of deleted events before auto-cleanup.
 * Setting one of these parameters to zero disables this feature.
 * The values are interpreted as 1/10000 of the total number of events,
 * i.e. to cleanup if 10 percent of all events are empty, set `maxempty'
 * to 1000.
 * The function returns a pointer to the new (empty) track or NULL on
 * errors.
 */
Track *track_new(unsigned long maxdesc, unsigned long maxempty);


/*
 * This check for EOT (end of tape), which is the position directly
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
 * Compress a track, i.e. flatten all links and remove all empty events.
 * If the compression fails for some reason, this function returns 0,
 * else 1. Regardless of the return value, the track can be expected to
 * be consistent in all cases.
 */
int track_compress(Track *t);


/*
 * Delete the event at the current position and increase the position.
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
 * The position will be set to point to `e'.
 * This function returns 1 on succes, else 0.
 */
int track_insert(Track *t, MFEvent *e);

#endif /*  __TRACK_H__ */
