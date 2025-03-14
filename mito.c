/* mito --- the midi tool */
/* XXX: the order of events (when using -e) is not deterministic! */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <sndio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include "chunk.h"
#include "event.h"
#include "print.h"
#include "score.h"
#include "util.h"
#include "vld.h"

static void usage(void) {
	fputs("usage: mito [-hleuqtnm012c] [-o file] [-d div] {[file][@sl]}...\n"
	    "overall options:\n"
	    "    -h:  show score headers\n"
	    "    -l:  show track lengths\n"
	    "    -e:  show events\n"
	    "    -u:  don't group noteon/noteoff events\n"
	    "    -q:  accumulative(1-3): no warning, midi errors, other errors\n"
	    "    -o:  write resulting output to `file'\n"
	    "    -t:  print events in real time\n"
	    "    -p:  play events to default midi device\n"
	    "         (implies -u, -m, and -t)\n"
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

/* Command line flags. */
static int f_showheaders = 0;
static int f_showtlengths = 0;
static int f_showevents = 0;
static int f_play = 0;
static int f_noheader = 0;
static int f_mergetracks = 0;
static int f_concattracks = 0;
static int f_fixgroups = 0;
static int f_ungroup = 0;
static int f_timed = 0;

/* The filename to print in warning and error messages. */
static char *warnname = NULL;

static int quiet = 0;
static int error = 0;

static int outformat = -1;
static int outdiv = 0;
static int outntrk = 0;

static volatile sig_atomic_t stop = 0;

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
static char *strdat(struct vld *vld) {
	static char buf[1024 * 4 + 1];
	long length = vld->length;
	int trunc = length > 1024;
	if (trunc)
		length = 1024 - 3;
	strvisx(buf, vld->data, length, VIS_CSTYLE);
	if (trunc)
		strlcat (buf, "...", sizeof(buf));
	return buf;
}

static void printevent(MFEvent *e) {
	static unsigned long lastt = 0;
	unsigned long dt, t;
	dt = e->time < lastt ? 0 : e->time - lastt;
	lastt = e->time;
	t = dt;
	switch (e->msg.cmd & 0xf0) {
	case NOTEOFF:
		midiprint(MPNote, "%8ld NoteOff %hd %hd %hd", t,
		    CHN(e->msg), e->msg.noteoff.note,
		    e->msg.noteoff.velocity);
		return;
	case NOTEON:
		if (e->msg.noteon.duration)
			midiprint(MPNote, "%8ld Note %hd %hd %hd %ld %hd", t,
			    CHN(e->msg), e->msg.noteon.note,
			    e->msg.noteon.velocity,
			    e->msg.noteon.duration, e->msg.noteon.release);
		else
			midiprint(MPNote, "%8ld NoteOn %hd %hd %hd", t,
			    CHN(e->msg), e->msg.noteon.note,
			    e->msg.noteon.velocity);
		return;
	case KEYPRESSURE:
		midiprint(MPNote, "%8ld KeyPressure %hd %hd %hd", t,
		    CHN(e->msg), e->msg.keypressure.note,
		    e->msg.keypressure.velocity);
		return;
	case CONTROLCHANGE:
		midiprint(MPNote, "%8ld ControlChange %hd %hd %hd", t,
		    CHN(e->msg),
		    e->msg.controlchange.controller,
		    e->msg.controlchange.value);
		return;
	case PROGRAMCHANGE:
		midiprint(MPNote, "%8ld ProgramChange %hd %hd", t,
		    CHN(e->msg),
		    e->msg.programchange.program);
		return;
	case CHANNELPRESSURE:
		midiprint(MPNote, "%8ld ChannelPressure %hd %hd", t,
		    CHN(e->msg),
		    e->msg.channelpressure.velocity);
		return;
	case PITCHWHEELCHANGE:
		midiprint(MPNote, "%8ld PitchWheelChange %hd %hd", t,
		    CHN(e->msg),
		    e->msg.pitchwheelchange.msb << 7 |
		    e->msg.pitchwheelchange.lsb);
		return;
	}

	switch (e->msg.cmd) {
	case SYSTEMEXCLUSIVE:
		midiprint(MPNote, "%8ld SystemExclusive `%s'", t,
		    strdat(e->msg.systemexclusive.data));
		return;
	case SYSTEMEXCLUSIVECONT:
		midiprint(MPNote, "%8ld SystemExclusiveCont `%s'", t,
		    strdat(e->msg.systemexclusivecont.data));
		return;
	case META:
		midiprint(MPNote, "%8ld Meta %hd `%s'", t,
		    e->msg.cmd, strdat(e->msg.meta.data));
		return;
	case SEQUENCENUMBER:
		midiprint(MPNote, "%8ld SequenceNumber %hu", t,
		    e->msg.sequencenumber.sequencenumber);
		return;
	case TEXT:
		midiprint(MPNote, "%8ld Text `%s'", t,
		    strdat(e->msg.text.text));
		return;
	case COPYRIGHTNOTICE:
		midiprint(MPNote, "%8ld CopyrightNotice `%s'", t,
		    strdat(e->msg.copyrightnotice.text));
		return;
	case TRACKNAME:
		midiprint(MPNote, "%8ld TrackName `%s'", t,
		    strdat(e->msg.trackname.text));
		return;
	case INSTRUMENTNAME:
		midiprint(MPNote, "%8ld InstrumentName `%s'", t,
		    strdat(e->msg.instrumentname.text));
		return;
	case LYRIC:
		midiprint(MPNote, "%8ld Lyric `%s'", t,
		    strdat(e->msg.lyric.text));
		return;
	case MARKER:
		midiprint(MPNote, "%8ld Marker `%s'", t,
		    strdat(e->msg.marker.text));
		return;
	case CUEPOINT:
		midiprint(MPNote, "%8ld CuePoint `%s'", t,
		    strdat(e->msg.cuepoint.text));
		return;
	case CHANNELPREFIX:
		midiprint(MPNote, "%8ld ChannelPrefix %hd", t,
		    e->msg.channelprefix.channel);
		return;
	case PORTPREFIX:
		midiprint(MPNote, "%8ld PortPrefix %hd", t,
		    e->msg.portprefix.port);
		return;
	case ENDOFTRACK:
		midiprint(MPNote, "%8ld EndOfTrack", t);
		return;
	case SETTEMPO:
		midiprint(MPNote, "%8ld SetTempo %ld (%ld bpm)", t,
		    e->msg.settempo.tempo,
		    60000000 / e->msg.settempo.tempo);
		return;
	case SMPTEOFFSET:
		midiprint(MPNote, "%8ld SMPTEOffset %hd %hd %hd %hd %hd",
		    t, e->msg.smpteoffset.hours,
		    e->msg.smpteoffset.minutes,
		    e->msg.smpteoffset.seconds,
		    e->msg.smpteoffset.frames,
		    e->msg.smpteoffset.subframes);
		return;
	case TIMESIGNATURE:
		midiprint(MPNote, "%8ld TimeSignature %hd %hd %hd %hd",
		    t, e->msg.timesignature.nominator,
		    e->msg.timesignature.denominator,
		    e->msg.timesignature.clocksperclick,
		    e->msg.timesignature.ttperquarter);
		return;
	case KEYSIGNATURE:
		midiprint(MPNote, "%8ld KeySignature %hd %hd", t,
		    e->msg.keysignature.sharpsflats,
		    e->msg.keysignature.minor);
		return;
	case SEQUENCERSPECIFIC:
		midiprint(MPNote, "%8ld SequencerSpecific `%s'", t,
		    strdat(e->msg.sequencerspecific.data));
		return;
	}

	midiprint(MPNote, "%8ld Unknown %hu", t, e->msg.cmd);
}

static void playevent(struct mio_hdl *hdl, MFEvent *e) {
	unsigned char buf[4];
	size_t n = 0;
	buf[n++] = e->msg.cmd;
	/* No SysEx for now. */
	switch (e->msg.cmd & 0xf0) {
	case PROGRAMCHANGE:
		buf[n++] = e->msg.programchange.program;
		break;
	case PITCHWHEELCHANGE:
		/* Beware of the byte order! (LSB first) */
		buf[n++] = e->msg.pitchwheelchange.lsb;
		buf[n++] = e->msg.pitchwheelchange.msb;
		break;
	case KEYPRESSURE:
		buf[n++] = e->msg.keypressure.note;
		buf[n++] = e->msg.keypressure.velocity;
		break;
	case CHANNELPRESSURE:
		buf[n++] = e->msg.channelpressure.velocity;
		break;
	case NOTEOFF:
		buf[n++] = e->msg.noteoff.note;
		buf[n++] = e->msg.noteoff.velocity;
		break;
	case NOTEON:
		buf[n++] = e->msg.noteon.note;
		buf[n++] = e->msg.noteon.velocity;
		break;
	case CONTROLCHANGE:
		buf[n++] = e->msg.controlchange.controller;
		buf[n++] = e->msg.controlchange.value;
		break;
	default:
		n = 0;
		break;
	}
	if (!n)
		return;
	if (mio_write(hdl, buf, n) != n)
		err(1, NULL);
}

static void stopplay(int sig) {
	stop = 1;
}

/* XXX: track channel states and write ordinary noteoff messages for
 * all active notes.
 */
static void shutup(struct mio_hdl *hdl) {
	int i;
	MFEvent e;
	e.time = 0;

	for (i = 0; i < 16; i++) {
		e.msg.cmd = CONTROLCHANGE | i;
		/* Reset all controllers. */
		e.msg.controlchange.controller = 121;
		e.msg.controlchange.value = 0;
		playevent(hdl, &e);
		/* All notes off. */
		e.msg.controlchange.controller = 123;
		playevent(hdl, &e);
	}
}

/* Sleep for the given division, tempo and delta time. */
static void msleep(int div, unsigned long tempo, unsigned long dt) {
	struct timespec tmo, now;
	static struct timespec then = {0, 0};
	tmo.tv_sec = tempo * dt / div / 1000000;
	tmo.tv_nsec = 1000 * tempo * dt / div % 1000000000;
#ifdef DEBUG
	printf("tmo:\t%32lld.%09ld\n", tmo.tv_sec, tmo.tv_nsec);
#endif
	/* Try to compensate any delay occurred between this and the
	 * last msleep().
	 */
	if (clock_gettime(CLOCK_MONOTONIC, &now))
		err(1, NULL);
	if (then.tv_sec == 0 || then.tv_nsec == 0)
		then = now;
	timespecadd(&tmo, &then, &tmo);
	timespecsub(&tmo, &now, &tmo);
	timespecadd(&now, &tmo, &then);
#ifdef DEBUG
	printf("\t%32lld.%09ld\n", tmo.tv_sec, tmo.tv_nsec);
#endif
	if (tmo.tv_sec < 0 || tmo.tv_nsec < 0)
		return;
	if ((nanosleep(&tmo, NULL)) == -1 && errno != EINTR)
		err(1, NULL);
}

/* Output the track data of `s'. */
static void showtracks(Score *s) {
	MFEvent *e;
	struct mio_hdl *hdl;
	unsigned long tempo = 500000;	/* 120 bpm */
	long t;

	for (t = 0; t < s->ntrk; t++) {
		unsigned long ne = track_nevents(s->tracks[t]);

		track_rewind(s->tracks[t]);

		if (f_showtlengths)
			midiprint(MPNote, "       %7lu", ne);
	}

	if (!f_showevents && !f_play)
		return;

	if (f_play && !(hdl = mio_open(MIO_PORTANY, MIO_OUT, 0)))
		errx(1, "failed to open midi port");

	for (t = 0; !stop && t < s->ntrk; t++) {
		unsigned long lastt = 0;
		while (!stop && (e = track_step(s->tracks[t], 0))) {
			unsigned long dt = e->time - lastt;
			lastt = e->time;
			/* XXX: ensure that ENDOFTRACK is really the
			 * last event of a track at load time and then
			 * just terminate this loop on ENDOFTRACK.
			 */
			if (f_timed && dt &&
			    e->msg.cmd != ENDOFTRACK)
				msleep(s->div, tempo, dt);
			if (e->msg.cmd == SETTEMPO)
				tempo = e->msg.settempo.tempo;
			if (f_showevents)
				printevent(e);
			if (f_play)
				playevent(hdl, e);
		}
	}
	if (stop)
		puts("");

	if (f_play) {
		shutup(hdl);
		mio_close(hdl);
	}
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
/* XXX: there are smf files out in the wild containing the same event at
 * the same time. Those events should be removed (except for the first
 * occurence).
 */
static void mergetracks(Score *s) {
	MFEvent *e;
	unsigned long t;

	for (t = 1; t < s->ntrk; t++) {
		track_rewind(s->tracks[t]);
		while ((e = track_step(s->tracks[t], 0))) {
			if (!track_insert(s->tracks[0], e)) {
				midiprint(MPFatal, "%s", strerror(errno));
				exit(EXIT_FAILURE);
			}
			else
				e->msg.cmd = EMPTY;
		}
		track_clear(s->tracks[t]);
	}

	s->ntrk = 1;

	/* Delete all but the last End Of Track events. */
	track_rewind(s->tracks[0]);
	e = track_step(s->tracks[0], 1);
	assert(e != NULL && e->msg.cmd == ENDOFTRACK);
	while ((e = track_step(s->tracks[0], 1)))
		if (e->msg.cmd == ENDOFTRACK)
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
		unsigned long time = 0;
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
			time += e->time -= time;

			/*
			 * If we are in concat mode, we only write the very last EOT
			 * event.
			 */
			if ((!concat || t == s->ntrk - 1 ||
				e->msg.cmd != ENDOFTRACK) &&
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
static int dofile(const char *spec) {
	FILE *f = stdin;
	static char _name[FILENAME_MAX];
	static char name[FILENAME_MAX];
	MBUF *b;
	Score *s;
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

	for (scorenum = 0; (sc1 < 0 || scorenum <= sc1) && (s = score_read(b)); scorenum++) {
		if (sc1 >= 0 && (sc0 > scorenum || scorenum > sc1))
			continue;

		if (tr1 >= 0)
			adjusttracks(s, tr0, tr1);

		if (!f_ungroup)
			group(s);

		if (f_mergetracks)
			mergetracks(s);

		if (f_showheaders)
			midiprint(MPNote, "%s(%d): %7d %7d %7d",
			    warnname, scorenum, s->fmt, s->ntrk, s->div);
		else if (f_showtlengths || f_showevents)
			midiprint(MPNote, "%s(%d):", warnname, scorenum);

		if (!outdiv)
			outdiv = s->div;
		if (outformat < 0)
			outformat = s->fmt;

		showtracks(s);

		if (outb) {
			ungroup(s);
			write_tracks(outb, s, f_concattracks);
			if (f_concattracks)
				outntrk++;
			else
				outntrk += s->ntrk;
		}

		score_clear(s);
	}

	if (!s && scorenum == 0) {
		midiprint(MPFatal, "no headers or tracks found");
		mbuf_free(b);
		return 1;
	}

	/* XXX: this is also triggered when processing a file containing
	 * two concatenated SMF scores where the second one contains chunk
	 * with an unknown type, probably because search_chunk() resets
	 * the position in this case.
	 */
	if ((sc1 < 0 || scorenum <= sc1) && mbuf_request(b, 1))
		midiprint(MPWarn, "garbage at end of input");

	mbuf_free(b);

	return error ? 1 : 0;
}

int main(int argc, char *argv[]) {
	unsigned long p = 0;
	int opt;
	int error = 0;

	FILE *outf;
	char *outname = NULL;

	/* Parse command line arguments. */
	while ((opt = getopt(argc, argv, ":hleuqtnmo:p012cfd:")) != -1)
		switch (opt) {
		case 'h':
			f_showheaders = 1;
			break;
		case 'l':
			f_showtlengths = 1;
			break;
		case 'e':
			f_showevents = 1;
			break;
		case 'u':
			f_ungroup = 1;
			break;
		case 'f':
			f_fixgroups = 1;
			break;
		case 'q':
			quiet++;
			break;
		case 't':
			f_timed = 1;
			break;
		case 'n':
			f_noheader = 1;
			break;
		case 'm':
			f_mergetracks = 1;
			break;
		case 'o':
			outname = optarg;
			break;
		case 'p':
			f_play = f_ungroup = f_mergetracks = f_timed = 1;
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
			f_concattracks = 1;
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

		if (!f_noheader &&
		    /* This will be rewritten later to insert the correct values. */
		    !write_MThd(outb, 0, 0, 0)) {
			perror(outname);
			return EXIT_FAILURE;
		}

		p = mbuf_pos(outb);
	}

	if (f_play && signal(SIGINT, stopplay) == SIG_ERR)
		err(1, NULL);
	if (f_play && signal(SIGTERM, stopplay) == SIG_ERR)
		err(1, NULL);

	if (!argc)
		error = dofile(NULL);
	else
		while (argc--)
			error |= dofile(*argv++);

	if (outb)
		p = mbuf_pos(outb) - p;

	if (error)
		return EXIT_FAILURE;
	else if (outb && !f_noheader && mbuf_set(outb, 0) != 0) {
		perror("rewinding buffer");
		return EXIT_FAILURE;
	} else if (outb && !f_noheader &&
			!write_MThd(outb, outformat, outntrk, outdiv)) {
		perror(outname);
		return EXIT_FAILURE;
	} else if (outb && f_concattracks && !write_MTrk(outb, p)) {
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
