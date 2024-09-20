/* mito --- the midi tool */
/* XXX: the order of events (when using -e) is not deterministic! */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "chunk.h"
#include "event.h"
#include "print.h"
#include "score.h"
#include "util.h"
#include "vld.h"

static void usage(void) {
	fputs("usage: mito [-hleuqnm012c] [-o file] [-d div] {[file][@sl]}...\n"
	    "overall options:\n"
	    "    -h:  show score headers\n"
	    "    -l:  show track lengths\n"
	    "    -e:  show events\n"
	    "    -u:  don't group noteon/noteoff events\n"
	    "    -q:  accumulative(1-3): no warning, midi errors, other errors\n"
	    "    -o:  write resulting output to `file'\n"
	    "input:\n"
	    "    -m: merge all tracks of each single score\n"
	    "    -f: fix nested / unmatched noteon/noteoff groups\n"
	    "    @sl: syntax: [scores][.tracks]; read selection\n"
	    "output options (only valid if `-o' is given):\n"
	    "    -[012]:  use this output format (default from first score)\n"
	    "    -d:  use output division `div' (default from first score)\n"
	    "    -n:  no header; only write the tracks\n"
	    "    -c:  concat all tracks to one\n", stderr);
	exit(EXIT_FAILURE);
}

/* Command line flags, bitwise coded. */
typedef enum{
	SHOWHEADERS	= 0x01,
	SHOWTLENGTHS	= 0x02,
	SHOWEVENTS	= 0x04,
	NOHEADER	= 0x08,
	MERGETRACKS	= 0x10,
	CONCATTRACKS	= 0x20,
	FIXGROUPS	= 0x40,
	UNGROUP		= 0x80
} Flags;

/* The filename to print in warning and error messages. */
static char *warnname = NULL;

static int quiet = 0;
static int error = 0;

static int outformat = -1;
static int outdiv = 0;
static int outntrk = 0;

/* If not NULL, write track data to this buffer. */
static MBUF *outb = NULL;

/* Warning and error printing hook. */
static void print(MPLevel level, const char *fmt, va_list args) {
	FILE *out = NULL;

	switch (level) {
	case MPNote:
		out = stdout;
		break;
	case MPWarn:
		if (quiet < 1)
			fprintf(out = stderr, "%s: warning: ", warnname);
		break;
	case MPError:
		if (quiet < 2)
			fprintf(out = stderr, "%s: mferror: ", warnname);
		break;
	case MPFatal:
		error = 1;
		if (quiet < 3)
			fprintf(out = stderr, "%s: error: ", warnname);
		break;
	default:
		fprintf(out = stderr, "%s: !!!: ", warnname);
	}

	if (out) {
		vfprintf(out, fmt, args);
		fputc('\n', out);
		fflush(out);
	}
}

/* Convert a vld into a printable string. */
static char *strdat(void *vld) {
	static char buf[1024 * 4 + 1];
	long length = vld_size(vld);
	int trunc = length > 1024;
	if (trunc)
		length = 1024 - 3;
	strvisx(buf, vld_data(vld), length, VIS_CSTYLE);
	if (trunc)
		strlcat (buf, "...", sizeof(buf));
	return buf;
}

static void printevent(MFEvent *e) {
	switch (e->msg.generic.cmd & 0xf0) {
	case NOTEOFF:
		midiprint(MPNote, "%8ld NoteOff %hd %hd %hd", e->time,
		    e->msg.noteoff.chn, e->msg.noteoff.note,
		    e->msg.noteoff.velocity);
		return;
	case NOTEON:
		if (e->msg.noteon.duration)
			midiprint(MPNote, "%8ld Note %hd %hd %hd %ld %hd", e->time,
			    e->msg.noteon.chn, e->msg.noteon.note,
			    e->msg.noteon.velocity,
			    e->msg.noteon.duration, e->msg.noteon.release);
		else
			midiprint(MPNote, "%8ld NoteOn %hd %hd %hd", e->time,
			    e->msg.noteon.chn, e->msg.noteon.note,
			    e->msg.noteon.velocity);
		return;
	case KEYPRESSURE:
		midiprint(MPNote, "%8ld KeyPressure %hd %hd %hd", e->time,
		    e->msg.keypressure.chn, e->msg.keypressure.note,
		    e->msg.keypressure.velocity);
		return;
	case CONTROLCHANGE:
		midiprint(MPNote, "%8ld ControlChange %hd %hd %hd", e->time,
		    e->msg.controlchange.chn,
		    e->msg.controlchange.controller,
		    e->msg.controlchange.value);
		return;
	case PROGRAMCHANGE:
		midiprint(MPNote, "%8ld ProgramChange %hd %hd", e->time,
		    e->msg.programchange.chn,
		    e->msg.programchange.program);
		return;
	case CHANNELPRESSURE:
		midiprint(MPNote, "%8ld ChannelPressure %hd %hd", e->time,
		    e->msg.channelpressure.chn,
		    e->msg.channelpressure.velocity);
		return;
	case PITCHWHEELCHANGE:
		midiprint(MPNote, "%8ld PitchWheelChange %hd %hd", e->time,
		    e->msg.pitchwheelchange.chn,
		    e->msg.pitchwheelchange.value);
		return;
	}

	switch (e->msg.generic.cmd) {
	case SYSTEMEXCLUSIVE:
		midiprint(MPNote, "%8ld SystemExclusive `%s'", e->time,
		    strdat(e->msg.systemexclusive.data));
		return;
	case SYSTEMEXCLUSIVECONT:
		midiprint(MPNote, "%8ld SystemExclusiveCont `%s'", e->time,
		    strdat(e->msg.systemexclusivecont.data));
		return;
	case META:
		midiprint(MPNote, "%8ld Meta %hd `%s'", e->time,
		    e->msg.meta.type, strdat(e->msg.meta.data));
		return;
	case SEQUENCENUMBER:
		midiprint(MPNote, "%8ld SequenceNumber %hu", e->time,
		    e->msg.sequencenumber.sequencenumber);
		return;
	case TEXT:
		midiprint(MPNote, "%8ld Text `%s'", e->time,
		    strdat(e->msg.text.text));
		return;
	case COPYRIGHTNOTICE:
		midiprint(MPNote, "%8ld CopyrightNotice `%s'", e->time,
		    strdat(e->msg.copyrightnotice.text));
		return;
	case TRACKNAME:
		midiprint(MPNote, "%8ld TrackName `%s'", e->time,
		    strdat(e->msg.trackname.text));
		return;
	case INSTRUMENTNAME:
		midiprint(MPNote, "%8ld InstrumentName `%s'", e->time,
		    strdat(e->msg.instrumentname.text));
		return;
	case LYRIC:
		midiprint(MPNote, "%8ld Lyric `%s'", e->time,
		    strdat(e->msg.lyric.text));
		return;
	case MARKER:
		midiprint(MPNote, "%8ld Marker `%s'", e->time,
		    strdat(e->msg.marker.text));
		return;
	case CUEPOINT:
		midiprint(MPNote, "%8ld CuePoint `%s'", e->time,
		    strdat(e->msg.cuepoint.text));
		return;
	case CHANNELPREFIX:
		midiprint(MPNote, "%8ld ChannelPrefix %hd", e->time,
		    e->msg.channelprefix.channel);
		return;
	case PORTPREFIX:
		midiprint(MPNote, "%8ld PortPrefix %hd", e->time,
		    e->msg.portprefix.port);
		return;
	case ENDOFTRACK:
		midiprint(MPNote, "%8ld EndOfTrack", e->time);
		return;
	case SETTEMPO:
		midiprint(MPNote, "%8ld SetTempo %ld", e->time,
		    e->msg.settempo.tempo);
		return;
	case SMPTEOFFSET:
		midiprint(MPNote, "%8ld SMPTEOffset %hd %hd %hd %hd %hd",
		    e->time, e->msg.smpteoffset.hours,
		    e->msg.smpteoffset.minutes,
		    e->msg.smpteoffset.seconds,
		    e->msg.smpteoffset.frames,
		    e->msg.smpteoffset.subframes);
		return;
	case TIMESIGNATURE:
		midiprint(MPNote, "%8ld TimeSignature %hd %hd %hd %hd",
		    e->time, e->msg.timesignature.nominator,
		    e->msg.timesignature.denominator,
		    e->msg.timesignature.clocksperclick,
		    e->msg.timesignature.ttperquarter);
		return;
	case KEYSIGNATURE:
		midiprint(MPNote, "%8ld KeySignature %hd %hd", e->time,
		    e->msg.keysignature.sharpsflats,
		    e->msg.keysignature.minor);
		return;
	case SEQUENCERSPECIFIC:
		midiprint(MPNote, "%8ld SequencerSpecific `%s'", e->time,
		    strdat(e->msg.sequencerspecific.data));
		return;
	}

	midiprint(MPNote, "%8ld Unknown %hu", e->time, e->msg.generic.cmd);
}

/* Print the track data of `s'. */
static void showtracks(Score *s, int flags) {
	MFEvent *e;
	long t;

	for (t = 0; t < s->ntrk; t++) {
		unsigned long ne = track_nevents(s->tracks[t]);

		track_rewind(s->tracks[t]);

		if (flags & SHOWTLENGTHS)
			midiprint(MPNote, "       %7lu", ne);
	}

	if (flags & SHOWEVENTS)
		for (t = 0; t < s->ntrk; t++)
			while ((e = track_step(s->tracks[t], 0)))
				printevent(e);
}

/* Delete all tracks that are not within the given range. */
static void adjusttracks(Score *s, long from, long to) {
	long t;

	/* Ensure that from and to are within the score's bounds. */
	if (to >= s->ntrk)
		to = s->ntrk - 1;

	if (from >= s->ntrk || from > to)
		return;

	for (t = 0; t < from; t++) {
		track_clear(s->tracks[t]);
		s->tracks[t] = s->tracks[t + from];
	}

	for (t = to + 1; t < s->ntrk; t++)
		track_clear(s->tracks[t]);

	s->ntrk = to + 1 - from;
}

/* Group matching NoteOn/NoteOff pairs. */
static void group(Score *s) {
	int t, n;
	for (t = 0; t < s->ntrk; t++)
		if ((n = pairNotes(s->tracks[t])) != 0)
			midiprint(MPWarn, "track %d: %d unmatched notes", t, n);
}

/* Ungroup matching NoteOn/NoteOff pairs and compress NoteOff events. */
static void ungroup(Score *s) {
	int t;
	for (t = 0; t < s->ntrk; t++) {
		(void) unpairNotes(s->tracks[t]);
		compressNoteOff(s->tracks[t], 0);
	}
}

/* Merge all the tracks of `s' into one. */
static void mergetracks(Score *s) {
	MFEvent *e;
	unsigned long t;

	for (t = 1; t < s->ntrk; t++) {
		track_rewind(s->tracks[t]);
		while ((e = track_step(s->tracks[t], 0)))
			if (!track_insert(s->tracks[0], e)) {
				midiprint(MPFatal, "%s", strerror(errno));
				exit(EXIT_FAILURE);
			}
			else
				e->msg.empty.type = EMPTY;

		track_clear(s->tracks[t]);
	}

	s->ntrk = 1;

	/* Delete all but the last End Of Track events. */
	track_rewind(s->tracks[0]);
	e = track_step(s->tracks[0], 1);
	assert(e != NULL && e->msg.endoftrack.type == ENDOFTRACK);
	while ((e = track_step(s->tracks[0], 1)))
		if (e->msg.endoftrack.type == ENDOFTRACK)
			track_delete(s->tracks[0]);
}

/*
 * For each track in `s', write a track header and all it's events into
 * the buffer. If `concat' is nonzero, only one header is written and
 * all tracks are appended.
 * Returns 1 on success, else 0.
 */
static int write_tracks(MBUF *b, Score *s, int concat) {
	long phdr = 0, ptrk = 0;
	MFEvent *e;
	long t;
	unsigned char running;

	if (s->ntrk < 1)
		return 1;

	for (t = 0; t < s->ntrk; t++) {
		long time = 0;
		errno = 0;

		if (t == 0 || !concat) {
			phdr = mbuf_pos(b);
			if ((t == 0 || !concat) && !write_MTrk(b, 0)) {
				midiprint(MPFatal, "%s", strerror(errno));
				return 0;
			}
			ptrk = mbuf_pos(b);
			running = 0;
		}

		track_rewind(s->tracks[t]);
		while ((e = track_step(s->tracks[t], 0))) {
			e->time -= time;
			time += e->time;

			/*
			 * If we are in concat mode, we only write the very last EOT
			 * event.
			 */
			if ((!concat || t == s->ntrk - 1 ||
				e->msg.endoftrack.type != ENDOFTRACK) &&
				!write_event(b, e, &running)) {
				if (errno)
					midiprint(MPFatal, "%s", strerror(errno));
				else
					midiprint(MPFatal, "writing event failed");
				return 0;
			}
		}

		if (!concat) {
			long p = mbuf_pos(b);
			mbuf_set(b, phdr);
			if (!write_MTrk(b, p - ptrk)) {
				midiprint(MPFatal, "%s", strerror(errno));
				return 0;
			}
			mbuf_set(b, p);
		}
	}

	return 1;
}

/* Handle one filespec. */
static int dofile(const char *spec, int flags) {
	FILE *f = stdin;
	static char _name[FILENAME_MAX];
	static char name[FILENAME_MAX];
	MBUF *b;
	Score _s, *s = &_s;
	int scorenum;

	/*
	 * Starting and end numbers of selected scores. If `sc1' is -1, all
	 * scores are selected and `sc0' is set to 0.
	 */
	long sc0 = 0, sc1 = -1;

	/* Dito, for track numbers. */
	long tr0 = 0, tr1 = -1;

	if (!spec)
		spec = "";

	/* Parse the spec. */
	if (sscanf(spec, "%[^@]@%lu-%lu.%lu-%lu", name, &sc0, &sc1, &tr0, &tr1) == 5)
		/* skip */;
	else if (sscanf(spec, "%[^@]@%lu-%lu.%lu", name, &sc0, &sc1, &tr0) == 4)
		tr1 = tr0;
	else if (sscanf(spec, "%[^@]@%lu-%lu", name, &sc0, &sc1) == 3)
		/* skip */;
	else if (sscanf(spec, "%[^@]@%lu", name, &sc0) == 2)
		sc1 = sc0;
	else if (sscanf(spec, "%[^@]@%lu.%lu-%lu", name, &sc0, &tr0, &tr1) == 4)
		sc1 = sc0;
	else if (sscanf(spec, "%[^@]@.%lu-%lu", name, &tr0, &tr1) == 3)
		/* skip */;
	else if (sscanf(spec, "%[^@]@.%lu", name, &tr0) == 2)
		tr1 = tr0;
	else
		/* XXX: check for truncation */
		strlcpy(name, spec, sizeof(name));

	if (!*name || !strcmp(name, "-")) {
		warnname = "-";
		*name = 0;
	} else {
		/* XXX: check for truncation */
		strlcpy(_name, name, sizeof(_name));
		warnname = _name;
	}

	midiprint_hook = print;

	if (*name && !(f = fopen(name, "rb"))) {
		midiprint(MPFatal, "%s", strerror(errno));
		return 1;
	}

	if (!(b = mbuf_new())) {
		midiprint(MPFatal, "%s", strerror(errno));
		fclose(f);
		return 1;
	}

	if (read_mbuf(b, f)) {
		fclose(f);
		mbuf_free(b);
		return 1;
	}

	fclose(f);

	error = 0;

	if (!(s = score_read(b))) {
		midiprint(MPFatal, "no headers or tracks found");
		mbuf_free(b);
		return 1;
	}

	scorenum = 0;

	if (sc1 < 0 || (sc0 <= scorenum && scorenum <= sc1)) {
		if (tr1 >= 0)
			adjusttracks(s, tr0, tr1);

		if (!(flags & UNGROUP))
			group(s);

		if (flags & MERGETRACKS)
			mergetracks(s);

		if (flags & SHOWHEADERS)
			midiprint(MPNote, "%s(%d): %7d %7d %7d",
			    warnname, scorenum, s->fmt, s->ntrk, s->div);
		else if (flags & (SHOWTLENGTHS | SHOWEVENTS))
			midiprint(MPNote, "%s(%d):", warnname, scorenum);

		if (!outdiv)
			outdiv = s->div;
		if (outformat < 0)
			outformat = s->fmt;

		showtracks(s, flags);

		if (outb) {
			ungroup(s);
			write_tracks(outb, s, flags & CONCATTRACKS);
			if (flags & CONCATTRACKS)
				outntrk++;
			else
				outntrk += s->ntrk;
		}

		score_clear(s);
	}

	scorenum++;

	while (mbuf_request(b, 1) && (s = score_read(b))) {
		if (sc1 < 0 || (sc0 <= scorenum && scorenum <= sc1)) {
			if (tr1 >= 0)
				adjusttracks(s, tr0, tr1);

			if (!(flags & UNGROUP))
				group(s);

			if (flags & MERGETRACKS)
				mergetracks(s);

			if (flags & SHOWHEADERS)
				midiprint(MPNote, "%s(%d): %7d %7d %7d",
				    warnname, scorenum, s->fmt, s->ntrk, s->div);
			else if (flags & (SHOWTLENGTHS | SHOWEVENTS))
				midiprint(MPNote, "%s(%d):", warnname, scorenum);

			if (!outdiv)
				outdiv = s->div;
			if (outformat < 0)
				outformat = s->fmt;

			showtracks(s, flags);

			if (outb) {
				ungroup(s);
				write_tracks(outb, s, flags & CONCATTRACKS);
				if (flags & CONCATTRACKS)
					outntrk++;
				else
					outntrk += s->ntrk;
			}

			score_clear(s);
		}

		scorenum++;
	}

	if (mbuf_request(b, 1))
		midiprint(MPWarn, "garbage at end of input");

	mbuf_free(b);

	return error ? 1 : 0;
}

int main(int argc, char *argv[]) {
	unsigned long p = 0;
	int opt;
	int error = 0;
	int flags = 0;

	FILE *outf;
	char *outname = NULL;

	/* Parse command line arguments. */
	while ((opt = getopt(argc, argv, ":hleuqnmo:012cfd:")) != -1)
		switch (opt) {
		case 'h':
			flags |= SHOWHEADERS;
			break;
		case 'l':
			flags |= SHOWTLENGTHS;
			break;
		case 'e':
			flags |= SHOWEVENTS;
			break;
		case 'u':
			flags |= UNGROUP;;
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
			if (sscanf(optarg, "%d", &outdiv) != 1 || !outdiv)
				usage();
			break;
		default:
			usage();
			break;
		}

	argc -= optind;
	argv += optind;

	if (outname) {
		if (!(outb = mbuf_new())) {
			perror(outname);
			return EXIT_FAILURE;
		}

		if (!(flags & NOHEADER) &&
		    /* This will be rewritten later to insert the correct values. */
		    !write_MThd(outb, 0, 0, 0)) {
			perror(outname);
			return EXIT_FAILURE;
		}

		p = mbuf_pos(outb);
	}

	if (!argc)
		error = dofile(NULL, flags);
	else
		while (argc--)
			error |= dofile(*argv++, flags);

	if (outb)
		p = mbuf_pos(outb) - p;

	if (error)
		return EXIT_FAILURE;
	else if (outb && !(flags & NOHEADER) && mbuf_set(outb, 0) != 0) {
		perror("rewinding buffer");
		return EXIT_FAILURE;
	} else if (outb && !(flags & NOHEADER) &&
			!write_MThd(outb, outformat, outntrk, outdiv)) {
		perror(outname);
		return EXIT_FAILURE;
	} else if (outb && (flags & CONCATTRACKS) && !write_MTrk(outb, p)) {
		perror(outname);
		return EXIT_FAILURE;
	} else if (outb && !(outf = fopen(outname, "wb"))) {
		perror(outname);
		return EXIT_FAILURE;
	} else if (outb && write_mbuf(outb, outf) < 0) {
		perror(outname);
		fclose(outf);
		return EXIT_FAILURE;
	} else if (outb) {
		fclose(outf);
		return EXIT_SUCCESS;
	} else
		return EXIT_SUCCESS;
}
