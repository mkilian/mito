/*
 * $Id: mito.c,v 1.5 1996/04/04 16:26:34 kilian Exp $
 *
 * mito --- the midi tool
 *
 * $Log: mito.c,v $
 * Revision 1.5  1996/04/04 16:26:34  kilian
 * Improved output of text messages.
 * Fixed concatenating of tracks.
 *
 * Revision 1.4  1996/04/03  14:27:56  kilian
 * Implemented merging of tracks and writing of midifiles.
 * Improved printing of events.
 *
 * Revision 1.3  1996/04/02  23:29:19  kilian
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
#include "vld.h"


static void usage(void)
{
  fputs("usage: mito [-hleqnm012c] [-o file] [-d div] {[file][@sl]}...\n"
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
        "    -m: merge all tracks of each single score\n"
        "    -f: fix nested / unmatched noteon/noteoff groups\n"
        "    @sl: syntax: [scores][.tracks]; read selection\n"
        "         scores: range of scores to read; empty = all\n"
        "         tracks: range of tracks to read; empty = all\n"
        "  output options (only valid if `-o' is given)\n"
        "    -[012]:  use this output format (default from first score)\n"
        "    -d:  use output division `div' (default from first score)\n"
        "    -n:  no header; only write the tracks\n"
        "    -c:  concat all tracks to one\n", stderr);
  exit(EXIT_FAILURE);
}


/*
 * Command line flags, bitwise coded.
 */
typedef enum{
  SHOWHEADERS   = 0x01,
  SHOWTLENGTHS  = 0x02,
  SHOWEVENTS    = 0x04,
  NOHEADER      = 0x08,
  MERGETRACKS   = 0x10,
  CONCATTRACKS  = 0x20,
  FIXGROUPS     = 0x40
} Flags;


/*
 * The filename to print in warning and error messages.
 */
static char *warnname = NULL;


static int quiet = 0;
static int error = 0;


static int outformat = -1;
static int outdiv = 0;
static int outntrk = 0;

/*
 * If not NULL, write track data to this buffer.
 */
static MBUF *outb = NULL;


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
 * Convert a vld into a printable string.
 */
static char *strdat(void *vld)
{
  static char buf[65];
  long length = vld_size(vld);
  const unsigned char *data = vld_data(vld);
  int i;

  for(i = 0; length > 0 && i < 65 - 4; i++, length--, data++)
    switch(*data)
      {
        case '\\':  buf[i++] = '\\'; buf[i] = '\\'; break;
        case '\a':  buf[i++] = '\\'; buf[i] = 'a'; break;
        case '\b':  buf[i++] = '\\'; buf[i] = 'b'; break;
        case '\f':  buf[i++] = '\\'; buf[i] = 'f'; break;
        case '\n':  buf[i++] = '\\'; buf[i] = 'n'; break;
        case '\r':  buf[i++] = '\\'; buf[i] = 'r'; break;
        case '\t':  buf[i++] = '\\'; buf[i] = 't'; break;
        case '\v':  buf[i++] = '\\'; buf[i] = 'b'; break;
        default:
          if(*data < ' ')
            {
              sprintf(buf + i, "\\%03hu", *data);
              i += 3;
            }
          else
            buf[i] = *data;
      }

  if(length > 0)
    strcpy(buf + i, "...");
  else
    buf[i] = 0;

  return buf;
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
        MFEvent *e = s->events + n;

        switch(e->msg.generic.cmd & 0xf0)
          {
            case NOTEOFF:
              midiprint(MPNote, "%8ld NoteOff %hd %hd %hd", e->time,
                        e->msg.noteoff.chn, e->msg.noteoff.note,
                        e->msg.noteoff.velocity);
              continue;
            case NOTEON:
              midiprint(MPNote, "%8ld NoteOn %hd %hd %hd", e->time,
                        e->msg.noteon.chn, e->msg.noteon.note,
                        e->msg.noteon.velocity);
              continue;
            case KEYPRESSURE:
              midiprint(MPNote, "%8ld KeyPressure %hd %hd %hd", e->time,
                        e->msg.keypressure.chn, e->msg.keypressure.note,
                        e->msg.keypressure.velocity);
              continue;
            case CONTROLCHANGE:
              midiprint(MPNote, "%8ld ControlChange %hd %hd %hd", e->time,
                        e->msg.controlchange.chn,
                        e->msg.controlchange.controller,
                        e->msg.controlchange.value);
              continue;
            case PROGRAMCHANGE:
              midiprint(MPNote, "%8ld ProgramChange %hd %hd", e->time,
                        e->msg.programchange.chn,
                        e->msg.programchange.program);
              continue;
            case CHANNELPRESSURE:
              midiprint(MPNote, "%8ld ChannelPressure %hd %hd", e->time,
                        e->msg.channelpressure.chn,
                        e->msg.channelpressure.velocity);
              continue;
            case PITCHWHEELCHANGE:
              midiprint(MPNote, "%8ld PitchWheelChange %hd %hd", e->time,
                        e->msg.pitchwheelchange.chn,
                        e->msg.pitchwheelchange.value);
              continue;
          }

        switch(e->msg.generic.cmd)
          {
            case SYSTEMEXCLUSIVE:
              midiprint(MPNote, "%8ld SystemExclusive `%s'", e->time,
                        strdat(e->msg.systemexclusive.data));
              continue;
            case SYSTEMEXCLUSIVECONT:
              midiprint(MPNote, "%8ld SystemExclusiveCont `%s'", e->time,
                        strdat(e->msg.systemexclusivecont.data));
              continue;
            case META:
              midiprint(MPNote, "%8ld Meta %hd `%s'", e->time,
                        e->msg.meta.type, strdat(e->msg.meta.data));
              continue;
            case SEQUENCENUMBER:
              midiprint(MPNote, "%8ld SequenceNumber %hu", e->time,
                        e->msg.sequencenumber.sequencenumber);
              continue;
            case TEXT:
              midiprint(MPNote, "%8ld Text `%s'", e->time,
                        strdat(e->msg.text.text));
              continue;
            case COPYRIGHTNOTICE:
              midiprint(MPNote, "%8ld CopyrightNotice `%s'", e->time,
                        strdat(e->msg.copyrightnotice.text));
              continue;
            case TRACKNAME:
              midiprint(MPNote, "%8ld TrackName `%s'", e->time,
                        strdat(e->msg.trackname.text));
              continue;
            case INSTRUMENTNAME:
              midiprint(MPNote, "%8ld InstrumentName `%s'", e->time,
                        strdat(e->msg.instrumentname.text));
              continue;
            case LYRIC:
              midiprint(MPNote, "%8ld Lyric `%s'", e->time,
                        strdat(e->msg.lyric.text));
              continue;
            case MARKER:
              midiprint(MPNote, "%8ld Marker `%s'", e->time,
                        strdat(e->msg.marker.text));
              continue;
            case CUEPOINT:
              midiprint(MPNote, "%8ld CuePoint `%s'", e->time,
                        strdat(e->msg.cuepoint.text));
              continue;
            case ENDOFTRACK:
              midiprint(MPNote, "%8ld EndOfTrack", e->time);
              continue;
            case SETTEMPO:
              midiprint(MPNote, "%8ld SetTempo %ld", e->time,
                        e->msg.settempo.tempo);
              continue;
            case SMPTEOFFSET:
              midiprint(MPNote, "%8ld SMPTEOffset %hd %hd %hd %hd %hd",
                        e->time, e->msg.smpteoffset.hours,
                        e->msg.smpteoffset.minutes,
                        e->msg.smpteoffset.seconds,
                        e->msg.smpteoffset.frames,
                        e->msg.smpteoffset.subframes);
              continue;
            case TIMESIGNATURE:
              midiprint(MPNote, "%8ld TimeSignature %hd %hd %hd %hd",
                        e->time, e->msg.timesignature.nominator,
                        e->msg.timesignature.denominator,
                        e->msg.timesignature.clocksperclick,
                        e->msg.timesignature.ttperquarter);
              continue;
            case KEYSIGNATURE:
              midiprint(MPNote, "%8ld KeySignature %hd %hd", e->time,
                        e->msg.keysignature.sharpsflats,
                        e->msg.keysignature.minor);
              continue;
            case SEQUENCERSPECIFIC:
              midiprint(MPNote, "%8ld SequencerSpecific `%s'", e->time,
                        strdat(e->msg.sequencerspecific.data));
              continue;
          }

        midiprint(MPNote, "%8ld Unknown %hu", e->time, e->msg.generic.cmd);
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
 * It takes care that `End Of Track' Events allways will be greater than
 * other events. If both events are `End Of Track' events, the one with
 * the *greater* time will be reported to be less than the event with
 * the smaller time value. This ensures that only the very last `End Of
 * Track' message will be used when merging.
 * If two events have the same time and neither of them is an `End Of
 * Track' event, the command fields are compared.
 */
static int _eventcmp(const void *_e1, const void *_e2)
{
  const MFEvent *e1 = _e1, *e2 = _e2;

  if(e1->msg.generic.cmd == ENDOFTRACK && e2->msg.generic.cmd == ENDOFTRACK)
    return e2->time - e1->time;
  else if(e1->msg.generic.cmd == ENDOFTRACK)
    return 1;
  else if(e2->msg.generic.cmd == ENDOFTRACK)
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
 * For each track in `s', write a track header and all it's events into
 * the buffer. If `concat' is nonzero, only one header is written and
 * all tracks are appended.
 * Returns 1 on success, else 0.
 */
static int write_tracks(MBUF *b, Score *s, int concat)
{
  long phdr = 0, ptrk = 0;
  MFEvent *es;
  long t, e, n;
  unsigned char running;

  if(s->ntrk < 1)
    return 1;

  for(t = 0, es = s->events; t < s->ntrk; t++)
    {
      errno = 0;

      if(t == 0 || !concat)
        {
          phdr = mbuf_pos(b);
          if((t == 0 || !concat) && !write_MTrk(b, 0))
            {
              midiprint(MPFatal, "%s", strerror(errno));
              return 0;
            }
          ptrk = mbuf_pos(b);
          running = 0;
        }

      n = s->tracks[t];
      if(concat && t < s->ntrk - 1)
        n--;

      for(e = 0; e < n; e++)
        if(!write_event(b, es + e, &running))
          {
            if(errno)
              midiprint(MPFatal, "%s", strerror(errno));
            else
              midiprint(MPFatal, "writing event failed");
            return 0;
          }

      if(!concat)
        {
          long p = mbuf_pos(b);
          mbuf_set(b, phdr);
          if(!write_MTrk(b, p - ptrk))
            {
              midiprint(MPFatal, "%s", strerror(errno));
              return 0;
            }
          mbuf_set(b, p);
        }

      es += s->tracks[t];
    }

  return 1;
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

      if(flags & MERGETRACKS)
        mergetracks(s);

      if(flags & SHOWHEADERS)
        midiprint(MPNote, "%s(%d): %7d %7d %7d",
                  warnname, scorenum, s->fmt, s->ntrk, s->div);
      else if(flags & (SHOWTLENGTHS | SHOWEVENTS))
        midiprint(MPNote, "%s(%d):", warnname, scorenum);

      if(!outdiv) outdiv = s->div;
      if(outformat < 0) outformat = s->fmt;

      showtracks(s, flags);

      if(outb)
        {
          abstodelta(s);
          write_tracks(outb, s, flags & CONCATTRACKS);
          if(flags & CONCATTRACKS)
            outntrk++;
          else
            outntrk += s->ntrk;
        }

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

          if(flags & MERGETRACKS)
            mergetracks(s);

          if(flags & SHOWHEADERS)
            midiprint(MPNote, "%s(%d): %7d %7d %7d",
                      warnname, scorenum, s->fmt, s->ntrk, s->div);
          else if(flags & (SHOWTLENGTHS | SHOWEVENTS))
            midiprint(MPNote, "%s(%d):", warnname, scorenum);

          if(!outdiv) outdiv = s->div;
          if(outformat < 0) outformat = s->fmt;

          showtracks(s, flags);

          if(outb)
            {
              abstodelta(s);
              write_tracks(outb, s, flags & CONCATTRACKS);
              if(flags & CONCATTRACKS)
                outntrk++;
              else
                outntrk += s->ntrk;
            }

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

  FILE *outf;
  char *outname = NULL;

  /*
   * Parse command line arguments.
   */
  while((opt = getopt(argc, argv, ":hleqnmo:012cfd:")) != -1)
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
        case 'f':
          flags |= FIXGROUPS;
          break;
        case 'q':
          quiet++;
          break;
        case 'n':
          flags |= NOHEADER;
          break;
        case 'm':
          flags |= MERGETRACKS;
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
        case 'c':
          flags |= CONCATTRACKS;
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

  if(outname)
    {
      static MBUF _outb;
      outb = &_outb;
      outb->b = NULL;
      outb->n = 0;
      outb->i = 0;

      if(!(flags & NOHEADER) &&
         /* This will be rewritten later to insert the correct values. */
         !write_MThd(outb, 0, 0, 0))
        {
          perror(outname);
          return EXIT_FAILURE;
        }
    }

  if(!argc)
    error = dofile(NULL, flags);
  else
    while(argc--)
      error |= dofile(*argv++, flags);

  if(error)
    return EXIT_FAILURE;
  else if(outb && !(flags & NOHEADER) && mbuf_set(outb, 0) != 0)
    {
      perror("rewinding buffer");
      return EXIT_FAILURE;
    }
  else if(outb && !(flags & NOHEADER) &&
          !write_MThd(outb, outformat, outntrk, outdiv))
    {
      perror(outname);
      return EXIT_FAILURE;
    }
  else if(outb && (flags & CONCATTRACKS) && !write_MTrk(outb, mbuf_rem(outb)))
    {
      perror(outname);
      return EXIT_FAILURE;
    }
  else if(outb && !(outf = fopen(outname, "wb")))
    {
      perror(outname);
      return EXIT_FAILURE;
    }
  else if(outb && write_mbuf(outb, outf) < 0)
    {
      perror(outname);
      fclose(outf);
      return EXIT_FAILURE;
    }
  else if(outb)
    {
      fclose(outf);
      return EXIT_SUCCESS;
    }
  else
    return EXIT_SUCCESS;
}
