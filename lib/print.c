/*
 * $Id: print.c,v 1.1 1996/04/01 20:20:21 kilian Exp $
 *
 * Message printing hooks.
 *
 * $Log: print.c,v $
 * Revision 1.1  1996/04/01 20:20:21  kilian
 * Initial revision
 *
 */

#include <stdio.h>

#include "print.h"


/*
 * The function pointer `midiprint_hook', if not NULL, is used to write
 * strings to the output of the application. Similar, the pointers
 * `midiwarn_hook' and `midierror_hook' are used to notify about
 * warnings or errors. They all are called like the vprintf-style
 * functions.
 */
void (*midiprint_hook)(const char *fmt, va_list args) = NULL;
void (*midiwarn_hook)(const char *fmt, va_list args) = NULL;
void (*midierror_hook)(const char *fmt, va_list args) = NULL;


/*
 * These call the corresponding hook if it is set.
 */
void midiprint(const char *fmt, ...)
{
  if(midiprint_hook)
    {
      va_list args;
      va_start(args, fmt);
      midiprint_hook(fmt, args);
      va_end(args);
    }
}

void midiwarn(const char *fmt, ...)
{
  if(midiwarn_hook)
    {
      va_list args;
      va_start(args, fmt);
      midiwarn_hook(fmt, args);
      va_end(args);
    }
}

void midierror(const char *fmt, ...)
{
  if(midierror_hook)
    {
      va_list args;
      va_start(args, fmt);
      midierror_hook(fmt, args);
      va_end(args);
    }
}
