/*
 * $Id: print.h,v 1.1 1996/04/01 19:10:57 kilian Exp $
 *
 * Message printing hooks.
 *
 * $Log: print.h,v $
 * Revision 1.1  1996/04/01 19:10:57  kilian
 * Initial revision
 *
 */

#ifndef __PRINT_H__
#define __PRINT_H__

#include <stdarg.h>


/*
 * The function pointer `midiprint_hook', if not NULL, is used to write
 * strings to the output of the application. Similar, the pointers
 * `midiwarn_hook' and `midierror_hook' are used to notify about
 * warnings or errors. They all are called like the vprintf-style
 * functions.
 */
void (*midiprint_hook)(const char *fmt, va_list args);
void (*midiwarn_hook)(const char *fmt, va_list args);
void (*midierror_hook)(const char *fmt, va_list args);


/*
 * These call the corresponding hook if it is set.
 */
void midiprint(const char *fmt, ...);
void midiwarn(const char *fmt, ...);
void midierror(const char *fmt, ...);


#endif /* __PRINT_H__ */
