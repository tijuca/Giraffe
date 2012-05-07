/*
 * Copyright 2005 - 2012  Zarafa B.V.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3, 
 * as published by the Free Software Foundation with the following additional 
 * term according to sec. 7:
 *  
 * According to sec. 7 of the GNU Affero General Public License, version
 * 3, the terms of the AGPL are supplemented with the following terms:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V. The licensing of
 * the Program under the AGPL does not imply a trademark license.
 * Therefore any rights, title and interest in our trademarks remain
 * entirely with us.
 * 
 * However, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the
 * Program. Furthermore you may use our trademarks where it is necessary
 * to indicate the intended purpose of a product or service provided you
 * use it in accordance with honest practices in industrial or commercial
 * matters.  If you want to propagate modified versions of the Program
 * under the name "Zarafa" or "Zarafa Server", you may only do so if you
 * have a written permission by Zarafa B.V. (to acquire a permission
 * please contact Zarafa at trademark@zarafa.com).
 * 
 * The interactive user interface of the software displays an attribution
 * notice containing the term "Zarafa" and/or the logo of Zarafa.
 * Interactive user interfaces of unmodified and modified versions must
 * display Appropriate Legal Notices according to sec. 5 of the GNU
 * Affero General Public License, version 3, when you propagate
 * unmodified or modified versions of the Program. In accordance with
 * sec. 7 b) of the GNU Affero General Public License, version 3, these
 * Appropriate Legal Notices must retain the logo of Zarafa or display
 * the words "Initial Development by Zarafa" if the display of the logo
 * is not reasonably feasible for technical reasons. The use of the logo
 * of Zarafa in Legal Notices is allowed for unmodified and modified
 * versions of the software.
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

#include "platform.h"

#include <stdio.h>
#include <stdarg.h>
#include "Trace.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifdef DEBUG
// Turn these on/off
#  define DO_TRACE_MAPI		1
#  define DO_TRACE_NOTIF	1
#  define DO_TRACE_SOAP		0
#  define DO_TRACE_INT		0
#  define DO_TRACE_STREAM	0
#  define DO_TRACE_EXT		0
#else
// Leave these alone .. release builds don't print squat
#  define DO_TRACE_MAPI		0
#  define DO_TRACE_NOTIF	0
#  define DO_TRACE_SOAP		0
#  define DO_TRACE_INT		0
#  define DO_TRACE_STREAM	0
#  define DO_TRACE_EXT		0
#endif

// Turn these on/off
#define FILTER_REF			0	// IUnknown interface
#define FILTER_COMPARE		1	// Compare functions

// Limit the debug results, 0 is endless
#define BUFFER_LIMIT		0 // 8096

void TraceMsg(char* lpMsg, int time, char *func, char *format, va_list va)
{
	char* lpTraceType = NULL;
	char* buffer = NULL;
	char debug[1024];
	int	 len=0;
	int pos = 0;
	va_list va_lentest;

	if(FILTER_REF) {
		if(	strstr(func,"QueryInterface") != NULL ||
			strstr(func,"AddRef") != NULL ||
			strstr(func,"Release") != NULL )
			return;
	}

	if(FILTER_COMPARE) {
		if( strstr(func,"CompareEntryID") != NULL ||
			strstr(func,"CompareStoreIDs") != NULL)
			return;
	}

	switch(time) {
	case TRACE_ENTRY:
		lpTraceType = "Call";
		break;
	case TRACE_WARNING:
		lpTraceType = "Warning";
		break;
	case TRACE_RETURN:
		lpTraceType = "Ret ";
		break;
	default:
		lpTraceType = "Unknown";
		break;
	}


	pos = _snprintf(debug, sizeof(debug), "%lu %08X %s %s: %s(", GetTickCount(), GetCurrentThreadId(), lpMsg, lpTraceType, func);

	len = pos + 3;

	if (format && va) {
		va_copy(va_lentest, va);
		len += _vsnprintf(NULL, 0, format, va_lentest);
		va_end(va_lentest);
	}

	if (BUFFER_LIMIT != 0 && pos+3 < BUFFER_LIMIT && len > BUFFER_LIMIT)
		len = BUFFER_LIMIT;

	buffer = (char*)malloc( len * sizeof(char) );

	memcpy(buffer, debug, pos);

	if (format && va)
		pos = _vsnprintf(buffer+pos, len-pos, format, va);

	if(pos == -1) {
		//Indicate that the string isn't compleet
		buffer[len-6] = '.';
		buffer[len-5] = '.';
		buffer[len-4] = '.';
	}

	buffer[len-3] = ')';
	buffer[len-2] = '\n';
	buffer[len-1] = 0;

	OutputDebugStringA(buffer);

	free( buffer );

}

void TraceMapi(int time, char *func, char *format, ...) {

	va_list va;

	if(!DO_TRACE_MAPI)
		return;

	va_start(va, format);

	TraceMsg("MAPI", time, func, format, va);

	va_end(va);
}

void TraceMapiLib(int time, char *func, char *format, ...) {

	va_list va;

	if(!DO_TRACE_MAPI)
		return;

	va_start(va, format);

	TraceMsg("MAPILIB", time, func, format, va);

	va_end(va);
}


void TraceNotify(int time, char *func, char *format, ...) {
	va_list va; 

	if(!DO_TRACE_NOTIF)
		return;

	va_start(va, format);

	TraceMsg("NOTIFY", time, func, format, va);

	va_end(va);
}

void TraceSoap(int time, char *func, char *format, ...) {
	va_list va; 

	if(!DO_TRACE_SOAP)
		return;

	va_start(va, format);

	TraceMsg("SOAP", time, func, format, va);

	va_end(va);
}

void TraceInternals(int time, char *tracetype, char *func, char *format, ...) {
	va_list va; 

	if(!DO_TRACE_INT)
		return;

	va_start(va, format);

	TraceMsg("INTERNAL", time, func, format, va);

	va_end(va);
}


void TraceStream(int time, char *func, char *format, ...) {
	va_list va; 

	if(!DO_TRACE_STREAM)
		return;

	va_start(va, format);

	TraceMsg("IStream", time, func, format, va);

	va_end(va);
}

void TraceECMapi(int time, char *func, char *format, ...) {
	va_list va; 

	va_start(va, format);

	TraceMsg("ECMAPI", time, func, format, va);

	va_end(va);
}

void TraceExt(int time, char *func, char *format, ...) {
	va_list va; 

	if(!DO_TRACE_EXT)
		return;

	va_start(va, format);

	TraceMsg("EXT", time, func, format, va);

	va_end(va);
}

void TraceRelease(char *format, ...) {
	char debug[1024];
	va_list va; 
	va_start(va, format);

	vsnprintf(debug, sizeof(debug), format, va);
	OutputDebugStringA(debug);

	va_end(va);
}
