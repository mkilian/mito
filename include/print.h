/*
 * Message printing hooks.
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
