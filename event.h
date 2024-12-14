/*
 * Read midi file messages and events.
 */

#ifndef __EVENT_H__
#define __EVENT_H__

#include "buffer.h"

/*
 * Midifile message types.
 * For channel voice commands, the lower nibble is set to 0.
 */
typedef enum {
	/* Channel voice messages */
	NOTEOFF			= 0x80,
	NOTEON			= 0x90,
	KEYPRESSURE		= 0xa0,
	CONTROLCHANGE		= 0xb0,
	PROGRAMCHANGE		= 0xc0,
	CHANNELPRESSURE		= 0xd0,
	PITCHWHEELCHANGE	= 0xe0,

	/* Sysex messages */
	SYSTEMEXCLUSIVE		= 0xf0,
	SYSTEMEXCLUSIVECONT	= 0xf7,

	/* Meta messages */
	META			= 0xff
} MFMessageType;

/* Meta message types */
typedef enum {
	SEQUENCENUMBER		= 0x00,
	TEXT			= 0x01,
	COPYRIGHTNOTICE		= 0x02,
	TRACKNAME		= 0x03,	/* This is also used as sequence name. */
	INSTRUMENTNAME		= 0x04,
	LYRIC			= 0x05,
	MARKER			= 0x06,
	CUEPOINT		= 0x07,
	CHANNELPREFIX		= 0x20,
	PORTPREFIX		= 0x21,	/* Source (standard) unknown. */
	ENDOFTRACK		= 0x2f,
	SETTEMPO		= 0x51,
	SMPTEOFFSET		= 0x54,
	TIMESIGNATURE		= 0x58,
	KEYSIGNATURE		= 0x59,
	SEQUENCERSPECIFIC	= 0x7f,

	/* Internal messages. */
	LINK			= 0x70,
	EMPTY			= 0x71,
	WARNING			= 0x72
} MFMetaType;

#define CHN(m) ((m.cmd) & 0x0f)

/*
 * Structures for all of the message types.
 */

typedef struct {
	unsigned char note, velocity;
} MFNoteOff;

typedef struct {
	unsigned char note, velocity;

	/* These are for a `Note' message that combines NoteOn and NoteOff.
	 * If duration is 0, it's a single NoteOn message.
	 * It's an error to try to write a `Note' message.
	 */
	long duration;
	unsigned char release;;
} MFNoteOn;

typedef struct {
	unsigned char note, velocity;
} MFKeyPressure;

typedef struct {
	unsigned char controller, value;
} MFControlChange;

typedef struct {
	unsigned char program;
} MFProgramChange;

typedef struct {
	unsigned char velocity;
} MFChannelPressure;

typedef struct {
	unsigned char lsb;
	unsigned char msb;
} MFPitchWheelChange;

/*
 * For sysex messages, the data is stored elsewhere. The structure itself
 * only contains a pointer to the data field.
 */
typedef struct {
	struct vld *data;
} MFSystemExclusive;

typedef struct {
	struct vld *data;
} MFSystemExclusiveCont;

typedef struct {
	struct vld *data;
} MFMeta;

/*
 * To avoid clutter, meta messages are flattened to the level of normal
 * messages instead of storing just a data pointer. Nevertheless several
 * meta messages will *contain* a pointer to data (e.g. text events).
 * To distinguish normal messages from meta messages, the latter will have
 * their type as command byte, i.e. bit 7 of cmd is cleared.
 * Note that meta messages of unknown type will appear as normal
 * messages, i.e. will have type `MFMeta' and command `META'.
 */
typedef struct {
	short sequencenumber;
} MFSequenceNumber;

typedef struct {
	struct vld *text;
} MFText;

typedef struct {
	struct vld *text;
} MFCopyrightNotice;

typedef struct {
	struct vld *text;
} MFTrackName;

typedef struct {
	struct vld *text;
} MFInstrumentName;

typedef struct {
	struct vld *text;
} MFLyric;

typedef struct {
	struct vld *text;
} MFMarker;

typedef struct {
	struct vld *text;
} MFCuePoint;

typedef struct {
	unsigned char channel;
} MFChannelPrefix;

typedef struct {
	unsigned char port;
} MFPortPrefix;

typedef struct {
} MFEndOfTrack;

typedef struct {
	unsigned long tempo;	/* 24 bit quantity; microseconds per
				 * midi quarternote */
} MFSetTempo;

typedef struct {
	unsigned char hours, minutes, seconds;
	unsigned char frames, subframes;
} MFSMPTEOffset;

typedef struct {
	unsigned char nominator, denominator;
	unsigned char clocksperclick, ttperquarter;
} MFTimeSignature;

typedef struct {
	char sharpsflats, minor;  /* Note: this is signed. */
} MFKeySignature;

typedef struct {
	struct vld *data;
} MFSequencerSpecific;

/*
 * Internal messages. These should never be seen by the application.
 */
typedef struct {
	struct vld *subtrack;
} MFLink;

typedef struct {
} MFEmpty;

typedef struct {
	unsigned char code;
	struct vld *text;
} MFWarning;

/*
 * Now comes the big message union.
 */
typedef struct {
	unsigned char cmd;	/* For meta messages, this is the type. */
	union {
		MFNoteOff		noteoff;
		MFNoteOn		noteon;
		MFKeyPressure		keypressure;
		MFControlChange		controlchange;
		MFProgramChange		programchange;
		MFChannelPressure	channelpressure;
		MFPitchWheelChange	pitchwheelchange;
		MFSystemExclusive	systemexclusive;
		MFSystemExclusiveCont	systemexclusivecont;
		MFMeta			meta;
		MFSequenceNumber	sequencenumber;
		MFText			text;
		MFCopyrightNotice	copyrightnotice;
		MFTrackName		trackname;
		MFInstrumentName	instrumentname;
		MFLyric			lyric;
		MFMarker		marker;
		MFCuePoint		cuepoint;
		MFChannelPrefix		channelprefix;
		MFPortPrefix		portprefix;
		MFEndOfTrack		endoftrack;
		MFSetTempo		settempo;
		MFSMPTEOffset		smpteoffset;
		MFTimeSignature		timesignature;
		MFKeySignature		keysignature;
		MFSequencerSpecific	sequencerspecific;
		MFLink			link;
		MFEmpty			empty;
		MFWarning		warning;
	};
} MFMessage;

/*
 * Get the next message from the buffer and store it at `msg'. For variable
 * sized messages, the necessary memory is automatically allocated.
 * `rs' points to a location that contains the current channel voice
 * status byte and is used to support running status. It is updated if
 * necessary. To reset running status, set `*rs' to 0 before calling
 * this function.
 * If something goes wrong, return 0, else 1.
 */
int read_message(MBUF *b, MFMessage *msg, unsigned char *rs);

/*
 * Write the event into the buffer. If `rs' is nonzero, it is used to
 * support running status. In contrast to `read_message', `rs' may be
 * NULL, which switches running status off.
 * Returns 1 on success and 0 on errors.
 */
int write_message(MBUF *b, MFMessage *msg, unsigned char *rs);

/*
 * To simplify cleanup, this function frees allocated data if `msg' is a
 * variable sized message.
 */
void clear_message(MFMessage *msg);

/*
 * An *event* is a message together with the message's (absolute,
 * i.e. relative to the start of the track) time.
 */
typedef struct {
	unsigned long time;
	MFMessage msg;
} MFEvent;

/*
 * Get the next event, i.e. the next (delta) time and message.
 * Parameters and return value are the same as of `read_message'.
 */
int read_event(MBUF *b, MFEvent *ev, unsigned char *rs);

/*
 * Write the next event.
 * Parameters and return values as of `write_message'.
 */
int write_event(MBUF *b, MFEvent *ev, unsigned char *rs);

#endif /* __EVENT_H__ */
