/*
 * $Id: event.h,v 1.3 1996/04/06 23:00:10 kilian Exp $
 *
 * Read midi file messages and events.
 *
 * $Log: event.h,v $
 * Revision 1.3  1996/04/06 23:00:10  kilian
 * Added new internal message types `link', `empty' and `warning'.
 *
 * Revision 1.2  1996/04/02  23:23:12  kilian
 * Fix: command and channel nibble swapped in channel voice messages.
 *
 * Revision 1.1  1996/04/01  19:10:57  kilian
 * Initial revision
 *
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
  NOTEOFF             = 0x80,
  NOTEON              = 0x90,
  KEYPRESSURE         = 0xa0,
  CONTROLCHANGE       = 0xb0,
  PROGRAMCHANGE       = 0xc0,
  CHANNELPRESSURE     = 0xd0,
  PITCHWHEELCHANGE    = 0xe0,

  /* Sysex messages */
  SYSTEMEXCLUSIVE     = 0xf0,
  SYSTEMEXCLUSIVECONT = 0xf7,

  /* Meta messages */
  META                = 0xff
} MFMessageType;


/*
 * Meta message types
 */
typedef enum {
  SEQUENCENUMBER    = 0x00,
  TEXT              = 0x01,
  COPYRIGHTNOTICE   = 0x02,
  TRACKNAME         = 0x03,   /* This is also used as sequence name. */
  INSTRUMENTNAME    = 0x04,
  LYRIC             = 0x05,
  MARKER            = 0x06,
  CUEPOINT          = 0x07,
  ENDOFTRACK        = 0x2f,
  SETTEMPO          = 0x51,
  SMPTEOFFSET       = 0x54,
  TIMESIGNATURE     = 0x58,
  KEYSIGNATURE      = 0x59,
  SEQUENCERSPECIFIC = 0x7f,

  /* Internal messages. */
  LINK              = 0x70,
  EMPTY             = 0x71,
  WARNING           = 0x72
} MFMetaType;


/*
 * Structures for all of the message types.
 * Each of this contains the status byte as it's first element. For
 * channel voice messages, this is further split into two 4-bit
 * quantities containing the command (upper nibble) and the channel
 * (lower nibble).
 * The first structure define a `generic' message containing just a status
 * byte.
 */
typedef struct {
  unsigned char cmd;
} MFGeneric;

typedef struct {
  unsigned char chn:4, cmd:4;
  unsigned char note, velocity;
} MFNoteOff;

typedef struct {
  unsigned char chn:4, cmd:4;
  unsigned char note, velocity;
} MFNoteOn;

typedef struct {
  unsigned char chn:4, cmd:4;
  unsigned char note, velocity;
} MFKeyPressure;

typedef struct {
  unsigned char chn:4, cmd:4;
  unsigned char controller, value;
} MFControlChange;

typedef struct {
  unsigned char chn:4, cmd:4;
  unsigned char program;
} MFProgramChange;

typedef struct {
  unsigned char chn:4, cmd:4;
  unsigned char velocity;
} MFChannelPressure;

typedef struct {
  unsigned char chn:4, cmd:4;
  short value;                /* Within the midi file, this is
                               * a 2x7bit quantity. */
} MFPitchWheelChange;


/*
 * For sysex messages, the data is stored elsewhere. The structure itself
 * only contains a pointer to the data field.
 */
typedef struct {
  unsigned char cmd;
  void *data;
} MFSystemExclusive;

typedef struct {
  unsigned char cmd;
  void *data;
} MFSystemExclusiveCont;

typedef struct {
  unsigned char cmd;
  unsigned char type;
  void *data;
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
  unsigned char type;
  short sequencenumber;
} MFSequenceNumber;

typedef struct {
  unsigned char type;
  void *text;
} MFText;

typedef struct {
  unsigned char type;
  void *text;
} MFCopyrightNotice;

typedef struct {
  unsigned char type;
  void *text;
} MFTrackName;

typedef struct {
  unsigned char type;
  void *text;
} MFInstrumentName;

typedef struct {
  unsigned char type;
  void *text;
} MFLyric;

typedef struct {
  unsigned char type;
  void *text;
} MFMarker;

typedef struct {
  unsigned char type;
  void *text;
} MFCuePoint;

typedef struct {
  unsigned char type;
} MFEndOfTrack;

typedef struct {
  unsigned char type;
  long tempo;         /* 24 bit quantity; microseconds per midi quarternote */
} MFSetTempo;

typedef struct {
  unsigned char type;
  unsigned char hours, minutes, seconds;
  unsigned char frames, subframes;
} MFSMPTEOffset;

typedef struct {
  unsigned char type;
  unsigned char nominator, denominator;
  unsigned char clocksperclick, ttperquarter;
} MFTimeSignature;

typedef struct {
  unsigned char type;
  char sharpsflats, minor;  /* Note: this is signed. */
} MFKeySignature;

typedef struct {
  unsigned char type;
  void *data;
} MFSequencerSpecific;


/*
 * Internal messages. These should never be seen by the application.
 */
typedef struct {
  unsigned char type;
  void *subtrack;
} MFLink;

typedef struct {
  unsigned char type;
} MFEmpty;

typedef struct {
  unsigned char type;
  unsigned char code;
  void *text;
} MFWarning;


/*
 * Now comes the big message union.
 * To get the actual type of an message, one should first look at
 * *.generic.cmd.
 */
typedef union {
  MFGeneric             generic;
  MFNoteOff             noteoff;
  MFNoteOn              noteon;
  MFKeyPressure         keypressure;
  MFControlChange       controlchange;
  MFProgramChange       programchange;
  MFChannelPressure     channelpressure;
  MFPitchWheelChange    pitchwheelchange;
  MFSystemExclusive     systemexclusive;
  MFSystemExclusiveCont systemexclusivecont;
  MFMeta                meta;
  MFSequenceNumber      sequencenumber;
  MFText                text;
  MFCopyrightNotice     copyrightnotice;
  MFTrackName           trackname;
  MFInstrumentName      instrumentname;
  MFLyric               lyric;
  MFMarker              marker;
  MFCuePoint            cuepoint;
  MFEndOfTrack          endoftrack;
  MFSetTempo            settempo;
  MFSMPTEOffset         smpteoffset;
  MFTimeSignature       timesignature;
  MFKeySignature        keysignature;
  MFSequencerSpecific   sequencerspecific;
  MFLink                link;
  MFEmpty               empty;
  MFWarning             warning;
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
 * An *event* is a message together with the message's time.
 */
typedef struct {
  long time;
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
