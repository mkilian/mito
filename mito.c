/*
 * $Id: mito.c,v 1.2 1996/04/02 10:16:36 kilian Exp $
 *
 * mito --- the midi tool
 *
 * $Log: mito.c,v $
 * Revision 1.2  1996/04/02 10:16:36  kilian
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
  fputs("usage: mito [-defnqt] [file ...]\n"
        "  -d:  show division\n"
        "  -e:  show events\n"
        "  -f:  show format\n"
        "  -n:  show number of tracks\n"
        "  -q:  be quiet (no warning messages)\n"
        "  -t:  show track lengths\n" , stderr);
  exit(EXIT_FAILURE);
}


/*
 * Command line flags, bitwise coded.
 */
#define SHOWFORMAT 0x01
#define SHOWNTRACKS 0x02
#define SHOWDIVISION 0x04
#define SHOWTLENGTHS 0x08
#define SHOWEVENTS 0x10


/*
 * The filename to print in warning and error messages.
 */
static char *warnname = NULL;


static int quiet = 0;


/*
 * Warning and error printing hook.
 */
static void print(MPLevel level, const char *fmt, va_list args)
{
  if(level == MPNote)
    {
      vprintf(fmt, args);
      putchar('\n');
      fflush(stdout);
    }
  else if(!quiet || level == MPFatal)
    {
      if(warnname)
        fprintf(stderr, "%s: ", warnname);

      vfprintf(stderr, fmt, args);
      fputc('\n', stderr);
      fflush(stderr);
    }
}


/*
 * Print the track data of `s'.
 */
static void showtracks(Score *s, int flags)
{
  long n;

  if(flags & SHOWTLENGTHS)
    for(n = 0; n < s->ntrk; n++)
      {
        long next = (n < s->ntrk - 1) ? s->tracks[n + 1] : s->nev;
        midiprint(MPNote, "      #E %6lu", next - s->tracks[n]);
      }

  if(flags & SHOWEVENTS)
    for(n = 0; n < s->nev; n++)
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


static int dofile(const char *name, int flags)
{
  FILE *f = stdin;
  static char _name[FILENAME_MAX];
  MBUF b;
  Score s;
  int scorenum = 1;
  static char out[64];

  if(!(flags & (SHOWFORMAT | SHOWNTRACKS | SHOWDIVISION |
                SHOWTLENGTHS | SHOWEVENTS)))
    flags |= SHOWFORMAT | SHOWNTRACKS | SHOWDIVISION | SHOWTLENGTHS;

  if(!name || !strcmp(name, "-"))
    {
      warnname = "{stdin}";
      name = NULL;
    }
  else
    {
      strcpy(_name, name);
      warnname = _name;
    }

  midiprint_hook = print;

  if(name && !(f = fopen(name, "rb")))
    {
      midiprint(MPError, "%s", strerror(errno));
      return 1;
    }

  if(read_mbuf(&b, f))
    {
      fclose(f);
      return 1;
    }

  fclose(f);

  if(!read_score(&b, &s))
    {
      midiprint(MPError, "no headers or tracks found");
      return 1;
    }

  *out = 0;
  if(flags & SHOWFORMAT)
    sprintf(out, " %7d", s.fmt);
  if(flags & SHOWNTRACKS)
    sprintf(out + strlen(out), " %7d", s.ntrk);
  if(flags & SHOWDIVISION)
    sprintf(out + strlen(out), " %7d", s.div);
  midiprint(MPNote, "%s:%s", warnname, out);

  showtracks(&s, flags);

  clear_score(&s);

  while(mbuf_rem(&b) > 0 && read_score(&b, &s))
    {
      *out = 0;
      if(flags & SHOWFORMAT)
        sprintf(out, " %7d", s.fmt);
      if(flags & SHOWNTRACKS)
        sprintf(out + strlen(out), " %7d", s.ntrk);
      if(flags & SHOWDIVISION)
        sprintf(out + strlen(out), " %7d", s.div);
      midiprint(MPNote, "%s(%d):%s", warnname, ++scorenum, out);

      showtracks(&s, flags);

      clear_score(&s);
    }

  if(mbuf_rem(&b) > 0)
    midiprint(MPWarn, "garbage at end of input");

  mbuf_free(&b);

  return 0;
}


int main(int argc, char *argv[])
{
  int opt;
  int error = 0;
  int flags = 0;

  /*
   * Parse command line arguments.
   */
  while((opt = getopt(argc, argv, ":defnqt")) != -1)
    switch(opt)
      {
        case 'd':
          flags |= SHOWDIVISION;
          break;
        case 'e':
          flags |= SHOWEVENTS;
          break;
        case 'f':
          flags |= SHOWFORMAT;
          break;
        case 'n':
          flags |= SHOWNTRACKS;
          break;
        case 'q':
          quiet++;
          break;
        case 't':
          flags |= SHOWTLENGTHS;
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
