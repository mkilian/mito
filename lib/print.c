/*
 * $Id: print.c,v 1.2 1996/04/02 10:19:11 kilian Exp $
 *
 * Message printing hooks.
 *
 * $Log: print.c,v $
 * Revision 1.2  1996/04/02 10:19:11  kilian
 * Use only one printing function resp. hook together with a printlevel.
 *
 * Revision 1.1  1996/04/01  20:20:21  kilian
 * Initial revision
 *
 */

#include <stdio.h>

#include "print.h"


/*
 * The function pointer `midiprint_hook', if not NULL, is used to write
 * strings to the output of the application.
 * It is called as vprintf-like functions, with an additional argument
 * `level' which specifies the level of message (see above).
 */
void (*midiprint_hook)(MPLevel level, const char *fmt, va_list args) = NULL;


/*
 * This calls the corresponding hook if it is set.
 */
void midiprint(MPLevel level, const char *fmt, ...)
{
  if(midiprint_hook)
    {
      va_list args;
      va_start(args, fmt);
      midiprint_hook(level, fmt, args);
      va_end(args);
    }
}
