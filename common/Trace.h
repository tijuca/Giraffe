/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef TRACE_H
#define TRACE_H

#define TRACE_ENTRY 1
#define TRACE_RETURN 2
#define TRACE_WARNING 3
#define TRACE_INFO 4

void TraceMapi(int time, const char *func, const char *format, ...);
void TraceMapiLib(int time, const char *func, const char *format, ...);
void TraceNotify(int time, const char *func, const char *format, ...);
void TraceSoap(int time, const char *func, const char *format, ...);
void TraceInternals(int time, const char *action, const char *func, const char *format, ...);
void TraceStream(int time, const char *func, const char *format, ...);
void TraceECMapi(int time, const char *func, const char *format, ...);
void TraceExt(int time, const char *func, const char *format, ...);
void TraceRelease(const char *format, ...);

#define TRACE_RELEASE	TraceRelease

#if !defined(WITH_TRACING) && defined(DEBUG)
# define WITH_TRACING
#endif

#ifdef WITH_TRACING
#define TRACE_MAPI		TraceMapi
#define TRACE_MAPILIB	TraceMapiLib
#define TRACE_ECMAPI	TraceECMapi
#define TRACE_NOTIFY	TraceNotify
#define TRACE_INTERNAL	TraceInternals
#define TRACE_SOAP		TraceSoap
#define TRACE_STREAM	TraceStream
#define TRACE_EXT		TraceExt
#else
#  define TRACE_MAPI(...)
#  define TRACE_MAPILIB(...)
#  define TRACE_ECMAPI(...)
#  define TRACE_NOTIFY(...)
#  define TRACE_INTERNAL(...)
#  define TRACE_SOAP(...)
#  define TRACE_STREAM(...)
#  define TRACE_EXT(...)
#endif

#endif // TRACE_H
