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

#ifndef ENTRYPOINT_H
#define ENTRYPOINT_H

#include <mapispi.h>

#include "ProviderUtil.h"
#include "tstring.h"

extern "C" {

HRESULT __cdecl MSProviderInit(HINSTANCE hInstance, LPMALLOC pmalloc, LPALLOCATEBUFFER pfnAllocBuf, LPALLOCATEMORE pfnAllocMore, LPFREEBUFFER pfnFreeBuf, ULONG ulFlags, ULONG ulMAPIVersion, ULONG * pulMDBVersion, LPMSPROVIDER * ppmsp);
HRESULT __stdcall MSGServiceEntry(HINSTANCE hInst, LPMALLOC lpMalloc, LPMAPISUP psup, ULONG ulUIParam, ULONG ulSEFlags, ULONG ulContext, ULONG cvals, LPSPropValue pvals, LPPROVIDERADMIN lpAdminProviders, LPMAPIERROR *lppMapiError);
HRESULT __cdecl XPProviderInit(HINSTANCE hInstance, LPMALLOC lpMalloc, LPALLOCATEBUFFER lpAllocateBuffer, LPALLOCATEMORE lpAllocateMore, LPFREEBUFFER lpFreeBuffer, ULONG ulFlags, ULONG ulMAPIVer, ULONG * lpulProviderVer, LPXPPROVIDER * lppXPProvider);
HRESULT  __cdecl ABProviderInit(HINSTANCE hInstance, LPMALLOC lpMalloc, LPALLOCATEBUFFER lpAllocateBuffer, LPALLOCATEMORE lpAllocateMore, LPFREEBUFFER lpFreeBuffer, ULONG ulFlags, ULONG ulMAPIVer, ULONG * lpulProviderVer, LPABPROVIDER * lppABProvider);


}

HRESULT InitializeProvider(LPPROVIDERADMIN lpAdminProvider, IProfSect *lpProfSect, sGlobalProfileProps sProfileProps, ULONG *lpcStoreID, LPENTRYID *lppStoreID);

// Global values
extern tstring	g_strCommonFilesZarafa;
extern tstring	g_strUserLocalAppDataZarafa;
extern tstring	g_strZarafaDirectory;
extern ECMapProvider g_mapProviders;
extern tstring		g_strManufacturer;
extern tstring		g_strProductName;
extern tstring		g_strProductNameShort;
extern bool g_isOEM;
extern ULONG g_ulLoadsim;


#endif // ENTRYPOINT_H
