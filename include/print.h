/*
 * $Id: print.h,v 1.2 1996/04/02 10:17:18 kilian Exp $
 *
 * Message printing hooks.
 *
 * $Log: print.h,v $
 * Revision 1.2  1996/04/02 10:17:18  kilian
 * Use only one printing function resp. hook together with a printlevel.
 *
 * Revision 1.1  1996/04/01  19:10:57  kilian
 * Initial revision
 *
 */

#ifndef __PRINT_H__
#define __PRINT_H__

#include <stdarg.h>


/*
 * Type of message to print.
 */
typedef enum {
  MPNote,   /* For general text output, e.g. status informations. */
  MPWarn,   /* For warnings, e.g. unknown meta messages. */
  MPError,  /* For recoverable errors wrt the midi file standard. */
  MPFatal   /* For system level errors. */
} MPLevel;


/*
 * The function pointer `midiprint_hook', if not NULL, is used to write
 * strings to the output of the application.
 * It is called as vprintf-like functions, with an additional argument
 * `level' which specifies the level of message (see above).
 */
void (*midiprint_hook)(MPLevel level, const char *fmt, va_list args);


/*
 * This calls the corresponding hook if it is set.
 */
void midiprint(MPLevel level, const char *fmt, ...);


#endif /* __PRINT_H__ */
