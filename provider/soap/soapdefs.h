#ifndef SOAPDEFS_H_
#define SOAPDEFS_H_

/* we want soap to use strtod_l */
#define WITH_C_LOCALE

#include <platform.h>

# include <bits/types.h>
# undef __FD_SETSIZE
# define __FD_SETSIZE 8192

#endif // ndef SOAPDEFS_H_
