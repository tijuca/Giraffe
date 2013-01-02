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

#include <platform.h>

#include "StreamUtil.h"
#include "StorageUtil.h"
#include "ZarafaCmdUtil.h"
#include "ECAttachmentStorage.h"
#include "ECSecurity.h"
#include "ECSessionManager.h"
#include "ECGenProps.h"
#include "ECTPropsPurge.h"
#include "ECICS.h"

#include "charset/convert.h"
#include "charset/utf8string.h"

#include <ECFifoBuffer.h>
#include <ECSerializer.h>
#include <stringutil.h>
#include <mapitags.h>
#include <mapiext.h>
#include <mapidefs.h>
#include <edkmdb.h>

#include <set>
#include <map>
#include <string>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/*
	streams are as follows:
	  <version>
	  <message>

	messages are as follows:
	  <# of props>:32bit
	    <property>
	  <# of sub objects>:32bit
	    <subobject>


	properties are as follows:
	  <prop tag>:32bit
	  [<length in bytes>:32bit]
	  <data bytes>
	  [<guid, kind, [id|bytes+string>]


	subobjects are as follows:
	  <type>:32bit
	  <id>:32bit
	  <message>
*/

const static struct StreamCaps {
	bool bSupportUnicode;
} g_StreamCaps[] = {
	{ false },		// version 0
	{ true },		// version 1
};

#define STREAM_VERSION			1	// encode strings in UTF-8.
#define STREAM_CAPS_CURRENT		(&g_StreamCaps[STREAM_VERSION])

#define CHARSET_WIN1252	"WINDOWS-1252//TRANSLIT"

// External objects
extern ECSessionManager *g_lpSessionManager;	// ECServerEntrypoint.cpp

static inline bool operator<(const GUID &lhs, const GUID &rhs) {
	return memcmp(&lhs, &rhs, sizeof(GUID)) < 0;
}

// Helper class for mapping named properties from the stream to local proptags
class NamedPropertyMapper
{
public:
	NamedPropertyMapper(ECDatabase *lpDatabase);

	ECRESULT GetId(const GUID &guid, unsigned int ulNameId, unsigned int *lpId);
	ECRESULT GetId(const GUID &guid, const std::string &strNameString, unsigned int *lpId);

private:
	typedef std::pair<GUID,unsigned int> nameidkey_t;
	typedef std::pair<GUID,std::string> namestringkey_t;
	typedef std::map<nameidkey_t,unsigned int> nameidmap_t;
	typedef std::map<namestringkey_t,unsigned int> namestringmap_t;

	ECDatabase *m_lpDatabase;
	nameidmap_t m_mapNameIds;
	namestringmap_t m_mapNameStrings;
};


NamedPropertyMapper::NamedPropertyMapper(ECDatabase *lpDatabase)
	: m_lpDatabase(lpDatabase) 
{
	ASSERT(lpDatabase != NULL);
}

ECRESULT NamedPropertyMapper::GetId(const GUID &guid, unsigned int ulNameId, unsigned int *lpulId)
{
	ECRESULT er = erSuccess;

	std::string strQuery;
	DB_RESULT lpResult = NULL;
	DB_ROW lpRow = NULL;

	nameidkey_t key(guid, ulNameId);
	nameidmap_t::const_iterator i = m_mapNameIds.find(key);

	if (i != m_mapNameIds.end()) {
		*lpulId = i->second;
		goto exit;
	}

	// Check the database
	strQuery = 
		"SELECT id FROM names "
			"WHERE nameid=" + stringify(ulNameId) +
			" AND guid=" + m_lpDatabase->EscapeBinary((unsigned char*)&guid, sizeof(guid));

	er = m_lpDatabase->DoSelect(strQuery, &lpResult);
	if (er != erSuccess)
		goto exit;

	if ((lpRow = m_lpDatabase->FetchRow(lpResult)) != NULL) {
		if (lpRow[0] == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		*lpulId = atoui((char*)lpRow[0]) + 0x8501;
	} else {
		// Create the named property
		strQuery = 
			"INSERT INTO names (nameid, guid) "
				"VALUES(" + stringify(ulNameId) + "," + m_lpDatabase->EscapeBinary((unsigned char*)&guid, sizeof(guid)) + ")";
		
		er = m_lpDatabase->DoInsert(strQuery, lpulId);
		if (er != erSuccess)
			goto exit;

		*lpulId += 0x8501;
	}

	// *lpulId now contains the local propid, update the cache
	m_mapNameIds.insert(nameidmap_t::value_type(key, *lpulId));


exit:
	if (lpResult)
		m_lpDatabase->FreeResult(lpResult);

	return er;
}

ECRESULT NamedPropertyMapper::GetId(const GUID &guid, const std::string &strNameString, unsigned int *lpulId)
{
	ECRESULT er = erSuccess;

	std::string strQuery;
	DB_RESULT lpResult = NULL;
	DB_ROW lpRow = NULL;

	namestringkey_t key(guid, strNameString);
	namestringmap_t::const_iterator i = m_mapNameStrings.find(key);

	if (i != m_mapNameStrings.end()) {
		*lpulId = i->second;
		goto exit;
	}

	// Check the database
	strQuery = 
		"SELECT id FROM names "
			"WHERE namestring='" + m_lpDatabase->Escape(strNameString) + "'"
			" AND guid=" + m_lpDatabase->EscapeBinary((unsigned char*)&guid, sizeof(guid));

	er = m_lpDatabase->DoSelect(strQuery, &lpResult);
	if (er != erSuccess)
		goto exit;

	if ((lpRow = m_lpDatabase->FetchRow(lpResult)) != NULL) {
		if (lpRow[0] == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		*lpulId = atoui((char*)lpRow[0]) + 0x8501;
	} else {
		// Create the named property
		strQuery = 
			"INSERT INTO names (namestring, guid) "
				"VALUES('" + m_lpDatabase->Escape(strNameString) + "'," + m_lpDatabase->EscapeBinary((unsigned char*)&guid, sizeof(guid)) + ")";
		
		er = m_lpDatabase->DoInsert(strQuery, lpulId);
		if (er != erSuccess)
			goto exit;

		*lpulId += 0x8501;
	}

	// *lpulId now contains the local propid, update the cache
	m_mapNameStrings.insert(namestringmap_t::value_type(key, *lpulId));


exit:
	if (lpResult)
		m_lpDatabase->FreeResult(lpResult);

	return er;
}


// Utility Functions
ECRESULT SerializeDatabasePropVal(LPCSTREAMCAPS lpStreamCaps, DB_ROW lpRow, DB_LENGTHS lpLen, ECSerializer *lpSink)
{
	ECRESULT er = erSuccess;
	unsigned int type = 0;
	unsigned int ulPropTag = 0;
	unsigned int ulCount;
	unsigned int ulLen;
	unsigned int ulLastPos;
	unsigned int ulLastPos2;
	std::string	strData;

	unsigned int ulKind;
	unsigned int ulNameId;
	std::string strNameString;
	locale_t loc = createlocale(LC_NUMERIC, "C");
	convert_context converter;

	short i;
	unsigned int ul;
	float flt;
	unsigned char b;
	double dbl;
	hiloLong hilo;
	long long li;

	er = GetValidatedPropType(lpRow, &type);
	if (er == ZARAFA_E_DATABASE_ERROR) {
		er = erSuccess;
		goto exit;
	} else if (er != erSuccess)
		goto exit;

	ulPropTag = PROP_TAG(type, atoi(lpRow[FIELD_NR_TAG]));
	er = lpSink->Write(&ulPropTag, sizeof(ulPropTag), 1);
	if (er != erSuccess)
		goto exit;

	switch (type) {
	case PT_I2:
		if (lpRow[FIELD_NR_ULONG] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		i = (short)atoi(lpRow[FIELD_NR_ULONG]);
		er = lpSink->Write(&i, sizeof(i), 1);
		break;
	case PT_LONG:
		if (lpRow[FIELD_NR_ULONG] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		ul = atoui(lpRow[FIELD_NR_ULONG]);
		er = lpSink->Write(&ul, sizeof(ul), 1);
		break;
	case PT_R4:
		if (lpRow[FIELD_NR_DOUBLE] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		flt = (float)strtod_l(lpRow[FIELD_NR_DOUBLE], NULL, loc);
		er = lpSink->Write(&flt, sizeof(flt), 1);
		break;
	case PT_BOOLEAN:
		if (lpRow[FIELD_NR_ULONG] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		b = (atoi(lpRow[FIELD_NR_ULONG]) ? 1 : 0);
		er = lpSink->Write(&b, sizeof(b), 1);
		break;
	case PT_DOUBLE:
	case PT_APPTIME:
		if (lpRow[FIELD_NR_DOUBLE] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		dbl = strtod_l(lpRow[FIELD_NR_DOUBLE], NULL, loc);
		er = lpSink->Write(&dbl, sizeof(dbl), 1);
		break;
	case PT_CURRENCY:
	case PT_SYSTIME:
		if (lpRow[FIELD_NR_HI] == NULL || lpRow[FIELD_NR_LO] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		hilo.hi = atoi(lpRow[FIELD_NR_HI]);
		hilo.lo = atoui(lpRow[FIELD_NR_LO]);
		er = lpSink->Write(&hilo.hi, sizeof(hilo.hi), 1);
		if (er == erSuccess)
			er = lpSink->Write(&hilo.lo, sizeof(hilo.lo), 1);
		break;
	case PT_I8:
		if (lpRow[FIELD_NR_LONGINT] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		li = _atoi64(lpRow[FIELD_NR_LONGINT]);
		er = lpSink->Write(&li, sizeof(li), 1);
		break;
	case PT_STRING8:
	case PT_UNICODE:
		if (lpRow[FIELD_NR_STRING] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		if (lpStreamCaps->bSupportUnicode) {
			ulLen = lpLen[FIELD_NR_STRING];
			er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
			if (er == erSuccess)
				er = lpSink->Write(lpRow[FIELD_NR_STRING], 1, lpLen[FIELD_NR_STRING]);
		} else {
			const std::string strEncoded = converter.convert_to<std::string>(CHARSET_WIN1252, lpRow[FIELD_NR_STRING], lpLen[FIELD_NR_STRING], "UTF-8");
			ulLen = strEncoded.length();
			er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
			if (er == erSuccess)
				er = lpSink->Write(strEncoded.data(), 1, ulLen);
		}
		break;
	case PT_CLSID:
	case PT_BINARY:
		if (lpRow[FIELD_NR_BINARY] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		ulLen = lpLen[FIELD_NR_BINARY];
		er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
		if (er == erSuccess)
			er = lpSink->Write(lpRow[FIELD_NR_BINARY], 1, lpLen[FIELD_NR_BINARY]);
		break;
	case PT_MV_I2:
		if (lpRow[FIELD_NR_ULONG] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		ulCount = atoi(lpRow[FIELD_NR_ID]);
		er = lpSink->Write(&ulCount, sizeof(ulCount), 1);
		ulLastPos = 0;
		for (unsigned x = 0 ;er == erSuccess && x < ulCount; x++)
		{
			ParseMVProp(lpRow[FIELD_NR_ULONG], lpLen[FIELD_NR_ULONG], &ulLastPos, &strData);
			i = (short)atoi(strData.c_str());
			er = lpSink->Write(&i, sizeof(i), 1);
		}
		break;
	case PT_MV_LONG:
		if (lpRow[FIELD_NR_ULONG] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		ulCount = atoi(lpRow[FIELD_NR_ID]);
		er = lpSink->Write(&ulCount, sizeof(ulCount), 1);
		ulLastPos = 0;
		for (unsigned x = 0; er == erSuccess && x < ulCount; x++)
		{
			ParseMVProp(lpRow[FIELD_NR_ULONG], lpLen[FIELD_NR_ULONG], &ulLastPos, &strData);
			ul = atoui((char*)strData.c_str());
			er = lpSink->Write(&ul, sizeof(ul), 1);
		}
		break;
	case PT_MV_R4:
		if (lpRow[FIELD_NR_DOUBLE] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		ulCount = atoi(lpRow[FIELD_NR_ID]);
		er = lpSink->Write(&ulCount, sizeof(ulCount), 1);
		ulLastPos = 0;
		for (unsigned x = 0; er == erSuccess && x < ulCount; x++)
		{
			ParseMVProp(lpRow[FIELD_NR_DOUBLE], lpLen[FIELD_NR_DOUBLE], &ulLastPos, &strData);
			flt = (float)strtod_l(strData.c_str(), NULL, loc);
			er = lpSink->Write(&flt, sizeof(flt), 1);
		}
		break;
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		if (lpRow[FIELD_NR_DOUBLE] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		ulCount = atoi(lpRow[FIELD_NR_ID]);
		er = lpSink->Write(&ulCount, sizeof(ulCount), 1);
		ulLastPos = 0;
		for (unsigned x = 0; er == erSuccess && x < ulCount; x++)
		{
			ParseMVProp(lpRow[FIELD_NR_DOUBLE], lpLen[FIELD_NR_DOUBLE], &ulLastPos, &strData);
			dbl = strtod_l(strData.c_str(), NULL, loc);
			er = lpSink->Write(&dbl, sizeof(flt), 1);
		}
		break;
	case PT_MV_CURRENCY:
	case PT_MV_SYSTIME:
		if (lpRow[FIELD_NR_HI] == NULL || lpRow[FIELD_NR_LO] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		ulCount = atoi(lpRow[FIELD_NR_ID]);
		er = lpSink->Write(&ulCount, sizeof(ulCount), 1);
		ulLastPos = 0;
		ulLastPos2 = 0;
		for (unsigned x = 0; er == erSuccess && x < ulCount; x++)
		{
			ParseMVProp(lpRow[FIELD_NR_LO], lpLen[FIELD_NR_LO], &ulLastPos, &strData);
			hilo.lo = atoui((char*)strData.c_str());
			ParseMVProp(lpRow[FIELD_NR_HI], lpLen[FIELD_NR_HI], &ulLastPos2, &strData);
			hilo.hi = atoi((char*)strData.c_str());
			er = lpSink->Write(&hilo.hi, sizeof(hilo.hi), 1);
			if (er == erSuccess)
				er = lpSink->Write(&hilo.lo, sizeof(hilo.lo), 1);
		}
		break;
	case PT_MV_BINARY:
	case PT_MV_CLSID:
		if (lpRow[FIELD_NR_BINARY] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		ulCount = atoi(lpRow[FIELD_NR_ID]);
		er = lpSink->Write(&ulCount, sizeof(ulCount), 1);
		ulLastPos = 0;
		for (unsigned x = 0; er == erSuccess && x < ulCount; x++)
		{
			ParseMVProp(lpRow[FIELD_NR_BINARY], lpLen[FIELD_NR_BINARY], &ulLastPos, &strData);
			ulLen = (unsigned int)strData.size();
			er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
			if (er == erSuccess)
				er = lpSink->Write(strData.c_str(), 1, ulLen);
		}
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		if (lpRow[FIELD_NR_STRING] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		ulCount = atoi(lpRow[FIELD_NR_ID]);
		er = lpSink->Write(&ulCount, sizeof(ulCount), 1);
		ulLastPos = 0;
		for (unsigned x = 0; er == erSuccess && x < ulCount; x++)
		{
			ParseMVProp(lpRow[FIELD_NR_STRING], lpLen[FIELD_NR_STRING], &ulLastPos, &strData);
			if (lpStreamCaps->bSupportUnicode) {
				ulLen = (unsigned int)strData.size();
				er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
				if (er == erSuccess)
					er = lpSink->Write(strData.c_str(), 1, ulLen);
			} else {
				const std::string strEncoded = converter.convert_to<std::string>(CHARSET_WIN1252, strData, rawsize(strData), "UTF-8");
				ulLen = (unsigned int)strEncoded.size();
				er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
				if (er == erSuccess)
					er = lpSink->Write(strEncoded.c_str(), 1, ulLen);
			}
		}
		break;
	case PT_MV_I8:
		if (lpRow[FIELD_NR_LONGINT] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		ulCount = atoi(lpRow[FIELD_NR_ID]);
		er = lpSink->Write(&ulCount, sizeof(ulCount), 1);
		ulLastPos = 0;
		for (unsigned x = 0; er == erSuccess && x < ulCount; x++)
		{
			ParseMVProp(lpRow[FIELD_NR_LONGINT], lpLen[FIELD_NR_LONGINT], &ulLastPos, &strData);
			li = _atoi64(strData.c_str());
			er = lpSink->Write(&li, sizeof(li), 1);
		}
		break;
	default:
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	// If property is named property in the dynamic range we need to add some extra info
	if (PROP_ID(ulPropTag) >= 0x8500) {
		// Send out the GUID.
		er = lpSink->Write(lpRow[FIELD_NR_MAX + 2], 1, lpLen[FIELD_NR_MAX + 2]);

		if (er == erSuccess && lpRow[FIELD_NR_MAX + 0] != NULL) {
			ulKind = MNID_ID;
			ulNameId = atoui((char*)lpRow[FIELD_NR_MAX + 0]);

			er = lpSink->Write(&ulKind, sizeof(ulKind), 1);
			if (er == erSuccess)
				er = lpSink->Write(&ulNameId, sizeof(ulNameId), 1);

		} else if (er == erSuccess && lpRow[FIELD_NR_MAX + 1] != NULL) {
			ulKind = MNID_STRING;
			ulLen = lpLen[FIELD_NR_MAX + 1];

			er = lpSink->Write(&ulKind, sizeof(ulKind), 1);
			if (er == erSuccess)
				er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
			if (er == erSuccess)
				er = lpSink->Write(lpRow[FIELD_NR_MAX + 1], 1, ulLen);

		} else if (er == erSuccess)
			er = ZARAFA_E_INVALID_TYPE;
	}

exit:
	freelocale(loc);
	return er;
}

ECRESULT SerializePropVal(LPCSTREAMCAPS lpStreamCaps, const struct propVal &sPropVal, ECSerializer *lpSink)
{
	ECRESULT er = erSuccess;
	unsigned int type = PROP_TYPE(sPropVal.ulPropTag);
	unsigned int ulLen;
	unsigned char b;
	std::string	strData;
	convert_context converter;

	if (PROP_ID(sPropVal.ulPropTag) >= 0x8500) {
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	er = lpSink->Write(&sPropVal.ulPropTag, sizeof(sPropVal.ulPropTag), 1);
	if (er != erSuccess)
		goto exit;

	switch (type) {
	case PT_I2:
		er = lpSink->Write(&sPropVal.Value.i, sizeof(sPropVal.Value.i), 1);
		break;
	case PT_LONG:
		er = lpSink->Write(&sPropVal.Value.ul, sizeof(sPropVal.Value.ul), 1);
		break;
	case PT_R4:
		er = lpSink->Write(&sPropVal.Value.flt, sizeof(sPropVal.Value.flt), 1);
		break;
	case PT_BOOLEAN:
		b = (sPropVal.Value.b ? 1 : 0);
		er = lpSink->Write(&b, sizeof(b), 1);
		break;
	case PT_DOUBLE:
	case PT_APPTIME:
		er = lpSink->Write(&sPropVal.Value.dbl, sizeof(sPropVal.Value.dbl), 1);
		break;
	case PT_CURRENCY:
	case PT_SYSTIME:
		er = lpSink->Write(&sPropVal.Value.hilo->hi, sizeof(sPropVal.Value.hilo->hi), 1);
		if (er == erSuccess)
			er = lpSink->Write(&sPropVal.Value.hilo->lo, sizeof(sPropVal.Value.hilo->lo), 1);
		break;
	case PT_I8:
		er = lpSink->Write(&sPropVal.Value.li, sizeof(sPropVal.Value.li), 1);
		break;
	case PT_STRING8:
	case PT_UNICODE:
		if (lpStreamCaps->bSupportUnicode) {
			ulLen = (unsigned)strlen(sPropVal.Value.lpszA);
			er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
			if (er == erSuccess)
				er = lpSink->Write(sPropVal.Value.lpszA, 1, ulLen);
		} else {
			const std::string strEncoded = converter.convert_to<std::string>(CHARSET_WIN1252, sPropVal.Value.lpszA, rawsize(sPropVal.Value.lpszA), "UTF-8");
			ulLen = strEncoded.length();
			er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
			if (er == erSuccess)
				er = lpSink->Write(strEncoded.data(), 1, ulLen);
		}
		break;
	case PT_CLSID:
	case PT_BINARY:
		er = lpSink->Write(&sPropVal.Value.bin->__size, sizeof(sPropVal.Value.bin->__size), 1);
		if (er == erSuccess)
			er = lpSink->Write(sPropVal.Value.bin->__ptr, 1, sPropVal.Value.bin->__size);
		break;
	case PT_MV_I2:
		er = lpSink->Write(&sPropVal.Value.mvi.__size, sizeof(sPropVal.Value.mvi.__size), 1);
		for (int x = 0; er == erSuccess && x < sPropVal.Value.mvi.__size; x++)
			er = lpSink->Write(&sPropVal.Value.mvi.__ptr[x], sizeof(sPropVal.Value.mvi.__ptr[x]), 1);
		break;
	case PT_MV_LONG:
		er = lpSink->Write(&sPropVal.Value.mvl.__size, sizeof(sPropVal.Value.mvl.__size), 1);
		for (int x = 0; er == erSuccess && x < sPropVal.Value.mvl.__size; x++)
			er = lpSink->Write(&sPropVal.Value.mvl.__ptr[x], sizeof(sPropVal.Value.mvl.__ptr[x]), 1);
		break;
	case PT_MV_R4:
		er = lpSink->Write(&sPropVal.Value.mvflt.__size, sizeof(sPropVal.Value.mvflt.__size), 1);
		for (int x = 0; er == erSuccess && x < sPropVal.Value.mvflt.__size; x++)
			er = lpSink->Write(&sPropVal.Value.mvflt.__ptr[x], sizeof(sPropVal.Value.mvflt.__ptr[x]), 1);
		break;
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		er = lpSink->Write(&sPropVal.Value.mvdbl.__size, sizeof(sPropVal.Value.mvdbl.__size), 1);
		for (int x = 0; er == erSuccess && x < sPropVal.Value.mvdbl.__size; x++)
			er = lpSink->Write(&sPropVal.Value.mvdbl.__ptr[x], sizeof(sPropVal.Value.mvdbl.__ptr[x]), 1);
		break;
	case PT_MV_CURRENCY:
	case PT_MV_SYSTIME:
		er = lpSink->Write(&sPropVal.Value.mvhilo.__size, sizeof(sPropVal.Value.mvhilo.__size), 1);
		for (int x = 0; er == erSuccess && x < sPropVal.Value.mvhilo.__size; x++) {
			er = lpSink->Write(&sPropVal.Value.mvhilo.__ptr[x].hi, sizeof(sPropVal.Value.mvhilo.__ptr[x].hi), 1);
			if (er == erSuccess)
				er = lpSink->Write(&sPropVal.Value.mvhilo.__ptr[x].lo, sizeof(sPropVal.Value.mvhilo.__ptr[x].lo), 1);
		}
		break;
	case PT_MV_BINARY:
	case PT_MV_CLSID:
		er = lpSink->Write(&sPropVal.Value.mvbin.__size, sizeof(sPropVal.Value.mvbin.__size), 1);
		for (int x = 0; er == erSuccess && x < sPropVal.Value.mvbin.__size; x++) {
			er = lpSink->Write(&sPropVal.Value.mvbin.__ptr[x].__size, sizeof(sPropVal.Value.mvbin.__ptr[x].__size), 1);
			if (er == erSuccess)
				er = lpSink->Write(&sPropVal.Value.mvbin.__ptr[x].__ptr, 1, sPropVal.Value.mvbin.__ptr[x].__size);
		}
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		er = lpSink->Write(&sPropVal.Value.mvszA.__size, sizeof(sPropVal.Value.mvszA.__size), 1);
		for (int x = 0; er == erSuccess && x < sPropVal.Value.mvszA.__size; x++) {
			if (lpStreamCaps->bSupportUnicode) {
				ulLen = (unsigned)strlen(sPropVal.Value.mvszA.__ptr[x]);
				er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
				if (er == erSuccess)
					er = lpSink->Write(sPropVal.Value.mvszA.__ptr[x], 1, ulLen);
			} else {
				const std::string strEncoded = converter.convert_to<std::string>(CHARSET_WIN1252, sPropVal.Value.mvszA.__ptr[x], rawsize(sPropVal.Value.mvszA.__ptr[x]), "UTF-8");
				ulLen = strEncoded.length();
				er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
				if (er == erSuccess)
					er = lpSink->Write(strEncoded.data(), 1, ulLen);
			}
		}
		break;
	case PT_MV_I8:
		er = lpSink->Write(&sPropVal.Value.mvli.__size, sizeof(sPropVal.Value.mvli.__size), 1);
		for (int x = 0; er == erSuccess && x < sPropVal.Value.mvli.__size; x++)
			er = lpSink->Write(&sPropVal.Value.mvli.__ptr[x], sizeof(sPropVal.Value.mvli.__ptr[x]), 1);
		break;

	default:
		er = ZARAFA_E_INVALID_TYPE;
	}

exit:
	return er;
}

ECRESULT SerializeProps(ECSession *lpecSession, ECAttachmentStorage *lpAttachmentStorage, LPCSTREAMCAPS lpStreamCaps, unsigned int ulObjId, unsigned int ulObjType, unsigned int ulStoreId, GUID *lpsGuid, ULONG ulFlags, ECSerializer *lpSink)
{
	ECRESULT		er = erSuccess;
	unsigned int	ulRows = 0;
	unsigned int	ulRowsMV = 0;
	unsigned int	ulRowsNamed = 0;
	unsigned int	ulRowsNamedMV = 0;
	unsigned int	ulCount = 0;
	unsigned int	ulLen = 0;

	unsigned int	ulPropTag = 0;
	unsigned int	ulBodyFlags = 0;
	unsigned int	ulUseBody = 0;
	struct soap		*soap = NULL;
	struct propVal	sPropVal = {0};

	ECDatabase*		lpDatabase = NULL;
	DB_ROW 			lpDBRow = NULL;
	DB_LENGTHS		lpDBLen = NULL;
	DB_RESULT		lpDBResult = NULL;
	DB_RESULT		lpDBResultMV = NULL;
	DB_RESULT		lpDBResultNamed = NULL;
	DB_RESULT		lpDBResultNamedMV = NULL;
	std::string		strQuery;
	unsigned int	ulBodyTrans = 0;

	std::list<struct propVal> sPropValList;

	enum {
		HAVE_PLAINTEXT = 1,
		HAVE_HTML = 2,
		HAVE_RTF = 4
	};

	static struct {
		unsigned int ulBestBody;	// The index in ulBodyIdx for the best body
		unsigned int ulBodyCount;	// The total number of bodies found
		unsigned int ulBodyTrans;	// The number of bodies that will be send (when using the best body)
		unsigned int ulWorstBody;	// THe index in ulBodyIdx for the wordt body
	} ulaBodySelect[8] = {
		{0,					0, 0, 0},					// No bodies found, should not happen
		{PR_BODY,			1, 1, PR_BODY},				// Only plain text
		{0x10130102,		1, 1, 0x10130102},			// Only HTML
		{0x10130102,		2, 2, PR_BODY},				// Plain text and HTML
		{PR_RTF_COMPRESSED, 1, 1, PR_RTF_COMPRESSED},	// Only RTF
		{PR_RTF_COMPRESSED, 2, 2, PR_BODY},				// Plain text and RTF
		{PR_RTF_COMPRESSED, 2, 1, 0x10130102},			// HTML and RTF
		{PR_RTF_COMPRESSED, 3, 2, PR_BODY}				// Plain text, HTML and RTF
	};

	ASSERT(lpStreamCaps != NULL);

	er = lpecSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	if (!lpAttachmentStorage) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// We'll (ab)use a soap structure as a memory pool.
	soap = soap_new();

	
	// PR_SOURCE_KEY
	if (ECGenProps::GetPropComputedUncached(soap, lpecSession, PR_SOURCE_KEY, ulObjId, 0, ulStoreId, 0, ulObjType, &sPropVal) == erSuccess)
		sPropValList.push_back(sPropVal);

	// Named properties need to be handled differently as we can't just send the proptag. However the
	// range 0x8000 - 0x84FF is handled client side and will be equal on all systems. So we handle them
	// as if they are regular properties.
	strQuery = "SELECT " + (std::string)PROPCOLORDER + " FROM properties WHERE hierarchyid=" + stringify(ulObjId) + " AND tag<0x8500";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	er = GetValidatedPropCount(lpDatabase, lpDBResult, &ulRows);
	if (er != erSuccess)
		goto exit;


	strQuery = 
		"SELECT " + (std::string)PROPCOLORDER + ",n.nameid,n.namestring,n.guid "
			"FROM properties JOIN names as n on (properties.tag-0x8501)=n.id "
			"WHERE properties.hierarchyid=" + stringify(ulObjId) + " AND properties.tag>=0x8500";

	er = lpDatabase->DoSelect(strQuery, &lpDBResultNamed);
	if (er != erSuccess)
		goto exit;

	er = GetValidatedPropCount(lpDatabase, lpDBResultNamed, &ulRowsNamed);
	if (er != erSuccess)
		goto exit;


	// Same for MV properties.
	strQuery = "SELECT " + (std::string)MVPROPCOLORDER + " FROM mvproperties WHERE hierarchyid=" + stringify(ulObjId) + " AND tag<0x8500 GROUP BY hierarchyid, tag";
	er = lpDatabase->DoSelect(strQuery, &lpDBResultMV);
	if (er != erSuccess)
		goto exit;

	er = GetValidatedPropCount(lpDatabase, lpDBResultMV, &ulRowsMV);
	if (er != erSuccess)
		goto exit;


	strQuery = 
		"SELECT " + (std::string)MVPROPCOLORDER + ",n.nameid,n.namestring,n.guid "
			"FROM mvproperties JOIN names as n on (mvproperties.tag-0x8501)=n.id "
			"WHERE mvproperties.hierarchyid=" + stringify(ulObjId) + " AND mvproperties.tag>=0x8500 GROUP BY hierarchyid, tag";

	er = lpDatabase->DoSelect(strQuery, &lpDBResultNamedMV);
	if (er != erSuccess)
		goto exit;

	er = GetValidatedPropCount(lpDatabase, lpDBResultNamedMV, &ulRowsNamedMV);
	if (er != erSuccess)
		goto exit;

	
	// See which bodies are available and select the best one.
	for (unsigned i = 0; i < ulRows; ++i) {
		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);
		if (lpDBRow == NULL || lpDBLen == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		ulPropTag = PROP_TAG(atoi(lpDBRow[FIELD_NR_TYPE]), atoi(lpDBRow[FIELD_NR_TAG]));
		switch (ulPropTag) {
		case PR_BODY:
			ulBodyFlags |= HAVE_PLAINTEXT;
			break;

		case 0x10130102:	/* PR_HTML */
			ulBodyFlags |= HAVE_HTML;
			break;

		case PR_RTF_COMPRESSED:
			ulBodyFlags |= HAVE_RTF;
			break;

		default:
			break;
		}
	}

	assert(ulBodyFlags >= 0 && ulBodyFlags < 8);
	
	if (ulFlags & (SYNC_LIMITED_IMESSAGE))
		ulBodyTrans = ulaBodySelect[ulBodyFlags].ulWorstBody ? 1 : 0; // Sending only one body
	else if(ulFlags & (SYNC_BEST_BODY))
		ulBodyTrans = ulaBodySelect[ulBodyFlags].ulBodyTrans; // Sending best body + plain so either 1 or 2 bodies (1 if best == plain)
	else
		ulBodyTrans = ulaBodySelect[ulBodyFlags].ulBodyCount; // Sending all bodies

	ulCount = ulRows + ulRowsNamed + ulRowsMV + ulRowsNamedMV - ulaBodySelect[ulBodyFlags].ulBodyCount + ulBodyTrans + sPropValList.size();
	er = lpSink->Write(&ulCount, sizeof(ulCount), 1);
	if (er != erSuccess)
		goto exit;

	// Regular properties
	lpDatabase->ResetResult(lpDBResult);
	while ((lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL) {
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);
		if (lpDBRow == NULL || lpDBLen == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		// Filter out the unwanted body properties
		if (ulFlags & (SYNC_BEST_BODY | SYNC_LIMITED_IMESSAGE)) {
			/* We only want the best body (RTF) plus plain text */
			if (ulFlags & SYNC_BEST_BODY)
				ulUseBody = ulaBodySelect[ulBodyFlags].ulBestBody;
	
			/* We only want the simplest body (plain text) */
			if (ulFlags & SYNC_LIMITED_IMESSAGE)
				ulUseBody = ulaBodySelect[ulBodyFlags].ulWorstBody;

			ulPropTag = PROP_TAG(atoi(lpDBRow[FIELD_NR_TYPE]), atoi(lpDBRow[FIELD_NR_TAG]));
			if ((ulPropTag == 0x10130102 || ulPropTag == PR_RTF_COMPRESSED) && (ulPropTag != ulUseBody))
				continue;
		}

		er = SerializeDatabasePropVal(lpStreamCaps, lpDBRow, lpDBLen, lpSink);
		if (er != erSuccess)
			goto exit;
	}

	// Named properties
	while ((lpDBRow = lpDatabase->FetchRow(lpDBResultNamed)) != NULL) {
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResultNamed);
		if (lpDBRow == NULL || lpDBLen == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		er = SerializeDatabasePropVal(lpStreamCaps, lpDBRow, lpDBLen, lpSink);
		if (er != erSuccess)
			goto exit;
	}

	// Regular MV properties
	while ((lpDBRow = lpDatabase->FetchRow(lpDBResultMV)) != NULL) {
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResultMV);
		if (lpDBRow == NULL || lpDBLen == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		er = SerializeDatabasePropVal(lpStreamCaps, lpDBRow, lpDBLen, lpSink);
		if (er != erSuccess)
			goto exit;
	}

	// Named MV properties
	while ((lpDBRow = lpDatabase->FetchRow(lpDBResultNamedMV)) != NULL) {
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResultNamedMV);
		if (lpDBRow == NULL || lpDBLen == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		er = SerializeDatabasePropVal(lpStreamCaps, lpDBRow, lpDBLen, lpSink);
		if (er != erSuccess)
			goto exit;
	}

	for (std::list<struct propVal>::const_iterator it = sPropValList.begin(); it != sPropValList.end(); ++it) {
		er = SerializePropVal(lpStreamCaps, *it, lpSink);
		if (er != erSuccess)
			goto exit;
	}

	if (ulObjType == MAPI_ATTACH) {
		
		if (lpAttachmentStorage->ExistAttachment(ulObjId, PROP_ID(PR_ATTACH_DATA_BIN))) {
			er = lpAttachmentStorage->GetSize(ulObjId, PROP_ID(PR_ATTACH_DATA_BIN), (ULONG*)&ulLen);
			if (er != erSuccess)
				goto exit;

			er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
			if (er != erSuccess)
				goto exit;

			er = lpAttachmentStorage->LoadAttachment(ulObjId, PROP_ID(PR_ATTACH_DATA_BIN), (int*)&ulLen, lpSink);
			if (er != erSuccess)
				goto exit;
		} else {
			ulLen = 0;
			er = lpSink->Write(&ulLen, sizeof(ulLen), 1);
			if (er != erSuccess)
				goto exit;
		}
	}



exit:
	if (lpDatabase) {
		if (lpDBResultNamedMV)
			lpDatabase->FreeResult(lpDBResultNamedMV);
		if (lpDBResultMV)
			lpDatabase->FreeResult(lpDBResultMV);
		if (lpDBResultNamed)
			lpDatabase->FreeResult(lpDBResultNamed);
		if (lpDBResult)
			lpDatabase->FreeResult(lpDBResult);
	}
	
	if (soap) {
		soap_end(soap);
		soap_free(soap);
	}

	return er;
}

/**
 * Serialize a Message directly from the database.
 * This method handles direct subobjects and recurses whenever an embedded
 * message is encountered.
 * 
 * @param[in] lpecSession			Pointer to the current session.
 * @param[in] lpStreamDatabase		Pointer to the database.
 * @param[in] lpAttachmentStorage	Pointer to the attachmentstore.
 * @param[in] lpStreamCaps			Pointer to a stream capability structore. Must be NULL except
 * 									when called by SerializeMessage itself.
 * @param[in] ulObjId				The hierarchyid of the object to serialize.
 * @param[in] ulObjType				The type of the object to serialize. Must be MAPI_MESSAGE.
 * @param[in] ulStoreId				The id of the store containing the message.
 * @param[in] lpsGuid				Seems to be unused.
 * @param[in] ulFlags				Flags (SYNC_BEST_BODY to stream the best body for the message,
 * 									SYNC_LIMITED_IMESSAGE to only stream the plain text body).
 * @param[in] lpSink				Pointer to an ECSerializer instance to which the serialized
 * 									data should be written.
 * @param[in] bTop					Specifies that this is a toplevel message. Must be true excep
 * 									when called by SerializeMessage itself.
 */
ECRESULT SerializeMessage(ECSession *lpecSession, ECAttachmentStorage *lpAttachmentStorage, LPCSTREAMCAPS lpStreamCaps, unsigned int ulObjId, unsigned int ulObjType, unsigned int ulStoreId, GUID *lpsGuid, ULONG ulFlags, ECSerializer *lpSink, bool bTop)
{
	ECRESULT		er = erSuccess;
	unsigned int	ulStreamVersion = STREAM_VERSION;
	unsigned int	ulSubObjId = 0;
	unsigned int	ulSubObjType = 0;
	unsigned int	ulCount = 0;

	ECDatabase*		lpDatabase = NULL;
	DB_ROW 			lpDBRow = NULL;
	DB_LENGTHS		lpDBLen = NULL;
	DB_RESULT		lpDBResult = NULL;
	std::string		strQuery;

	er = lpecSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	if (lpStreamCaps == NULL) {
		lpStreamCaps = STREAM_CAPS_CURRENT;	// Set to current stream capabilities.

		if ((lpecSession->GetCapabilities() & ZARAFA_CAP_UNICODE) == 0) {
			ulStreamVersion = 0;
			lpStreamCaps = &g_StreamCaps[0];
		}

		er = lpSink->Write(&ulStreamVersion, sizeof(ulStreamVersion), 1);
		if (er != erSuccess)
			goto exit;
	}

	er = SerializeProps(lpecSession, lpAttachmentStorage, lpStreamCaps, ulObjId, ulObjType, ulStoreId, lpsGuid, ulFlags, lpSink);
	if (er != erSuccess)
		goto exit;

	if (ulObjType == MAPI_MESSAGE || ulObjType == MAPI_ATTACH) {
		// Serialize sub objects
		strQuery = "SELECT id,type FROM hierarchy WHERE parent=" + stringify(ulObjId);
		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if (er != erSuccess)
			goto exit;

		ulCount = lpDatabase->GetNumRows(lpDBResult);
		er = lpSink->Write(&ulCount, sizeof(ulCount), 1);
		if (er != erSuccess)
			goto exit;

		for (unsigned i = 0; i < ulCount; ++i) {
			lpDBRow = lpDatabase->FetchRow(lpDBResult);
			lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

			if (lpDBRow == NULL || lpDBLen == NULL) {
				er = ZARAFA_E_DATABASE_ERROR;
				goto exit;
			}

			ulSubObjType = atoi(lpDBRow[1]);
			er = lpSink->Write(&ulSubObjType, sizeof(ulSubObjType), 1);
			if (er != erSuccess)
				goto exit;

			ulSubObjId = atoi(lpDBRow[0]);
			er = lpSink->Write(&ulSubObjId, sizeof(ulSubObjId), 1);
			if (er != erSuccess)
				goto exit;

			er = SerializeMessage(lpecSession, lpAttachmentStorage, lpStreamCaps, ulSubObjId, ulSubObjType, ulStoreId, lpsGuid, ulFlags, lpSink, false);
			if (er != erSuccess)
				goto exit;
		}
	}


exit:
	if (er != erSuccess) {
		lpecSession->GetSessionManager()->GetLogger()->Log(EC_LOGLEVEL_ERROR, "SerializeObject failed with error code 0x%08x", er);
	}

	if (lpDatabase) {
		if (lpDBResult)
			lpDatabase->FreeResult(lpDBResult);
	}

	return er;
}

ECRESULT DeserializePropVal(struct soap *soap, LPCSTREAMCAPS lpStreamCaps, NamedPropertyMapper &namedPropertyMapper, propVal **lppsPropval, ECSerializer *lpSource)
{
	ECRESULT		er = erSuccess;
	unsigned int	ulCount;
	unsigned int	ulLen;
	propVal			*lpsPropval = NULL;
	unsigned char	b;

	GUID			guid = {0};
	unsigned int	ulKind = 0;
	unsigned int	ulNameId = 0;
	std::string		strNameString;
	unsigned int	ulLocalId = 0;

	convert_context	converter;

	lpsPropval = s_alloc<propVal>(soap);
	er = lpSource->Read(&lpsPropval->ulPropTag, sizeof(lpsPropval->ulPropTag), 1);
	if (er != erSuccess)
		goto exit;

	switch (PROP_TYPE(lpsPropval->ulPropTag)) {
	case PT_I2:
		lpsPropval->__union = SOAP_UNION_propValData_i;
		er = lpSource->Read(&lpsPropval->Value.i, sizeof(lpsPropval->Value.i), 1);
		break;
	case PT_LONG:
		lpsPropval->__union = SOAP_UNION_propValData_ul;
		er = lpSource->Read(&lpsPropval->Value.ul, sizeof(lpsPropval->Value.ul), 1);
		break;
	case PT_R4:
		lpsPropval->__union = SOAP_UNION_propValData_flt;
		er = lpSource->Read(&lpsPropval->Value.flt, sizeof(lpsPropval->Value.flt), 1);
		break;
	case PT_BOOLEAN:
		lpsPropval->__union = SOAP_UNION_propValData_b;
		er = lpSource->Read(&b, sizeof(b), 1);
		lpsPropval->Value.b = (b != 0);
		break;
	case PT_DOUBLE:
	case PT_APPTIME:
		lpsPropval->__union = SOAP_UNION_propValData_dbl;
		er = lpSource->Read(&lpsPropval->Value.dbl, sizeof(lpsPropval->Value.dbl), 1);
		break;
	case PT_CURRENCY:
	case PT_SYSTIME:
		lpsPropval->__union = SOAP_UNION_propValData_hilo;
		lpsPropval->Value.hilo = s_alloc<hiloLong>(soap);
		er = lpSource->Read(&lpsPropval->Value.hilo->hi, sizeof(lpsPropval->Value.hilo->hi), 1);
		if (er == erSuccess)
			er = lpSource->Read(&lpsPropval->Value.hilo->lo, sizeof(lpsPropval->Value.hilo->lo), 1);
		break;
	case PT_I8:
		lpsPropval->__union = SOAP_UNION_propValData_li;
		er = lpSource->Read(&lpsPropval->Value.li, sizeof(lpsPropval->Value.li), 1);
		break;
	case PT_STRING8:
	case PT_UNICODE:
		lpsPropval->__union = SOAP_UNION_propValData_lpszA;
		er = lpSource->Read(&ulLen, sizeof(ulLen), 1);
		if (er == erSuccess) {
			if (lpStreamCaps->bSupportUnicode) {
				lpsPropval->Value.lpszA = s_alloc<char>(soap, ulLen + 1);
				memset(lpsPropval->Value.lpszA, 0, ulLen + 1);
				er = lpSource->Read(lpsPropval->Value.lpszA, 1, ulLen);
			} else {
				std::string strData(ulLen, '\0');
				er = lpSource->Read((void*)strData.data(), 1, ulLen);
				if (er == erSuccess) {
					const utf8string strConverted = converter.convert_to<utf8string>(strData, rawsize(strData), "WINDOWS-1252");
					lpsPropval->Value.lpszA = s_strcpy(soap, strConverted.c_str());
				}
			}
		}
		break;
	case PT_CLSID:
	case PT_BINARY:
		lpsPropval->__union = SOAP_UNION_propValData_bin;
		er = lpSource->Read(&ulLen, sizeof(ulLen), 1);
		if (er == erSuccess) {
			lpsPropval->Value.bin = s_alloc<xsd__base64Binary>(soap);
			lpsPropval->Value.bin->__size = ulLen;
			lpsPropval->Value.bin->__ptr = s_alloc<unsigned char>(soap, ulLen);
			er = lpSource->Read(lpsPropval->Value.bin->__ptr, 1, ulLen);
		}
		break;
	case PT_MV_I2:
		lpsPropval->__union = SOAP_UNION_propValData_mvi;
		er = lpSource->Read(&ulCount, sizeof(ulCount), 1);
		if (er == erSuccess) {
			lpsPropval->Value.mvi.__size = ulCount;
			lpsPropval->Value.mvi.__ptr = s_alloc<short>(soap, ulCount);
			er = lpSource->Read(lpsPropval->Value.mvi.__ptr, sizeof *lpsPropval->Value.mvi.__ptr, ulCount);
		}
		break;
	case PT_MV_LONG:
		lpsPropval->__union = SOAP_UNION_propValData_mvl;
		er = lpSource->Read(&ulCount, sizeof(ulCount), 1);
		if (er == erSuccess) {
			lpsPropval->Value.mvl.__size = ulCount;
			lpsPropval->Value.mvl.__ptr = s_alloc<unsigned int>(soap, ulCount);
			er = lpSource->Read(lpsPropval->Value.mvl.__ptr, sizeof *lpsPropval->Value.mvl.__ptr, ulCount);
		}
		break;
	case PT_MV_R4:
		lpsPropval->__union = SOAP_UNION_propValData_mvflt;
		er = lpSource->Read(&ulCount, sizeof(ulCount), 1);
		if (er == erSuccess) {
			lpsPropval->Value.mvflt.__size = ulCount;
			lpsPropval->Value.mvflt.__ptr = s_alloc<float>(soap, ulCount);
			er = lpSource->Read(lpsPropval->Value.mvflt.__ptr, sizeof *lpsPropval->Value.mvflt.__ptr, ulCount);
		}
		break;
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		lpsPropval->__union = SOAP_UNION_propValData_mvdbl;
		er = lpSource->Read(&ulCount, sizeof(ulCount), 1);
		if (er == erSuccess) {
			lpsPropval->Value.mvdbl.__size = ulCount;
			lpsPropval->Value.mvdbl.__ptr = s_alloc<double>(soap, ulCount);
			er = lpSource->Read(lpsPropval->Value.mvdbl.__ptr, sizeof *lpsPropval->Value.mvdbl.__ptr, ulCount);
		}
		break;
	case PT_MV_CURRENCY:
	case PT_MV_SYSTIME:
		lpsPropval->__union = SOAP_UNION_propValData_mvhilo;
		er = lpSource->Read(&ulCount, sizeof(ulCount), 1);
		if (er == erSuccess) {
			lpsPropval->Value.mvhilo.__size = ulCount;
			lpsPropval->Value.mvhilo.__ptr = s_alloc<hiloLong>(soap, ulCount);
			for (unsigned x = 0; er == erSuccess &&  x < ulCount; x++) {
				er = lpSource->Read(&lpsPropval->Value.mvhilo.__ptr[x].hi, sizeof(lpsPropval->Value.mvhilo.__ptr[x].hi), ulCount);
				if (er == erSuccess)
					er = lpSource->Read(&lpsPropval->Value.mvhilo.__ptr[x].lo, sizeof(lpsPropval->Value.mvhilo.__ptr[x].lo), ulCount);
			}
		}
		break;
	case PT_MV_BINARY:
	case PT_MV_CLSID:
		lpsPropval->__union = SOAP_UNION_propValData_mvbin;
		er = lpSource->Read(&ulCount, sizeof(ulCount), 1);
		if (er == erSuccess) {
			lpsPropval->Value.mvbin.__size = ulCount;
			lpsPropval->Value.mvbin.__ptr = s_alloc<xsd__base64Binary>(soap, ulCount);
			for (unsigned x = 0; er == erSuccess && x < ulCount; x++) {
				er = lpSource->Read(&ulLen, sizeof(ulLen), 1);
				if (er == erSuccess) {
					lpsPropval->Value.mvbin.__ptr[x].__size = ulLen;
					lpsPropval->Value.mvbin.__ptr[x].__ptr = s_alloc<unsigned char>(soap, ulLen);
					er = lpSource->Read(lpsPropval->Value.mvbin.__ptr[x].__ptr, 1, ulLen);
				}
			}
		}
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		lpsPropval->__union = SOAP_UNION_propValData_mvszA;
		er = lpSource->Read(&ulCount, sizeof(ulCount), 1);
		if (er == erSuccess) {
			lpsPropval->Value.mvszA.__size = ulCount;
			lpsPropval->Value.mvszA.__ptr = s_alloc<char*>(soap, ulCount);
			for (unsigned x = 0; er == erSuccess && x < ulCount; x++) {
				er = lpSource->Read(&ulLen, sizeof(ulLen), 1);
				if (er == erSuccess) {
					if (lpStreamCaps->bSupportUnicode) {
						lpsPropval->Value.mvszA.__ptr[x] = s_alloc<char>(soap, ulLen + 1);
						memset(lpsPropval->Value.mvszA.__ptr[x], 0, ulLen + 1);
						er = lpSource->Read(lpsPropval->Value.mvszA.__ptr[x], 1, ulLen);
					} else {
						std::string strData(ulLen, '\0');
						er = lpSource->Read((void*)strData.data(), 1, ulLen);
						if (er == erSuccess) {
							const utf8string strConverted = converter.convert_to<utf8string>(strData, rawsize(strData), "WINDOWS-1252");
							lpsPropval->Value.mvszA.__ptr[x] = s_strcpy(soap, strConverted.c_str());
						}
					}
				}
			}
		}
		break;
	case PT_MV_I8:
		lpsPropval->__union = SOAP_UNION_propValData_mvli;
		er = lpSource->Read(&ulCount, sizeof(ulCount), 1);
		if (er == erSuccess) {
			lpsPropval->Value.mvli.__size = ulCount;
			lpsPropval->Value.mvli.__ptr = s_alloc<LONG64>(soap, ulCount);
			er = lpSource->Read(lpsPropval->Value.mvli.__ptr, sizeof *lpsPropval->Value.mvli.__ptr, ulCount);
		}
		break;
	default:
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	// If the proptag is in the dynamic named property range, we need to get the correct local proptag
	if (PROP_ID(lpsPropval->ulPropTag) >= 0x8500) {
		er = lpSource->Read(&guid, 1, sizeof(guid));
		if (er == erSuccess)
			er = lpSource->Read(&ulKind, sizeof(ulKind), 1);
		if (er == erSuccess && ulKind == MNID_ID) {
			er = lpSource->Read(&ulNameId, sizeof(ulNameId), 1);
			if (er == erSuccess)
				er = namedPropertyMapper.GetId(guid, ulNameId, &ulLocalId);
		} else if (er == erSuccess && ulKind == MNID_STRING) {
			er = lpSource->Read(&ulLen, sizeof(ulLen), 1);
			if (er == erSuccess) {
				strNameString.resize(ulLen, 0);
				er = lpSource->Read((void*)strNameString.data(), 1, ulLen);
			}
			if (er == erSuccess)
				er = namedPropertyMapper.GetId(guid, strNameString, &ulLocalId);
		}
		if (er == erSuccess)
			lpsPropval->ulPropTag = PROP_TAG(PROP_TYPE(lpsPropval->ulPropTag), ulLocalId);
	}

	*lppsPropval = lpsPropval;

exit:
	return er;
}

ECRESULT DeserializeProps(ECSession *lpecSession, ECDatabase *lpDatabase, ECAttachmentStorage *lpAttachmentStorage, LPCSTREAMCAPS lpStreamCaps, unsigned int ulObjId, unsigned int ulObjType, unsigned int ulStoreId, GUID *lpsGuid, bool bNewItem, ECSerializer *lpSource, struct propValArray **lppPropValArray)
{
	ECRESULT		er = erSuccess;
	unsigned int	ulCount = 0;
	unsigned int	ulFlags = 0;
	unsigned int	ulParentId = 0;
	unsigned int	ulOwner = 0;
	unsigned int	ulParentType = 0;
	unsigned int	nMVItems = 0;
	unsigned int	ulAffected = 0;
	unsigned int	ulLen = 0;
	propVal			*lpsPropval = NULL;
	struct soap		*soap = NULL;
	struct propValArray *lpPropValArray = NULL;
	NamedPropertyMapper namedPropertyMapper(lpDatabase);

	std::string		strQuery;
	std::string		strInsertQuery;
	std::string		strInsertTProp;
	std::string		strColData;
	std::string		strColName;

	SOURCEKEY		sSourceKey;

	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;

	std::set<unsigned int>				setInserted;
	std::set<unsigned int>::iterator	iterInserted;

	if (!lpDatabase) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if (!lpAttachmentStorage) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulObjId, &ulParentId, &ulOwner, &ulFlags, NULL);
	if (er != erSuccess)
		goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulParentId, NULL, NULL, NULL, &ulParentType);
	if (er != erSuccess)
		goto exit;

	// Get the number of properties
	er = lpSource->Read(&ulCount, sizeof(ulCount), 1);
	if (er != erSuccess)
		goto exit;
		
	if (lppPropValArray) {
		// If requested we can store upto ulCount properties. Currently we won't store them all though.
		// Note that we test on lppPropValArray but allocate lpPropValArray. We'll assign that to
		// *lppPropValArray later if all went well.
		lpPropValArray = s_alloc<struct propValArray>(NULL);
		
		lpPropValArray->__ptr = s_alloc<struct propVal>(NULL, ulCount);
		memset(lpPropValArray->__ptr, 0, sizeof(struct propVal) * ulCount);
		
		lpPropValArray->__size = 0;
	}

	for (unsigned i = 0; i < ulCount; ++i) {
		// We'll (ab)use a soap structure as a memory pool.
		ASSERT(soap == NULL);
		soap = soap_new();

		er = DeserializePropVal(soap, lpStreamCaps, namedPropertyMapper, &lpsPropval, lpSource);
		if (er != erSuccess)
			goto exit;

		iterInserted = setInserted.find(lpsPropval->ulPropTag);
		if (iterInserted != setInserted.end())
			goto next_property;

		if (ECGenProps::IsPropRedundant(lpsPropval->ulPropTag, ulObjType) == erSuccess)
			goto next_property;

		// Same goes for flags in PR_MESSAGE_FLAGS
		if (lpsPropval->ulPropTag == PR_MESSAGE_FLAGS) {
		    // ulFlags is obtained from the hierarchy table, which should only contain
		    // 'unsettable' flags
		    ASSERT((ulFlags & ~MSGFLAG_UNSETTABLE) == 0);

			// Normalize PR_MESSAGE_FLAGS so that the user cannot change flags that are also
			// stored in the hierarchy table.
		    lpsPropval->Value.ul = (lpsPropval->Value.ul & ~MSGFLAG_UNSETTABLE) | ulFlags;

			if (lpPropValArray)
				CopyPropVal(lpsPropval, lpPropValArray->__ptr + lpPropValArray->__size++);
		}


		// Make sure we dont have a colliding PR_SOURCE_KEY. This can happen if a user imports an exported message for example.
		if (lpsPropval->ulPropTag == PR_SOURCE_KEY) {
			// don't use the sourcekey if found.
			// Don't query the cache as that can be out of sync with the db in rare occasions.
			strQuery = 
				"SELECT hierarchyid FROM indexedproperties "
					"WHERE tag=" + stringify(PROP_ID(PR_SOURCE_KEY)) + 
					" AND val_binary=" + lpDatabase->EscapeBinary(lpsPropval->Value.bin->__ptr, lpsPropval->Value.bin->__size);
			
			er = lpDatabase->DoSelect(strQuery, &lpDBResult);
			if(er != erSuccess)
				goto exit;

			lpDBRow = lpDatabase->FetchRow(lpDBResult);
			lpDatabase->FreeResult(lpDBResult); 
			lpDBResult = NULL;

			// We can't use lpDBRow here except for checking if it was NULL.
			if (lpDBRow != NULL)
				continue;


			strQuery = "REPLACE INTO indexedproperties(hierarchyid,tag,val_binary) VALUES (" + stringify(ulObjId) + "," + stringify(PROP_ID(PR_SOURCE_KEY)) + "," + lpDatabase->EscapeBinary(lpsPropval->Value.bin->__ptr, lpsPropval->Value.bin->__size) + ")";
			er = lpDatabase->DoInsert(strQuery);
			if (er != erSuccess)
				goto exit;

			setInserted.insert(lpsPropval->ulPropTag);

			g_lpSessionManager->GetCacheManager()->SetObjectProp(PROP_ID(PR_SOURCE_KEY), lpsPropval->Value.bin->__size, lpsPropval->Value.bin->__ptr, ulObjId);
			goto next_property;
		}

		if ((PROP_TYPE(lpsPropval->ulPropTag) & MV_FLAG) == MV_FLAG) {
			nMVItems = GetMVItemCount(lpsPropval);
			for (unsigned j = 0; j < nMVItems; ++j) {
				er = CopySOAPPropValToDatabaseMVPropVal(lpsPropval, j, strColName, strColData, lpDatabase);
				if (er != erSuccess) {
					er = erSuccess;
					goto next_property;
				}

				strQuery = "REPLACE INTO mvproperties(hierarchyid,orderid,tag,type," + strColName + ") VALUES(" + stringify(ulObjId) + "," + stringify(j) + "," + stringify(PROP_ID(lpsPropval->ulPropTag)) + "," + stringify(PROP_TYPE(lpsPropval->ulPropTag)) + "," + strColData + ")";
				er = lpDatabase->DoInsert(strQuery, NULL, &ulAffected);
				if (er != erSuccess)
					goto exit;
				if (ulAffected != 1) {
					er = ZARAFA_E_DATABASE_ERROR;
					goto exit;
				}
			}
			
		} else {
			// Write the property to the database
			er = WriteSingleProp(lpDatabase, ulObjId, ulParentId, lpsPropval, false, lpDatabase->GetMaxAllowedPacket(), strInsertQuery);
			if (er == ZARAFA_E_TOO_BIG) {
				er = lpDatabase->DoInsert(strInsertQuery);
				if (er == erSuccess) {
					strInsertQuery.clear();
					er = WriteSingleProp(lpDatabase, ulObjId, ulParentId, lpsPropval, false, lpDatabase->GetMaxAllowedPacket(), strInsertQuery);
				}
			}
			if (er != erSuccess)
				goto exit;
			
			// Write the property to the table properties if needed (only on objects in folders (folders, messages), and if the property is being tracked here.
			if (ulParentType == MAPI_FOLDER) {
				// Cache the written value
				sObjectTableKey key(ulObjId, 0);
				g_lpSessionManager->GetCacheManager()->SetCell(&key, lpsPropval->ulPropTag, lpsPropval);
				
				if (0) {
					// FIXME do we need this code? Currently we get always a deferredupdate!
					// Please also update zarafacmd.cpp:WriteProps
					er = WriteSingleProp(lpDatabase, ulObjId, ulParentId, lpsPropval, true, lpDatabase->GetMaxAllowedPacket(), strInsertTProp);
					if (er == ZARAFA_E_TOO_BIG) {
						er = lpDatabase->DoInsert(strInsertTProp);
						if (er == erSuccess) {
							strInsertTProp.clear();
							er = WriteSingleProp(lpDatabase, ulObjId, ulParentId, lpsPropval, true, lpDatabase->GetMaxAllowedPacket(), strInsertTProp);
						}
					}
					if(er != erSuccess)
						goto exit;
				}
			}
		}

		setInserted.insert(lpsPropval->ulPropTag);

next_property:
		soap_end(soap);
		soap_free(soap);
		soap = NULL;
	}

	if (!strInsertQuery.empty()) {
		er = lpDatabase->DoInsert(strInsertQuery);
		if (er != erSuccess)
			goto exit;
	}

	if(ulParentType == MAPI_FOLDER) {
		if (0) {
			/* Modification, just directly write the tproperties
			 * The idea behind this is that we'd need some serious random-access reads to properties later when flushing
			 * tproperties, and we have the properties in memory now anyway. Also, modifications usually are just a few properties, causing
			 * only minor random I/O on tproperties, and a tproperties flush reads all the properties, not just the modified ones.
			 */
			if (!strInsertTProp.empty()) {
				er = lpDatabase->DoInsert(strInsertTProp);
				if (er != erSuccess)
					goto exit;
			}
		} else {
			// Instead of writing directly to tproperties, save a delayed write request (flushed on table open).
			if (ulParentId != CACHE_NO_PARENT) {
                er = ECTPropsPurge::AddDeferredUpdateNoPurge(lpDatabase, ulParentId, 0, ulObjId);
				if(er != erSuccess)
					goto exit;
			}
		}
	}

	if (bNewItem && ulParentType == MAPI_FOLDER && RealObjType(ulObjType, ulParentType) == MAPI_MESSAGE) {
		iterInserted = setInserted.find(PR_SOURCE_KEY);
		if (iterInserted == setInserted.end()) {
			er = lpecSession->GetNewSourceKey(&sSourceKey);
			if (er != erSuccess)
				goto exit;

			strQuery = "INSERT INTO indexedproperties(hierarchyid,tag,val_binary) VALUES(" + stringify(ulObjId) + "," + stringify(PROP_ID(PR_SOURCE_KEY)) + "," + lpDatabase->EscapeBinary(sSourceKey, sSourceKey.size()) + ")";
			er = lpDatabase->DoInsert(strQuery);
			if (er != erSuccess)
				goto exit;

			g_lpSessionManager->GetCacheManager()->SetObjectProp(PROP_ID(PR_SOURCE_KEY), sSourceKey.size(), sSourceKey, ulObjId);
		}
	}

	if (RealObjType(ulObjType, ulParentType) == MAPI_ATTACH) {

		er = lpSource->Read(&ulLen, sizeof(ulLen), 1);
		if (er != erSuccess)
			goto exit;

		// We don't require the instance id, since we have no way of returning the instance id of this new object to the client.
		er = lpAttachmentStorage->SaveAttachment(ulObjId, PROP_ID(PR_ATTACH_DATA_BIN), true, ulLen, lpSource, NULL);
		if (er != erSuccess)
			goto exit;
	}

	if (!bNewItem)
		g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulObjId);

	g_lpSessionManager->GetCacheManager()->SetObject(ulObjId, ulParentId, ulOwner, ulFlags, ulObjType);
	
	if (lpPropValArray) {
		ASSERT(lppPropValArray != NULL);
		*lppPropValArray = lpPropValArray;
		lpPropValArray = NULL;
	}

exit:
	if (lpPropValArray)
		FreePropValArray(lpPropValArray, true);

	if (soap) {
		soap_end(soap);
		soap_free(soap);
	}

	return er;
}

ECRESULT DeserializeObject(ECSession *lpecSession, ECDatabase *lpDatabase, ECAttachmentStorage *lpAttachmentStorage, LPCSTREAMCAPS lpStreamCaps, unsigned int ulObjId, unsigned int ulStoreId, GUID *lpsGuid, bool bNewItem, unsigned long long ullIMAP, ECSerializer *lpSource, struct propValArray **lppPropValArray)
{
	ECRESULT		er = erSuccess;
	unsigned int	ulStreamVersion = 0;
	unsigned int	ulObjType = 0;
	unsigned int	ulRealObjType = 0;
	unsigned int	ulParentId = 0;
	unsigned int	ulParentType = 0;
	unsigned int	ulSize =0 ;
	struct propValArray *lpPropValArray = NULL;
	std::string		strQuery;

	if (!lpDatabase) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulObjId, &ulParentId, NULL, NULL, &ulObjType);
	if (er != erSuccess)
		goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulParentId, NULL, NULL, NULL, &ulParentType);
	if (er != erSuccess)
		goto exit;
		
	// Normalize the object type, but keep the original for storing in the db
	ulRealObjType = RealObjType(ulObjType, ulParentType);

	if (ulRealObjType != MAPI_MESSAGE && ulRealObjType != MAPI_ATTACH && ulRealObjType != MAPI_MAILUSER && ulRealObjType != MAPI_DISTLIST) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	if (lpStreamCaps == NULL) {
		er = lpSource->Read(&ulStreamVersion, sizeof(ulStreamVersion), 1);
		if (er != erSuccess)
			goto exit;

		if (ulStreamVersion >= arraySize(g_StreamCaps)) {
			er = ZARAFA_E_NO_SUPPORT;
			goto exit;
		}

		lpStreamCaps = &g_StreamCaps[ulStreamVersion];
	}

	er = DeserializeProps(lpecSession, lpDatabase, lpAttachmentStorage, lpStreamCaps, ulObjId, ulObjType, ulStoreId, lpsGuid, bNewItem, lpSource, lppPropValArray ? &lpPropValArray : NULL);
	if (er != erSuccess)
		goto exit;



	if (ulParentType == MAPI_FOLDER) {
		sObjectTableKey key(ulObjId, 0);
		propVal sProp;

		if (bNewItem) {
			er = g_lpSessionManager->GetNewSequence(ECSessionManager::SEQ_IMAP, &ullIMAP);
			if (er != erSuccess)
				goto exit;
		}

		strQuery = "INSERT INTO properties(hierarchyid, tag, type, val_ulong) VALUES(" +
						stringify(ulObjId) + "," +
						stringify(PROP_ID(PR_EC_IMAP_ID)) + "," +
						stringify(PROP_TYPE(PR_EC_IMAP_ID)) + "," +
						stringify_int64(ullIMAP) + ")";
		er = lpDatabase->DoInsert(strQuery);
		if (er != erSuccess)
			goto exit;

		sProp.ulPropTag = PR_EC_IMAP_ID;
		sProp.Value.ul = (unsigned int)ullIMAP;
		sProp.__union = SOAP_UNION_propValData_ul;
		er = g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_EC_IMAP_ID, &sProp);
		if (er != erSuccess)
			goto exit;
	}



	if (ulRealObjType == MAPI_MESSAGE || ulRealObjType == MAPI_ATTACH) {
		unsigned int	ulSubObjCount = 0;
		unsigned int	ulSubObjId = 0;
		unsigned int	ulSubObjType = 0;
		BOOL			fHasAttach = FALSE;
		
		er = lpSource->Read(&ulSubObjCount, sizeof(ulSubObjCount), 1);
		if (er != erSuccess)
			goto exit;

		for (unsigned i = 0; i < ulSubObjCount; ++i) {
			er = lpSource->Read(&ulSubObjType, sizeof(ulSubObjType), 1);
			if (er != erSuccess)
				goto exit;

			if (RealObjType(ulSubObjType, ulRealObjType) == MAPI_ATTACH)
				fHasAttach = TRUE;

			er = lpSource->Read(&ulSubObjId, sizeof(ulSubObjId), 1);
			if (er != erSuccess)
				goto exit;

			/**
			 * For new items we're not interested in the ulSubObjId from the stream, we do need
			 * to create the object with the current object as its parent
			 **/
			er = CreateObject(lpecSession, lpDatabase, ulObjId, ulSubObjType, 0, &ulSubObjId);
			if (er != erSuccess)
				goto exit;

			er = DeserializeObject(lpecSession, lpDatabase, lpAttachmentStorage, lpStreamCaps, ulSubObjId, ulStoreId, lpsGuid, bNewItem, 0, lpSource, NULL);
			if (er != erSuccess)
				goto exit;
		}

		if (ulRealObjType == MAPI_MESSAGE) {
			// We have to generate/update PR_HASATTACH
			
			sObjectTableKey key(ulObjId, 0);
			std::string strQuery;
			struct propVal sPropHasAttach;
			sPropHasAttach.ulPropTag = PR_HASATTACH;
			sPropHasAttach.Value.b = (fHasAttach != FALSE);
			sPropHasAttach.__union = SOAP_UNION_propValData_b;
	
			// Write in properties		
			strQuery.clear();
			WriteSingleProp(lpDatabase, ulObjId, ulParentId, &sPropHasAttach, false, 0, strQuery);
			er = lpDatabase->DoInsert(strQuery);
			if(er != erSuccess)
				goto exit;
				
			// Write in tproperties
			strQuery.clear();
			WriteSingleProp(lpDatabase, ulObjId, ulParentId, &sPropHasAttach, true, 0, strQuery);
			er = lpDatabase->DoInsert(strQuery);
			if(er != erSuccess)
				goto exit;
			
			// Update cache, since it may have been written before by WriteProps with a possibly wrong value
			g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_HASATTACH, &sPropHasAttach);
			
			// Update MSGFLAG_HASATTACH in the same way. We can assume PR_MESSAGE_FLAGS is already available, so we
			// just do an update (instead of REPLACE INTO)
			strQuery = (std::string)"UPDATE properties SET val_ulong = val_ulong " + (fHasAttach ? " | 0x10 " : " & ~0x10") + " WHERE hierarchyid = " + stringify(ulObjId) + " AND tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS));
			er = lpDatabase->DoUpdate(strQuery);
			if(er != erSuccess)
				goto exit;
				
			// Update cache if it's actually in the cache
			if(g_lpSessionManager->GetCacheManager()->GetCell(&key, PR_MESSAGE_FLAGS, &sPropHasAttach, NULL, false) == erSuccess) {
				sPropHasAttach.Value.ul &= ~MSGFLAG_HASATTACH;
				sPropHasAttach.Value.ul |= fHasAttach ? MSGFLAG_HASATTACH : 0;
				g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_MESSAGE_FLAGS, &sPropHasAttach);
			}
		}

		// Calc size of object, now that all children are saved.
		// Add new size
		if (CalculateObjectSize(lpDatabase, ulObjId, ulObjType, &ulSize) == erSuccess) {
			er = UpdateObjectSize(lpDatabase, ulObjId, ulObjType, UPDATE_SET, ulSize);
			if (er != erSuccess)
				goto exit;

			if (ulRealObjType == MAPI_MESSAGE && ulParentType == MAPI_FOLDER) {
				er = UpdateObjectSize(lpDatabase, ulStoreId, MAPI_STORE, UPDATE_ADD, ulSize);
				if (er != erSuccess)
					goto exit;
			}

		} else {
			ASSERT(FALSE);
		}
	}
	
	if (lpPropValArray) {
		ASSERT(lppPropValArray != NULL);
		*lppPropValArray = lpPropValArray;
		lpPropValArray = NULL;
	}

#ifdef EXPERIMENTAL
	g_lpSessionManager->GetCacheManager()->SetComplete(ulObjId);
#endif

exit:
	if (er != erSuccess) {
		lpSource->Flush(); // Flush the whole stream
		lpecSession->GetSessionManager()->GetLogger()->Log(EC_LOGLEVEL_ERROR, "DeserializeObject failed with error code 0x%08x", er);
	}	

	if (lpPropValArray)
		FreePropValArray(lpPropValArray, true);

	return er;
}


ECRESULT GetValidatedPropType(DB_ROW lpRow, unsigned int *lpulType)
{
	ECRESULT er = ZARAFA_E_DATABASE_ERROR;
	unsigned int ulType = 0;

	if (lpRow == NULL || lpulType == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	ulType = atoi(lpRow[FIELD_NR_TYPE]);
	switch (ulType) {
	case PT_I2:
		if (lpRow[FIELD_NR_ULONG] == NULL)
			goto exit;
		break;
	case PT_LONG:
		if (lpRow[FIELD_NR_ULONG] == NULL)
			goto exit;
		break;
	case PT_R4:
		if (lpRow[FIELD_NR_DOUBLE] == NULL)
			goto exit;
		break;
	case PT_BOOLEAN:
		if (lpRow[FIELD_NR_ULONG] == NULL)
			goto exit;
		break;
	case PT_DOUBLE:
	case PT_APPTIME:
		if (lpRow[FIELD_NR_DOUBLE] == NULL)
			goto exit;
		break;
	case PT_CURRENCY:
	case PT_SYSTIME:
		if (lpRow[FIELD_NR_HI] == NULL || lpRow[FIELD_NR_LO] == NULL) 
			goto exit;
		break;
	case PT_I8:
		if (lpRow[FIELD_NR_LONGINT] == NULL)
			goto exit;
		break;
	case PT_STRING8:
	case PT_UNICODE:
		if (lpRow[FIELD_NR_STRING] == NULL) 
			goto exit;
		break;
	case PT_CLSID:
	case PT_BINARY:
		if (lpRow[FIELD_NR_BINARY] == NULL) 
			goto exit;
		break;
	case PT_MV_I2:
		if (lpRow[FIELD_NR_ULONG] == NULL) 
			goto exit;
		break;
	case PT_MV_LONG:
		if (lpRow[FIELD_NR_ULONG] == NULL) 
			goto exit;
		break;
	case PT_MV_R4:
		if (lpRow[FIELD_NR_DOUBLE] == NULL) 
			goto exit;
		break;
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		if (lpRow[FIELD_NR_DOUBLE] == NULL) 
			goto exit;
		break;
	case PT_MV_CURRENCY:
	case PT_MV_SYSTIME:
		if (lpRow[FIELD_NR_HI] == NULL || lpRow[FIELD_NR_LO] == NULL) 
			goto exit;
		break;
	case PT_MV_BINARY:
	case PT_MV_CLSID:
		if (lpRow[FIELD_NR_BINARY] == NULL) 
			goto exit;
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		if (lpRow[FIELD_NR_STRING] == NULL) 
			goto exit;
		break;
	case PT_MV_I8:
		if (lpRow[FIELD_NR_LONGINT] == NULL) 
			goto exit;
		break;
	default:
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	er = erSuccess;
	*lpulType = ulType;

exit:
	return er;
}

ECRESULT GetValidatedPropCount(ECDatabase *lpDatabase, DB_RESULT lpDBResult, unsigned int *lpulCount)
{
	ECRESULT er = erSuccess;
	unsigned int ulCount = 0;
	DB_ROW lpRow;

	if (lpDatabase == NULL || lpDBResult == NULL || lpulCount == 0) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	while ((lpRow = lpDatabase->FetchRow(lpDBResult)) != NULL) {
		unsigned int ulType;
		er = GetValidatedPropType(lpRow, &ulType);	// Ignore ulType, we just need the validation
		if (er == ZARAFA_E_DATABASE_ERROR) {
			er = erSuccess;
			continue;
		} else if (er != erSuccess)
			goto exit;

		ulCount++;
	}

	lpDatabase->ResetResult(lpDBResult);
	*lpulCount = ulCount;

exit:
	return er;
}
