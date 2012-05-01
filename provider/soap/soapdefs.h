#ifndef SOAPDEFS_H_
#define SOAPDEFS_H_

/* we want soap to use strtod_l */
#define WITH_C_LOCALE

#include <platform.h>

/* 
 * this gSoap still uses select(), and only handles sockets > 1024 if
 * you don't have a glibc which checks for compiletime boundaries on
 * this limit.
 */
#ifndef _FORTIFY_SOURCE
# include <bits/types.h>
# undef __FD_SETSIZE
# define __FD_SETSIZE 8192
#endif

#endif // ndef SOAPDEFS_H_
