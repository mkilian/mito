/*
 * $Id: event.c,v 1.4 1996/04/03 14:26:57 kilian Exp $
 *
 * Read midi file messages and events.
 *
 * $Log: event.c,v $
 * Revision 1.4  1996/04/03 14:26:57  kilian
 * Many fixes in write_event.
 *
 * Revision 1.3  1996/04/02  23:28:40  kilian
 * Treat writing of unknown message types as fatal error.
 *
 * Revision 1.2  1996/04/02  10:19:57  kilian
 * Adapted changes of the print functions.
 *
 * Revision 1.1  1996/04/01  19:11:06  kilian
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "print.h"
#include "event.h"
#include "vld.h"


/*
 * This converts a general meta message into a specific message, if
 * possible. If conversion into a fixed size message is possible, the
 * allocated data will be freed automatically.
 * If something goes wrong, it frees the allocated data and return 0,
 * else it returns 1.
 */
static int convert_meta(MFMessage *msg)
{
  void *vld = msg->meta.data;
  long length = vld_size(vld);
  const unsigned char *data = vld_data(vld);
  int result = 0;

  switch(msg->meta.type)
    {
      case SEQUENCENUMBER:
        if(length < 2)
          {
            midiprint(MPError, "sequencenumber: too short data");
            break;
          }
        if(length > 2)
          midiprint(MPWarn, "sequencenumber: long data");

        msg->sequencenumber.type = msg->meta.type;
        msg->sequencenumber.sequencenumber = data[0] << 8 | data[1];
        result = 1;
        break;
      case TEXT:
      case COPYRIGHTNOTICE:
      case TRACKNAME:
      case INSTRUMENTNAME:
      case LYRIC:
      case MARKER:
      case CUEPOINT:
        msg->text.type = msg->meta.type;
        msg->text.text = vld;
        vld = NULL;
        result = 1;
        break;
      case ENDOFTRACK:
        if(length > 0)
          midiprint(MPWarn, "end of track: long data");

        msg->endoftrack.type = msg->meta.type;
        result = 1;
        break;
      case SETTEMPO:
        if(length < 3)
          {
            midiprint(MPError, "set tempo: too short data");
            break;
          }
        if(length > 3)
          midiprint(MPWarn, "set tempo: long data");

        msg->settempo.type = msg->meta.type;
        msg->settempo.tempo = data[0] << 16 | data[1] << 8 | data[2];
        result = 1;
        break;
      case SMPTEOFFSET:
        if(length < 5)
          {
            midiprint(MPError, "SMPTE offset: too short data");
            break;
          }
        if(length > 5)
          midiprint(MPWarn, "SMPTE offset: long data");

        msg->smpteoffset.type = msg->meta.type;
        msg->smpteoffset.hours = data[0];
        msg->smpteoffset.minutes = data[1];
        msg->smpteoffset.seconds = data[2];
        msg->smpteoffset.frames = data[3];
        msg->smpteoffset.subframes = data[4];
        result = 1;
        break;
      case TIMESIGNATURE:
        if(length < 4)
          {
            midiprint(MPError, "time signature: too short data");
            break;
          }
        if(length > 4)
          midiprint(MPWarn, "time signature: long data");

        msg->timesignature.type = msg->meta.type;
        msg->timesignature.nominator = data[0];
        msg->timesignature.denominator = data[1];
        msg->timesignature.clocksperclick = data[2];
        msg->timesignature.ttperquarter = data[3];
        result = 1;
        break;
      case KEYSIGNATURE:
        if(length < 2)
          {
            midiprint(MPError, "key signature: too short data");
            break;
          }
        if(length > 2)
          midiprint(MPWarn, "key signature: long data");

        msg->keysignature.type = msg->meta.type;
        msg->keysignature.sharpsflats = data[0];
        msg->keysignature.minor = data[1];
        result = 1;
        break;
      case SEQUENCERSPECIFIC:
        msg->sequencerspecific.type = msg->meta.type;
        msg->sequencerspecific.data = vld;
        vld = 0;
        result = 1;
        break;
      default:
        midiprint(MPWarn, "unknown meta type %hd", msg->meta.type);
        vld = 0;
        result = 1;
        break;
    }

  if(vld)
    free(vld);

  return result;
}


/*
 * Get the next message from the buffer and store it at `msg'. For variable
 * sized messages, the necessary memory is automatically allocated.
 * `rs' points to a location that contains the current channel voice
 * status byte and is used to support running status. It is updated if
 * necessary. To reset running status, set `*rs' to 0 before calling
 * this function.
 * If something goes wrong, return 0, else 1.
 */
int read_message(MBUF *b, MFMessage *msg, unsigned char *rs)
{
  long i = mbuf_pos(b);

  if(mbuf_rem(b) <= 0)
    {
      midiprint(MPError, "reading message: end of input");
      return 0;
    }

  if(!((msg->generic.cmd = mbuf_get(b)) & 0x80))
    {
      mbuf_set(b, i);
      msg->generic.cmd = *rs;
    }

  if(!(msg->generic.cmd & 0x80))
    {
      midiprint(MPError, "reading message: got data byte %h", msg->generic.cmd);
      mbuf_set(b, i);
      return 0;
    }

  switch(msg->generic.cmd & 0xf0)
    {
      /* Two-byte messages. */
      case NOTEOFF:
      case NOTEON:
      case KEYPRESSURE:
      case CONTROLCHANGE:
        if(mbuf_rem(b) < 2)
          {
            midiprint(MPError, "reading message: end of input");
            mbuf_set(b, i);
            return 0;
          }
        msg->noteoff.note = mbuf_get(b);
        msg->noteoff.velocity = mbuf_get(b);
        *rs = msg->generic.cmd;
        return 1;
        break;

      /* One-byte messages. */
      case PROGRAMCHANGE:
      case CHANNELPRESSURE:
        if(mbuf_rem(b) < 1)
          {
            midiprint(MPError, "reading message: end of input");
            mbuf_set(b, i);
            return 0;
          }
        msg->programchange.program = mbuf_get(b);
        *rs = msg->generic.cmd;
        return 1;
        break;

      /* This is special since it contains a 2x7bit quantity: */
      case PITCHWHEELCHANGE:
        if(mbuf_rem(b) < 2)
          {
            midiprint(MPError, "reading message: end of input");
            mbuf_set(b, i);
            return 0;
          }
        msg->pitchwheelchange.value = mbuf_get(b) << 7 | mbuf_get(b);
        msg->pitchwheelchange.value -= 0x2000;
        *rs = msg->generic.cmd;
        return 1;
        break;
    }

  /* Non-channel voice messages. */
  switch(msg->generic.cmd)
    {
      /* Sysex messages. */
      case SYSTEMEXCLUSIVE:
      case SYSTEMEXCLUSIVECONT:
        if(!(msg->systemexclusive.data = read_vld(b)))
          {
            mbuf_set(b, i);
            return 0;
          }
        return 1;
        break;

      case META:
        /*
         * Get the type. There must be at least two bytes; one for the
         * type and one for the size.
         */
        if(mbuf_rem(b) < 2)
          {
            midiprint(MPError, "reading message: end of input");
            mbuf_set(b, i);
            return 0;
          }
        msg->meta.type = mbuf_get(b);
        /* Get the data. */
        if(!(msg->meta.data = read_vld(b)))
          {
            mbuf_set(b, i);
            return 0;
          }
        if(!convert_meta(msg))
          {
            free(msg->meta.data);
            mbuf_set(b, i);
            return 0;
          }
        return 1;
        break;
    }

  /* What's that? */
  midiprint(MPError, "unknown message type %hu", msg->generic.cmd);
  mbuf_set(b, i);
  return 0;
}


/*
 * Write the event into the buffer. If `rs' is nonzero, it is used to
 * support running status. In contrast to `read_message', `rs' may be
 * NULL, which switches running status off.
 * Returns 1 on success and 0 on errors.
 */
int write_message(MBUF *b, MFMessage *msg, unsigned char *rs)
{
  unsigned char cmd = msg->generic.cmd;
  long value;

  if(cmd >= 0xf0)
    {
      /* This is a system common message. */
      if(mbuf_put(b, cmd) == EOF)
        return 0;

      switch(cmd)
        {
          case SYSTEMEXCLUSIVE:
          case SYSTEMEXCLUSIVECONT:
            return write_vld(b, msg->systemexclusive.data);
          case META:
            return mbuf_put(b, msg->meta.type) != EOF &&
                   write_vld(b, msg->meta.data);
        }
    }
  else if(cmd >= 0x80)
    {
      /* This is a channel voice message. */
      /*
       * Write the command if no running status exists or differs from
       * the command.
       */
      if(!(rs && *rs == cmd) && mbuf_put(b, cmd) == EOF)
        return 0;

      /* Update the running status. */
      if(rs)
        *rs = cmd;

      switch(cmd & 0xf0)
        {
          case NOTEOFF:
          case NOTEON:
          case KEYPRESSURE:
          case CONTROLCHANGE:
            return mbuf_put(b, msg->noteoff.note) != EOF &&
                   mbuf_put(b, msg->noteoff.velocity) != EOF;
          case PROGRAMCHANGE:
          case CHANNELPRESSURE:
            return mbuf_put(b, msg->programchange.program) != EOF;
          case PITCHWHEELCHANGE:
            value = msg->pitchwheelchange.value + 0x2000;
            return mbuf_put(b, (value >> 7) & 0x7f) != EOF &&
                   mbuf_put(b, value & 0x7f) != EOF ;
        }
    }
  else
    {
      /* This is a meta message. */
      if(mbuf_put(b, META) == EOF || mbuf_put(b, cmd) == EOF)
        return 0;

      switch(cmd)
        {
          case SEQUENCENUMBER:
            value = msg->sequencenumber.sequencenumber;
            return write_vlq(b, 2) &&
                   mbuf_put(b, (value >> 8) & 0xff) != EOF &&
                   mbuf_put(b, value & 0xff) != EOF;
          case TEXT:
          case COPYRIGHTNOTICE:
          case TRACKNAME:
          case INSTRUMENTNAME:
          case LYRIC:
          case MARKER:
          case CUEPOINT:
            return write_vld(b, msg->text.text);
          case ENDOFTRACK:
            return write_vlq(b, 0);
          case SETTEMPO:
            value = msg->settempo.tempo;
            return write_vlq(b, 3) &&
                   mbuf_put(b, (value >> 16) & 0xff) != EOF &&
                   mbuf_put(b, (value >> 8) & 0xff) != EOF &&
                   mbuf_put(b, value & 0xff) != EOF;
          case SMPTEOFFSET:
            return write_vlq(b, 5) &&
                   mbuf_put(b, msg->smpteoffset.hours) != EOF &&
                   mbuf_put(b, msg->smpteoffset.minutes) != EOF &&
                   mbuf_put(b, msg->smpteoffset.seconds) != EOF &&
                   mbuf_put(b, msg->smpteoffset.frames) != EOF &&
                   mbuf_put(b, msg->smpteoffset.subframes) != EOF;
          case TIMESIGNATURE:
            return write_vlq(b, 4) &&
                   mbuf_put(b, msg->timesignature.nominator) != EOF &&
                   mbuf_put(b, msg->timesignature.denominator) != EOF &&
                   mbuf_put(b, msg->timesignature.clocksperclick) != EOF &&
                   mbuf_put(b, msg->timesignature.ttperquarter) != EOF;
          case KEYSIGNATURE:
            return write_vlq(b, 2) &&
                   mbuf_put(b, msg->keysignature.sharpsflats) != EOF &&
                   mbuf_put(b, msg->keysignature.minor) != EOF;
          case SEQUENCERSPECIFIC:
            return write_vld(b, msg->sequencerspecific.data);
        }
    }

  /* This should never happen! */
  midiprint(MPFatal, "writing message: unknown message type %hd", cmd);
  return 0;
}


/*
 * To simplify cleanup, this function frees allocated data if `msg' is a
 * variable sized message.
 */
void clear_message(MFMessage *msg)
{
  switch(msg->generic.cmd)
    {
      case SYSTEMEXCLUSIVE:     free(msg->systemexclusive.data); break;
      case SYSTEMEXCLUSIVECONT: free(msg->systemexclusivecont.data); break;
      case META:                free(msg->meta.data); break;
      case TEXT:                free(msg->text.text); break;
      case COPYRIGHTNOTICE:     free(msg->copyrightnotice.text); break;
      case TRACKNAME:           free(msg->trackname.text); break;
      case INSTRUMENTNAME:      free(msg->instrumentname.text); break;
      case LYRIC:               free(msg->lyric.text); break;
      case MARKER:              free(msg->marker.text); break;
      case CUEPOINT:            free(msg->cuepoint.text); break;
      case SEQUENCERSPECIFIC:   free(msg->sequencerspecific.data); break;
    }
}


/*
 * Get the next event, i.e. the next (delta) time and message.
 * Parameters and return value are the same as of `read_message'.
 */
int read_event(MBUF *b, MFEvent *ev, unsigned char *rs)
{
  long pos = mbuf_pos(b);

  if((ev->time = read_vlq(b)) < 0 || !read_message(b, &ev->msg, rs))
    {
      mbuf_set(b, pos);
      return 0;
    }
  else
    return 1;
}


/*
 * Write the next event.
 * Parameters and return values as of `write_message'.
 */
int write_event(MBUF *b, MFEvent *ev, unsigned char *rs)
{
  return write_vlq(b, ev->time) && write_message(b, &ev->msg, rs);
}
