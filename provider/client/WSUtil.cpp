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
#include <sys/un.h>
#include "WSUtil.h"
#include "ECIConv.h"
#include "ECGuid.h"
#include "Trace.h"

#include "Mem.h"

#include "mapiext.h"

// For the static row getprop functions
#include "ECMAPIProp.h"
#include "ECMAPIFolder.h"
#include "ECMessage.h"

#include "ECMailUser.h"
#include "ECDistList.h"
#include "ECABContainer.h"

#include "SOAPUtils.h"
#include "CommonUtil.h"

#include <charset/convert.h>
#include <charset/utf8string.h>
#include "EntryPoint.h"
#include "ECGetText.h"


#include "threadutil.h"
#include "SOAPSock.h"

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	pbMUIDECSABGuid	"\xac\x21\xa9\x50\x40\xd3\xee\x48\xb3\x19\xfb\xa7\x53\x30\x44\x25"

#define CONVERT_TO(_context, _charset, ...) ((_context) ? (_context)->convert_to<_charset>(__VA_ARGS__) : convert_to<_charset>(__VA_ARGS__))

HRESULT CopyMAPIPropValToSOAPPropVal(propVal *lpPropValDst, LPSPropValue lpPropValSrc, convert_context *lpConverter)
{
	TRACE_MAPI(TRACE_ENTRY, (char*)__FUNCTION__, " Prop: 0x%X", lpPropValSrc->ulPropTag);

	HRESULT hr = hrSuccess;
	int i = 0;

	lpPropValDst->ulPropTag = lpPropValSrc->ulPropTag;
	memset(&lpPropValDst->Value, 0, sizeof(propValData));

	switch(PROP_TYPE(lpPropValSrc->ulPropTag)) {
	case PT_I2:
		lpPropValDst->__union = SOAP_UNION_propValData_i;
		lpPropValDst->Value.i = lpPropValSrc->Value.i;
		break;
	case PT_LONG: // or PT_ULONG
		lpPropValDst->__union = SOAP_UNION_propValData_ul;
		lpPropValDst->Value.ul = lpPropValSrc->Value.ul;
		break;
	case PT_R4:
		lpPropValDst->__union = SOAP_UNION_propValData_flt;
		lpPropValDst->Value.flt = lpPropValSrc->Value.flt;
		break;
	case PT_DOUBLE:
		lpPropValDst->__union = SOAP_UNION_propValData_dbl;
		lpPropValDst->Value.dbl = lpPropValSrc->Value.dbl;
		break;
	case PT_CURRENCY:
		lpPropValDst->__union = SOAP_UNION_propValData_hilo;
		lpPropValDst->Value.hilo = new hiloLong;
		lpPropValDst->Value.hilo->hi = lpPropValSrc->Value.cur.Hi;
		lpPropValDst->Value.hilo->lo = lpPropValSrc->Value.cur.Lo;
		break;
	case PT_APPTIME:
		lpPropValDst->__union = SOAP_UNION_propValData_dbl;
		lpPropValDst->Value.dbl = lpPropValSrc->Value.at;
		break;
	case PT_ERROR:
		lpPropValDst->__union = SOAP_UNION_propValData_ul;
		lpPropValDst->Value.ul = lpPropValSrc->Value.err;
		break;
	case PT_BOOLEAN:
		lpPropValDst->__union = SOAP_UNION_propValData_b;
		lpPropValDst->Value.b = lpPropValSrc->Value.b == 0 ? false : true;
		break;
	case PT_OBJECT:
		// can never be transmitted over the wire!
		hr = MAPI_E_INVALID_TYPE;
		break;
	case PT_I8:
		lpPropValDst->__union = SOAP_UNION_propValData_li;
		lpPropValDst->Value.li = lpPropValSrc->Value.li.QuadPart;
		break;
	case PT_STRING8:
		{
			utf8string u8 = CONVERT_TO(lpConverter, utf8string, lpPropValSrc->Value.lpszA);	// SOAP lpszA = UTF-8, MAPI lpszA = current locale charset
			lpPropValDst->__union = SOAP_UNION_propValData_lpszA;
			lpPropValDst->Value.lpszA = new char[u8.size() + 1];
			strcpy(lpPropValDst->Value.lpszA, u8.c_str());
		}
		break;
	case PT_UNICODE:
		{
			utf8string u8 = CONVERT_TO(lpConverter, utf8string, lpPropValSrc->Value.lpszW);
			lpPropValDst->__union = SOAP_UNION_propValData_lpszA;
			lpPropValDst->Value.lpszA = new char[u8.size() + 1];
			strcpy(lpPropValDst->Value.lpszA, u8.c_str());
		}
		break;
	case PT_SYSTIME:
		lpPropValDst->__union = SOAP_UNION_propValData_hilo;
		lpPropValDst->Value.hilo = new hiloLong;
		lpPropValDst->Value.hilo->hi = lpPropValSrc->Value.ft.dwHighDateTime;
		lpPropValDst->Value.hilo->lo = lpPropValSrc->Value.ft.dwLowDateTime;
		break;
	case PT_CLSID:
		lpPropValDst->__union = SOAP_UNION_propValData_bin;
		lpPropValDst->Value.bin = new xsd__base64Binary;
		lpPropValDst->Value.bin->__ptr = new unsigned char[sizeof(GUID)];
		lpPropValDst->Value.bin->__size = sizeof(GUID);
		memcpy((void *)lpPropValDst->Value.bin->__ptr, lpPropValSrc->Value.lpguid, sizeof(GUID));
		break;
	case PT_BINARY:
		lpPropValDst->__union = SOAP_UNION_propValData_bin;
		lpPropValDst->Value.bin = new xsd__base64Binary;
		lpPropValDst->Value.bin->__ptr = new unsigned char[lpPropValSrc->Value.bin.cb];
		lpPropValDst->Value.bin->__size = lpPropValSrc->Value.bin.cb;
		memcpy((void *)lpPropValDst->Value.bin->__ptr, lpPropValSrc->Value.bin.lpb, lpPropValSrc->Value.bin.cb);
		break;
	case PT_MV_I2:
		lpPropValDst->__union = SOAP_UNION_propValData_mvi;
		lpPropValDst->Value.mvi.__size = lpPropValSrc->Value.MVi.cValues;
        lpPropValDst->Value.mvi.__ptr = new short int[lpPropValDst->Value.mvi.__size];
		memcpy(lpPropValDst->Value.mvi.__ptr, lpPropValSrc->Value.MVi.lpi, sizeof(short int) * lpPropValDst->Value.mvi.__size);
		break;
	case PT_MV_LONG:
		lpPropValDst->__union = SOAP_UNION_propValData_mvl;
		lpPropValDst->Value.mvl.__size = lpPropValSrc->Value.MVl.cValues;
        lpPropValDst->Value.mvl.__ptr = new unsigned int[lpPropValDst->Value.mvl.__size];
		memcpy(lpPropValDst->Value.mvl.__ptr, lpPropValSrc->Value.MVl.lpl, sizeof(unsigned int) * lpPropValDst->Value.mvl.__size);
		break;
	case PT_MV_R4:
		lpPropValDst->__union = SOAP_UNION_propValData_mvflt;
		lpPropValDst->Value.mvflt.__size = lpPropValSrc->Value.MVflt.cValues;
        lpPropValDst->Value.mvflt.__ptr = new float[lpPropValDst->Value.mvflt.__size];
		memcpy(lpPropValDst->Value.mvflt.__ptr, lpPropValSrc->Value.MVflt.lpflt, sizeof(float) * lpPropValDst->Value.mvflt.__size);
		break;
	case PT_MV_DOUBLE:
		lpPropValDst->__union = SOAP_UNION_propValData_mvdbl;
		lpPropValDst->Value.mvdbl.__size = lpPropValSrc->Value.MVdbl.cValues;
        lpPropValDst->Value.mvdbl.__ptr = new double[lpPropValDst->Value.mvdbl.__size];
		memcpy(lpPropValDst->Value.mvdbl.__ptr, lpPropValSrc->Value.MVdbl.lpdbl, sizeof(double) * lpPropValDst->Value.mvdbl.__size);
		break;
	case PT_MV_CURRENCY:
		lpPropValDst->__union = SOAP_UNION_propValData_mvhilo;
		lpPropValDst->Value.mvhilo.__size = lpPropValSrc->Value.MVcur.cValues;
        lpPropValDst->Value.mvhilo.__ptr = new hiloLong[lpPropValDst->Value.mvhilo.__size];
		for(i=0; i < lpPropValDst->Value.mvhilo.__size; i++)
		{
			lpPropValDst->Value.mvhilo.__ptr[i].hi = lpPropValSrc->Value.MVcur.lpcur[i].Hi;
			lpPropValDst->Value.mvhilo.__ptr[i].lo = lpPropValSrc->Value.MVcur.lpcur[i].Lo;
		}
		break;
	case PT_MV_APPTIME:
		lpPropValDst->__union = SOAP_UNION_propValData_mvdbl;
		lpPropValDst->Value.mvdbl.__size = lpPropValSrc->Value.MVat.cValues;
        lpPropValDst->Value.mvdbl.__ptr = new double[lpPropValDst->Value.mvdbl.__size];
		memcpy(lpPropValDst->Value.mvdbl.__ptr, lpPropValSrc->Value.MVat.lpat, sizeof(double) * lpPropValDst->Value.mvdbl.__size);
		break;
	case PT_MV_SYSTIME:
		lpPropValDst->__union = SOAP_UNION_propValData_mvhilo;
		lpPropValDst->Value.mvhilo.__size = lpPropValSrc->Value.MVft.cValues;
        lpPropValDst->Value.mvhilo.__ptr = new hiloLong[lpPropValDst->Value.mvhilo.__size];
		for(i=0; i < lpPropValDst->Value.mvhilo.__size; i++)
		{
			lpPropValDst->Value.mvhilo.__ptr[i].hi = lpPropValSrc->Value.MVft.lpft[i].dwHighDateTime;
			lpPropValDst->Value.mvhilo.__ptr[i].lo = lpPropValSrc->Value.MVft.lpft[i].dwLowDateTime;
		}
		break;
	case PT_MV_BINARY:
		lpPropValDst->__union = SOAP_UNION_propValData_mvbin;
		lpPropValDst->Value.mvbin.__size = lpPropValSrc->Value.MVbin.cValues;
		lpPropValDst->Value.mvbin.__ptr = new xsd__base64Binary[lpPropValDst->Value.mvbin.__size];
		for(i=0; i < lpPropValDst->Value.mvbin.__size; i++)
		{
			lpPropValDst->Value.mvbin.__ptr[i].__size = lpPropValSrc->Value.MVbin.lpbin[i].cb;
			lpPropValDst->Value.mvbin.__ptr[i].__ptr = new unsigned char[lpPropValDst->Value.mvbin.__ptr[i].__size];
			memcpy(lpPropValDst->Value.mvbin.__ptr[i].__ptr, lpPropValSrc->Value.MVbin.lpbin[i].lpb, lpPropValDst->Value.mvbin.__ptr[i].__size);
		}
		break;
	case PT_MV_STRING8:
		if (lpConverter == NULL) {
			convert_context converter;
			CopyMAPIPropValToSOAPPropVal(lpPropValDst, lpPropValSrc, &converter);
		} else {
			lpPropValDst->__union = SOAP_UNION_propValData_mvszA;
			lpPropValDst->Value.mvszA.__size = lpPropValSrc->Value.MVszA.cValues;
			lpPropValDst->Value.mvszA.__ptr = new char*[lpPropValDst->Value.mvszA.__size];
			for (i = 0; i < lpPropValDst->Value.mvszA.__size; i++)
			{
				utf8string u8 = lpConverter->convert_to<utf8string>(lpPropValSrc->Value.MVszA.lppszA[i]);
				lpPropValDst->Value.mvszA.__ptr[i] = new char[u8.size() + 1];
				strcpy(lpPropValDst->Value.mvszA.__ptr[i], u8.c_str());
			}
		}
		break;
	case PT_MV_UNICODE:
		if (lpConverter == NULL) {
			convert_context converter;
			CopyMAPIPropValToSOAPPropVal(lpPropValDst, lpPropValSrc, &converter);
		} else {
			lpPropValDst->__union = SOAP_UNION_propValData_mvszA;
			lpPropValDst->Value.mvszA.__size = lpPropValSrc->Value.MVszA.cValues;
			lpPropValDst->Value.mvszA.__ptr = new char*[lpPropValDst->Value.mvszA.__size];
			for(i=0; i < lpPropValDst->Value.mvszA.__size; i++)
			{
				utf8string u8 = lpConverter->convert_to<utf8string>(lpPropValSrc->Value.MVszW.lppszW[i]);
				lpPropValDst->Value.mvszA.__ptr[i] = new char[u8.size() + 1];
				strcpy(lpPropValDst->Value.mvszA.__ptr[i], u8.c_str());
			}
		}
		break;
	case PT_MV_CLSID:
		lpPropValDst->__union = SOAP_UNION_propValData_mvbin;
		lpPropValDst->Value.mvbin.__size = lpPropValSrc->Value.MVguid.cValues;
		lpPropValDst->Value.mvbin.__ptr = new xsd__base64Binary[lpPropValDst->Value.mvbin.__size];
		for(i=0; i < lpPropValDst->Value.mvbin.__size; i++)
		{
			lpPropValDst->Value.mvbin.__ptr[i].__size = sizeof(GUID);
			lpPropValDst->Value.mvbin.__ptr[i].__ptr = new unsigned char[lpPropValDst->Value.mvbin.__ptr[i].__size];
			memcpy(lpPropValDst->Value.mvbin.__ptr[i].__ptr, &lpPropValSrc->Value.MVguid.lpguid[i], lpPropValDst->Value.mvbin.__ptr[i].__size);
		}
		break;
	case PT_MV_I8:
		lpPropValDst->__union = SOAP_UNION_propValData_mvli;
		lpPropValDst->Value.mvli.__size = lpPropValSrc->Value.MVli.cValues;
        lpPropValDst->Value.mvli.__ptr = new LONG64[lpPropValDst->Value.mvli.__size];
		for(i=0; i < lpPropValDst->Value.mvli.__size; i++) {
			lpPropValDst->Value.mvli.__ptr[i] = lpPropValSrc->Value.MVli.lpli[i].QuadPart;
		}
		break;
	case PT_SRESTRICTION:
		lpPropValDst->__union = SOAP_UNION_propValData_res;
		// NOTE: we placed the object pointer in lpszA to make sure it's on the same offset as Value.x on 32bit as 64bit machines
		hr = CopyMAPIRestrictionToSOAPRestriction(&lpPropValDst->Value.res, (LPSRestriction)lpPropValSrc->Value.lpszA, lpConverter);
		break;
	case PT_ACTIONS: {
		// NOTE: we placed the object pointer in lpszA to make sure it's on the same offset as Value.x on 32bit as 64bit machines
		ACTIONS *lpSrcActions = ((ACTIONS*)lpPropValSrc->Value.lpszA);
		lpPropValDst->__union = SOAP_UNION_propValData_actions;
		lpPropValDst->Value.actions = new struct actions;
		lpPropValDst->Value.actions->__ptr = new struct action [ lpSrcActions->cActions ];
		lpPropValDst->Value.actions->__size = lpSrcActions->cActions;

		for(unsigned int i=0; i<lpSrcActions->cActions;i++) {
			ACTION *lpSrcAction = &lpSrcActions->lpAction[i];
			struct action *lpDstAction = &lpPropValDst->Value.actions->__ptr[i];

			lpDstAction->acttype = lpSrcAction->acttype;
			lpDstAction->flavor = lpSrcAction->ulActionFlavor;
			lpDstAction->flags = lpSrcAction->ulFlags;

			switch(lpSrcActions->lpAction[i].acttype) {
				case OP_MOVE:
				case OP_COPY:
					lpDstAction->__union = SOAP_UNION__act_moveCopy;

					lpDstAction->act.moveCopy.store.__ptr = new unsigned char [lpSrcAction->actMoveCopy.cbStoreEntryId];
					memcpy(lpDstAction->act.moveCopy.store.__ptr, lpSrcAction->actMoveCopy.lpStoreEntryId, lpSrcAction->actMoveCopy.cbStoreEntryId);
					lpDstAction->act.moveCopy.store.__size = lpSrcAction->actMoveCopy.cbStoreEntryId;

					lpDstAction->act.moveCopy.folder.__ptr = new unsigned char [lpSrcAction->actMoveCopy.cbFldEntryId];
					memcpy(lpDstAction->act.moveCopy.folder.__ptr, lpSrcAction->actMoveCopy.lpFldEntryId, lpSrcAction->actMoveCopy.cbFldEntryId);
					lpDstAction->act.moveCopy.folder.__size = lpSrcAction->actMoveCopy.cbFldEntryId;

					break;
				case OP_REPLY:
				case OP_OOF_REPLY:
					lpDstAction->__union = SOAP_UNION__act_reply;
					lpDstAction->act.reply.message.__ptr = new unsigned char [lpSrcAction->actReply.cbEntryId];
					memcpy(lpDstAction->act.reply.message.__ptr, lpSrcAction->actReply.lpEntryId, lpSrcAction->actReply.cbEntryId);
					lpDstAction->act.reply.message.__size = lpSrcAction->actReply.cbEntryId;

					lpDstAction->act.reply.guid.__size = sizeof(GUID);
					lpDstAction->act.reply.guid.__ptr = new unsigned char [ sizeof(GUID) ];
					memcpy(lpDstAction->act.reply.guid.__ptr, (void *)&lpSrcAction->actReply.guidReplyTemplate, sizeof(GUID));

					break;
				case OP_DEFER_ACTION:
					lpDstAction->__union = SOAP_UNION__act_defer;
					lpDstAction->act.defer.bin.__ptr = new unsigned char [ lpSrcAction->actDeferAction.cbData ];
					lpDstAction->act.defer.bin.__size = lpSrcAction->actDeferAction.cbData;
					memcpy(lpDstAction->act.defer.bin.__ptr,lpSrcAction->actDeferAction.pbData, lpSrcAction->actDeferAction.cbData);
					break;
				case OP_BOUNCE:
					lpDstAction->__union = SOAP_UNION__act_bouncecode;
					lpDstAction->act.bouncecode = lpSrcAction->scBounceCode;
					break;
				case OP_FORWARD:
				case OP_DELEGATE:
					lpDstAction->__union = SOAP_UNION__act_adrlist;
					hr = CopyMAPIRowSetToSOAPRowSet((LPSRowSet)lpSrcAction->lpadrlist, &lpDstAction->act.adrlist, lpConverter);
					if(hr != hrSuccess)
						goto exit;
					break;
				case OP_TAG:
					lpDstAction->__union = SOAP_UNION__act_prop;
					lpDstAction->act.prop = new propVal;
					hr = CopyMAPIPropValToSOAPPropVal(lpDstAction->act.prop, &lpSrcAction->propTag, lpConverter);
					break;
				case OP_DELETE:
				case OP_MARK_AS_READ:
					// no other data needed
					break;
			}
		}
		break;
	}
	default:
		hr = MAPI_E_INVALID_TYPE;
	}

exit:
	return hr;
}

HRESULT CopySOAPPropValToMAPIPropVal(LPSPropValue lpPropValDst, struct propVal *lpPropValSrc, void *lpBase, convert_context *lpConverter)
{
	HRESULT hr = hrSuccess;

	lpPropValDst->ulPropTag = lpPropValSrc->ulPropTag;
	lpPropValDst->dwAlignPad = 0;

	// FIXME check pointer is OK before using in (lpPropValSrc->Value.hilo may be NULL!)
	switch(PROP_TYPE(lpPropValSrc->ulPropTag)) {
	case PT_I2:
		lpPropValDst->Value.i = lpPropValSrc->Value.i;
		break;
	case PT_LONG:
		lpPropValDst->Value.ul = lpPropValSrc->Value.ul;
		break;
	case PT_R4:
		lpPropValDst->Value.flt = lpPropValSrc->Value.flt;
		break;
	case PT_DOUBLE:
		lpPropValDst->Value.dbl = lpPropValSrc->Value.dbl;
		break;
	case PT_CURRENCY:
		if(lpPropValSrc->__union && lpPropValSrc->Value.hilo) {
			lpPropValDst->Value.cur.Hi = lpPropValSrc->Value.hilo->hi;
			lpPropValDst->Value.cur.Lo = lpPropValSrc->Value.hilo->lo;
		} else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_APPTIME:
		lpPropValDst->Value.at = lpPropValSrc->Value.dbl;
		break;
	case PT_ERROR:
		lpPropValDst->Value.err = ZarafaErrorToMAPIError(lpPropValSrc->Value.ul);
		break;
	case PT_BOOLEAN:
		lpPropValDst->Value.b = lpPropValSrc->Value.b;
		break;
	case PT_OBJECT:
		// can never be transmitted over the wire!
		hr = MAPI_E_INVALID_TYPE;
		break;

	case PT_I8:
		lpPropValDst->Value.li.QuadPart = lpPropValSrc->Value.li;
		break;
	case PT_STRING8:
		if(lpPropValSrc->__union && lpPropValSrc->Value.lpszA) {
			string s = CONVERT_TO(lpConverter, string, lpPropValSrc->Value.lpszA, rawsize(lpPropValSrc->Value.lpszA), "UTF-8");
			ECAllocateMore(s.length() + 1, lpBase, (void **) &lpPropValDst->Value.lpszA);
			strcpy(lpPropValDst->Value.lpszA, s.c_str());
		} else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_UNICODE:
		if(lpPropValSrc->__union && lpPropValSrc->Value.lpszA) {
			wstring ws = CONVERT_TO(lpConverter, wstring, lpPropValSrc->Value.lpszA, rawsize(lpPropValSrc->Value.lpszA), "UTF-8");
			ECAllocateMore(sizeof(wstring::value_type) * (ws.length() + 1), lpBase, (void **) &lpPropValDst->Value.lpszW);
			wcscpy(lpPropValDst->Value.lpszW, ws.c_str());
		} else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_SYSTIME:
		if(lpPropValSrc->__union && lpPropValSrc->Value.hilo) {
			lpPropValDst->Value.ft.dwHighDateTime = lpPropValSrc->Value.hilo->hi;
			lpPropValDst->Value.ft.dwLowDateTime = lpPropValSrc->Value.hilo->lo;
		} else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_CLSID:
		if(lpPropValSrc->__union && lpPropValSrc->Value.bin && lpPropValSrc->Value.bin->__size == sizeof(MAPIUID)) {
			ECAllocateMore(lpPropValSrc->Value.bin->__size, lpBase, (void **) &lpPropValDst->Value.lpguid);
			memcpy((void *)lpPropValDst->Value.lpguid, lpPropValSrc->Value.bin->__ptr, lpPropValSrc->Value.bin->__size);
		} else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_BINARY:
		if(lpPropValSrc->__union && lpPropValSrc->Value.bin) {
			ECAllocateMore(lpPropValSrc->Value.bin->__size, lpBase, (void **) &lpPropValDst->Value.bin.lpb);
			memcpy((void *)lpPropValDst->Value.bin.lpb, lpPropValSrc->Value.bin->__ptr, lpPropValSrc->Value.bin->__size);
			lpPropValDst->Value.bin.cb = lpPropValSrc->Value.bin->__size;
		}else if(lpPropValSrc->__union == 0) {
			lpPropValDst->Value.bin.lpb = NULL;
			lpPropValDst->Value.bin.cb = 0;
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}

		break;
	case PT_MV_I2:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvi.__ptr) {
			lpPropValDst->Value.MVi.cValues = lpPropValSrc->Value.mvi.__size;
			ECAllocateMore(sizeof(short int)*lpPropValDst->Value.MVi.cValues, lpBase, (void**)&lpPropValDst->Value.MVi.lpi);
			memcpy(lpPropValDst->Value.MVi.lpi, lpPropValSrc->Value.mvi.__ptr, sizeof(short int)*lpPropValDst->Value.MVi.cValues);
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_LONG:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvl.__ptr) {
			lpPropValDst->Value.MVl.cValues = lpPropValSrc->Value.mvl.__size;
			ECAllocateMore(sizeof(unsigned int)*lpPropValDst->Value.MVl.cValues, lpBase, (void**)&lpPropValDst->Value.MVl.lpl);
			memcpy(lpPropValDst->Value.MVl.lpl, lpPropValSrc->Value.mvl.__ptr, sizeof(unsigned int)*lpPropValDst->Value.MVl.cValues);
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_R4:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvflt.__ptr) {
			lpPropValDst->Value.MVflt.cValues = lpPropValSrc->Value.mvflt.__size;
			ECAllocateMore(sizeof(float)*lpPropValDst->Value.MVflt.cValues, lpBase, (void**)&lpPropValDst->Value.MVflt.lpflt);
			memcpy(lpPropValDst->Value.MVflt.lpflt, lpPropValSrc->Value.mvflt.__ptr, sizeof(float)*lpPropValDst->Value.MVflt.cValues);
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_DOUBLE:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvdbl.__ptr) {
			lpPropValDst->Value.MVdbl.cValues = lpPropValSrc->Value.mvdbl.__size;
			ECAllocateMore(sizeof(double)*lpPropValDst->Value.MVdbl.cValues, lpBase, (void**)&lpPropValDst->Value.MVdbl.lpdbl);
			memcpy(lpPropValDst->Value.MVdbl.lpdbl, lpPropValSrc->Value.mvdbl.__ptr, sizeof(double)*lpPropValDst->Value.MVdbl.cValues);
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_CURRENCY:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvhilo.__ptr) {
			lpPropValDst->Value.MVcur.cValues = lpPropValSrc->Value.mvhilo.__size;
			ECAllocateMore(sizeof(hiloLong)*lpPropValDst->Value.MVcur.cValues, lpBase, (void**)&lpPropValDst->Value.MVcur.lpcur);
		for(unsigned int i=0; i < lpPropValDst->Value.MVcur.cValues; i++)
		{
			lpPropValDst->Value.MVcur.lpcur[i].Hi = lpPropValSrc->Value.mvhilo.__ptr[i].hi;
			lpPropValDst->Value.MVcur.lpcur[i].Lo = lpPropValSrc->Value.mvhilo.__ptr[i].lo;
		}
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_APPTIME:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvdbl.__ptr) {
			lpPropValDst->Value.MVat.cValues = lpPropValSrc->Value.mvdbl.__size;
			ECAllocateMore(sizeof(double)*lpPropValDst->Value.MVat.cValues, lpBase, (void**)&lpPropValDst->Value.MVat.lpat);
			memcpy(lpPropValDst->Value.MVat.lpat, lpPropValSrc->Value.mvdbl.__ptr, sizeof(double)*lpPropValDst->Value.MVat.cValues);
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_SYSTIME:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvhilo.__ptr) {
			lpPropValDst->Value.MVft.cValues = lpPropValSrc->Value.mvhilo.__size;
			ECAllocateMore(sizeof(hiloLong)*lpPropValDst->Value.MVft.cValues, lpBase, (void**)&lpPropValDst->Value.MVft.lpft);
		for(unsigned int i=0; i < lpPropValDst->Value.MVft.cValues; i++)
		{
			lpPropValDst->Value.MVft.lpft[i].dwHighDateTime = lpPropValSrc->Value.mvhilo.__ptr[i].hi;
			lpPropValDst->Value.MVft.lpft[i].dwLowDateTime = lpPropValSrc->Value.mvhilo.__ptr[i].lo;
		}
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_BINARY:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvbin.__ptr) {
			lpPropValDst->Value.MVbin.cValues = lpPropValSrc->Value.mvbin.__size;
			ECAllocateMore(sizeof(SBinary)*lpPropValDst->Value.MVbin.cValues, lpBase, (void**)&lpPropValDst->Value.MVbin.lpbin);
		for(unsigned int i=0; i< lpPropValDst->Value.MVbin.cValues; i++)
		{
			lpPropValDst->Value.MVbin.lpbin[i].cb =  lpPropValSrc->Value.mvbin.__ptr[i].__size;
			if(lpPropValDst->Value.MVbin.lpbin[i].cb > 0)
			{
				ECAllocateMore(sizeof(unsigned char)*lpPropValDst->Value.MVbin.lpbin[i].cb, lpBase, (void**)&lpPropValDst->Value.MVbin.lpbin[i].lpb);
				memcpy(lpPropValDst->Value.MVbin.lpbin[i].lpb, lpPropValSrc->Value.mvbin.__ptr[i].__ptr, sizeof(unsigned char)*lpPropValDst->Value.MVbin.lpbin[i].cb);
			}
			else
				lpPropValDst->Value.MVbin.lpbin[i].lpb = NULL;
		}
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_STRING8:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvszA.__ptr) {
			if (lpConverter == NULL) {
				convert_context converter;
				CopySOAPPropValToMAPIPropVal(lpPropValDst, lpPropValSrc, lpBase, &converter);
			} else {
				lpPropValDst->Value.MVszA.cValues = lpPropValSrc->Value.mvszA.__size;
				ECAllocateMore(sizeof(LPSTR)*lpPropValDst->Value.MVszA.cValues, lpBase, (void**)&lpPropValDst->Value.MVszA.lppszA);

				for (unsigned int i=0; i < lpPropValDst->Value.MVszA.cValues; i++) {
					if (lpPropValSrc->Value.mvszA.__ptr[i] != NULL) {
						string s = lpConverter->convert_to<string>(lpPropValSrc->Value.mvszA.__ptr[i], rawsize(lpPropValSrc->Value.mvszA.__ptr[i]), "UTF-8");
						ECAllocateMore(s.size() + 1, lpBase, (void**)&lpPropValDst->Value.MVszA.lppszA[i]);
						strcpy(lpPropValDst->Value.MVszA.lppszA[i], s.c_str());
					} else {
						ECAllocateMore(1, lpBase, (void**)&lpPropValDst->Value.MVszA.lppszA[i]);
						lpPropValDst->Value.MVszA.lppszA[i][0] = '\0';
					}
				}
			}
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_UNICODE:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvszA.__ptr) {
			if (lpConverter == NULL) {
				convert_context converter;
				CopySOAPPropValToMAPIPropVal(lpPropValDst, lpPropValSrc, lpBase, &converter);
			} else {
				lpPropValDst->Value.MVszW.cValues = lpPropValSrc->Value.mvszA.__size;
				ECAllocateMore(sizeof(LPWSTR)*lpPropValDst->Value.MVszW.cValues, lpBase, (void**)&lpPropValDst->Value.MVszW.lppszW);

				for (unsigned int i=0; i < lpPropValDst->Value.MVszW.cValues; i++) {
					if (lpPropValSrc->Value.mvszA.__ptr[i] != NULL) {
						wstring ws = lpConverter->convert_to<wstring>(lpPropValSrc->Value.mvszA.__ptr[i], rawsize(lpPropValSrc->Value.mvszA.__ptr[i]), "UTF-8");
						ECAllocateMore(sizeof(wstring::value_type) * (ws.length() + 1), lpBase, (void**)&lpPropValDst->Value.MVszW.lppszW[i]);
						wcscpy(lpPropValDst->Value.MVszW.lppszW[i], ws.c_str());
					} else {
						ECAllocateMore(1, lpBase, (void**)&lpPropValDst->Value.MVszW.lppszW[i]);
						lpPropValDst->Value.MVszW.lppszW[i][0] = '\0';
					}
				}
			}
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_CLSID:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvbin.__ptr) {
			lpPropValDst->Value.MVguid.cValues = lpPropValSrc->Value.mvbin.__size;
			ECAllocateMore(sizeof(GUID)*lpPropValDst->Value.MVguid.cValues, lpBase, (void**)&lpPropValDst->Value.MVguid.lpguid);
			for(unsigned int i=0; i< lpPropValDst->Value.MVguid.cValues; i++)
			{
				memcpy(&lpPropValDst->Value.MVguid.lpguid[i], lpPropValSrc->Value.mvbin.__ptr[i].__ptr, sizeof(GUID));
			}
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_MV_I8:
		if(lpPropValSrc->__union && lpPropValSrc->Value.mvli.__ptr) {
			lpPropValDst->Value.MVli.cValues = lpPropValSrc->Value.mvli.__size;
			ECAllocateMore(sizeof(LARGE_INTEGER)*lpPropValDst->Value.MVli.cValues, lpBase, (void**)&lpPropValDst->Value.MVli.lpli);
			for(unsigned int i=0; i< lpPropValDst->Value.MVli.cValues; i++)
			{
				lpPropValDst->Value.MVli.lpli[i].QuadPart = lpPropValSrc->Value.mvli.__ptr[i];
			}
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_SRESTRICTION:
		if(lpPropValSrc->__union && lpPropValSrc->Value.res) {
			// NOTE: we place the object pointer in lpszA to make sure it's on the same offset as Value.x on 32bit as 64bit machines
			ECAllocateMore(sizeof(SRestriction), lpBase, (void **) &lpPropValDst->Value.lpszA);
			hr = CopySOAPRestrictionToMAPIRestriction((LPSRestriction)lpPropValDst->Value.lpszA, lpPropValSrc->Value.res, lpBase, lpConverter);
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	case PT_ACTIONS: {
		if(lpPropValSrc->__union && lpPropValSrc->Value.actions) {
			// NOTE: we place the object pointer in lpszA to make sure it's on the same offset as Value.x on 32bit as 64bit machines
			ACTIONS *lpDstActions;
			ECAllocateMore(sizeof(ACTIONS), lpBase, (void **)&lpPropValDst->Value.lpszA);
			lpDstActions = (ACTIONS *)lpPropValDst->Value.lpszA;

			lpDstActions->cActions = lpPropValSrc->Value.actions->__size;
			ECAllocateMore(sizeof(ACTION) * lpPropValSrc->Value.actions->__size, lpBase, (void **)&lpDstActions->lpAction);

			lpDstActions->ulVersion = EDK_RULES_VERSION;

			for(int i=0;i<lpPropValSrc->Value.actions->__size;i++) {
				ACTION *lpDstAction = &lpDstActions->lpAction[i];
				struct action *lpSrcAction = &lpPropValSrc->Value.actions->__ptr[i];

				lpDstAction->acttype = (ACTTYPE)lpSrcAction->acttype;
				lpDstAction->ulActionFlavor = lpSrcAction->flavor;
				lpDstAction->ulFlags = lpSrcAction->flags;
				lpDstAction->lpRes = NULL;
				lpDstAction->lpPropTagArray = NULL;

				switch(lpSrcAction->acttype) {
					case OP_MOVE:
					case OP_COPY:
						lpDstAction->actMoveCopy.cbStoreEntryId = lpSrcAction->act.moveCopy.store.__size;
						ECAllocateMore(lpSrcAction->act.moveCopy.store.__size, lpBase, (void **)&lpDstAction->actMoveCopy.lpStoreEntryId);
						memcpy(lpDstAction->actMoveCopy.lpStoreEntryId, lpSrcAction->act.moveCopy.store.__ptr, lpSrcAction->act.moveCopy.store.__size);

						lpDstAction->actMoveCopy.cbFldEntryId = lpSrcAction->act.moveCopy.folder.__size;
						ECAllocateMore(lpSrcAction->act.moveCopy.folder.__size, lpBase, (void **)&lpDstAction->actMoveCopy.lpFldEntryId);
						memcpy(lpDstAction->actMoveCopy.lpFldEntryId, lpSrcAction->act.moveCopy.folder.__ptr, lpSrcAction->act.moveCopy.folder.__size);
						break;
					case OP_REPLY:
					case OP_OOF_REPLY:
						lpDstAction->actReply.cbEntryId = lpSrcAction->act.reply.message.__size;
						ECAllocateMore(lpSrcAction->act.reply.message.__size, lpBase, (void **)&lpDstAction->actReply.lpEntryId);
						memcpy(lpDstAction->actReply.lpEntryId, lpSrcAction->act.reply.message.__ptr, lpSrcAction->act.reply.message.__size);

						if(lpSrcAction->act.reply.guid.__size != sizeof(GUID)) {
							hr = MAPI_E_CORRUPT_DATA;
							goto exit;
						}
						memcpy((void *)&lpDstAction->actReply.guidReplyTemplate, lpSrcAction->act.reply.guid.__ptr, lpSrcAction->act.reply.guid.__size);
						break;
					case OP_DEFER_ACTION:
						ECAllocateMore(lpSrcAction->act.defer.bin.__size, lpBase, (void **)&lpDstAction->actDeferAction.pbData);
						lpDstAction->actDeferAction.cbData = lpSrcAction->act.defer.bin.__size;
						memcpy(lpDstAction->actDeferAction.pbData, lpSrcAction->act.defer.bin.__ptr,lpSrcAction->act.defer.bin.__size);
						break;
					case OP_BOUNCE:
						lpDstAction->scBounceCode = lpSrcAction->act.bouncecode;
						break;
					case OP_FORWARD:
					case OP_DELEGATE:
						if(lpSrcAction->act.adrlist == NULL) {
							hr = MAPI_E_CORRUPT_DATA;
							goto exit;
						}

						ECAllocateMore(CbNewSRowSet(lpSrcAction->act.adrlist->__size), lpBase, (void**)&lpDstAction->lpadrlist);

						lpDstAction->lpadrlist->cEntries = lpSrcAction->act.adrlist->__size;

						for(int j=0; j < lpSrcAction->act.adrlist->__size; j++) {
							lpDstAction->lpadrlist->aEntries[j].ulReserved1 = 0;
							lpDstAction->lpadrlist->aEntries[j].cValues = lpSrcAction->act.adrlist->__ptr[j].__size;

							// new rowset allocate more on old rowset, so we can just call FreeProws once
							ECAllocateMore(sizeof(SPropValue) * lpSrcAction->act.adrlist->__ptr[j].__size, lpBase, (void **)&lpDstAction->lpadrlist->aEntries[j].rgPropVals);

							hr = CopySOAPRowToMAPIRow(&lpSrcAction->act.adrlist->__ptr[j], lpDstAction->lpadrlist->aEntries[j].rgPropVals, lpBase, lpConverter);
							if(hr != hrSuccess)
								goto exit;
						}
						// FIXME rowset is not coupled to action -> leaks!

						break;
					case OP_TAG:
						hr = CopySOAPPropValToMAPIPropVal(&lpDstAction->propTag, lpSrcAction->act.prop, lpBase, lpConverter);
						break;
				}
			}
		}else {
			lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
			lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		}
		break;
	}
	default:
		lpPropValDst->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropValSrc->ulPropTag));
		lpPropValDst->Value.err = MAPI_E_NOT_FOUND;
		break;
	}

exit:
	return hr;
}

HRESULT CopySOAPRowToMAPIRow(void* lpProvider, struct propValArray *lpsRowSrc, LPSPropValue lpsRowDst, void **lpBase, ULONG ulType, convert_context *lpConverter)
{
	HRESULT hr = hrSuccess;
	int j=0;

	if (lpConverter == NULL && lpsRowSrc->__size > 1) {
		// Try again with a converter to reuse the iconv instances
		convert_context converter;
		hr = CopySOAPRowToMAPIRow(lpProvider, lpsRowSrc, lpsRowDst, lpBase, ulType, &converter);
		goto exit;
	}

	for(j=0;j<lpsRowSrc->__size;j++) {
		// First, try the default TableRowGetProp from ECMAPIProp
		if((ulType == MAPI_STORE || ulType == MAPI_FOLDER || ulType == MAPI_MESSAGE || ulType == MAPI_ATTACH) &&
			ECMAPIProp::TableRowGetProp(lpProvider, &lpsRowSrc->__ptr[j], &lpsRowDst[j], lpBase, ulType) == erSuccess)
			continue;
		else if((ulType == MAPI_MAILUSER || ulType == MAPI_ABCONT || ulType == MAPI_DISTLIST)&&
			ECABProp::TableRowGetProp(lpProvider, &lpsRowSrc->__ptr[j], &lpsRowDst[j], lpBase, ulType) == erSuccess)
			continue;

		switch(ulType) {
			case MAPI_FOLDER:
				// Then, try the specialized TableRowGetProp for the type of table we're handling
				if(ECMAPIFolder::TableRowGetProp(lpProvider, &lpsRowSrc->__ptr[j], &lpsRowDst[j], lpBase, ulType) == erSuccess)
					continue;
				break;
			case MAPI_MESSAGE:
				if(ECMessage::TableRowGetProp(lpProvider, &lpsRowSrc->__ptr[j], &lpsRowDst[j], lpBase, ulType) == erSuccess)
					continue;
				break;
			case MAPI_MAILUSER:
				if(ECMailUser::TableRowGetProp(lpProvider, &lpsRowSrc->__ptr[j], &lpsRowDst[j], lpBase, ulType) == erSuccess)
					continue;
				break;
			case MAPI_DISTLIST:
				if(ECDistList::TableRowGetProp(lpProvider, &lpsRowSrc->__ptr[j], &lpsRowDst[j], lpBase, ulType) == erSuccess)
					continue;
				break;
			case MAPI_ABCONT:
				if(ECABContainer::TableRowGetProp(lpProvider, &lpsRowSrc->__ptr[j], &lpsRowDst[j], lpBase, ulType) == erSuccess)
					continue;
				break;
			case MAPI_STORE:
				if (ECMsgStore::TableRowGetProp(lpProvider, &lpsRowSrc->__ptr[j], &lpsRowDst[j], lpBase, ulType) == erSuccess)
					continue;
				break;
		}

		if (ECGenericProp::TableRowGetProp(lpProvider, &lpsRowSrc->__ptr[j], &lpsRowDst[j], lpBase, ulType) == erSuccess)
			continue;

		// If all fails, get the actual data from the server
		CopySOAPPropValToMAPIPropVal(&lpsRowDst[j], &lpsRowSrc->__ptr[j], lpBase, lpConverter);
	}

exit:
	return hr;
}

HRESULT CopySOAPEntryId(entryId *lpSrc, entryId* lpDst)
{
	HRESULT hr = hrSuccess;

	if (!lpSrc || !lpDst) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	lpDst->__size = lpSrc->__size;
	lpDst->__ptr = new unsigned char[lpDst->__size];
	memcpy(lpDst->__ptr, lpSrc->__ptr, lpDst->__size);

exit:
	return hr;
}

HRESULT CopyMAPIEntryIdToSOAPEntryId(ULONG cbEntryIdSrc, LPENTRYID lpEntryIdSrc, entryId** lppDest)
{
	HRESULT hr = hrSuccess;

	entryId* lpDest = new entryId;

	hr = CopyMAPIEntryIdToSOAPEntryId(cbEntryIdSrc, lpEntryIdSrc, lpDest, false);
	if(hr != hrSuccess)
		goto exit;

	*lppDest = lpDest;
exit:
	if(hr != hrSuccess)
		delete lpDest;

	return hr;
}

HRESULT CopyMAPIEntryIdToSOAPEntryId(ULONG cbEntryIdSrc, LPENTRYID lpEntryIdSrc, entryId* lpDest, bool bCheapCopy)
{
	HRESULT hr = hrSuccess;

	if((cbEntryIdSrc > 0 && lpEntryIdSrc == NULL) || lpDest == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if(cbEntryIdSrc == 0)	{
		lpDest->__ptr = NULL;
		lpDest->__size = 0;
		goto exit;
	}

	if(bCheapCopy == false) {
		lpDest->__ptr = new unsigned char[cbEntryIdSrc];
		memcpy(lpDest->__ptr, lpEntryIdSrc, cbEntryIdSrc);
	}else{
		lpDest->__ptr = (LPBYTE)lpEntryIdSrc;
	}

	lpDest->__size = cbEntryIdSrc;

exit:
	return hr;
}

HRESULT CopySOAPEntryIdToMAPIEntryId(entryId* lpSrc, ULONG* lpcbDest, LPENTRYID* lppEntryIdDest, void* lpBase)
{
	HRESULT		hr = hrSuccess;
	LPENTRYID	lpEntryId = NULL;

	if(lpSrc == NULL || lpcbDest == NULL || lppEntryIdDest == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if(lpSrc->__size == 0) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	if(lpBase)
		hr = ECAllocateMore(lpSrc->__size, lpBase, (void**)&lpEntryId);
	else
		hr = ECAllocateBuffer(lpSrc->__size, (void**)&lpEntryId);
	if(hr != hrSuccess)
		goto exit;

	memcpy(lpEntryId, lpSrc->__ptr, lpSrc->__size);

	*lppEntryIdDest = lpEntryId;
	*lpcbDest = lpSrc->__size;

exit:
	return hr;
}

HRESULT CopySOAPEntryIdToMAPIEntryId(entryId* lpSrc, ULONG ulObjId, ULONG ulType, ULONG* lpcbDest, LPENTRYID* lppEntryIdDest, void *lpBase)
{
	HRESULT		hr = hrSuccess;
	ULONG		cbEntryId = 0;
	LPENTRYID	lpEntryId = NULL;

	if(lpSrc == NULL || lpcbDest == NULL || lppEntryIdDest == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if((unsigned int)lpSrc->__size < CbNewABEID("") || lpSrc->__ptr == NULL)
	{
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	if (lpBase != NULL)
		hr = MAPIAllocateMore(lpSrc->__size, lpBase, (void**)&lpEntryId);
	else
		hr = MAPIAllocateBuffer(lpSrc->__size, (void**)&lpEntryId);

	if (hr != hrSuccess)
		goto exit;

	memcpy(lpEntryId, lpSrc->__ptr, lpSrc->__size);
	cbEntryId = lpSrc->__size;

	*lppEntryIdDest = lpEntryId;
	*lpcbDest = cbEntryId;

exit:
	return hr;
}

HRESULT CopySOAPEntryIdToMAPIEntryId(entryId* lpSrc, ULONG ulObjId, ULONG* lpcbDest, LPENTRYID* lppEntryIdDest, void *lpBase)
{
	return CopySOAPEntryIdToMAPIEntryId(lpSrc, ulObjId, MAPI_MAILUSER, lpcbDest, lppEntryIdDest, lpBase);
}

HRESULT CopyMAPIEntryListToSOAPEntryList(ENTRYLIST *lpMsgList, struct entryList* lpsEntryList)
{
	HRESULT hr = hrSuccess;
	unsigned int i = 0;

	if(lpMsgList == NULL || lpsEntryList == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if(lpMsgList->cValues == 0 || lpMsgList->lpbin == NULL)	{
		lpsEntryList->__ptr = NULL;
		lpsEntryList->__size = 0;
		goto exit;
	}

	lpsEntryList->__ptr = new entryId[lpMsgList->cValues];

	for(i=0; i < lpMsgList->cValues; i++) {
		lpsEntryList->__ptr[i].__ptr = new unsigned char[lpMsgList->lpbin[i].cb];

		memcpy(lpsEntryList->__ptr[i].__ptr, lpMsgList->lpbin[i].lpb, lpMsgList->lpbin[i].cb);

		lpsEntryList->__ptr[i].__size = lpMsgList->lpbin[i].cb;
	}

	lpsEntryList->__size = i;

exit:
	return hr;
}

HRESULT CopySOAPEntryListToMAPIEntryList(struct entryList* lpsEntryList, LPENTRYLIST* lppMsgList)
{
	HRESULT			hr = hrSuccess;
	unsigned int	i = 0;
	ENTRYLIST*		lpMsgList = NULL;

	if(lpsEntryList == NULL || lppMsgList == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = ECAllocateBuffer(sizeof(ENTRYLIST), (void**)&lpMsgList);
	if(hr != hrSuccess)
		goto exit;

	if(lpsEntryList->__size == 0) {
		lpMsgList->cValues = 0;
		lpMsgList->lpbin = NULL;
	} else {

		hr = ECAllocateMore(lpsEntryList->__size * sizeof(SBinary), lpMsgList, (void**)&lpMsgList->lpbin);
		if(hr != hrSuccess)
			goto exit;
	}

	for(i=0; i<lpsEntryList->__size; i++) {

		hr = ECAllocateMore(lpsEntryList->__ptr[i].__size, lpMsgList, (void**)&lpMsgList->lpbin[i].lpb);
		if(hr != hrSuccess)
			goto exit;

		memcpy(lpMsgList->lpbin[i].lpb, lpsEntryList->__ptr[i].__ptr, lpsEntryList->__ptr[i].__size);

		lpMsgList->lpbin[i].cb = lpsEntryList->__ptr[i].__size;

	}

	lpMsgList->cValues = i;

	*lppMsgList = lpMsgList;

exit:
	if(hr != hrSuccess && lpMsgList)
		ECFreeBuffer(lpMsgList);

	return hr;
}

HRESULT CopySOAPRowToMAPIRow(struct propValArray *lpsRowSrc, LPSPropValue lpsRowDst, void *lpBase, convert_context *lpConverter)
{
	HRESULT hr = hrSuccess;
	int j=0;

	if (lpConverter == NULL && lpsRowSrc->__size > 1) {
		convert_context converter;
		hr = CopySOAPRowToMAPIRow(lpsRowSrc, lpsRowDst, lpBase, &converter);
		goto exit;
	}

	for(j=0;j<lpsRowSrc->__size;j++) {

		// If all fails, get the actual data from the server
		hr = CopySOAPPropValToMAPIPropVal(&lpsRowDst[j], &lpsRowSrc->__ptr[j], lpBase, lpConverter);
		if(hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}

HRESULT CopyMAPIRowToSOAPRow(LPSRow lpRowSrc, struct propValArray *lpsRowDst, convert_context *lpConverter)
{
	HRESULT hr = hrSuccess;
	struct propVal* lpPropVal = NULL;

	if (lpConverter == NULL && lpRowSrc->cValues > 1) {
		convert_context converter;
		hr = CopyMAPIRowToSOAPRow(lpRowSrc, lpsRowDst, &converter);
		goto exit;
	}

	lpPropVal = new struct propVal[lpRowSrc->cValues];
	memset(lpPropVal, 0, sizeof(struct propVal) *lpRowSrc->cValues);

	for(unsigned int i=0; i<lpRowSrc->cValues; i++) {
		hr = CopyMAPIPropValToSOAPPropVal(&lpPropVal[i], &lpRowSrc->lpProps[i], lpConverter);
		if(hr != hrSuccess)
			goto exit;
	}

	lpsRowDst->__ptr = lpPropVal;
	lpsRowDst->__size = lpRowSrc->cValues;

exit:
	//@todo: remove memory on an error

	return hr;
}

HRESULT CopyMAPIRowSetToSOAPRowSet(LPSRowSet lpRowSetSrc, struct rowSet **lppsRowSetDst, convert_context *lpConverter)
{
	HRESULT hr = hrSuccess;
	struct rowSet *lpsRowSetDst = NULL;

	if (lpConverter == NULL && lpRowSetSrc->cRows > 1) {
		convert_context converter;
		hr = CopyMAPIRowSetToSOAPRowSet(lpRowSetSrc, lppsRowSetDst, &converter);
		goto exit;
	}

	lpsRowSetDst = new struct rowSet;

	lpsRowSetDst->__ptr = new propValArray[lpRowSetSrc->cRows];
	lpsRowSetDst->__size = lpRowSetSrc->cRows;

	for(unsigned int i=0; i < lpRowSetSrc->cRows; i++)
	{
		hr = CopyMAPIRowToSOAPRow(&lpRowSetSrc->aRow[i], &lpsRowSetDst->__ptr[i], lpConverter);
		if(hr != hrSuccess)
			goto exit;
	}

	*lppsRowSetDst = lpsRowSetDst;

exit:
	//@todo: remove memory on an error

	return hr;
}

// Copies a row set, filling in client-side generated values on the fly
HRESULT CopySOAPRowSetToMAPIRowSet(void* lpProvider, struct rowSet *lpsRowSetSrc, LPSRowSet *lppRowSetDst, ULONG ulType)
{
	HRESULT hr = hrSuccess;
	ULONG ulRows = 0;
	LPSRowSet lpRowSet = NULL;
	ULONG i=0;
	convert_context converter;

	ulRows = lpsRowSetSrc->__size;

	// Allocate space for the rowset
	ECAllocateBuffer(CbNewSRowSet(ulRows), (void **)&lpRowSet);
	lpRowSet->cRows = ulRows;

	// Loop through all the rows and values, fill in any client-side generated values, or translate
	// some serverside values through TableRowGetProps

	for(i=0;i<lpRowSet->cRows;i++) {
		lpRowSet->aRow[i].ulAdrEntryPad = 0;
		lpRowSet->aRow[i].cValues = lpsRowSetSrc->__ptr[i].__size;
		ECAllocateBuffer(sizeof(SPropValue) * lpsRowSetSrc->__ptr[i].__size, (void **)&lpRowSet->aRow[i].lpProps);
		CopySOAPRowToMAPIRow(lpProvider, &lpsRowSetSrc->__ptr[i], lpRowSet->aRow[i].lpProps, (void **)lpRowSet->aRow[i].lpProps, ulType, &converter);
	}

	*lppRowSetDst = lpRowSet;

	return hr;
}

HRESULT CopySOAPRestrictionToMAPIRestriction(LPSRestriction lpDst, struct restrictTable *lpSrc, void *lpBase, convert_context *lpConverter)
{
	HRESULT hr = hrSuccess;
	unsigned int i=0;

	if(lpSrc == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpConverter == NULL) {
		convert_context converter;
		CopySOAPRestrictionToMAPIRestriction(lpDst, lpSrc, lpBase, &converter);
		goto exit;
	}

	memset(lpDst, 0, sizeof(SRestriction));
	lpDst->rt = lpSrc->ulType;

	switch(lpSrc->ulType) {
	case RES_OR:
		if(lpSrc->lpOr == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		lpDst->res.resOr.cRes = lpSrc->lpOr->__size;
		ECAllocateMore(sizeof(SRestriction) * lpSrc->lpOr->__size, lpBase, (void **) &lpDst->res.resOr.lpRes);

		for(i=0;i<lpSrc->lpOr->__size;i++) {
			hr = CopySOAPRestrictionToMAPIRestriction(&lpDst->res.resOr.lpRes[i], lpSrc->lpOr->__ptr[i], lpBase, lpConverter);

			if(hr != hrSuccess)
				break;
		}
		break;

	case RES_AND:
		if(lpSrc->lpAnd == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		lpDst->res.resAnd.cRes = lpSrc->lpAnd->__size;
		ECAllocateMore(sizeof(SRestriction) * lpSrc->lpAnd->__size, lpBase, (void **) &lpDst->res.resAnd.lpRes);

		for(unsigned i=0;i<lpSrc->lpAnd->__size;i++) {
			hr = CopySOAPRestrictionToMAPIRestriction(&lpDst->res.resAnd.lpRes[i], lpSrc->lpAnd->__ptr[i], lpBase, lpConverter);

			if(hr != hrSuccess)
				break;
		}
		break;

	case RES_BITMASK:
		if(lpSrc->lpBitmask == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		lpDst->res.resBitMask.relBMR = lpSrc->lpBitmask->ulType;
		lpDst->res.resBitMask.ulMask = lpSrc->lpBitmask->ulMask;
		lpDst->res.resBitMask.ulPropTag = lpSrc->lpBitmask->ulPropTag;
		break;

	case RES_COMMENT:
		if (lpSrc->lpComment == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		hr = ECAllocateMore(sizeof(SRestriction), lpBase, (void **) &lpDst->res.resComment.lpRes);
		if (hr != hrSuccess)
			goto exit;
		hr = CopySOAPRestrictionToMAPIRestriction(lpDst->res.resComment.lpRes, lpSrc->lpComment->lpResTable, lpBase, lpConverter);
		if (hr != hrSuccess)
			goto exit;

		lpDst->res.resComment.cValues = lpSrc->lpComment->sProps.__size;
		hr = ECAllocateMore(sizeof(SPropValue) * lpSrc->lpComment->sProps.__size, lpBase, (void **)&lpDst->res.resComment.lpProp);
		if (hr != hrSuccess)
			goto exit;
		for (int i=0; i<lpSrc->lpComment->sProps.__size; i++) {
			hr = CopySOAPPropValToMAPIPropVal(&lpDst->res.resComment.lpProp[i], &lpSrc->lpComment->sProps.__ptr[i], lpBase, lpConverter);
			if (hr != hrSuccess)
				break;
		}
		break;

	case RES_COMPAREPROPS:
		if(lpSrc->lpCompare == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		lpDst->res.resCompareProps.relop = lpSrc->lpCompare->ulType;
		lpDst->res.resCompareProps.ulPropTag1 = lpSrc->lpCompare->ulPropTag1;
		lpDst->res.resCompareProps.ulPropTag2 = lpSrc->lpCompare->ulPropTag2;
		break;

	case RES_CONTENT:
		if(lpSrc->lpContent == NULL || lpSrc->lpContent->lpProp == NULL)  {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		lpDst->res.resContent.ulFuzzyLevel = lpSrc->lpContent->ulFuzzyLevel;
		lpDst->res.resContent.ulPropTag = lpSrc->lpContent->ulPropTag;

		hr = ECAllocateMore(sizeof(SPropValue), lpBase, (void **) &lpDst->res.resContent.lpProp);
		if(hr != hrSuccess)
			goto exit;

		hr = CopySOAPPropValToMAPIPropVal(lpDst->res.resContent.lpProp, lpSrc->lpContent->lpProp, lpBase, lpConverter);
		if(hr != hrSuccess)
			goto exit;
		break;

	case RES_EXIST:
		if(lpSrc->lpExist == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		lpDst->res.resExist.ulPropTag = lpSrc->lpExist->ulPropTag;
		break;

	case RES_NOT:
		if(lpSrc->lpNot == NULL || lpSrc->lpNot->lpNot == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		ECAllocateMore(sizeof(SRestriction), lpBase, (void **)&lpDst->res.resNot.lpRes);

		hr = CopySOAPRestrictionToMAPIRestriction(lpDst->res.resNot.lpRes, lpSrc->lpNot->lpNot, lpBase, lpConverter);

		break;

	case RES_PROPERTY:
		if(lpSrc->lpProp == NULL || lpSrc->lpProp->lpProp == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		ECAllocateMore(sizeof(SPropValue), lpBase, (void **)&lpDst->res.resProperty.lpProp);

		lpDst->res.resProperty.relop = lpSrc->lpProp->ulType;
		lpDst->res.resProperty.ulPropTag = lpSrc->lpProp->ulPropTag;

		hr = CopySOAPPropValToMAPIPropVal(lpDst->res.resProperty.lpProp, lpSrc->lpProp->lpProp, lpBase, lpConverter);

		break;

	case RES_SIZE:
		if(lpSrc->lpSize == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		lpDst->res.resSize.cb = lpSrc->lpSize->cb;
		lpDst->res.resSize.relop = lpSrc->lpSize->ulType;
		lpDst->res.resSize.ulPropTag = lpSrc->lpSize->ulPropTag;
		break;

	case RES_SUBRESTRICTION:
		if(lpSrc->lpSub == NULL || lpSrc->lpSub->lpSubObject == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		lpDst->res.resSub.ulSubObject = lpSrc->lpSub->ulSubObject;
		ECAllocateMore(sizeof(SRestriction), lpBase, (void **)&lpDst->res.resSub.lpRes);

		hr = CopySOAPRestrictionToMAPIRestriction(lpDst->res.resSub.lpRes, lpSrc->lpSub->lpSubObject, lpBase, lpConverter);
		break;

	default:
		hr = MAPI_E_INVALID_PARAMETER;
		break;
	}

exit:
	return hr;

}

HRESULT CopyMAPIRestrictionToSOAPRestriction(struct restrictTable **lppDst, LPSRestriction lpSrc, convert_context *lpConverter)
{
	HRESULT hr = hrSuccess;
	struct restrictTable *lpDst = NULL;
	unsigned int i=0;

	if (lpConverter == NULL) {
		convert_context converter;
		hr = CopyMAPIRestrictionToSOAPRestriction(lppDst, lpSrc, &converter);
		goto exit;
	}

	lpDst = new struct restrictTable;
	memset(lpDst, 0, sizeof(restrictTable));
	lpDst->ulType = lpSrc->rt;

	switch(lpSrc->rt) {
	case RES_OR:
		lpDst->lpOr = new restrictOr;
		memset(lpDst->lpOr,0,sizeof(restrictOr));

		lpDst->lpOr->__ptr = new restrictTable *[lpSrc->res.resOr.cRes];
		memset(lpDst->lpOr->__ptr, 0, sizeof(restrictTable*) * lpSrc->res.resOr.cRes);
		lpDst->lpOr->__size = lpSrc->res.resOr.cRes;

		for(i=0;i<lpSrc->res.resOr.cRes;i++) {
			hr = CopyMAPIRestrictionToSOAPRestriction(&(lpDst->lpOr->__ptr[i]), &lpSrc->res.resOr.lpRes[i], lpConverter);

			if(hr != hrSuccess)
				goto exit;
		}
		break;

	case RES_AND:
		lpDst->lpAnd = new restrictAnd;
		memset(lpDst->lpAnd,0,sizeof(restrictAnd));

		lpDst->lpAnd->__ptr = new restrictTable *[lpSrc->res.resAnd.cRes];
		memset(lpDst->lpAnd->__ptr, 0, sizeof(restrictTable*) * lpSrc->res.resAnd.cRes);
		lpDst->lpAnd->__size = lpSrc->res.resAnd.cRes;

		for(i=0;i<lpSrc->res.resAnd.cRes;i++) {
			hr = CopyMAPIRestrictionToSOAPRestriction(&lpDst->lpAnd->__ptr[i], &lpSrc->res.resAnd.lpRes[i], lpConverter);

			if(hr != hrSuccess)
				goto exit;
		}
		break;

	case RES_BITMASK:
		lpDst->lpBitmask = new restrictBitmask;
		memset(lpDst->lpBitmask, 0, sizeof(restrictBitmask));

		lpDst->lpBitmask->ulMask = lpSrc->res.resBitMask.ulMask;
		lpDst->lpBitmask->ulPropTag = lpSrc->res.resBitMask.ulPropTag;
		lpDst->lpBitmask->ulType = lpSrc->res.resBitMask.relBMR;
		break;

	case RES_COMMENT:
		lpDst->lpComment = new restrictComment;
		memset(lpDst->lpComment, 0, sizeof(restrictComment));

		lpDst->lpComment->sProps.__ptr = new propVal[lpSrc->res.resComment.cValues];
		lpDst->lpComment->sProps.__size = lpSrc->res.resComment.cValues;
		for (unsigned int i=0; i < lpSrc->res.resComment.cValues; i++) {
			hr = CopyMAPIPropValToSOAPPropVal(&lpDst->lpComment->sProps.__ptr[i], &lpSrc->res.resComment.lpProp[i], lpConverter);
			if(hr != hrSuccess)
				goto exit;
		}

		hr = CopyMAPIRestrictionToSOAPRestriction(&lpDst->lpComment->lpResTable, lpSrc->res.resComment.lpRes, lpConverter);
		if (hr != hrSuccess)
			goto exit;
		break;

	case RES_COMPAREPROPS:
		lpDst->lpCompare = new restrictCompare;
		memset(lpDst->lpCompare, 0, sizeof(restrictCompare));

		lpDst->lpCompare->ulPropTag1 = lpSrc->res.resCompareProps.ulPropTag1;
		lpDst->lpCompare->ulPropTag2 = lpSrc->res.resCompareProps.ulPropTag2;
		lpDst->lpCompare->ulType = lpSrc->res.resCompareProps.relop;
		break;

	case RES_CONTENT:
		lpDst->lpContent = new restrictContent;
		memset(lpDst->lpContent, 0, sizeof(restrictContent));

		if( (PROP_TYPE(lpSrc->res.resContent.lpProp->ulPropTag) != PT_BINARY &&
			PROP_TYPE(lpSrc->res.resContent.lpProp->ulPropTag) != PT_MV_BINARY &&
			PROP_TYPE(lpSrc->res.resContent.lpProp->ulPropTag) != PT_STRING8 &&
			PROP_TYPE(lpSrc->res.resContent.lpProp->ulPropTag) != PT_MV_STRING8 &&
			PROP_TYPE(lpSrc->res.resContent.lpProp->ulPropTag) != PT_UNICODE &&
			PROP_TYPE(lpSrc->res.resContent.lpProp->ulPropTag) != PT_MV_UNICODE) ||
			(PROP_TYPE(lpSrc->res.resContent.lpProp->ulPropTag) == PT_BINARY && lpSrc->res.resContent.lpProp->Value.bin.cb >0 && lpSrc->res.resContent.lpProp->Value.bin.lpb == NULL) ||
			(PROP_TYPE(lpSrc->res.resContent.lpProp->ulPropTag) == PT_STRING8 && lpSrc->res.resContent.lpProp->Value.lpszA == NULL) ||
			(PROP_TYPE(lpSrc->res.resContent.lpProp->ulPropTag) == PT_UNICODE && lpSrc->res.resContent.lpProp->Value.lpszW == NULL)) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		lpDst->lpContent->ulFuzzyLevel = lpSrc->res.resContent.ulFuzzyLevel;
		lpDst->lpContent->ulPropTag = lpSrc->res.resContent.ulPropTag;

		lpDst->lpContent->lpProp = new struct propVal;
		memset(lpDst->lpContent->lpProp, 0, sizeof(propVal));

		hr = CopyMAPIPropValToSOAPPropVal(lpDst->lpContent->lpProp, lpSrc->res.resContent.lpProp, lpConverter);
		if(hr != hrSuccess)
			goto exit;
		break;

	case RES_EXIST:
		lpDst->lpExist = new restrictExist;
		memset(lpDst->lpExist, 0, sizeof(restrictExist));

		lpDst->lpExist->ulPropTag = lpSrc->res.resExist.ulPropTag;
		break;

	case RES_NOT:
		lpDst->lpNot = new restrictNot;
		memset(lpDst->lpNot, 0, sizeof(restrictNot));
		hr = CopyMAPIRestrictionToSOAPRestriction(&lpDst->lpNot->lpNot, lpSrc->res.resNot.lpRes, lpConverter);
		if(hr != hrSuccess)
			goto exit;
		break;

	case RES_PROPERTY:
		lpDst->lpProp = new restrictProp;
		memset(lpDst->lpProp, 0, sizeof(restrictProp));

		lpDst->lpProp->ulType = lpSrc->res.resProperty.relop;
		lpDst->lpProp->lpProp = new struct propVal;
		memset(lpDst->lpProp->lpProp, 0, sizeof(propVal));
		lpDst->lpProp->ulPropTag = lpSrc->res.resProperty.ulPropTag;

		hr = CopyMAPIPropValToSOAPPropVal(lpDst->lpProp->lpProp, lpSrc->res.resProperty.lpProp, lpConverter);
		if(hr != hrSuccess)
			goto exit;
		break;

	case RES_SIZE:
		lpDst->lpSize = new restrictSize;
		memset(lpDst->lpSize, 0, sizeof(restrictSize));

		lpDst->lpSize->cb = lpSrc->res.resSize.cb;
		lpDst->lpSize->ulPropTag = lpSrc->res.resSize.ulPropTag;
		lpDst->lpSize->ulType = lpSrc->res.resSize.relop;
		break;

	case RES_SUBRESTRICTION:
		lpDst->lpSub = new restrictSub;
		memset(lpDst->lpSub, 0, sizeof(restrictSub));

		lpDst->lpSub->ulSubObject = lpSrc->res.resSub.ulSubObject;
		hr = CopyMAPIRestrictionToSOAPRestriction(&lpDst->lpSub->lpSubObject, lpSrc->res.resSub.lpRes, lpConverter);
		if(hr != hrSuccess)
			goto exit;
		break;

	default:
		hr = MAPI_E_INVALID_PARAMETER;
		if(hr != hrSuccess)
			goto exit;
		break;
	}

	*lppDst = lpDst;

exit:
	if(hr != hrSuccess && lpDst != NULL)
		FreeRestrictTable(lpDst);

	return hr;
}

HRESULT CopySOAPPropTagArrayToMAPIPropTagArray(struct propTagArray* lpsPropTagArray, LPSPropTagArray* lppPropTagArray, void* lpBase)
{
	HRESULT			hr = hrSuccess;
	LPSPropTagArray	lpPropTagArray = NULL;

	if(lpsPropTagArray == NULL || lppPropTagArray == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if(lpBase)
		hr = ECAllocateMore(CbNewSPropTagArray(lpsPropTagArray->__size), lpBase, (void**)&lpPropTagArray);
	else
		hr = ECAllocateBuffer(CbNewSPropTagArray(lpsPropTagArray->__size), (void**)&lpPropTagArray);
	if(hr != hrSuccess)
		goto exit;

	lpPropTagArray->cValues = lpsPropTagArray->__size;

	if(lpsPropTagArray->__size > 0)
		memcpy(lpPropTagArray->aulPropTag, lpsPropTagArray->__ptr, sizeof(unsigned int)*lpsPropTagArray->__size);

	*lppPropTagArray = lpPropTagArray;

exit:
	return hr;
}

HRESULT Utf8ToTString(LPCSTR lpszUtf8, ULONG ulFlags, LPVOID lpBase, convert_context *lpConverter, LPTSTR *lppszTString)
{
	HRESULT	hr = hrSuccess;
	std::string strDest;
	size_t cbDest;

	if (lpszUtf8 == NULL || lppszTString == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	strDest = CONVERT_TO(lpConverter, std::string, ((ulFlags & MAPI_UNICODE) ? CHARSET_WCHAR : CHARSET_CHAR), lpszUtf8, rawsize(lpszUtf8), "UTF-8");
	cbDest = strDest.length() + ((ulFlags & MAPI_UNICODE) ? sizeof(WCHAR) : sizeof(CHAR));

	if (lpBase)
		hr = ECAllocateMore(cbDest, lpBase, (LPVOID*)lppszTString);
	else
		hr = ECAllocateBuffer(cbDest, (LPVOID*)lppszTString);

	if (hr != hrSuccess)
		goto exit;

	memset(*lppszTString, 0, cbDest);
	memcpy(*lppszTString, strDest.c_str(), strDest.length());


exit:
	return hr;
}

HRESULT TStringToUtf8(LPCTSTR lpszTstring, ULONG ulFlags, LPVOID lpBase, convert_context *lpConverter, LPSTR *lppszUtf8)
{
	HRESULT	hr = hrSuccess;
	std::string strDest;
	size_t cbDest;

	if (lpszTstring == NULL || lppszUtf8 == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (ulFlags & MAPI_UNICODE)
		strDest = CONVERT_TO(lpConverter, std::string, "UTF-8", (wchar_t*)lpszTstring, rawsize((wchar_t*)lpszTstring), CHARSET_WCHAR);
	else
		strDest = CONVERT_TO(lpConverter, std::string, "UTF-8", (char*)lpszTstring, rawsize((char*)lpszTstring), CHARSET_CHAR);
	cbDest = strDest.length() + 1;

	if (lpBase)
		hr = ECAllocateMore(cbDest, lpBase, (LPVOID*)lppszUtf8);
	else
		hr = ECAllocateBuffer(cbDest, (LPVOID*)lppszUtf8);

	if (hr != hrSuccess)
		goto exit;

	memcpy(*lppszUtf8, strDest.c_str(), cbDest);

exit:
	return hr;
}

HRESULT CopyABPropsFromSoap(struct propmapPairArray *lpsoapPropmap, struct propmapMVPairArray *lpsoapMVPropmap,
							SPROPMAP *lpPropmap, MVPROPMAP *lpMVPropmap, void *lpBase, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	unsigned int nLen = 0;
	convert_context converter;

	if (lpsoapPropmap != NULL) {
		lpPropmap->cEntries = lpsoapPropmap->__size;
		nLen = sizeof(*lpPropmap->lpEntries) * lpPropmap->cEntries;
		hr = ECAllocateMore(nLen, lpBase, (void**)&lpPropmap->lpEntries);
		if (hr != hrSuccess)
			goto exit;

		for (unsigned int i = 0; i < lpsoapPropmap->__size; i++) {
			lpPropmap->lpEntries[i].ulPropId = CHANGE_PROP_TYPE(lpsoapPropmap->__ptr[i].ulPropId, ((ulFlags & MAPI_UNICODE) ? PT_UNICODE : PT_STRING8));
			hr = Utf8ToTString(lpsoapPropmap->__ptr[i].lpszValue, ulFlags, lpBase, &converter, &lpPropmap->lpEntries[i].lpszValue);
			if (hr != hrSuccess)
				goto exit;
		}
	}

	if (lpsoapMVPropmap != NULL) {
		lpMVPropmap->cEntries = lpsoapMVPropmap->__size;
		hr = ECAllocateMore(sizeof(*lpMVPropmap->lpEntries) * lpMVPropmap->cEntries, lpBase, (void**)&lpMVPropmap->lpEntries);
		if (hr != hrSuccess)
			goto exit;

		for (unsigned int i = 0; i < lpsoapMVPropmap->__size; i++) {
			lpMVPropmap->lpEntries[i].ulPropId = CHANGE_PROP_TYPE(lpsoapMVPropmap->__ptr[i].ulPropId, ((ulFlags & MAPI_UNICODE) ? PT_MV_UNICODE : PT_MV_STRING8));

			lpMVPropmap->lpEntries[i].cValues = lpsoapMVPropmap->__ptr[i].sValues.__size;
			nLen = sizeof(*lpMVPropmap->lpEntries[i].lpszValues) * lpMVPropmap->lpEntries[i].cValues;
			hr = ECAllocateMore(nLen, lpBase, (void**)&lpMVPropmap->lpEntries[i].lpszValues);
			if (hr != hrSuccess)
				goto exit;

			for (int j = 0; j < lpsoapMVPropmap->__ptr[i].sValues.__size; j++) {
				hr = Utf8ToTString(lpsoapMVPropmap->__ptr[i].sValues.__ptr[j], ulFlags, lpBase, &converter, &lpMVPropmap->lpEntries[i].lpszValues[j]);
				if (hr != hrSuccess)
					goto exit;
			}
		}
	}

exit:
	return hr;
}

HRESULT CopyABPropsToSoap(SPROPMAP *lpPropmap, MVPROPMAP *lpMVPropmap, ULONG ulFlags, 
						  struct propmapPairArray **lppsoapPropmap, struct propmapMVPairArray **lppsoapMVPropmap)
{
	HRESULT hr = hrSuccess;
	struct propmapPairArray *soapPropmap = NULL;
	struct propmapMVPairArray *soapMVPropmap = NULL;
	convert_context	converter;


	if (lpPropmap && lpPropmap->cEntries) {
		hr = ECAllocateBuffer(sizeof *soapPropmap, (void**)&soapPropmap);
		if (hr != hrSuccess)
			goto exit;

		soapPropmap->__size = lpPropmap->cEntries;
		hr = ECAllocateMore(soapPropmap->__size * sizeof *soapPropmap->__ptr, soapPropmap, (void**)&soapPropmap->__ptr);
		if (hr != hrSuccess)
			goto exit;

		for (unsigned int i = 0; i < soapPropmap->__size; i++) {
			soapPropmap->__ptr[i].ulPropId = CHANGE_PROP_TYPE(lpPropmap->lpEntries[i].ulPropId, PT_STRING8);
			hr = TStringToUtf8(lpPropmap->lpEntries[i].lpszValue, ulFlags, soapPropmap, &converter, &soapPropmap->__ptr[i].lpszValue);
			if (hr != hrSuccess)
				goto exit;
		}
	}

	if (lpMVPropmap && lpMVPropmap->cEntries) {
		hr = ECAllocateBuffer(sizeof *soapMVPropmap, (void**)&soapMVPropmap);
		if (hr != hrSuccess)
			goto exit;

		soapMVPropmap->__size = lpMVPropmap->cEntries;
		hr = ECAllocateMore(soapMVPropmap->__size * sizeof *soapMVPropmap->__ptr, soapMVPropmap, (void**)&soapMVPropmap->__ptr);
		if (hr != hrSuccess)
			goto exit;

		for (unsigned int i = 0; i < soapMVPropmap->__size; i++) {
			soapMVPropmap->__ptr[i].ulPropId = CHANGE_PROP_TYPE(lpMVPropmap->lpEntries[i].ulPropId, PT_MV_STRING8);
			soapMVPropmap->__ptr[i].sValues.__size = lpMVPropmap->lpEntries[i].cValues;
			hr = ECAllocateMore(soapMVPropmap->__ptr[i].sValues.__size * sizeof * soapMVPropmap->__ptr[i].sValues.__ptr, soapMVPropmap, (void**)&soapMVPropmap->__ptr[i].sValues.__ptr);
			if (hr != hrSuccess)
				goto exit;

			for (int j = 0; j < soapMVPropmap->__ptr[i].sValues.__size; j++) {
				hr = TStringToUtf8(lpMVPropmap->lpEntries[i].lpszValues[j], ulFlags, soapMVPropmap, &converter, &soapMVPropmap->__ptr[i].sValues.__ptr[j]);
				if (hr != hrSuccess)
					goto exit;
			}
		}
	}

	if (lppsoapPropmap) {
		*lppsoapPropmap = soapPropmap;
		soapPropmap = NULL;
	}
	if (lppsoapMVPropmap) {
		*lppsoapMVPropmap = soapMVPropmap;
		soapMVPropmap = NULL;
	}

exit:
	if (soapPropmap)
		ECFreeBuffer(soapPropmap);
	if (soapMVPropmap)
		ECFreeBuffer(soapMVPropmap);

	return hr;
}

HRESULT FreeABProps(struct propmapPairArray *lpsoapPropmap, struct propmapMVPairArray *lpsoapMVPropmap)
{
	if (lpsoapPropmap)
		ECFreeBuffer(lpsoapPropmap);

	if (lpsoapMVPropmap)
		ECFreeBuffer(lpsoapMVPropmap);

	return hrSuccess;
}

HRESULT SoapUserToUser(struct user *lpUser, LPECUSER lpsUser, ULONG ulFlags, void *lpBase, convert_context &converter)
{
	HRESULT 	hr		= hrSuccess;

	if (lpUser == NULL || lpsUser == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpBase == NULL)
		lpBase = lpsUser;

	memset(lpsUser, 0, sizeof(*lpsUser));

	hr = Utf8ToTString(lpUser->lpszUsername, ulFlags, lpBase, &converter, &lpsUser->lpszUsername);

	if (hr == hrSuccess && lpUser->lpszFullName != NULL)
		hr = Utf8ToTString(lpUser->lpszFullName, ulFlags, lpBase, &converter, &lpsUser->lpszFullName);

	if (hr == hrSuccess && lpUser->lpszMailAddress != NULL)
		hr = Utf8ToTString(lpUser->lpszMailAddress, ulFlags, lpBase, &converter, &lpsUser->lpszMailAddress);

	if (hr == hrSuccess && lpUser->lpszServername != NULL)
		hr = Utf8ToTString(lpUser->lpszServername, ulFlags, lpBase, &converter, &lpsUser->lpszServername);

	if (hr != hrSuccess)
		goto exit;

	hr = CopyABPropsFromSoap(lpUser->lpsPropmap, lpUser->lpsMVPropmap,
							 &lpsUser->sPropmap, &lpsUser->sMVPropmap, lpBase, ulFlags);
	if (hr != hrSuccess)
		goto exit;

	hr = CopySOAPEntryIdToMAPIEntryId(&lpUser->sUserId, lpUser->ulUserId, (ULONG*)&lpsUser->sUserId.cb, (LPENTRYID*)&lpsUser->sUserId.lpb, lpBase);
	if (hr != hrSuccess)
		goto exit;

	lpsUser->ulIsAdmin		= lpUser->ulIsAdmin;
	lpsUser->ulIsABHidden	= lpUser->ulIsABHidden;
	lpsUser->ulCapacity		= lpUser->ulCapacity;

	/**
	 * If we're talking to a pre 6.40 server we won't get a object class,
	 * only an is-non-active flag. Luckily we don't have to support that.
	 * However, a 6.40.0 server will put the object class information in
	 * that is-non-active field. We (6.40.1 and up) expect the object class
	 * information in a dedicated object class field, and reverted the
	 * is-non-active field to its original usage.
	 *
	 * We can easily determine what's the case here:
	 *  If ulClass is missing (value == 0), we're dealing with a pre 6.40.1
	 *  server. In that case the ulIsNonActive either contains is-non-active
	 *  information or an object class. We can distinguish this since an
	 *  object class has data in the high 16-bit of its value, the
	 *  is-non-active field is either 0 or 1.
	 *  If we detect a class, we put the class in the ulClass field.
	 *  If we detect a is-non-active, we'll simply return an error since we're
	 *  not required to be able to communicate with a pre 6.40.
	 *  We could guess things here, but why bother?
	 */
	if (lpUser->ulObjClass == 0) {
		if (OBJECTCLASS_TYPE(lpUser->ulIsNonActive) != 0)
			lpsUser->ulObjClass = (objectclass_t)lpUser->ulIsNonActive;	// ulIsNonActive itself will be ignored by the offline server.

		else {
			hr = MAPI_E_UNABLE_TO_COMPLETE;
			goto exit;
		}
	} else
		lpsUser->ulObjClass = (objectclass_t)lpUser->ulObjClass;

exit:
	return hr;
}

HRESULT SoapUserArrayToUserArray(struct userArray* lpUserArray, ULONG ulFlags, ULONG *lpcUsers, LPECUSER* lppsUsers)
{
	HRESULT 		hr = hrSuccess;
	LPECUSER 		lpECUsers = NULL;
	unsigned 		int i = 0;
	convert_context	converter;

	if(lpUserArray == NULL || lpcUsers == NULL || lppsUsers == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	ECAllocateBuffer(sizeof(ECUSER) * lpUserArray->__size, (void**)&lpECUsers);
	memset(lpECUsers, 0, sizeof(ECUSER) * lpUserArray->__size);

	for(i=0; i < lpUserArray->__size; i++) {
		hr = SoapUserToUser(lpUserArray->__ptr + i, lpECUsers + i, ulFlags, lpECUsers, converter);
		if (hr != hrSuccess)
			goto exit;
	}

	*lppsUsers = lpECUsers;
	*lpcUsers = lpUserArray->__size;

exit:
	return hr;
}

HRESULT SoapUserToUser(struct user *lpUser, ULONG ulFlags, LPECUSER *lppsUser)
{
	HRESULT			hr		= hrSuccess;
	LPECUSER		lpsUser	= NULL;
	convert_context	converter;

	if (lpUser == NULL || lppsUser == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = ECAllocateBuffer(sizeof *lpsUser, (void**)&lpsUser);
	if (hr != hrSuccess)
		goto exit;

	hr = SoapUserToUser(lpUser, lpsUser, ulFlags, NULL, converter);
	if (hr != hrSuccess)
		goto exit;

	*lppsUser = lpsUser;
	lpsUser = NULL;

exit:
	if (lpsUser != NULL)
		ECFreeBuffer(lpsUser);

	return hr;
}

HRESULT SoapGroupToGroup(struct group *lpGroup, LPECGROUP lpsGroup, ULONG ulFlags, void *lpBase, convert_context &converter)
{
	HRESULT 	hr = hrSuccess;

	if (lpGroup == NULL || lpsGroup == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpBase == NULL)
		lpBase = lpsGroup;

	memset(lpsGroup, 0, sizeof(*lpsGroup));

	if (lpGroup->lpszGroupname == NULL)
	{
		hr = MAPI_E_INVALID_OBJECT;
		goto exit;
	}

	hr = Utf8ToTString(lpGroup->lpszGroupname, ulFlags, lpBase, &converter, &lpsGroup->lpszGroupname);

	if (hr == hrSuccess && lpGroup->lpszFullname)
		hr = Utf8ToTString(lpGroup->lpszFullname, ulFlags, lpBase, &converter, &lpsGroup->lpszFullname);

	if (hr == hrSuccess && lpGroup->lpszFullEmail) 
		hr = Utf8ToTString(lpGroup->lpszFullEmail, ulFlags, lpBase, &converter, &lpsGroup->lpszFullEmail);

	if (hr != hrSuccess)
		goto exit;

	hr = CopyABPropsFromSoap(lpGroup->lpsPropmap, lpGroup->lpsMVPropmap,
							 &lpsGroup->sPropmap, &lpsGroup->sMVPropmap, lpBase, ulFlags);
	if (hr != hrSuccess)
		goto exit;


	hr = CopySOAPEntryIdToMAPIEntryId(&lpGroup->sGroupId, lpGroup->ulGroupId, (ULONG*)&lpsGroup->sGroupId.cb, (LPENTRYID*)&lpsGroup->sGroupId.lpb);
	if (hr != hrSuccess)
		goto exit;

	lpsGroup->ulIsABHidden	= lpGroup->ulIsABHidden;

exit:
	return hr;
}

HRESULT SoapGroupArrayToGroupArray(struct groupArray* lpGroupArray, ULONG ulFlags, ULONG *lpcGroups, LPECGROUP *lppsGroups)
{
	HRESULT			hr = hrSuccess;
	unsigned int	i;
	LPECGROUP		lpECGroups = NULL;
	convert_context	converter;

	if(lpGroupArray == NULL || lpcGroups == NULL || lppsGroups == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	ECAllocateBuffer(sizeof(ECGROUP) * lpGroupArray->__size, (void**)&lpECGroups);
	memset(lpECGroups, 0, sizeof(ECGROUP) * lpGroupArray->__size);

	for(i=0; i < lpGroupArray->__size; i++) {
		hr = SoapGroupToGroup(lpGroupArray->__ptr + i, lpECGroups + i, ulFlags, lpECGroups, converter);
		if (hr != hrSuccess)
			goto exit;
	}

	*lppsGroups = lpECGroups;
	*lpcGroups = lpGroupArray->__size;
	lpECGroups = NULL;

exit:
	if (lpECGroups != NULL)
		ECFreeBuffer(lpECGroups);

	return hr;
}

HRESULT SoapGroupToGroup(struct group *lpGroup, ULONG ulFlags, LPECGROUP *lppsGroup)
{
	HRESULT			hr			= hrSuccess;
	LPECGROUP		lpsGroup	= NULL;
	convert_context	converter;

	if (lpGroup == NULL || lppsGroup == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = ECAllocateBuffer(sizeof *lpsGroup, (void**)&lpsGroup);
	if (hr != hrSuccess)
		goto exit;

	hr = SoapGroupToGroup(lpGroup, lpsGroup, ulFlags, NULL, converter);
	if (hr != hrSuccess)
		goto exit;

	*lppsGroup = lpsGroup;
	lpsGroup = NULL;

exit:
	if (lpsGroup != NULL)
		ECFreeBuffer(lpsGroup);

	return hr;
}

HRESULT SoapCompanyToCompany(struct company *lpCompany, LPECCOMPANY lpsCompany, ULONG ulFlags, void *lpBase, convert_context &converter)
{
	HRESULT 	hr		= hrSuccess;

	if (lpCompany == NULL || lpsCompany == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpBase == NULL)
		lpBase = lpsCompany;

	memset(lpsCompany, 0, sizeof(*lpsCompany));

	hr = Utf8ToTString(lpCompany->lpszCompanyname, ulFlags, lpBase, &converter, &lpsCompany->lpszCompanyname);

	if (hr == hrSuccess && lpCompany->lpszServername != NULL)
		hr = Utf8ToTString(lpCompany->lpszServername, ulFlags, lpBase, &converter, &lpsCompany->lpszServername);

	if (hr != hrSuccess)
		goto exit;

	hr = CopyABPropsFromSoap(lpCompany->lpsPropmap, lpCompany->lpsMVPropmap,
							 &lpsCompany->sPropmap, &lpsCompany->sMVPropmap, lpBase, ulFlags);
	if (hr != hrSuccess)
		goto exit;

	hr = CopySOAPEntryIdToMAPIEntryId(&lpCompany->sAdministrator, lpCompany->ulAdministrator, (ULONG*)&lpsCompany->sAdministrator.cb, (LPENTRYID*)&lpsCompany->sAdministrator.lpb, lpBase);
	if (hr != hrSuccess)
		goto exit;

	hr = CopySOAPEntryIdToMAPIEntryId(&lpCompany->sCompanyId, lpCompany->ulCompanyId, (ULONG*)&lpsCompany->sCompanyId.cb, (LPENTRYID*)&lpsCompany->sCompanyId.lpb, lpBase);
	if (hr != hrSuccess)
		goto exit;

	lpsCompany->ulIsABHidden	= lpCompany->ulIsABHidden;

exit:
	return hr;
}

HRESULT SoapCompanyArrayToCompanyArray(struct companyArray* lpCompanyArray, ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY *lppsCompanies)
{
	HRESULT 		hr = hrSuccess;
	LPECCOMPANY 	lpECCompanies = NULL;
	convert_context	converter;

	if (lpCompanyArray == NULL || lpcCompanies == NULL || lppsCompanies == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	ECAllocateBuffer(sizeof(ECCOMPANY) * lpCompanyArray->__size, (void**)&lpECCompanies);
	memset(lpECCompanies, 0, sizeof(ECCOMPANY) * lpCompanyArray->__size);

	for (unsigned int i=0; i < lpCompanyArray->__size; i++) {
		hr = SoapCompanyToCompany(&lpCompanyArray->__ptr[i], lpECCompanies + i, ulFlags, lpECCompanies, converter);
		if (hr != hrSuccess)
			goto exit;
	}

	*lppsCompanies = lpECCompanies;
	*lpcCompanies = lpCompanyArray->__size;
	lpECCompanies = NULL;

exit:
	if (lpECCompanies != NULL)
		ECFreeBuffer(lpECCompanies);

	return hr;
}

HRESULT SoapCompanyToCompany(struct company *lpCompany, ULONG ulFlags, LPECCOMPANY *lppsCompany)
{
	HRESULT			hr			= hrSuccess;
	LPECCOMPANY		lpsCompany	= NULL;
	convert_context	converter;

	if (lpCompany == NULL || lppsCompany == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = ECAllocateBuffer(sizeof *lpsCompany, (void**)&lpsCompany);
	if (hr != hrSuccess)
		goto exit;

	hr = SoapCompanyToCompany(lpCompany, lpsCompany, ulFlags, NULL, converter);
	if (hr != hrSuccess)
		goto exit;

	*lppsCompany = lpsCompany;
	lpsCompany = NULL;

exit:
	if (lpsCompany != NULL)
		ECFreeBuffer(lpsCompany);

	return hr;
}

HRESULT SvrNameListToSoapMvString8(LPECSVRNAMELIST lpSvrNameList, ULONG ulFlags, struct mv_string8 **lppsSvrNameList)
{
	HRESULT				hr = hrSuccess;
	struct mv_string8	*lpsSvrNameList = NULL;
	convert_context		converter;

	if (lpSvrNameList == NULL || lppsSvrNameList == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	hr = ECAllocateBuffer(sizeof *lpsSvrNameList, (void**)&lpsSvrNameList);
	if (hr != hrSuccess)
		goto exit;
	memset(lpsSvrNameList, 0, sizeof *lpsSvrNameList);
	
	if (lpSvrNameList->cServers > 0) {
		lpsSvrNameList->__size = lpSvrNameList->cServers;
		hr = ECAllocateMore(lpSvrNameList->cServers * sizeof *lpsSvrNameList->__ptr, lpsSvrNameList, (void**)&lpsSvrNameList->__ptr);
		if (hr != hrSuccess)
			goto exit;
		memset(lpsSvrNameList->__ptr, 0, lpSvrNameList->cServers * sizeof *lpsSvrNameList->__ptr);
		
		for (unsigned i = 0; i < lpSvrNameList->cServers; ++i) {
			hr = TStringToUtf8(lpSvrNameList->lpszaServer[i], ulFlags, lpSvrNameList, &converter, &lpsSvrNameList->__ptr[i]);
			if (hr != hrSuccess)
				goto exit;
		}
	}
	
	*lppsSvrNameList = lpsSvrNameList;
	lpsSvrNameList = NULL;
	
exit:
	if (lpsSvrNameList && hr != hrSuccess)
		ECFreeBuffer(lpsSvrNameList);
		
	return hr;
}

HRESULT SoapServerListToServerList(struct serverList *lpsServerList, ULONG ulFLags, LPECSERVERLIST *lppServerList)
{
	HRESULT			hr = hrSuccess;
	LPECSERVERLIST	lpServerList = NULL;
	convert_context	converter;

	if (lpsServerList == NULL || lppServerList == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	hr = ECAllocateBuffer(sizeof *lpServerList, (void**)&lpServerList);
	if (hr != hrSuccess)
		goto exit;
	memset(lpServerList, 0, sizeof *lpServerList);
	
	if (lpsServerList->__size > 0 && lpsServerList->__ptr != NULL) {
		lpServerList->cServers = lpsServerList->__size;
		hr = ECAllocateMore(lpsServerList->__size * sizeof *lpServerList->lpsaServer, lpServerList, (void**)&lpServerList->lpsaServer);
		if (hr != hrSuccess)
			goto exit;
		memset(lpServerList->lpsaServer, 0, lpsServerList->__size * sizeof *lpServerList->lpsaServer);
		
		for (unsigned i = 0; i < lpsServerList->__size; ++i) {
			// Flags
			lpServerList->lpsaServer[i].ulFlags = lpsServerList->__ptr[i].ulFlags;
		
			// Name
			if (lpsServerList->__ptr[i].lpszName != NULL) {
				hr = Utf8ToTString(lpsServerList->__ptr[i].lpszName, ulFLags, lpServerList, &converter, &lpServerList->lpsaServer[i].lpszName);
				if (hr != hrSuccess)
					goto exit;
			}
			
			// FilePath
			if (lpsServerList->__ptr[i].lpszFilePath != NULL) {
				hr = Utf8ToTString(lpsServerList->__ptr[i].lpszFilePath, ulFLags, lpServerList, &converter, &lpServerList->lpsaServer[i].lpszFilePath);
				if (hr != hrSuccess)
					goto exit;
			}
			
			// HttpPath
			if (lpsServerList->__ptr[i].lpszHttpPath != NULL) {
				hr = Utf8ToTString(lpsServerList->__ptr[i].lpszHttpPath, ulFLags, lpServerList, &converter, &lpServerList->lpsaServer[i].lpszHttpPath);
				if (hr != hrSuccess)
					goto exit;
			}
			
			// SslPath
			if (lpsServerList->__ptr[i].lpszSslPath != NULL) {
				hr = Utf8ToTString(lpsServerList->__ptr[i].lpszSslPath, ulFLags, lpServerList, &converter, &lpServerList->lpsaServer[i].lpszSslPath);
				if (hr != hrSuccess)
					goto exit;
			}
			
			// PreferedPath
			if (lpsServerList->__ptr[i].lpszPreferedPath != NULL) {
				hr = Utf8ToTString(lpsServerList->__ptr[i].lpszPreferedPath, ulFLags, lpServerList, &converter, &lpServerList->lpsaServer[i].lpszPreferedPath);
				if (hr != hrSuccess)
					goto exit;
			}
		}
	}
	
	*lppServerList = lpServerList;
	lpServerList = NULL;
	
exit:
	if (lpServerList)
		ECFreeBuffer(lpServerList);

	return hr;
}

HRESULT CreateSoapTransport(ULONG ulUIFlags, sGlobalProfileProps sProfileProps, ZarafaCmd **lppCmd)
{
	return CreateSoapTransport(ulUIFlags,
							sProfileProps.strServerPath,
							sProfileProps.strSSLKeyFile,
							sProfileProps.strSSLKeyPass,
							sProfileProps.ulConnectionTimeOut,
							sProfileProps.strProxyHost,
							sProfileProps.ulProxyPort,
							sProfileProps.strProxyUserName,
							sProfileProps.strProxyPassword,
							sProfileProps.ulProxyFlags,
							SOAP_IO_KEEPALIVE | SOAP_C_UTFSTRING,
							SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_C_UTFSTRING,
							lppCmd);
}

// Wrap the server store entryid to client store entry. (Add a servername)
HRESULT WrapServerClientStoreEntry(const char* lpszServerName, entryId* lpsStoreId, ULONG* lpcbStoreID, LPENTRYID* lppStoreID)
{
	HRESULT		hr = hrSuccess;
	LPENTRYID	lpStoreID = NULL;
	ULONG		ulSize;

	if(lpsStoreId == NULL || lpszServerName == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// The new entryid size is, current size + servername size + 1 byte term 0 - 4 bytes padding
	ulSize = lpsStoreId->__size+strlen(lpszServerName)+1-4;

	hr = ECAllocateBuffer(ulSize, (void**)&lpStoreID);
	if(hr != hrSuccess)
		goto exit;

	memset(lpStoreID, 0, ulSize );

	//Copy the entryid without servername
	memcpy(lpStoreID, lpsStoreId->__ptr, lpsStoreId->__size);

	// Add the server name
	strcpy((char*)lpStoreID+(lpsStoreId->__size-4), lpszServerName);

	*lpcbStoreID = ulSize;
	*lppStoreID = lpStoreID;

exit:

	return hr;
}

// Un wrap the client store entryid to server store entry. (remove a servername)
HRESULT UnWrapServerClientStoreEntry(ULONG cbWrapStoreID, LPENTRYID lpWrapStoreID, ULONG* lpcbUnWrapStoreID, LPENTRYID* lppUnWrapStoreID)
{
	HRESULT	hr = hrSuccess;
	LPENTRYID lpUnWrapStoreID = NULL;
	PEID	peid = NULL;
	ULONG	ulSize = 0;

	if(lpWrapStoreID == NULL || lppUnWrapStoreID == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	//FIXME: check or it's an zarafa entry?

	peid = (PEID)lpWrapStoreID;

	if(peid->ulVersion == 0) {
		ulSize = sizeof(EID_V0);
	}else if(peid->ulVersion == 1) {
		ulSize = sizeof(EID);
	}else {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	if(cbWrapStoreID < ulSize) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	hr = ECAllocateBuffer(ulSize, (void**)&lpUnWrapStoreID);
	if(hr != hrSuccess)
		goto exit;

	memset(lpUnWrapStoreID, 0, ulSize);

	// Remove servername
	memcpy(lpUnWrapStoreID, lpWrapStoreID, ulSize-4);

	*lppUnWrapStoreID = lpUnWrapStoreID;
	*lpcbUnWrapStoreID = ulSize;

exit:
	return hr;
}

HRESULT UnWrapServerClientABEntry(ULONG cbWrapABID, LPENTRYID lpWrapABID, ULONG* lpcbUnWrapABID, LPENTRYID* lppUnWrapABID)
{
	HRESULT	hr = hrSuccess;
	LPENTRYID lpUnWrapABID = NULL;
	PABEID	pabeid = NULL;
	ULONG	ulSize = 0;

	if(lpWrapABID == NULL || lppUnWrapABID == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// Check minimum size of EntryID
	if (cbWrapABID < sizeof(ABEID)) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	//FIXME: check or it's an zarafa entry?

	pabeid = (PABEID)lpWrapABID;

	if(pabeid->ulVersion == 0) {
		ulSize = sizeof(ABEID);
	}else if(pabeid->ulVersion == 1) {
		ulSize = CbABEID(pabeid);
	}else {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	if(cbWrapABID < ulSize) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	hr = ECAllocateBuffer(ulSize, (void**)&lpUnWrapABID);
	if(hr != hrSuccess)
		goto exit;

	memset(lpUnWrapABID, 0, ulSize);

	// Remove servername
	memcpy(lpUnWrapABID, lpWrapABID, ulSize-4);

	*lppUnWrapABID = lpUnWrapABID;
	*lpcbUnWrapABID = ulSize;

exit:
	return hr;
}

HRESULT CopySOAPNotificationToMAPINotification(void *lpProvider, struct notification *lpSrc, LPNOTIFICATION *lppDst, convert_context *lpConverter) {
	HRESULT hr = hrSuccess;
	LPNOTIFICATION lpNotification = NULL;
	int nLen;

	ECAllocateBuffer(sizeof(NOTIFICATION), (void**)&lpNotification);
	memset(lpNotification, 0, sizeof(NOTIFICATION));

	lpNotification->ulEventType = lpSrc->ulEventType;

	switch(lpSrc->ulEventType){
		case fnevCriticalError:// ERROR_NOTIFICATION
			hr = MAPI_E_INVALID_PARAMETER;
			break;
		case fnevNewMail://NEWMAIL_NOTIFICATION
			if(lpSrc->newmail->pEntryId) {
				// Ignore error now
				// FIXME: This must exist, so maybe give an error or skip them
				CopySOAPEntryIdToMAPIEntryId(lpSrc->newmail->pEntryId, &lpNotification->info.newmail.cbEntryID, &lpNotification->info.newmail.lpEntryID, (void **)lpNotification);
			}
			if(lpSrc->newmail->pParentId) {
				// Ignore error
				CopySOAPEntryIdToMAPIEntryId(lpSrc->newmail->pParentId, &lpNotification->info.newmail.cbParentID, &lpNotification->info.newmail.lpParentID, (void **)lpNotification);
			}


			if(lpSrc->newmail->lpszMessageClass != NULL) {
				nLen = strlen(lpSrc->newmail->lpszMessageClass)+1;
				ECAllocateMore(nLen, lpNotification, (void**)&lpNotification->info.newmail.lpszMessageClass);
				memcpy(lpNotification->info.newmail.lpszMessageClass, lpSrc->newmail->lpszMessageClass, nLen);
			}

			lpNotification->info.newmail.ulFlags = 0;
			lpNotification->info.newmail.ulMessageFlags = lpSrc->newmail->ulMessageFlags;

			break;
		case fnevObjectCreated:// OBJECT_NOTIFICATION
		case fnevObjectDeleted:
		case fnevObjectModified:
		case fnevObjectCopied:
		case fnevObjectMoved:
		case fnevSearchComplete:
			// FIXME for each if statement below, check the ELSE .. we can't send a TABLE_ROW_ADDED without lpProps for example ..
			lpNotification->info.obj.ulObjType = lpSrc->obj->ulObjType;

			// All errors of CopySOAPEntryIdToMAPIEntryId are ignored
			if(lpSrc->obj->pEntryId != NULL) {
				CopySOAPEntryIdToMAPIEntryId(lpSrc->obj->pEntryId, &lpNotification->info.obj.cbEntryID, &lpNotification->info.obj.lpEntryID, (void **)lpNotification);
			}

			if(lpSrc->obj->pParentId != NULL) {
				CopySOAPEntryIdToMAPIEntryId(lpSrc->obj->pParentId, &lpNotification->info.obj.cbParentID, &lpNotification->info.obj.lpParentID, (void **)lpNotification);
			}

			if(lpSrc->obj->pOldId != NULL) {
				CopySOAPEntryIdToMAPIEntryId(lpSrc->obj->pOldId, &lpNotification->info.obj.cbOldID, &lpNotification->info.obj.lpOldID, (void **)lpNotification);
			}

			if(lpSrc->obj->pOldParentId != NULL){
				CopySOAPEntryIdToMAPIEntryId(lpSrc->obj->pOldParentId, &lpNotification->info.obj.cbOldParentID, &lpNotification->info.obj.lpOldParentID, (void **)lpNotification);
			}

			if(lpSrc->obj->pPropTagArray) {
				// ignore errors
				CopySOAPPropTagArrayToMAPIPropTagArray(lpSrc->obj->pPropTagArray, &lpNotification->info.obj.lpPropTagArray, (void **)lpNotification);
			}
			break;
		case fnevTableModified:// TABLE_NOTIFICATION
			lpNotification->info.tab.ulTableEvent = lpSrc->tab->ulTableEvent;
			lpNotification->info.tab.propIndex.ulPropTag = lpSrc->tab->propIndex.ulPropTag;

			if(lpSrc->tab->propIndex.Value.bin){
				lpNotification->info.tab.propIndex.Value.bin.cb = lpSrc->tab->propIndex.Value.bin->__size;
				ECAllocateMore(lpNotification->info.tab.propIndex.Value.bin.cb, lpNotification, (void**)&lpNotification->info.tab.propIndex.Value.bin.lpb);

				memcpy(lpNotification->info.tab.propIndex.Value.bin.lpb, lpSrc->tab->propIndex.Value.bin->__ptr, lpSrc->tab->propIndex.Value.bin->__size);
			}

			lpNotification->info.tab.propPrior.ulPropTag = lpSrc->tab->propPrior.ulPropTag;

			if(lpSrc->tab->propPrior.Value.bin){
				lpNotification->info.tab.propPrior.Value.bin.cb = lpSrc->tab->propPrior.Value.bin->__size;
				ECAllocateMore(lpNotification->info.tab.propPrior.Value.bin.cb, lpNotification, (void**)&lpNotification->info.tab.propPrior.Value.bin.lpb);


				memcpy(lpNotification->info.tab.propPrior.Value.bin.lpb, lpSrc->tab->propPrior.Value.bin->__ptr, lpSrc->tab->propPrior.Value.bin->__size);
			}

			if(lpSrc->tab->pRow)
			{

				lpNotification->info.tab.row.cValues = lpSrc->tab->pRow->__size;
				ECAllocateMore(sizeof(SPropValue)*lpNotification->info.tab.row.cValues, lpNotification, (void**)&lpNotification->info.tab.row.lpProps);

				CopySOAPRowToMAPIRow(lpProvider, lpSrc->tab->pRow, lpNotification->info.tab.row.lpProps, (void **)lpNotification, lpSrc->tab->ulObjType, lpConverter);

			}
			break;
		case fnevStatusObjectModified: // STATUS_OBJECT_NOTIFICATION
			hr = MAPI_E_INVALID_PARAMETER;
			break;
		case fnevExtended: // EXTENDED_NOTIFICATION
			hr = MAPI_E_INVALID_PARAMETER;
			break;
		default:
			hr = MAPI_E_INVALID_PARAMETER;
			break;
	}

	if(hr != hrSuccess)
		goto exit;

	*lppDst = lpNotification;
	lpNotification = NULL;

exit:
	if (lpNotification)
		MAPIFreeBuffer(lpNotification);

	return hr;
}

HRESULT CopySOAPChangeNotificationToSyncState(struct notification *lpSrc, LPSBinary *lppDst, void *lpBase)
{
	HRESULT hr = hrSuccess;
	LPSBinary lpSBinary = NULL;

	if (lpSrc->ulEventType != fnevZarafaIcsChange) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpBase == NULL)
		ECAllocateBuffer(sizeof *lpSBinary, (void**)&lpSBinary);
	else
		ECAllocateMore(sizeof *lpSBinary, lpBase, (void**)&lpSBinary);
	memset(lpSBinary, 0, sizeof *lpSBinary);

	lpSBinary->cb = lpSrc->ics->pSyncState->__size;

	if (lpBase == NULL)
		ECAllocateMore(lpSBinary->cb, lpSBinary, (void**)&lpSBinary->lpb);
	else
		ECAllocateMore(lpSBinary->cb, lpBase, (void**)&lpSBinary->lpb);

	memcpy(lpSBinary->lpb, lpSrc->ics->pSyncState->__ptr, lpSBinary->cb);

	*lppDst = lpSBinary;
	lpSBinary = NULL;

exit:
    if (lpSBinary)
		MAPIFreeBuffer(lpSBinary);

	return hr;
}

HRESULT CopyMAPISourceKeyToSoapSourceKey(SBinary *lpsMAPISourceKey, struct xsd__base64Binary *lpsSoapSourceKey, void *lpBase)
{
	HRESULT						hr = hrSuccess;
	struct xsd__base64Binary	sSoapSourceKey = {0};

	if (lpsMAPISourceKey == NULL || lpsSoapSourceKey == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	sSoapSourceKey.__size = (int)lpsMAPISourceKey->cb;
	if (lpBase)
		hr = MAPIAllocateMore(lpsMAPISourceKey->cb, lpBase, (void**)&sSoapSourceKey.__ptr);
	else
		hr = MAPIAllocateBuffer(lpsMAPISourceKey->cb, (void**)&sSoapSourceKey.__ptr);
	if (hr != hrSuccess)
		goto exit;

	memcpy(sSoapSourceKey.__ptr, lpsMAPISourceKey->lpb, lpsMAPISourceKey->cb);
	*lpsSoapSourceKey = sSoapSourceKey;

exit:
	return hr;
}

HRESULT CopyICSChangeToSOAPSourceKeys(ULONG cbChanges, ICSCHANGE *lpsChanges, sourceKeyPairArray **lppsSKPA)
{
	HRESULT				hr = hrSuccess;
	sourceKeyPairArray	*lpsSKPA = NULL;

	if (lpsChanges == NULL || lppsSKPA == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = MAPIAllocateBuffer(sizeof *lpsSKPA, (void**)&lpsSKPA);
	if (hr != hrSuccess)
		goto exit;
	memset(lpsSKPA, 0, sizeof *lpsSKPA);

	if (cbChanges > 0) {
		lpsSKPA->__size = cbChanges;

		hr = MAPIAllocateMore(cbChanges * sizeof *lpsSKPA->__ptr, lpsSKPA, (void**)&lpsSKPA->__ptr);
		if (hr != hrSuccess)
			goto exit;
		memset(lpsSKPA->__ptr, 0, cbChanges * sizeof *lpsSKPA->__ptr);

		for (unsigned i = 0; i < cbChanges; ++i) {
			hr = CopyMAPISourceKeyToSoapSourceKey(&lpsChanges[i].sSourceKey, &lpsSKPA->__ptr[i].sObjectKey, lpsSKPA);
			if (hr != hrSuccess)
				goto exit;

			hr = CopyMAPISourceKeyToSoapSourceKey(&lpsChanges[i].sParentSourceKey, &lpsSKPA->__ptr[i].sParentKey, lpsSKPA);
			if (hr != hrSuccess)
				goto exit;
		}
	}

	*lppsSKPA = lpsSKPA;
	lpsSKPA = NULL;

exit:
	if (lpsSKPA)
		MAPIFreeBuffer(lpsSKPA);

	return hr;
}

HRESULT CopyUserClientUpdateStatusFromSOAP(struct userClientUpdateStatusResponse &sUCUS, ULONG ulFlags, LPECUSERCLIENTUPDATESTATUS *lppECUCUS)
{
	HRESULT hr = hrSuccess;
	LPECUSERCLIENTUPDATESTATUS lpECUCUS = NULL;
	convert_context converter;

	hr = MAPIAllocateBuffer(sizeof(ECUSERCLIENTUPDATESTATUS), (void**)&lpECUCUS);
	if (hr != hrSuccess)
		goto exit;

	memset(lpECUCUS, 0, sizeof(ECUSERCLIENTUPDATESTATUS));
	lpECUCUS->ulTrackId = sUCUS.ulTrackId;
	lpECUCUS->tUpdatetime = sUCUS.tUpdatetime;
	lpECUCUS->ulStatus = sUCUS.ulStatus;

	if (sUCUS.lpszCurrentversion)
		hr = Utf8ToTString(sUCUS.lpszCurrentversion, ulFlags, lpECUCUS, &converter, &lpECUCUS->lpszCurrentversion);

	if (hr == hrSuccess && sUCUS.lpszLatestversion)
		hr = Utf8ToTString(sUCUS.lpszLatestversion, ulFlags, lpECUCUS, &converter, &lpECUCUS->lpszLatestversion);

	if (hr == hrSuccess && sUCUS.lpszComputername)
		hr = Utf8ToTString(sUCUS.lpszComputername,  ulFlags, lpECUCUS, &converter, &lpECUCUS->lpszComputername);

	if (hr != hrSuccess)
		goto exit;


	*lppECUCUS = lpECUCUS;
	lpECUCUS = NULL;

exit:
	if (lpECUCUS)
		MAPIFreeBuffer(lpECUCUS);

	return hr;
}

HRESULT ConvertString8ToUnicode(char *lpszA, WCHAR **lppszW, void *base, convert_context &converter)
{
	HRESULT hr = hrSuccess;
	wstring wide;
	WCHAR *lpszW = NULL;

	if (lpszA == NULL || lppszW == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	TryConvert(lpszA, wide);
	hr = ECAllocateMore((wide.length() +1) * sizeof(wstring::value_type), base, (void**)&lpszW);
	if (hr != hrSuccess)
		goto exit;
	wcscpy(lpszW, wide.c_str());
	*lppszW = lpszW;

exit:
	return hr;
}

HRESULT ConvertString8ToUnicode(LPSRestriction lpRestriction, void *base, convert_context &converter)
{
	HRESULT hr = hrSuccess;
	ULONG i;

	if (lpRestriction == NULL)
		goto exit;

	switch (lpRestriction->rt) {
	case RES_OR:
		for (i = 0; i < lpRestriction->res.resOr.cRes; i++) {
			hr = ConvertString8ToUnicode(&lpRestriction->res.resOr.lpRes[i], base, converter);
			if (hr != hrSuccess)
				goto exit;
		}
		break;
	case RES_AND:
		for (i = 0; i < lpRestriction->res.resAnd.cRes; i++) {
			hr = ConvertString8ToUnicode(&lpRestriction->res.resAnd.lpRes[i], base, converter);
			if (hr != hrSuccess)
				goto exit;
		}
		break;
	case RES_NOT:
		hr = ConvertString8ToUnicode(lpRestriction->res.resNot.lpRes, base, converter);
		if (hr != hrSuccess)
			goto exit;
		break;
	case RES_COMMENT:
		if (lpRestriction->res.resComment.lpRes) {
			hr = ConvertString8ToUnicode(lpRestriction->res.resComment.lpRes, base, converter);
			if (hr != hrSuccess)
				goto exit;
		}
		for (i = 0; i < lpRestriction->res.resComment.cValues; i++) {
			if (PROP_TYPE(lpRestriction->res.resComment.lpProp[i].ulPropTag) == PT_STRING8) {
				hr = ConvertString8ToUnicode(lpRestriction->res.resComment.lpProp[i].Value.lpszA, &lpRestriction->res.resComment.lpProp[i].Value.lpszW, base, converter);
				if (hr != hrSuccess)
					goto exit;
				lpRestriction->res.resComment.lpProp[i].ulPropTag = CHANGE_PROP_TYPE(lpRestriction->res.resComment.lpProp[i].ulPropTag, PT_UNICODE);
			}
		}
		break;
	case RES_COMPAREPROPS:
		break;
	case RES_CONTENT:
		if (PROP_TYPE(lpRestriction->res.resContent.ulPropTag) == PT_STRING8) {
			hr = ConvertString8ToUnicode(lpRestriction->res.resContent.lpProp->Value.lpszA, &lpRestriction->res.resContent.lpProp->Value.lpszW, base, converter);
			if (hr != hrSuccess)
				goto exit;
			lpRestriction->res.resContent.lpProp->ulPropTag = CHANGE_PROP_TYPE(lpRestriction->res.resContent.lpProp->ulPropTag, PT_UNICODE);
			lpRestriction->res.resContent.ulPropTag = CHANGE_PROP_TYPE(lpRestriction->res.resContent.ulPropTag, PT_UNICODE);
		}
		break;
	case RES_PROPERTY:
		if (PROP_TYPE(lpRestriction->res.resProperty.ulPropTag) == PT_STRING8) {
			hr = ConvertString8ToUnicode(lpRestriction->res.resProperty.lpProp->Value.lpszA, &lpRestriction->res.resProperty.lpProp->Value.lpszW, base, converter);
			if (hr != hrSuccess)
				goto exit;
			lpRestriction->res.resProperty.lpProp->ulPropTag = CHANGE_PROP_TYPE(lpRestriction->res.resProperty.lpProp->ulPropTag, PT_UNICODE);
			lpRestriction->res.resProperty.ulPropTag = CHANGE_PROP_TYPE(lpRestriction->res.resProperty.ulPropTag, PT_UNICODE);
		}
		break;
	case RES_SUBRESTRICTION:
		hr = ConvertString8ToUnicode(lpRestriction->res.resSub.lpRes, base, converter);
		if (hr != hrSuccess)
			goto exit;
		break;
	};

exit:
	return hr;
}

HRESULT ConvertString8ToUnicode(LPADRLIST lpAdrList, void *base, convert_context &converter)
{
	HRESULT hr = hrSuccess;

	if (lpAdrList == NULL)
		goto exit;

	for (ULONG c = 0; c < lpAdrList->cEntries; c++) {
		// treat as row
		hr = ConvertString8ToUnicode((LPSRow)&lpAdrList->aEntries[c], base, converter);
		if (hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}

HRESULT ConvertString8ToUnicode(ACTIONS* lpActions, void *base, convert_context &converter)
{
	HRESULT hr = hrSuccess;

	if (lpActions == NULL)
		goto exit;

	for (ULONG c = 0; c < lpActions->cActions; c++) {
		if (lpActions->lpAction[c].acttype == OP_FORWARD || lpActions->lpAction[c].acttype == OP_DELEGATE) {
			hr = ConvertString8ToUnicode(lpActions->lpAction[c].lpadrlist, base, converter);
			if (hr != hrSuccess)
				goto exit;
		}
	}

exit:
	return hr;
}

HRESULT ConvertString8ToUnicode(LPSRow lpRow, void *base, convert_context &converter)
{
	HRESULT hr = hrSuccess;

	if (lpRow == NULL)
		goto exit;

	for (ULONG c = 0; c < lpRow->cValues; c++) {
		if (PROP_TYPE(lpRow->lpProps[c].ulPropTag) == PT_SRESTRICTION) {
			hr = ConvertString8ToUnicode((LPSRestriction)lpRow->lpProps[c].Value.lpszA, base ? base : lpRow->lpProps, converter);
		} else if (PROP_TYPE(lpRow->lpProps[c].ulPropTag) == PT_ACTIONS) {
			hr = ConvertString8ToUnicode((ACTIONS*)lpRow->lpProps[c].Value.lpszA, base ? base : lpRow->lpProps, converter);
		} else if (base && PROP_TYPE(lpRow->lpProps[c].ulPropTag) == PT_STRING8) {
			// only for "base" items: eg. the lpadrlist data, not the PR_RULE_NAME from the top-level
			hr = ConvertString8ToUnicode(lpRow->lpProps[c].Value.lpszA, &lpRow->lpProps[c].Value.lpszW, base, converter);
			if (hr != hrSuccess)
				goto exit;
			lpRow->lpProps[c].ulPropTag = CHANGE_PROP_TYPE(lpRow->lpProps[c].ulPropTag, PT_UNICODE);
		}
		if (hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}

/** 
 * Converts PT_STRING8 to PT_UNICODE inside PT_SRESTRICTION and
 * PT_ACTION properties inside the rows
 * 
 * @param[in,out] lpRowSet Rowset to modify
 * 
 * @return MAPI Error code
 */
HRESULT ConvertString8ToUnicode(LPSRowSet lpRowSet)
{
	HRESULT hr = hrSuccess;
	convert_context converter;

	if (lpRowSet == NULL)
		goto exit;

	for (ULONG c = 0; c < lpRowSet->cRows; c++) {
		hr = ConvertString8ToUnicode(&lpRowSet->aRow[c], NULL, converter);
		if (hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}
