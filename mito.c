/*
 * $Id: mito.c,v 1.3 1996/04/02 23:29:19 kilian Exp $
 *
 * mito --- the midi tool
 *
 * $Log: mito.c,v $
 * Revision 1.3  1996/04/02 23:29:19  kilian
 * Lots of changes.
 * Implemented score and track range selection.
 * Implemented track merging.
 * Prepared writing of midi files.
 *
 * Revision 1.2  1996/04/02  10:16:36  kilian
 * Lots of changes...
 *
 * Revision 1.1  1996/04/01  19:10:51  kilian
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "midi.h"


static void usage(void)
{
  fputs("usage: mito [-hleqnm012] [-o file] [-d div] {[file][@sl]}...\n"
        "  overall options:\n"
        "    -h:  show score headers\n"
        "    -l:  show track lengths\n"
        "    -e:  show events\n"
        "    -q:  be quiet; accumulative:\n"
        "           1x = no warnings\n"
        "           2x = no recoverable midi file errors\n"
        "           3x = no errors at all\n"
        "    -o:  write resulting output to `file'\n"
        "  input options\n"
        "    -m: merge tracks into after reading\n"
        "    sl = [scores][.tracks]; read selection\n"
        "         scores: range of scores to read; empty = all\n"
        "         tracks: range of tracks to read; empty = all\n"
        "  output options (only valid if `-o' is given)\n"
        "    -[012]:  use this output format (default from first score)\n"
        "    -d:  use output division `div' (default from first score)\n"
        "    -n:  no header; only write the tracks\n", stderr);
  exit(EXIT_FAILURE);
}


/*
 * Command line flags, bitwise coded.
 */
#define SHOWHEADERS 0x01
#define SHOWTLENGTHS 0x02
#define SHOWEVENTS 0x04
#define NOHEADER 0x08
#define MERGE 0x10


/*
 * The filename to print in warning and error messages.
 */
static char *warnname = NULL;


static int quiet = 0;
static int error = 0;


static int outformat = -1;
static int outdiv = 0;
static char *outname = NULL;

static MBUF _outbuf = {0,0,NULL}, *outbuf = &_outbuf;


/*
 * Warning and error printing hook.
 */
static void print(MPLevel level, const char *fmt, va_list args)
{
  FILE *out = NULL;

  switch(level)
    {
      case MPNote:
        out = stdout;
        break;
      case MPWarn:
        if(quiet < 1)
          fprintf(out = stderr, "%s: warning: ", warnname);
        break;
      case MPError:
        if(quiet < 2)
          fprintf(out = stderr, "%s: mferror: ", warnname);
        break;
      case MPFatal:
        error = 1;
        if(quiet < 3)
          fprintf(out = stderr, "%s: error: ", warnname);
        break;
      default:
        fprintf(out = stderr, "%s: !!!: ", warnname);
    }

  if(out)
    {
      vfprintf(out, fmt, args);
      fputc('\n', out);
      fflush(out);
    }
}


/*
 * Print the track data of `s'.
 */
static void showtracks(Score *s, int flags)
{
  long n, ntotal;

  for(n = ntotal = 0; n < s->ntrk; n++)
    {
      ntotal += s->tracks[n];
      if(flags & SHOWTLENGTHS)
        midiprint(MPNote, "       %7lu", s->tracks[n]);
    }

  if(flags & SHOWEVENTS)
    for(n = 0; n < ntotal; n++)
      {
        char *cmd = NULL;
        switch(s->events[n].msg.generic.cmd & 0xf0)
          {
            case NOTEOFF:           cmd = "Note Off"; break;
            case NOTEON:            cmd = "Note On"; break;
            case KEYPRESSURE:       cmd = "Key Pressure"; break;
            case CONTROLCHANGE:     cmd = "Control Change"; break;
            case PROGRAMCHANGE:     cmd = "Program Change"; break;
            case CHANNELPRESSURE:   cmd = "Channel Pressure"; break;
            case PITCHWHEELCHANGE:  cmd = "Pitch Wheel Change"; break;
          }

        switch(s->events[n].msg.generic.cmd)
          {
            case SYSTEMEXCLUSIVE:     cmd = "System Exclusive"; break;
            case SYSTEMEXCLUSIVECONT: cmd = "System Exclusive Cont"; break;
            case META:                cmd = "Meta"; break;
            case SEQUENCENUMBER:      cmd = "Sequence Number"; break;
            case TEXT:                cmd = "Text"; break;
            case COPYRIGHTNOTICE:     cmd = "Copyright Notice"; break;
            case TRACKNAME:           cmd = "Track Name"; break;
            case INSTRUMENTNAME:      cmd = "Instrument Name"; break;
            case LYRIC:               cmd = "Lyric"; break;
            case MARKER:              cmd = "Marker"; break;
            case CUEPOINT:            cmd = "Cue Point"; break;
            case ENDOFTRACK:          cmd = "End Of Track"; break;
            case SETTEMPO:            cmd = "Set Tempo"; break;
            case SMPTEOFFSET:         cmd = "SMPTE Offset"; break;
            case TIMESIGNATURE:       cmd = "Time Signature"; break;
            case KEYSIGNATURE:        cmd = "Key Signature"; break;
            case SEQUENCERSPECIFIC:   cmd = "Sequencer Specific"; break;
          }

        if(!cmd) cmd = "Unknown";
        midiprint(MPNote, "%8ld %s", s->events[n].time, cmd);
      }
}


/*
 * Convert delta times to absolute times.
 */
static void deltatoabs(Score *s)
{
  long t, etotal, e;

  for(etotal = t = 0; t < s->ntrk; t++)
    {
      for(e = 1; e < s->tracks[t]; e++)
        s->events[etotal+e].time += s->events[etotal+e-1].time;

      etotal += s->tracks[t];
    }
}


/*
 * Convert absolute times to delta times.
 */
static void abstodelta(Score *s)
{
  long t, etotal, e;

  for(etotal = t = 0; t < s->ntrk; t++)
    {
      for(e = s->tracks[t] - 1; e > 1; e--)
        s->events[etotal+e].time -= s->events[etotal+e-1].time;

      etotal += s->tracks[t];
    }
}


/*
 * Delete all tracks that are not within the given range.
 */
static void adjusttracks(Score *s, long from, long to)
{
  long t, startx, endx, tendx;

  /* Ensure that from and to are within the score's bounds. */
  if(to >= s->ntrk)
    to = s->ntrk - 1;

  if(from >= s->ntrk || from > to)
    return;

  /*
   * Clear all events before track `from' and remember the starting
   * position.
   */
  for(t = startx = 0; t < from; t++)
    while(s->tracks[t]--)
      clear_message(&s->events[startx++].msg);

  /*
   * Search the end position, i.e. the index of the first element after
   * track `to'.
   */
  for(endx = startx, t = from; t <= to; t++)
    endx += s->tracks[t];

  /*
   * Clear all events after `to'.
   */
  for(tendx = endx, t = to + 1; t < s->ntrk; t++)
    while(s->tracks[t]--)
      clear_message(&s->events[tendx++].msg);

  /*
   * Move down the track indices.
   */
  memmove(s->tracks, s->tracks + from, (to - from + 1) * sizeof(*s->tracks));

  /*
   * Move down the events.
   */
  memmove(s->events, s->events + startx, (endx - startx) * sizeof(*s->events));

  /*
   * Adjust the number of tracks.
   */
  s->ntrk = to - from + 1;
}


/*
 * This function is used to sort events by time.
 * It takes care that `End Of Track' Events allways will greater than
 * other events.
 * If two events have the same time and neither of them is an `End Of
 * Track' event, the command fields are compared.
 */
static int _eventcmp(const void *_e1, const void *_e2)
{
  const MFEvent *e1 = _e1, *e2 = _e2;

  if(e1->msg.generic.cmd == ENDOFTRACK && e2->msg.generic.cmd != ENDOFTRACK)
    return 1;
  else if(e1->msg.generic.cmd != ENDOFTRACK && e2->msg.generic.cmd == ENDOFTRACK)
    return -1;
  else if(e1->time != e2->time)
    return e1->time - e2->time;
  else
    return e1->msg.generic.cmd - e2->msg.generic.cmd;
}


/*
 * Merge all the tracks of `s' into one.
 * The time fields of the events must be absolute times.
 */
static void mergetracks(Score *s)
{
  long etotal, t;

  /*
   * If there are less than two tracks, there is not much to do.
   */
  if(s->ntrk < 2)
    return;

  /*
   * Count the total number of events.
   */
  for(etotal = t = 0; t < s->ntrk; t++)
    etotal += s->tracks[t];

  /*
   * Now, sort all events by time.
   */
  qsort(s->events, etotal, sizeof(*s->events), _eventcmp);

  /*
   * Update the size of the first track and the number of tracks.
   * Since each track ends with an `End Of Track' event, we have to
   * forget the last `s->ntrk - 1' events, so that exactly one `End Of
   * Track' event will be left over.
   */
  s->tracks[0] = etotal - (s->ntrk - 1);
  s->ntrk = 1;
}



/*
 * Handle one filespec.
 */
static int dofile(const char *spec, int flags)
{
  FILE *f = stdin;
  static char _name[FILENAME_MAX];
  static char name[FILENAME_MAX];
  MBUF _b, *b = &_b;
  Score _s, *s = &_s;
  int scorenum;

  /*
   * Starting and end numbers of selected scores. If `sc1' is -1, all
   * scores are selected and `sc0' is set to 0.
   */
  long sc0 = 0, sc1 = -1;

  /* Dito, for track numbers. */
  long tr0 = 0, tr1 = -1;

  if(!spec)
    spec = "";

  /*
   * Parse the spec.
   */
  if(sscanf(spec, "%[^@]@%lu-%lu.%lu-%lu", name, &sc0, &sc1, &tr0, &tr1) == 5)
    /* skip */;
  else if(sscanf(spec, "%[^@]@%lu-%lu.%lu", name, &sc0, &sc1, &tr0) == 4)
    tr1 = tr0;
  else if(sscanf(spec, "%[^@]@%lu-%lu", name, &sc0, &sc1) == 3)
    /* skip */;
  else if(sscanf(spec, "%[^@]@%lu", name, &sc0) == 2)
    sc1 = sc0;
  else if(sscanf(spec, "%[^@]@%lu.%lu-%lu", name, &sc0, &tr0, &tr1) == 4)
    sc1 = sc0;
  else if(sscanf(spec, "%[^@]@.%lu-%lu", name, &tr0, &tr1) == 3)
    /* skip */;
  else if(sscanf(spec, "%[^@]@.%lu", name, &tr0) == 2)
    tr1 = tr0;
  else
    strcpy(name, spec);

  if(!*name || !strcmp(name, "-"))
    {
      warnname = "-";
      *name = 0;
    }
  else
    {
      strcpy(_name, name);
      warnname = _name;
    }

  midiprint_hook = print;

  if(*name && !(f = fopen(name, "rb")))
    {
      midiprint(MPFatal, "%s", strerror(errno));
      return 1;
    }

  if(read_mbuf(b, f))
    {
      fclose(f);
      return 1;
    }

  fclose(f);

  error = 0;

  if(!read_score(b, s))
    {
      midiprint(MPFatal, "no headers or tracks found");
      return 1;
    }

  scorenum = 0;

  if(sc1 < 0 || sc0 <= scorenum && scorenum <= sc1)
    {
      deltatoabs(s);

      if(tr1 >= 0)
        adjusttracks(s, tr0, tr1);

      if(flags & MERGE)
        mergetracks(s);

      if(flags & SHOWHEADERS)
        midiprint(MPNote, "%s(%d): %7d %7d %7d",
                  warnname, scorenum, s->fmt, s->ntrk, s->div);
      else if(flags & (SHOWTLENGTHS | SHOWEVENTS))
        midiprint(MPNote, "%s(%d):", warnname, scorenum);

      if(!outdiv) outdiv = s->div;
      if(outformat < 0) outformat = s->fmt;

      showtracks(s, flags);

      /*
       * abstodelta(s);
       * write
       */

      clear_score(s);
    }

  scorenum++;

  while(mbuf_rem(b) > 0 && read_score(b, s))
    {
      if(sc1 < 0 || sc0 <= scorenum && scorenum <= sc1)
        {
          deltatoabs(s);

          if(tr1 >= 0)
            adjusttracks(s, tr0, tr1);

          if(flags & MERGE)
            mergetracks(s);

          if(flags & SHOWHEADERS)
            midiprint(MPNote, "%s(%d): %7d %7d %7d",
                      warnname, scorenum, s->fmt, s->ntrk, s->div);
          else if(flags & (SHOWTLENGTHS | SHOWEVENTS))
            midiprint(MPNote, "%s(%d):", warnname, scorenum);

          if(!outdiv) outdiv = s->div;
          if(outformat < 0) outformat = s->fmt;

          showtracks(s, flags);

          /*
           * abstodelta(s);
           * write
           */

          clear_score(s);
        }

      scorenum++;
    }

  if(mbuf_rem(b) > 0)
    midiprint(MPWarn, "garbage at end of input");

  mbuf_free(b);

  return error ? 1 : 0;
}


int main(int argc, char *argv[])
{
  int opt;
  int error = 0;
  int flags = 0;

  /*
   * Parse command line arguments.
   */
  while((opt = getopt(argc, argv, ":hleqnmo:012d:")) != -1)
    switch(opt)
      {
        case 'h':
          flags |= SHOWHEADERS;
          break;
        case 'l':
          flags |= SHOWTLENGTHS;
          break;
        case 'e':
          flags |= SHOWEVENTS;
          break;
        case 'q':
          quiet++;
          break;
        case 'n':
          flags |= NOHEADER;
          break;
        case 'm':
          flags |= MERGE;
          break;
        case 'o':
          outname = optarg;
          break;
        case '0':
          outformat = 0;
          break;
        case '1':
          outformat = 1;
          break;
        case '2':
          outformat = 2;
          break;
        case 'd':
          if(sscanf(optarg, "%d", &outdiv) != 1 || !outdiv)
            usage();
          break;
        default:
          usage();
          break;
      }

  argc -= optind;
  argv += optind;

  if(!argc)
    error = dofile(NULL, flags);
  else
    while(argc--)
      error |= dofile(*argv++, flags);

  return error ? EXIT_FAILURE : 0;
}
