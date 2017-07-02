/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include <kopano/platform.h>

#include <mapidefs.h>
#include <mapitags.h>

#include "kcore.hpp"
#include "soapH.h"
#include "ECDatabase.h"
#include "ECDatabaseFactory.h"
#include "ECDatabaseUtils.h"
#include "SOAPUtils.h"
#include <kopano/stringutil.h>
#include "ECSessionManager.h"
#include <string>

namespace KC {

ECRESULT GetPropSize(DB_ROW lpRow, DB_LENGTHS lpLen, unsigned int *lpulSize)
{
	ECRESULT er = erSuccess;
	unsigned int ulSize = 0;
	unsigned int type = atoi(lpRow[FIELD_NR_TYPE]);

	switch(type) {
	case PT_I2:
		ulSize = 2;//FIXME: is this correct ?
		break;
	case PT_LONG:
	case PT_R4:
	case PT_BOOLEAN:
		ulSize = 1;
		break;
	case PT_DOUBLE:
	case PT_CURRENCY:
	case PT_APPTIME:
	case PT_SYSTIME:
	case PT_I8:
		ulSize = 8;
		break;
	case PT_STRING8:
	case PT_UNICODE:
		ulSize = lpLen[FIELD_NR_STRING];
		break;
	case PT_CLSID:
	case PT_BINARY:
		ulSize = lpLen[FIELD_NR_BINARY];
		break;
	default:
		er = KCERR_INVALID_TYPE;
	}

	*lpulSize = ulSize;
	return er;
}

// Case insensitive find
static size_t
ci_find_substr(const std::string &first, const std::string &second)
{
	return strToLower(first).find(strToLower(second));
}

ECRESULT CopySOAPPropValToDatabasePropVal(struct propVal *lpPropVal, unsigned int *lpulColNr, std::string &strColData, ECDatabase *lpDatabase, bool bTruncate)
{
	ULONG type = PROP_TYPE(lpPropVal->ulPropTag);

	switch(type) {
	case PT_I2:
		if (lpPropVal->__union != SOAP_UNION_propValData_i)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify(lpPropVal->Value.i);
		*lpulColNr = VALUE_NR_ULONG;
		break;
	case PT_LONG:
		if (lpPropVal->__union != SOAP_UNION_propValData_ul)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify(lpPropVal->Value.ul);
		*lpulColNr = VALUE_NR_ULONG;
		break;
	case PT_R4:
		if (lpPropVal->__union != SOAP_UNION_propValData_flt)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify_float(lpPropVal->Value.flt);
		*lpulColNr = VALUE_NR_DOUBLE;
		break;
	case PT_BOOLEAN:
		if (lpPropVal->__union != SOAP_UNION_propValData_b)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify(lpPropVal->Value.b);
		*lpulColNr = VALUE_NR_ULONG;
		break;
	case PT_DOUBLE:
		if (lpPropVal->__union != SOAP_UNION_propValData_dbl)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify_double(lpPropVal->Value.dbl);
		*lpulColNr = VALUE_NR_DOUBLE;
		if (ci_find_substr(strColData, std::string("nan")) != std::string::npos) {
			strColData = "0.0";
			ec_log_debug("%s:%d double value (%f) found, stringified to %s.", __FUNCTION__, __LINE__, lpPropVal->Value.dbl, strColData.c_str());
		}
	break;
	case PT_CURRENCY:
		if (lpPropVal->__union != SOAP_UNION_propValData_hilo ||
		    lpPropVal->Value.hilo == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify(lpPropVal->Value.hilo->hi,false,true) + "," + stringify(lpPropVal->Value.hilo->lo);
		*lpulColNr = VALUE_NR_HILO;
		break;
	case PT_APPTIME:
		if (lpPropVal->__union != SOAP_UNION_propValData_dbl)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify_double(lpPropVal->Value.dbl);
		*lpulColNr = VALUE_NR_DOUBLE;
		break;
	case PT_SYSTIME:
		if (lpPropVal->__union != SOAP_UNION_propValData_hilo ||
		    lpPropVal->Value.hilo == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify(lpPropVal->Value.hilo->hi,false,true) + "," + stringify(lpPropVal->Value.hilo->lo);
		*lpulColNr = VALUE_NR_HILO;
		break;
	case PT_I8:
		if (lpPropVal->__union != SOAP_UNION_propValData_li)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify_int64(int64_t(lpPropVal->Value.li));
		*lpulColNr = VALUE_NR_LONGINT;
		break;
	case PT_UNICODE:
	case PT_STRING8: {
		std::string strData;
		if (lpPropVal->__union != SOAP_UNION_propValData_lpszA ||
		    lpPropVal->Value.lpszA == NULL)
			return KCERR_INVALID_PARAMETER;
		if (bTruncate)
			u8_ncpy(lpPropVal->Value.lpszA, TABLE_CAP_STRING, &strData);
		else
			strData = lpPropVal->Value.lpszA;
		
		strColData = "'" +  lpDatabase->Escape(lpDatabase->FilterBMP(strData)) + "'";
		*lpulColNr = VALUE_NR_STRING;
		break;
	}
	case PT_BINARY: {
		unsigned int ulSize;
		if (lpPropVal->__union != SOAP_UNION_propValData_bin ||
		    lpPropVal->Value.bin == NULL ||
		    lpPropVal->Value.bin->__ptr == NULL)
			return KCERR_INVALID_PARAMETER;

		if (bTruncate && lpPropVal->Value.bin->__size > TABLE_CAP_BINARY)
			ulSize = TABLE_CAP_BINARY;
		else
			ulSize = lpPropVal->Value.bin->__size;
		
		strColData = lpDatabase->EscapeBinary(lpPropVal->Value.bin->__ptr, ulSize);
		*lpulColNr = VALUE_NR_BINARY;
		break;
	}
	case PT_CLSID:
		if (lpPropVal->__union != SOAP_UNION_propValData_bin ||
		    lpPropVal->Value.bin == NULL ||
		    lpPropVal->Value.bin->__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = lpDatabase->EscapeBinary(lpPropVal->Value.bin->__ptr, lpPropVal->Value.bin->__size);
		*lpulColNr = VALUE_NR_BINARY;
		break;
	default:
		return KCERR_INVALID_TYPE;
	}
	return erSuccess;
}

gsoap_size_t GetMVItemCount(struct propVal *lpPropVal)
{
	ULONG type = PROP_TYPE(lpPropVal->ulPropTag);
	gsoap_size_t ulSize = 0;

	switch(type) {
	case PT_MV_I2:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvi || lpPropVal->Value.mvi.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvi.__size;
		break;
	case PT_MV_LONG:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvl || lpPropVal->Value.mvl.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvl.__size;
		break;
	case PT_MV_R4:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvflt || lpPropVal->Value.mvflt.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvflt.__size;
		break;
	case PT_MV_DOUBLE:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvdbl || lpPropVal->Value.mvdbl.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvdbl.__size;
		break;
	case PT_MV_CURRENCY:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvhilo || lpPropVal->Value.mvhilo.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvhilo.__size;
		break;
	case PT_MV_APPTIME:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvdbl || lpPropVal->Value.mvdbl.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvdbl.__size;
		break;
	case PT_MV_SYSTIME:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvhilo || lpPropVal->Value.mvhilo.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvhilo.__size;
		break;
	case PT_MV_BINARY:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvbin || lpPropVal->Value.mvbin.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvbin.__size;
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvszA || lpPropVal->Value.mvszA.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvszA.__size;
		break;
	case PT_MV_CLSID:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvbin || lpPropVal->Value.mvbin.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvbin.__size;
		break;
	case PT_MV_I8:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvli || lpPropVal->Value.mvli.__ptr == NULL)
			ulSize = 0;
		else
			ulSize = lpPropVal->Value.mvli.__size;
		break;
	default:
		ulSize = 0;
		break;
	}

	return ulSize;
}

ECRESULT CopySOAPPropValToDatabaseMVPropVal(struct propVal *lpPropVal, int nItem, std::string &strColName, std::string &strColData, ECDatabase *lpDatabase)
{
	ULONG type = PROP_TYPE(lpPropVal->ulPropTag);

	switch(type) {
	case PT_MV_I2:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvi ||
		    lpPropVal->Value.mvi.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify(lpPropVal->Value.mvi.__ptr[nItem]);
		strColName = PROPCOL_ULONG;
		break;
	case PT_MV_LONG:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvl ||
		    lpPropVal->Value.mvl.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify(lpPropVal->Value.mvl.__ptr[nItem]);
		strColName = PROPCOL_ULONG;
		break;
	case PT_MV_R4:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvflt ||
		    lpPropVal->Value.mvflt.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify_float(lpPropVal->Value.mvflt.__ptr[nItem]);
		strColName = PROPCOL_DOUBLE;
		break;
	case PT_MV_DOUBLE:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvdbl ||
		    lpPropVal->Value.mvdbl.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify_double(lpPropVal->Value.mvdbl.__ptr[nItem]);
		strColName = PROPCOL_DOUBLE;
		break;
	case PT_MV_CURRENCY:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvhilo ||
		    lpPropVal->Value.mvhilo.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify(lpPropVal->Value.mvhilo.__ptr[nItem].hi,false,true) + "," + stringify(lpPropVal->Value.mvhilo.__ptr[nItem].lo);
		strColName = PROPCOL_HILO;
		break;
	case PT_MV_APPTIME:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvdbl ||
		    lpPropVal->Value.mvdbl.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify_double(lpPropVal->Value.mvdbl.__ptr[nItem]);
		strColName = PROPCOL_DOUBLE;
		break;
	case PT_MV_SYSTIME:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvhilo ||
		    lpPropVal->Value.mvhilo.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify(lpPropVal->Value.mvhilo.__ptr[nItem].hi,false,true) + "," + stringify(lpPropVal->Value.mvhilo.__ptr[nItem].lo);
		strColName = PROPCOL_HILO;
		break;
	case PT_MV_BINARY:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvbin ||
		    lpPropVal->Value.mvbin.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = lpDatabase->EscapeBinary(lpPropVal->Value.mvbin.__ptr[nItem].__ptr, lpPropVal->Value.mvbin.__ptr[nItem].__size);
		strColName = PROPCOL_BINARY;
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvszA ||
		    lpPropVal->Value.mvszA.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = "'" + lpDatabase->Escape(lpDatabase->FilterBMP(lpPropVal->Value.mvszA.__ptr[nItem])) + "'";
		strColName = PROPCOL_STRING;
		break;
	case PT_MV_CLSID:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvbin ||
		    lpPropVal->Value.mvbin.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = lpDatabase->EscapeBinary(lpPropVal->Value.mvbin.__ptr[nItem].__ptr, lpPropVal->Value.mvbin.__ptr[nItem].__size);
		strColName = PROPCOL_BINARY;
		break;
	case PT_MV_I8:
		if (lpPropVal->__union != SOAP_UNION_propValData_mvli ||
		    lpPropVal->Value.mvli.__ptr == NULL)
			return KCERR_INVALID_PARAMETER;
		strColData = stringify_int64(lpPropVal->Value.mvli.__ptr[nItem]);
		strColName = PROPCOL_LONGINT;
		break;
	default:
		return KCERR_INVALID_TYPE;
	}
	return erSuccess;
}

ECRESULT ParseMVProp(const char *lpRowData, ULONG ulSize,
    unsigned int *lpulLastPos, std::string *lpstrData)
{
	ULONG	ulPos = *lpulLastPos;
	ULONG   ulLen = 0;
	char	*lpEnd = NULL;
	// lpRowData -> length:datalength:data
	assert(ulPos < ulSize);

	if (ulPos >= ulSize)
		return KCERR_INVALID_PARAMETER;
	ulLen = strtoul(lpRowData + ulPos, &lpEnd, 10);
	if (lpEnd == lpRowData + ulPos || lpEnd == NULL || *lpEnd != ':')
		return KCERR_INVALID_PARAMETER;
	if (lpRowData + ulSize < lpEnd + 1 + ulLen)
		// not enough data from mysql
		return KCERR_INVALID_PARAMETER;

	lpstrData->assign(lpEnd + 1, ulLen);
	*lpulLastPos = (lpEnd - lpRowData) + 1 + ulLen;
	return erSuccess;
}

ULONG GetColOffset(unsigned int ulPropTag)
{
	ULONG ulField = 0;
	
	switch(PROP_TYPE(ulPropTag) & ~ (MV_FLAG | MV_INSTANCE)) {
	case PT_I2:
		ulField = FIELD_NR_ULONG;
		break;
	case PT_LONG:
		ulField = FIELD_NR_ULONG;
		break;
	case PT_R4:
		ulField = FIELD_NR_DOUBLE;
		break;
	case PT_BOOLEAN:
		ulField = FIELD_NR_ULONG;
		break;
	case PT_DOUBLE:
		ulField = FIELD_NR_DOUBLE;
		break;
	case PT_APPTIME:
		ulField = FIELD_NR_DOUBLE;
		break;
	case PT_I8:
		ulField = FIELD_NR_LONGINT;
		break;
	case PT_STRING8:
		ulField = FIELD_NR_STRING;
		break;
	case PT_UNICODE:
		ulField = FIELD_NR_STRING;
		break;
	case PT_CLSID:
		ulField = FIELD_NR_BINARY;
		break;
	case PT_BINARY:
		ulField = FIELD_NR_BINARY;
		break;
	default:
		break;
	}
	
	return ulField;
}

std::string GetPropColOrder(unsigned int ulPropTag,
    const std::string &strSubQuery)
{
	std::string strPropColOrder = "0," + stringify(PROP_ID(ulPropTag)) + "," + stringify(PROP_TYPE(ulPropTag));
	unsigned int ulField = 0;
	
	ulField = GetColOffset(ulPropTag);
	
	for (unsigned int i = 3; i < FIELD_NR_MAX; ++i) {
		strPropColOrder += ",";
		if(i == ulField)
			strPropColOrder += "(" + strSubQuery + ")";
		else
			strPropColOrder += "NULL";
	}
	
	return strPropColOrder;
}

// Copies a single PropVal from the given row (all columns from properties must be passed) to the
// given propvalType pointer
ECRESULT CopyDatabasePropValToSOAPPropVal(struct soap *soap, DB_ROW lpRow, DB_LENGTHS lpLen, propVal *lpPropVal)
{
	ECRESULT er = erSuccess;
	unsigned int ulLastPos;
	std::string	strData;
	unsigned int type = atoi(lpRow[FIELD_NR_TYPE]);
	unsigned int ulPropTag;
	locale_t loc = createlocale(LC_NUMERIC, "C");

	if ((type & MVI_FLAG) == MVI_FLAG)
		// Treat MVI as normal property
		type &= ~MVI_FLAG;

	ulPropTag = PROP_TAG(type,atoi(lpRow[FIELD_NR_TAG]));

	switch(type) {
	case PT_I2:
		if(lpRow[FIELD_NR_ULONG] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_i;
		lpPropVal->Value.i = (short)atoi(lpRow[FIELD_NR_ULONG]);
		break;
	case PT_LONG:
		if(lpRow[FIELD_NR_ULONG] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_ul;
		lpPropVal->Value.ul = atoui(lpRow[FIELD_NR_ULONG]);
		break;
	case PT_R4:
		if(lpRow[FIELD_NR_DOUBLE] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_flt;
		lpPropVal->Value.flt = (float)strtod_l(lpRow[FIELD_NR_DOUBLE], NULL, loc);
		break;
	case PT_BOOLEAN:
		if(lpRow[FIELD_NR_ULONG] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_b;
		lpPropVal->Value.b = atoi(lpRow[FIELD_NR_ULONG]) ? true : false;
		break;
	case PT_DOUBLE:
		if(lpRow[FIELD_NR_DOUBLE] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_dbl;
		lpPropVal->Value.dbl = strtod_l(lpRow[FIELD_NR_DOUBLE], NULL, loc);
		break;
	case PT_CURRENCY:
		if(lpRow[FIELD_NR_HI] == NULL || lpRow[FIELD_NR_LO] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_hilo;
		lpPropVal->Value.hilo = s_alloc<hiloLong>(soap);
		lpPropVal->Value.hilo->hi = atoi(lpRow[FIELD_NR_HI]);
		lpPropVal->Value.hilo->lo = atoui(lpRow[FIELD_NR_LO]);
		break;
	case PT_APPTIME:
		if(lpRow[FIELD_NR_DOUBLE] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_dbl;
		lpPropVal->Value.dbl = strtod_l(lpRow[FIELD_NR_DOUBLE], NULL, loc);
		break;
	case PT_SYSTIME:
		if(lpRow[FIELD_NR_HI] == NULL || lpRow[FIELD_NR_LO] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_hilo;
		lpPropVal->Value.hilo = s_alloc<hiloLong>(soap);
		lpPropVal->Value.hilo->hi = atoi(lpRow[FIELD_NR_HI]);
		lpPropVal->Value.hilo->lo = atoui(lpRow[FIELD_NR_LO]);
		break;
	case PT_I8:
		if(lpRow[FIELD_NR_LONGINT] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_li;
		lpPropVal->Value.li = atoll(lpRow[FIELD_NR_LONGINT]);
		break;
	case PT_STRING8:
	case PT_UNICODE:
		if(lpRow[FIELD_NR_STRING] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_lpszA;
		lpPropVal->Value.lpszA = s_alloc<char>(soap, lpLen[FIELD_NR_STRING]+1);
		strcpy(lpPropVal->Value.lpszA, lpRow[FIELD_NR_STRING]);
		ulPropTag = CHANGE_PROP_TYPE(ulPropTag, PT_UNICODE); // return unicode strings to client, because database contains UTF-8

		break;
	case PT_CLSID:
		if(lpRow[FIELD_NR_BINARY] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_bin;
		lpPropVal->Value.bin = s_alloc<struct xsd__base64Binary>(soap);
		lpPropVal->Value.bin->__ptr = s_alloc<unsigned char>(soap, lpLen[FIELD_NR_BINARY]);
		memcpy(lpPropVal->Value.bin->__ptr, lpRow[FIELD_NR_BINARY],lpLen[FIELD_NR_BINARY]);
		lpPropVal->Value.bin->__size = lpLen[FIELD_NR_BINARY];
		break;
	case PT_BINARY:
		if(lpRow[FIELD_NR_BINARY] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_bin;
		lpPropVal->Value.bin = s_alloc<struct xsd__base64Binary>(soap);
		lpPropVal->Value.bin->__ptr = s_alloc<unsigned char>(soap, lpLen[FIELD_NR_BINARY]);
		memcpy(lpPropVal->Value.bin->__ptr, lpRow[FIELD_NR_BINARY],lpLen[FIELD_NR_BINARY]);
		lpPropVal->Value.bin->__size = lpLen[FIELD_NR_BINARY];
		break;
	case PT_MV_I2:
		if(lpRow[FIELD_NR_ULONG] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}

		lpPropVal->__union = SOAP_UNION_propValData_mvi;
		lpPropVal->Value.mvi.__size = atoi(lpRow[FIELD_NR_ID]);
		lpPropVal->Value.mvi.__ptr = s_alloc<short int>(soap, lpPropVal->Value.mvi.__size);
		ulLastPos = 0;
		for (gsoap_size_t i = 0; i < lpPropVal->Value.mvi.__size; ++i) {
			ParseMVProp(lpRow[FIELD_NR_ULONG], lpLen[FIELD_NR_ULONG], &ulLastPos, &strData);
			lpPropVal->Value.mvi.__ptr[i] = (short)atoui((char *)strData.c_str());
		}
		break;
	case PT_MV_LONG:
		if(lpRow[FIELD_NR_ULONG] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}

		lpPropVal->__union = SOAP_UNION_propValData_mvl;
		lpPropVal->Value.mvl.__size = atoi(lpRow[FIELD_NR_ID]);
		lpPropVal->Value.mvl.__ptr = s_alloc<unsigned int>(soap, lpPropVal->Value.mvl.__size);
		ulLastPos = 0;
		for (gsoap_size_t i = 0; i < lpPropVal->Value.mvl.__size; ++i) {
			ParseMVProp(lpRow[FIELD_NR_ULONG], lpLen[FIELD_NR_ULONG], &ulLastPos, &strData);
			lpPropVal->Value.mvl.__ptr[i] = atoui((char*)strData.c_str());
		}
		break;
	case PT_MV_R4:
		if(lpRow[FIELD_NR_DOUBLE] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}

		lpPropVal->__union = SOAP_UNION_propValData_mvflt;
		lpPropVal->Value.mvflt.__size = atoi(lpRow[FIELD_NR_ID]);
		lpPropVal->Value.mvflt.__ptr = s_alloc<float>(soap, lpPropVal->Value.mvflt.__size);
		ulLastPos = 0;
		for (gsoap_size_t i = 0; i < lpPropVal->Value.mvflt.__size; ++i) {
			ParseMVProp(lpRow[FIELD_NR_DOUBLE], lpLen[FIELD_NR_DOUBLE], &ulLastPos, &strData);
			lpPropVal->Value.mvflt.__ptr[i] = (float)strtod_l(strData.c_str(), NULL, loc);
		}
		break;
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		if(lpRow[FIELD_NR_DOUBLE] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}

		lpPropVal->__union = SOAP_UNION_propValData_mvdbl;
		lpPropVal->Value.mvdbl.__size = atoi(lpRow[FIELD_NR_ID]);
		lpPropVal->Value.mvdbl.__ptr = s_alloc<double>(soap, lpPropVal->Value.mvdbl.__size);
		ulLastPos = 0;
		for (gsoap_size_t i = 0; i < lpPropVal->Value.mvdbl.__size; ++i) {
			ParseMVProp(lpRow[FIELD_NR_DOUBLE], lpLen[FIELD_NR_DOUBLE], &ulLastPos, &strData);
			lpPropVal->Value.mvdbl.__ptr[i] = strtod_l(strData.c_str(), NULL, loc);
		}
		break;
	case PT_MV_CURRENCY:
	case PT_MV_SYSTIME:
		if(lpRow[FIELD_NR_HI] == NULL || lpRow[FIELD_NR_LO] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_mvhilo;
		lpPropVal->Value.mvhilo.__size = atoi(lpRow[FIELD_NR_ID]);
		lpPropVal->Value.mvhilo.__ptr = s_alloc<hiloLong>(soap, lpPropVal->Value.mvhilo.__size);
		//Scan low
		ulLastPos = 0;
		for (gsoap_size_t i = 0; i < lpPropVal->Value.mvhilo.__size; ++i) {
			ParseMVProp(lpRow[FIELD_NR_LO], lpLen[FIELD_NR_LO], &ulLastPos, &strData);
			lpPropVal->Value.mvhilo.__ptr[i].lo = atoui((char*)strData.c_str());
		}
		//Scan high
		ulLastPos = 0;
		for (gsoap_size_t i = 0; i < lpPropVal->Value.mvhilo.__size; ++i) {
			ParseMVProp(lpRow[FIELD_NR_HI], lpLen[FIELD_NR_HI], &ulLastPos, &strData);
			lpPropVal->Value.mvhilo.__ptr[i].hi = atoi((char*)strData.c_str());
		}
		break;
	case PT_MV_BINARY:
	case PT_MV_CLSID:
		if(lpRow[FIELD_NR_BINARY] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_mvbin;
		lpPropVal->Value.mvbin.__size = atoi(lpRow[FIELD_NR_ID]);
		lpPropVal->Value.mvbin.__ptr = s_alloc<struct xsd__base64Binary>(soap, lpPropVal->Value.mvbin.__size);
		ulLastPos = 0;
		for (gsoap_size_t i = 0; i < lpPropVal->Value.mvbin.__size; ++i) {
			ParseMVProp(lpRow[FIELD_NR_BINARY], lpLen[FIELD_NR_BINARY], &ulLastPos, &strData);
			lpPropVal->Value.mvbin.__ptr[i].__size = strData.size();
			lpPropVal->Value.mvbin.__ptr[i].__ptr = s_alloc<unsigned char>(soap, lpPropVal->Value.mvbin.__ptr[i].__size);
			memcpy(lpPropVal->Value.mvbin.__ptr[i].__ptr, strData.c_str(), sizeof(unsigned char) * lpPropVal->Value.mvbin.__ptr[i].__size);
		}
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		if(lpRow[FIELD_NR_STRING] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}
		lpPropVal->__union = SOAP_UNION_propValData_mvszA;
		lpPropVal->Value.mvszA.__size = atoi(lpRow[FIELD_NR_ID]);
		lpPropVal->Value.mvszA.__ptr = s_alloc<char *>(soap, lpPropVal->Value.mvszA.__size);
		ulLastPos = 0;
		for (gsoap_size_t i = 0; i < lpPropVal->Value.mvszA.__size; ++i) {
			ParseMVProp(lpRow[FIELD_NR_STRING], lpLen[FIELD_NR_STRING], &ulLastPos, &strData);
			lpPropVal->Value.mvszA.__ptr[i] = s_alloc<char>(soap, strData.size() + 1);
			memcpy(lpPropVal->Value.mvszA.__ptr[i], strData.c_str(), strData.size() + 1);
		}
		ulPropTag = CHANGE_PROP_TYPE(ulPropTag, PT_MV_UNICODE); // return unicode strings to client, because database contains UTF-8
		break;
	case PT_MV_I8:
		if(lpRow[FIELD_NR_LONGINT] == NULL) {
			er = KCERR_NOT_FOUND;
			goto exit;
		}

		lpPropVal->__union = SOAP_UNION_propValData_mvli;
		lpPropVal->Value.mvli.__size = atoi(lpRow[FIELD_NR_ID]);
		lpPropVal->Value.mvli.__ptr = s_alloc<LONG64>(soap, lpPropVal->Value.mvli.__size);
		ulLastPos = 0;
		for (gsoap_size_t i = 0; i < lpPropVal->Value.mvli.__size; ++i) {
			ParseMVProp(lpRow[FIELD_NR_LONGINT], lpLen[FIELD_NR_LONGINT], &ulLastPos, &strData);
			lpPropVal->Value.mvli.__ptr[i] = atoll(strData.c_str());
		}
		break;
	default:
		er = KCERR_INVALID_TYPE;
		goto exit;
	}

	lpPropVal->ulPropTag = ulPropTag;

exit:
	freelocale(loc);
	return er;
}

unsigned int NormalizeDBPropTag(unsigned int ulPropTag)
{
	switch (PROP_TYPE(ulPropTag)) {
	case PT_UNICODE:					return CHANGE_PROP_TYPE(ulPropTag, PT_STRING8);
	case PT_MV_UNICODE: 				return CHANGE_PROP_TYPE(ulPropTag, PT_MV_STRING8);
	case (PT_MV_UNICODE | MV_INSTANCE):	return CHANGE_PROP_TYPE(ulPropTag, (PT_MV_STRING8 | MV_INSTANCE));
	default: 							return ulPropTag;
	}
}

bool CompareDBPropTag(unsigned int ulPropTag1, unsigned int ulPropTag2)
{
	ulPropTag1 = NormalizeDBPropTag(ulPropTag1);
	ulPropTag2 = NormalizeDBPropTag(ulPropTag2);
	
	return ulPropTag1 == ulPropTag2;
}

ECRESULT GetDatabaseSettingAsInteger(ECDatabase *lpDatabase, const std::string &strSettings, unsigned int *lpulResult)
{
	ECRESULT		er = erSuccess;
	DB_RESULT lpDBResult;
	DB_ROW			lpDBRow = NULL;
	std::string		strQuery;

	strQuery = "SELECT `value` FROM settings WHERE `name` = '"+lpDatabase->Escape(strSettings)+"'";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		return er;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if (lpDBRow == nullptr || lpDBRow[0] == nullptr)
		return KCERR_NOT_FOUND;
	*lpulResult = atoui(lpDBRow[0]);
	return erSuccess;
}

ECRESULT SetDatabaseSetting(ECDatabase *lpDatabase, const std::string &strSettings, unsigned int ulValue)
{
	ECRESULT		er = erSuccess;
	std::string		strQuery;

	strQuery = "REPLACE INTO settings (`name`, `value`) VALUES('"+lpDatabase->Escape(strSettings)+"', '"+stringify(ulValue)+"')";
	er = lpDatabase->DoUpdate(strQuery);

	return er;
}

SuppressLockErrorLogging::SuppressLockErrorLogging(ECDatabase *lpDatabase)
: m_lpDatabase(lpDatabase)
, m_bResetValue(lpDatabase ? lpDatabase->SuppressLockErrorLogging(true) : false)
{ }

SuppressLockErrorLogging::~SuppressLockErrorLogging()
{
	if (m_lpDatabase)
		m_lpDatabase->SuppressLockErrorLogging(m_bResetValue);
}

} /* namespace */
