/*
 * $Id: track.c,v 1.1 1996/04/06 23:01:42 kilian Exp $
 *
 * Managing tracks, i.e. sequences of events.
 *
 * $Log: track.c,v $
 * Revision 1.1  1996/04/06 23:01:42  kilian
 * Initial revision
 *
 */

#include <stdlib.h>
#include <assert.h>

#include "track.h"


/*
 * Build a new track.
 * `maxdesc' is the maximum number of descendants before auto-cleanup.
 * `maxempty' is the maximum number of deleted events before auto-cleanup.
 * Setting one of these parameters to zero disables this feature.
 * The function returns a pointer to the new (empty) track or NULL on
 * errors.
 */
Track *track_new(unsigned long maxdesc, unsigned long maxempty)
{
  Track *t;

  if(!(t = malloc(sizeof(*t))))
    return NULL;

  t->nevents = 0;
  t->events = NULL;
  t->parent = NULL;
  t->link = 0;
  t->descs = 0;
  t->empty = 0;
  t->total = 0;
  t->maxdesc = maxdesc;
  t->maxempty = maxempty;
  t->current = 0;
  return t;
}


/*
 * This check for EOT (end of tape), which is the position directly
 * after the last event as well as the position directly before the
 * first event. Thus, EOT is like a special mark in a circular
 * structure.
 * Passing a NULL pointer allways returns true.
 */
static int _track_eot(Track *t)
{
  /* EOT is at t->nevents. */
  return !t || t->current >= t->nevents;
}

int track_eot(Track *t)
{
  return _track_eot(t);
}


/*
 * Increase or, if `rew' is true, decrease
 * the position of the track, ignoring subtracks or empty events. If the
 * current position is EOT, the position will be set to the first resp.
 * last event. If the new position is EOT, return true else false.
 */
static int track_inc(Track *t, int rew)
{
  if(rew)
    {
      if(_track_eot(t))
        t->current = t->nevents - 1;
      else if(!t->current)
        t->current = t->nevents;
      else
        t->current--;
    }
  else
    {
      if(_track_eot(t))
        t->current = 0;
      else
        t->current++;
    }

  return _track_eot(t);
}


/*
 * Return the address of the subtrack of a link-event `l'. If `l' is no
 * link-event, return NULL.
 */
static Track *get_link(MFEvent *l)
{
  return (l->msg.link.type == LINK) ? l->msg.link.subtrack : NULL;
}


/*
 * Rewind the track position. If `t' is NULL, do nothing.
 */
void track_rewind(Track *t)
{
  if(t)
    t->current = t->nevents;
}


/*
 * Set the position to the next or, if `rew' is true, the previous
 * non-empty event.
 * If there are no more non-empty events, the position will be set to
 * EOT. If `t' is NULL, nothing is done.
 * Returns the address of the event at the new position, or NULL.
 */
static MFEvent *track_notempty(Track *t, int rew)
{
  if(!t)
    return NULL;

  while(!track_inc(t, rew) &&
        t->events[t->current].msg.empty.type == EMPTY)
    /* skip */;

  return _track_eot(t) ? NULL : &t->events[t->current];
}


/*
 * Step to the next or, if `rew' is true, the previous  event and update
 * the position.
 * Returns the address of the event or NULL, if EOT is reached. Note
 * that `EOT' means `end of track' in both directions.
 * If `t' is NULL, NULL is returned.
 */
MFEvent *track_step(Track *t, int rew)
{
  Track *s;
  MFEvent *e;

  if(_track_eot(t))
    {
      /* Follow links, rewinding all of them. */
      while((s = get_link((e = track_notempty(t, rew)))))
        track_rewind((t = s));

      return e;
    }

  /* Goto the current position by following all links. */
  while((s = get_link(&t->events[t->current])))
    {
      t = s;
      assert(!_track_eot(t));  /* Should never happen. */
    }

  /*
   * Go to the next positition. As long as this is EOT, and we are not
   * at the root track, proceed within the parent track.
   */
  while(t && !(e = track_notempty(t, rew)))
    t = t->parent;

  if(!t)
    /* We have reached EOT in the root track. */
    return NULL;

  assert(e != NULL);  /* This MUST have been set above */

  /*
   * We now have to follow links, rewinding each visited child. Finally,
   * we must arrive at a nonempty leaf, since subtracks with only empty
   * events are not allowed.
   */
  while((t = get_link(e)))
    {
      track_rewind(t);
      e = track_notempty(t, rew);
      assert(e != NULL);
    }

  return e;
}


/*
 * This sets t->current before the first element with a time equal to or
 * greater than the given time.
 */
static void track_set_time(Track *t, long time)
{
  long size;

  t->current = 0;
  size = t->nevents / 2;
  while(size > 0)
    if(t->events[t->current + size].time < time)
      {
        t->current += size;
        size = (t->nevents - t->current) / 2;
      }
    else
      size /= 2;
}


/*
 * Search the first event with a time field equal to or greater than
 * `time'. Returns the found event, or NULL, if EOT is reached.
 * In both cases, the position will be updated, i.e. will be either the
 * position of the found event or EOT.
 */
MFEvent *track_find(Track *t, long time)
{
  Track *s;
  MFEvent *e;

  if(!t || !t->nevents)
    return NULL;

  /* Tracks must not contain empty events only. */
  assert(t->total != t->empty);

  track_set_time(t, time);

  if((s = get_link(&t->events[t->current])) && (e = track_find(s, time)))
    return e;
  else if(s)
    {
      track_notempty(t, 0);
      return _track_eot(t) ? NULL : &t->events[t->current];
    }
  else
    return track_step(t, 0);
}


/*
 * Completely delete a track.
 */
void track_clear(Track *t)
{
  if(!t) return;

  t->current = 0;
  while(t->current < t->nevents)
    {
      track_clear(get_link(&t->events[t->current]));
      clear_message(&t->events[t->current++].msg);
    }

  if(t->events) free(t->events);
  free(t);
}


/*
 * Compress a track, i.e. flatten all links and remove all empty events.
 */
void track_compress(Track *t)
{
  /* Not yet implemented */
}


/*
 * If `t' only contains empty events, it will be completely deleted. If
 * it contains exactly one non-empty events, this event will be moved to
 * the parent's link to `t', and `t' will also be deleted. In both
 * cases, the parent's fields will be updated accordingly, and the whole
 * procedure will be applied to the parent recursively. For the
 * applications's sake, if the root track is reached and and completely
 * empty, only it's `events' field will be free'd, the track structure
 * itself will stay intact.
 * The function returns 1 if something has been changed, else 0.
 */
static int track_release(Track *t)
{
  Track *p;
  unsigned long rem; /* Remaining events */

  if(!t) return 0;

  rem = t->total - t->empty;
  p = t->parent;

  if(rem > 1) return 0;       /* More than one event. */

  if(rem && p)
    {
      /* Search the non-empty event. */
      track_rewind(t);
      track_notempty(t, 0);
      assert(!_track_eot(t));

      /* Replace the link event in the parent by the current event. */
      p->events[t->link] = t->events[t->current];

      /* Update the parent's field and free this track. */
      p->total -= t->total - 1;
      p->empty -= t->empty;
      p->descs--;
      free(t->events);

      track_release(p);
      return 1;
    }
  else if(!rem && p)
    {
      /* Replace the link and set the parent's current position to the
       * next non-empty event. */
      p->events[t->link].msg.empty.type = EMPTY;
      track_notempty(p, 0);

      /* Update the parent's fields. */
      p->total -= t->total;
      p->empty -= t->empty;
      p->descs--;
      p->empty++;

      /* Try to release the parent. */
      track_release(p);

      /* And free this structure. */
      if(t->events) free(t->events);
      free(t);

      return 1;
    }
  else if(!rem)
    {
      /* We are the root track */
      free(t->events);
      t->total = t->empty = t->descs = 0;

      return 1;
    }
  else
    return 0;
}


/*
 * Delete the event at the current position and increase the position.
 * If the current position is EOT, or the track is empty at all, return
 * 0, else 1. In other words, this function returns the number of
 * deleted events.
 */
/* Internal:
 * If `t' is a subtrack and after deletion only contains empty events,
 * it is completely deleted. The link event within the parent will be
 * updated as well as the `total', `empty' and `descs' fields of all
 * anchestors. If this results in one or more of the anchestors to
 * become also totally empty, these will be deleted recursively.
 * Similarly, if `t' is a subtrack and after deletion contains exactly
 * one event, the link event within the parent will be replaced by this
 * one event, and so on.
 * Thus, deletion of an event first goes the tree down to the leafs, and
 * then probably back up to the root.
 * In the extreme cases, if after deletion the whole tree is empty, the
 * root's `events' field will be freed, but not the root structure
 * itself.
 */
int track_delete(Track *t)
{
  if(!t || _track_eot(t))
    return 0;

  /* The current position NEVER points to an empty event. */
  assert(t->events[t->current].msg.empty.type != EMPTY);

  /* First try to delete the event of a possible subtrack. */
  if(track_delete(get_link(&t->events[t->current])))
    return 1;

  /* Ok, no subtrack, so directly delete it. */
  clear_message(&t->events[t->current].msg);
  t->events[t->current].msg.empty.type = EMPTY;

  t->empty++;

  if(track_release(t))
    return 1;
  else if(t->maxempty && t->empty > t->maxempty)
    /* Empty event limit reached. */
    track_compress(t);

  return 1;
}


/*
 * Enlarge a track's event list by `n' more events. The `total' fields
 * will be updated. Note that the elements will not be initialized.
 * The current position will point to the first new event.
 * Returns the address of the first new event.
 */
static MFEvent *track_enlarge(Track *t, long n)
{
  MFEvent *e, *es;

  assert(t != NULL);

  if(!(es = realloc(t->events, (t->nevents + n) * sizeof(*es))))
    return NULL;

  t->current = t->nevents;
  t->events = es;
  t->nevents += n;
  t->total += n;

  e = &t->events[t->current];

  while((t = t->parent))
    t->total += n;

  return e;
}


/*
 * Add a new event after the current position of the track and return
 * the new event's address.
 * If the current position is EOT, the track will just be enlarged.
 * If the current position is a link, it will be followed.
 * If the position after the current position is EOT, the track will
 * again be enlarged.
 * If the event after the current position is empty, the empty fields of
 * the track and all of it's anchestors will be decreased and the
 * address of the empty event will be returned.
 * Otherwise, a new subtrack is built at the current position containing
 * the original event as it's first and the (uninitialized) new event as
 * it' second event. Fields of the track and it's anchestros are
 * updated, too.
 * The current position will point to the new event.
 * If something goes wrong (out of memory), return NULL.
 */
static MFEvent *track_newevent(Track *t)
{
  MFEvent *e, *eold;
  Track *s;

  assert(t != NULL);

  /* Follow links. */
  while(!_track_eot(t) && (s = get_link(&t->events[t->current])))
    t = s;

  if(_track_eot(t))
    return track_enlarge(t, 1);

  /* Ok, now increase current position. */
  if(track_inc(t, 0))
    return track_enlarge(t, 1);

  if(t->events[t->current].msg.empty.type == EMPTY)
    {
      /* Reuse empty events. */
      e = &t->events[t->current];

      t->empty--;
      while((t = t->parent))
        t->empty--;

      return e;
    }

  /* And back again. */
  track_inc(t, 1);

  /* Build a new subtrack... */
  if(!(s = track_new(t->maxdesc, t->maxempty)))
    return NULL;

  /*
   * ... which holds two events. Note that the parent field of `s' is
   * yet unset, so the total fields must be updated later.
   */
  if(!(eold = track_enlarge(s, 2)))
    {
      track_clear(s);
      return NULL;
    }

  track_inc(s, 0);
  e = &s->events[s->current];

  /* Move the old event into the subtrack... */
  *eold = t->events[t->current];

  /* ... and create the link structure. */
  s->parent = t;
  s->link = t->current;
  t->events[t->current].msg.link.type = LINK;
  t->events[t->current].msg.link.subtrack = s;

  /* Finally, update all total and desc fields. */
  t->descs++;
  t->total += 2;
  while((t = t->parent))
    {
      t->descs++;
      t->total += 2;
    }

  return e;
}


/*
 * Insert the given event `e' into `t'.
 * If there are already events at the time of `e' within `t', `e' will
 * be the last event with this time. It is not possible to insert events
 * in front of a track that already contains events of time 0.
 * The position will be set to point to `e'.
 * This function returns 1 on succes, else 0.
 */
int track_insert(Track *t, MFEvent *e)
{
  MFEvent *enew, *efirst;
  int found;

  /* We just search the event *after* `e'...  */
  found = track_find(t, e->time + 1) != NULL;

  /*
   * If we have found nothing, we just get a new event at the end of
   * the track.
   */
  if(!found)
    {
      if(!(enew = track_newevent(t)))
        return 0;

      *enew = *e;
      return 1;
    }

  /*
   * Otherwise, we have to step back, i.e. before the first event with a
   * time greater than `e'.
   * If this fails, we have to insert at the very beginning of the
   * track.
   */
  if(track_step(t, 1))
    {
      if(!(enew = track_newevent(t)))
        return 0;

      *enew = *e;
      return 1;
    }

  /*
   * We do the trick by inserting after the first event and swapping the
   * two events. Since we are at EOT, we must do a step forward to get
   * the first event.
   */
  efirst = track_step(t, 0);
  assert(efirst != NULL);     /* This must hold! */
  if(!(enew = track_newevent(t)))
    return 0;

  *enew = *efirst;
  *efirst = *e;
  return 1;
}
