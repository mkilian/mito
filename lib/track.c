/*
 * $Id: track.c,v 1.3 1996/04/07 22:30:18 kilian Exp $
 *
 * Managing tracks, i.e. sequences of events.
 *
 * $Log: track.c,v $
 * Revision 1.3  1996/04/07 22:30:18  kilian
 * Added consistency check.
 * More assertions.
 * Hopefully fixed track_release.
 * User-controllable debugging level.
 *
 * Revision 1.3  1996/04/07  19:43:47  kilian
 * Added consistency check.
 * More assertions.
 * Hopefully fixed track_release.
 *
 * Revision 1.2  1996/04/07  16:46:20  kilian
 * Implemented track_compress.
 * Changed return value of track_compress to int.
 * Some minor changes.
 *
 * Revision 1.1  1996/04/06  23:01:42  kilian
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "track.h"


/*
 * Debugging control.
 * This defaults to 0.
 */
int track_debug = 0;

#define TD track_debug


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
  t->maxdescs = maxdesc;
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
 * last event. If the new position is EOT, return false else true.
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

  return !_track_eot(t);
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

  while(track_inc(t, rew) &&
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

  if(t->events)
    {
      free(t->events);
      t->events = NULL;
      t->nevents = t->total = t->empty = t->descs = 0;
    }
  free(t);
}


/*
 * This performs a consistency check on a track.
 * If the check fails, the program is aborted.
 */
static void track_check(Track *t)
{
#ifndef NDEBUG
  Track *s;
  unsigned long c, descs, empty, total;
  long lasttime;

  if(!(TD & TRACK_CHECK))
    return;

  /* Only the root track may be totally empty. */
  assert(t->events || !t->nevents && !t->parent);
  assert(t->nevents <= t->total);
  assert(t->total >= t->descs + t->empty);

  for(c = descs = empty = total = lasttime = 0; c < t->nevents; c++, total++)
    {
      MFEvent *e = &t->events[c];
      assert(e->time >= lasttime);
      lasttime = e->time;
      if(e->msg.empty.type == EMPTY)
        empty++;
      else if((s = get_link(e)))
        {
          track_check(s);
          assert(s->parent == t);
          assert(s->link == c);
          total += s->total;
          empty += s->empty;
          descs += 1 + s->descs;
        }

      assert(total <= t->total);
      assert(empty <= t->empty);
      assert(descs <= t->descs);
    }

  assert(total == t->total);
  assert(empty == t->empty);
  assert(descs == t->descs);
#endif /* NDEBUG */
}


/*
 * Compress a track, i.e. flatten all links and remove all empty events.
 * If the compression fails for some reason, this function returns 0,
 * else 1. Regardless of the return value, the track can be expected to
 * be consistent in all cases.
 */
int track_compress(Track *t)
{
  Track *s;
  MFEvent *e, *es;
  unsigned long dest, source;
  unsigned long newsize;

  if(t)
    track_check(t);

  if(!t || !t->descs && !t->empty)
    return 1;

#ifndef NDEBUG
  if(TD & TRACK_ACV)
    fprintf(stderr, "Compress: %ld ev, %ld total, %ld descs, %ld empty...",
            t->nevents, t->total, t->descs, t->empty);
#endif /* NDEBUG */

  /*
   * First, we have to reallocate the new size, but only if the new size
   * is larger than the current. Otherwise, we do the realloc after the
   * flattening.
   * If reallocation fails, no flattening will be done.
   */
  newsize = t->total - t->descs - t->empty;
  es = NULL;
  if(newsize > t->nevents)
    if(!(es = realloc(t->events, newsize * sizeof(*es))))
      {
#ifndef NDEBUG
        if(TD & TRACK_ACV)
          {
            perror(" aborted");
            fflush(stderr);
          }
#endif /* NDEBUG */
        track_check(t);
        return 0;
      }
    else
      t->events = es;

  assert(t->events != NULL);

  /* Remove leading empty events by moving events down.  */
  source = dest = 0;
  while(dest <= source && source < t->nevents)
    if(t->events[source].msg.empty.type == EMPTY)
      source++;
    else if((s = get_link(&t->events[source])))
      {
        /* Leave links yet untouched. */
        source++;
        dest += s->total - s->descs - s->empty;
      }
    else if(source > dest)
      {
        /* We have to keep track of the current position. */
        if(t->current == source)
          t->current = dest;

        t->events[dest++] = t->events[source++];
      }
    else
      source++, dest++;

  /*
   * Traverse from the end of the event list, removing empty events by
   * moving events up and flatten all found links.
   */
  source = t->nevents - 1;
  dest = newsize - 1;
  while(source + 1 > 0 && dest + 1 > 0)
    if(t->events[source].msg.empty.type == EMPTY)
      source--;
    else if((s = get_link(&t->events[source])))
      {
        MFEvent *ec = NULL;
        Track *tc = s;

        /*
         * If the current position is at this link, we have to get the
         * corresponding event to update the current position later.
         */
        while(!_track_eot(tc) &&
              ((tc = get_link((ec = &tc->events[tc->current])))))
          /* skip */;

        if(tc && _track_eot(tc))
          ec = NULL;

        /*
         * Copy events from s to dest, starting at the end.
         * The parent's field is cleared to avoid garbling the current
         * track's fields and infinite loops.
         */
        s->parent = NULL;
        track_rewind(s);
        while((e = track_step(s, 1)))
          {
            /* Update the current position, if necessary. */
            if(e == ec)
              t->current = dest;

            t->events[dest--] = *e;
            /* Overwrite it with something uncritical. */
            e->msg.endoftrack.type = ENDOFTRACK;
          }

        /* Now delete the subtrack. */
        track_clear(s);

        /*
         * Update the source position. In rare cases where there were
         * empty events in front of the link and the subtrack contained
         * more real events than there was space between source and
         * dest, source may point into the newly copied events.
         */
        if(source > dest)
          source = dest;
        else
          source--;
      }
    else if(source < dest)
      {
        /* We have to keep track of the current position. */
        if(t->current == source)
          t->current = dest;

        t->events[dest--] = t->events[source--];
      }
    else
      source--, dest--;

  /*
   * Update counts and propagate them to the parents.
   */
  s = t;
  while((s = s->parent))
    {
      s->total -= t->empty + t->descs;
      s->empty -= t->empty;
      s->descs -= t->descs;
    }

  t->total -= t->empty + t->descs;
  t->nevents = t->total;
  t->empty = t->descs = 0;

  assert(t->nevents == newsize);

  /*
   * If we use less space than before, we now have to shrink the events
   * array to it's new size. This should never fail...
   */
  if(!es)
    t->events = realloc(t->events, newsize * sizeof(*t->events));

  /* ... but we do an assertion anyways. */
  assert(t->events != NULL);

#ifndef NDEBUG
  if(TD & TRACK_ACV)
    {
      fprintf(stderr, " done.\n");
      fflush(stderr);
    }
#endif /* NDEBUG */
  track_check(t);

  return 1;
}


/*
 * This is a simple compression wrapper.
 * It compresses the track if one of the limits is reached.
 */
static void _track_comp(Track *t)
{
  /* TODO: avoid overflow. */
  if(t->maxdescs && t->descs * 10000 > t->total * t->maxdescs ||
     t->maxempty && t->empty * 10000 > t->total * t->maxempty)
    track_compress(t);
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
      t->events = NULL;
      t->nevents = t->total = t->empty = t->descs = 0;

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
      if(t->events)
        {
          free(t->events);
          t->events = NULL;
          t->nevents = t->total = t->empty = t->descs = 0;
        }
      free(t);

      return 1;
    }
  else if(!rem)
    {
      /* We are the root track */
      free(t->events);
      t->events = NULL;
      t->nevents = t->total = t->empty = t->descs = 0;

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
static int _track_delete(Track *t)
{
  Track *s;

  if(!t || _track_eot(t))
    return 0;

  /* The current position NEVER points to an empty event. */
  assert(t->events[t->current].msg.empty.type != EMPTY);

  /* First try to delete the event of a possible subtrack. */
  if(_track_delete(get_link(&t->events[t->current])))
    return 1;

  /* Ok, no subtrack, so directly delete it. */
  clear_message(&t->events[t->current].msg);
  t->events[t->current].msg.empty.type = EMPTY;
  t->empty++;

  /* Propagate the new number of empty events to the anchestors. */
  s = t;
  while((s = s->parent))
    s->empty++;

  /* Go to the next position. */
  track_step(t, 0);

  track_release(t);

  return 1;
}

int track_delete(Track *t)
{
  if(!_track_delete(t))
    return 0;

  /* Check for auto-cleanup. */
  _track_comp(t);
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
  if(!track_inc(t, 0))
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
  if(!(s = track_new(t->maxdescs, t->maxempty)))
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
      _track_comp(t);
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
      _track_comp(t);
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
  _track_comp(t);
  return 1;
}
