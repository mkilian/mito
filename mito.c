/*
 * $Id: mito.c,v 1.1 1996/04/01 19:10:51 kilian Exp $
 *
 * mito --- the midi tool
 *
 * $Log: mito.c,v $
 * Revision 1.1  1996/04/01 19:10:51  kilian
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
#define QUIET 0x80


/*
 * The filename to print in warning and error messages.
 */
static char *warnname = NULL;


/*
 * The current buffer position. Used for warning and error messages, if
 * not negative.
 */
static long warnpos = -1;


/*
 * Warning and error printing hook.
 */
static void warn(const char *fmt, va_list args)
{
  if(warnname)
    fprintf(stderr, "%s: ", warnname);
  if(warnpos >= 0)
    fprintf(stderr, "at %lu: ", warnpos);

  vfprintf(stderr, fmt, args);
  fputc('\n', stderr);
}


/*
 * Normal printing hook.
 */
static void print(const char *fmt, va_list args)
{
  vprintf(fmt, args);
  putchar('\n');
}


/*
 * Print out all events of `b'.
 */
static void printevents(MBUF *b)
{
  char running = 0;
  long basepos = warnpos;

  while(mbuf_rem(b) > 0)
    {
      long delta;
      MFMessage e;
      char *cmd = NULL;

      warnpos = basepos + mbuf_pos(b);
      if((delta = read_vlq(b)) < 0)
        {
          mbuf_get(b);
          continue;
        }
      warnpos = basepos + mbuf_pos(b);
      if(!read_message(b, &e, &running))
        {
          mbuf_get(b);
          continue;
        }

      switch(e.generic.cmd & 0xf0)
        {
          case NOTEOFF:           cmd = "Note Off"; break;
          case NOTEON:            cmd = "Note On"; break;
          case KEYPRESSURE:       cmd = "Key Pressure"; break;
          case CONTROLCHANGE:     cmd = "Control Change"; break;
          case PROGRAMCHANGE:     cmd = "Program Change"; break;
          case CHANNELPRESSURE:   cmd = "Channel Pressure"; break;
          case PITCHWHEELCHANGE:  cmd = "Pitch Wheel Change"; break;
        }

      if(!cmd)
        switch(e.generic.cmd)
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

      if(!cmd)
        cmd = "Unknown";

      clear_message(&e);
      midiprint("%8ld %s", delta, cmd);
    }
}


static int dofile(const char *name, int flags)
{
  FILE *f = stdin;
  static char _name[FILENAME_MAX];
  MBUF b;
  int tracks = 0, hdrs = 0, totaltracks = 0, error = 0;
  long skip, expect = 0;
  CHUNK chunk;

  warnpos = -1;

  if(!(flags & (SHOWFORMAT | SHOWNTRACKS | SHOWDIVISION |
                SHOWTLENGTHS | SHOWEVENTS)))
    flags |= SHOWFORMAT | SHOWNTRACKS | SHOWDIVISION | SHOWTLENGTHS;

  if(!name || !strcmp(name, "-"))
    {
      warnname = "standard input";
      name = NULL;
    }
  else
    {
      strcpy(_name, name);
      warnname = _name;
    }

  midiprint_hook = print;

  if(!(flags & QUIET))
    midiwarn_hook = midierror_hook = warn;

  if(name && !(f = fopen(name, "rb")))
    {
      midierror("%s", strerror(errno));
      return 1;
    }

  if(read_mbuf(&b, f))
    {
      fclose(f);
      return 1;
    }

  fclose(f);

  warnpos = 0;
  while(!error && (skip = search_chunk(&b, &chunk, 0)) >= 0)
    {
      warnpos = mbuf_pos(&b);

      if(skip > expect)
        midierror("%lu bytes skipped", skip - expect);
      if(skip < expect)
        midierror("%lu bytes lost", expect - skip);

      if(chunk.type == MThd)
        {
          if(tracks > 0)
            midierror("%d tracks missing", tracks);

          if(chunk.hdr.mthd.xsize)
            midiwarn("%ld extra bytes in header", chunk.hdr.mthd.xsize);

          if(flags & SHOWFORMAT)
            midiprint("%7d ", chunk.hdr.mthd.fmt);
          if(flags & SHOWNTRACKS)
            midiprint("%7d ", chunk.hdr.mthd.ntrk);
          if(flags & SHOWDIVISION)
            midiprint("%7d ", chunk.hdr.mthd.div);

          if(hdrs++)
            midiprint("STILL %s", warnname);
          else
            midiprint("%s", warnname);

          expect = 0;
          tracks = chunk.hdr.mthd.ntrk;
        }
      else if(chunk.type == MTrk)
        {
          MBUF t;

          if(!hdrs)
            midierror("missing header");
          else if(!tracks)
            midiwarn("extraneous tracks");

          if(flags & SHOWTLENGTHS)
            midiprint("      T %7lu", chunk.hdr.mtrk.size);

          expect = chunk.hdr.mtrk.size;
          tracks--;
          totaltracks++;

          if(flags & SHOWEVENTS)
            {
              t.n = chunk.hdr.mtrk.size;
              t.i = 0;
              t.b = b.b + b.i;
              warnpos = mbuf_pos(&b);
              printevents(&t);
            }
        }
      else
        error = 1;
    }

  warnpos = mbuf_pos(&b);

  if(!error && b.i < b.n)
    midiwarn("garbage at end");

  if(!error && !totaltracks && !hdrs)
    {
      error = 1;
      midiwarn("no headers or tracks found");
    }

  if(!error && !totaltracks)
    midiwarn("no tracks");

  if(!error && tracks > 0)
    midiwarn("%d tracks missing", tracks);

  free(b.b);

  if(error)
    return 1;
  else
    return 0;
}


int main(int argc, char *argv[])
{
  int opt;
  int error = 0;
  int flags = 0;

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

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
          flags |= QUIET;
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
