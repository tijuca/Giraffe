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

#include "platform.h"

#include <mapidefs.h>
#include <mapitags.h>
#include <edkmdb.h>
#include "ECGuid.h"
#include <Zarafa.h>

#include "SOAPUtils.h"
#include "SOAPAlloc.h"
#include "stringutil.h"
#include "ustringutil.h"
#include "base64.h"

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

class MVPropProxy
{
public:
	MVPropProxy(struct propVal *lpMVProp): m_lpMVProp(lpMVProp)
	{ }

	unsigned int size() const {
		if (m_lpMVProp == NULL || (PROP_TYPE(m_lpMVProp->ulPropTag) & MV_FLAG) == 0)
			return 0;

		switch (PROP_TYPE(m_lpMVProp->ulPropTag)) {
			case PT_MV_I2:		return m_lpMVProp->Value.mvi.__size;
			case PT_MV_LONG:	return m_lpMVProp->Value.mvl.__size;
			case PT_MV_R4:		return m_lpMVProp->Value.mvflt.__size;
			case PT_MV_DOUBLE:
			case PT_MV_APPTIME:	return m_lpMVProp->Value.mvdbl.__size;
			case PT_MV_I8:		return m_lpMVProp->Value.mvl.__size;
			case PT_MV_SYSTIME:
			case PT_MV_CURRENCY:return m_lpMVProp->Value.mvhilo.__size;
			case PT_MV_CLSID:
			case PT_MV_BINARY:	return m_lpMVProp->Value.mvbin.__size;
			case PT_MV_STRING8:
			case PT_MV_UNICODE:	return m_lpMVProp->Value.mvszA.__size;
			default: return 0;
		}
	}

	ECRESULT compare(unsigned int ulIndex, const struct propVal *lpProp, const ECLocale &locale, int *lpnCompareResult)
	{
		ECRESULT er = erSuccess;
		int nCompareResult = 0;

		if (m_lpMVProp == NULL || 
			(PROP_TYPE(m_lpMVProp->ulPropTag) & MV_FLAG) == 0 || 
			(PROP_TYPE(m_lpMVProp->ulPropTag) & ~MV_FLAG) != PROP_TYPE(lpProp->ulPropTag) ||
			ulIndex >= size())
		{
			er = ZARAFA_E_INVALID_PARAMETER;
			goto exit;
		}

		switch (PROP_TYPE(m_lpMVProp->ulPropTag)) {
			case PT_MV_I2:		
				nCompareResult = m_lpMVProp->Value.mvi.__ptr[ulIndex] - lpProp->Value.i;
				break;

			case PT_MV_LONG:	
				nCompareResult = m_lpMVProp->Value.mvl.__ptr[ulIndex] - lpProp->Value.ul;
				break;

			case PT_MV_R4:		
				nCompareResult = m_lpMVProp->Value.mvflt.__ptr[ulIndex] - lpProp->Value.flt;
				break;

			case PT_MV_DOUBLE:
			case PT_MV_APPTIME:	
				nCompareResult = m_lpMVProp->Value.mvdbl.__ptr[ulIndex] - lpProp->Value.dbl;
				break;

			case PT_MV_I8:		
				nCompareResult = m_lpMVProp->Value.mvl.__ptr[ulIndex] - lpProp->Value.li;
				break;

			case PT_MV_SYSTIME:
			case PT_MV_CURRENCY:
				if (m_lpMVProp->Value.mvhilo.__ptr[ulIndex].hi == lpProp->Value.hilo->hi)
					nCompareResult = m_lpMVProp->Value.mvhilo.__ptr[ulIndex].lo - lpProp->Value.hilo->lo;
				else
					nCompareResult = m_lpMVProp->Value.mvhilo.__ptr[ulIndex].hi - lpProp->Value.hilo->hi;
				break;

			case PT_MV_CLSID:
			case PT_MV_BINARY:
				nCompareResult = m_lpMVProp->Value.mvbin.__ptr[ulIndex].__size - lpProp->Value.bin->__size;
				if (nCompareResult == 0)
					nCompareResult = memcmp(m_lpMVProp->Value.mvbin.__ptr[ulIndex].__ptr, lpProp->Value.bin->__ptr, lpProp->Value.bin->__size);
				break;

			case PT_MV_STRING8:
			case PT_MV_UNICODE:
				if (m_lpMVProp->Value.mvszA.__ptr[ulIndex] == NULL && lpProp->Value.lpszA != NULL)
					nCompareResult = 1;
				else if (m_lpMVProp->Value.mvszA.__ptr[ulIndex] != NULL && lpProp->Value.lpszA == NULL)
					nCompareResult = -1;
				else 
					nCompareResult = u8_icompare(m_lpMVProp->Value.mvszA.__ptr[ulIndex], lpProp->Value.lpszA, locale);
				break;

			default:
				er = ZARAFA_E_INVALID_PARAMETER;
				goto exit;
		}

		*lpnCompareResult = nCompareResult;

	exit:
		return er;
	}

private:
	struct propVal *m_lpMVProp;
};

void FreeSortOrderArray(struct sortOrderArray *lpsSortOrder)
{

	if(lpsSortOrder == NULL)
		return;

	if(lpsSortOrder->__size > 0)
		delete[] lpsSortOrder->__ptr;

	delete lpsSortOrder;

}

int CompareSortOrderArray(const struct sortOrderArray *lpsSortOrder1,
    const struct sortOrderArray *lpsSortOrder2)
{
	int i;

	if(lpsSortOrder1 == NULL && lpsSortOrder2 == NULL)
		return 0; // both NULL

	if(lpsSortOrder1 == NULL || lpsSortOrder2 == NULL)
		return -1; // not equal due to one of them being NULL

	if(lpsSortOrder1->__size != lpsSortOrder2->__size)
		return lpsSortOrder1->__size - lpsSortOrder2->__size;

	for(i=0;i<lpsSortOrder1->__size;i++) {
		if(lpsSortOrder1->__ptr[i].ulPropTag != lpsSortOrder2->__ptr[i].ulPropTag)
			return -1;
		if(lpsSortOrder1->__ptr[i].ulOrder != lpsSortOrder2->__ptr[i].ulOrder)
			return -1;
	}

	// Exact match
	return 0;
}

ECRESULT CopyPropTagArray(struct soap *soap,
    const struct propTagArray *lpPTsSrc, struct propTagArray **lppsPTsDst)
{
	ECRESULT er = erSuccess;
	struct propTagArray* lpPTsDst = NULL;

	if(lppsPTsDst == NULL || lpPTsSrc == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpPTsDst = s_alloc<struct propTagArray>(soap);
	lpPTsDst->__size = lpPTsSrc->__size;

	if(lpPTsSrc->__size > 0) {
		lpPTsDst->__ptr = s_alloc<unsigned int>(soap, lpPTsSrc->__size );
		memcpy(lpPTsDst->__ptr, lpPTsSrc->__ptr, sizeof(unsigned int) * lpPTsSrc->__size);
	} else {
		lpPTsDst->__ptr = NULL;
	}

	*lppsPTsDst = lpPTsDst;

exit:
	return er;
}

void FreePropTagArray(struct propTagArray *lpsPropTags, bool bFreeBase)
{

	if(lpsPropTags == NULL)
		return;

	if(lpsPropTags->__size > 0)
		delete [] lpsPropTags->__ptr;

	if(bFreeBase)
		delete lpsPropTags;

}

/**
 * Finds a specific property tag in an soap propValArray.
 *
 * @param[in]	lpPropValArray	SOAP propValArray
 * @param[in]	ulPropTagq		Property to search for in array, type may also be PT_UNSPECIFIED to find the first match on the PROP_ID
 * @return		propVal*		Direct pointer into the propValArray where the found property is, or NULL if not found.
 */
struct propVal *FindProp(const struct propValArray *lpPropValArray,
    unsigned int ulPropTag)
{
	int i = 0;

	if (lpPropValArray == NULL)
		return NULL;

	for (i = 0; i < lpPropValArray->__size; i++) {
		if (lpPropValArray->__ptr[i].ulPropTag == ulPropTag ||
			(PROP_TYPE(ulPropTag) == PT_UNSPECIFIED && PROP_ID(lpPropValArray->__ptr[i].ulPropTag) == PROP_ID(ulPropTag)))
			return &lpPropValArray->__ptr[i];
	}

	return NULL;
}

/*
this function check if the right proptag with the value and is't null
*/
static ECRESULT PropCheck(const struct propVal *lpProp)
{
	ECRESULT er = erSuccess;

	if(lpProp == NULL)
		return ZARAFA_E_INVALID_PARAMETER;

	switch(PROP_TYPE(lpProp->ulPropTag))
	{
	case PT_I2:
		if(lpProp->__union != SOAP_UNION_propValData_i)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_LONG:
		if(lpProp->__union != SOAP_UNION_propValData_ul)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_R4:
		if(lpProp->__union != SOAP_UNION_propValData_flt)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_BOOLEAN:
		if(lpProp->__union != SOAP_UNION_propValData_b)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_DOUBLE:
		if(lpProp->__union != SOAP_UNION_propValData_dbl)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_APPTIME:
		if(lpProp->__union != SOAP_UNION_propValData_dbl)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_I8:
		if(lpProp->__union != SOAP_UNION_propValData_li)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_SYSTIME:
		if(lpProp->__union != SOAP_UNION_propValData_hilo)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_CURRENCY:
		if(lpProp->__union != SOAP_UNION_propValData_hilo)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_UNICODE:
		if(lpProp->__union != SOAP_UNION_propValData_lpszA)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_STRING8:
		if(lpProp->__union != SOAP_UNION_propValData_lpszA)
			er = ZARAFA_E_INVALID_PARAMETER;
		else {
			if(lpProp->Value.lpszA == NULL)
				er = ZARAFA_E_INVALID_PARAMETER;
			else
				er = erSuccess;
		}
		break;
	case PT_BINARY:
		if(lpProp->__union != SOAP_UNION_propValData_bin)
			er = ZARAFA_E_INVALID_PARAMETER;
		else {
			if(lpProp->Value.bin->__size > 0)
			{
				if(lpProp->Value.bin->__ptr == NULL)
					er = ZARAFA_E_INVALID_PARAMETER;
			}
		}
		break;
	case PT_CLSID:
		if(lpProp->__union != SOAP_UNION_propValData_bin)
			er = ZARAFA_E_INVALID_PARAMETER;
		else {
			if(lpProp->Value.bin->__size > 0)
			{
				if(lpProp->Value.bin->__ptr == NULL || (lpProp->Value.bin->__size%sizeof(GUID)) != 0)
					er = ZARAFA_E_INVALID_PARAMETER;
			}
		}
		break;

		// TODO: check __ptr pointers?
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		if(lpProp->__union != SOAP_UNION_propValData_mvdbl)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_MV_CLSID:
	case PT_MV_BINARY:
		if(lpProp->__union != SOAP_UNION_propValData_mvbin)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_MV_SYSTIME:
	case PT_MV_CURRENCY:
		if(lpProp->__union != SOAP_UNION_propValData_mvhilo)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_MV_FLOAT:
		if(lpProp->__union != SOAP_UNION_propValData_mvflt)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_MV_I2:
		if(lpProp->__union != SOAP_UNION_propValData_mvi)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_MV_I8:
		if(lpProp->__union != SOAP_UNION_propValData_mvli)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_MV_LONG:
		if(lpProp->__union != SOAP_UNION_propValData_mvl)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_MV_UNICODE:
	case PT_MV_STRING8:
		if(lpProp->__union != SOAP_UNION_propValData_mvszA)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_ACTIONS:
		if(lpProp->__union != SOAP_UNION_propValData_actions)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	case PT_SRESTRICTION:
		if(lpProp->__union != SOAP_UNION_propValData_res)
			er = ZARAFA_E_INVALID_PARAMETER;
		break;
	default:
		er = erSuccess;
		break;
	}
	return er;
}

static ECRESULT CompareABEID(const struct propVal *lpProp1,
    const struct propVal *lpProp2, int *lpCompareResult)
{
	ECRESULT er = erSuccess;
	int iResult = 0;

	ASSERT(lpProp1 != NULL && PROP_TYPE(lpProp1->ulPropTag) == PT_BINARY);
	ASSERT(lpProp2 != NULL && PROP_TYPE(lpProp2->ulPropTag) == PT_BINARY);
	ASSERT(lpCompareResult != NULL);

	const ABEID *peid1 = (PABEID)lpProp1->Value.bin->__ptr;
	const ABEID *peid2 = (PABEID)lpProp2->Value.bin->__ptr;

	if (memcmp(&peid1->guid, &MUIDECSAB, sizeof(GUID)) || memcmp(&peid2->guid, &MUIDECSAB, sizeof(GUID))) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (peid1->ulVersion == peid2->ulVersion) {
		if (lpProp1->Value.bin->__size != lpProp2->Value.bin->__size)
			iResult = (int)(lpProp1->Value.bin->__size - lpProp2->Value.bin->__size);

		else if (peid1->ulVersion == 0)
			iResult = (int)(peid1->ulId - peid2->ulId);
		
		else
			iResult = strcmp((const char *)peid1->szExId, (const char *)peid2->szExId);
	} else {
		/**
		 * Different ABEID version, so check on the legacy ulId field. This implies that
		 * when a V0 ABEID is stored somewhere in the database, and the server was upgraded and
		 * an additional server was added, that the comparison will yield invalid results as
		 * we're not allowed to compare the legacy field cross server.
		 **/
		iResult = (int)(peid1->ulId - peid2->ulId);
	}

	if (iResult == 0)
		iResult = (int)(peid1->ulType - peid2->ulType);

exit:
	*lpCompareResult = iResult;

	return er;
}

ECRESULT CompareProp(const struct propVal *lpProp1,
    const struct propVal *lpProp2, const ECLocale &locale,
    int *lpCompareResult)
{
	ECRESULT	er = erSuccess;
	int			nCompareResult = 0;
	int			i;
	unsigned int ulPropTag1;
	unsigned int ulPropTag2;

	// List of prperties that get special treatment
	static const struct {
		ULONG		ulPropTag;
		ECRESULT	(*lpfnComparer)(const struct propVal *, const struct propVal *, int *);
	} sSpecials[] = {
		{PR_ADDRESS_BOOK_ENTRYID, &CompareABEID},
	};

	if(lpProp1 == NULL || lpProp2 == NULL || lpCompareResult == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	ulPropTag1 = NormalizePropTag(lpProp1->ulPropTag);
	ulPropTag2 = NormalizePropTag(lpProp2->ulPropTag);

	if(PROP_TYPE(ulPropTag1) != PROP_TYPE(ulPropTag2)) {
		// Treat this as equal
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// check soap union types and null pointers
	if(PropCheck(lpProp1) != erSuccess || PropCheck(lpProp2) != erSuccess){
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// First check if the any of the properties is in the sSpecials list
	for (unsigned x = 0; x < sizeof(sSpecials) / sizeof *sSpecials; ++x) {
		if ((lpProp1->ulPropTag == sSpecials[x].ulPropTag && PROP_TYPE(lpProp2->ulPropTag) == PROP_TYPE(sSpecials[x].ulPropTag)) ||
			(PROP_TYPE(lpProp1->ulPropTag) == PROP_TYPE(sSpecials[x].ulPropTag) && lpProp2->ulPropTag == sSpecials[x].ulPropTag))
		{
			er = sSpecials[x].lpfnComparer(lpProp1, lpProp2, &nCompareResult);
			if (er == erSuccess)
				goto skip_check;

			er = erSuccess;
			break;
		}
	}

	// Perform a regular comparison
	switch(PROP_TYPE(lpProp1->ulPropTag)) {
	case PT_I2:
		nCompareResult = lpProp1->Value.i - lpProp2->Value.i;
		break;
	case PT_LONG:
		if(lpProp1->Value.ul == lpProp2->Value.ul)
			nCompareResult = 0;
		else if(lpProp1->Value.ul < lpProp2->Value.ul)
			nCompareResult = -1;
		else
			nCompareResult = 1;
		break;
	case PT_R4:
		if(lpProp1->Value.flt == lpProp2->Value.flt)
			nCompareResult = 0;
		else if(lpProp1->Value.flt < lpProp2->Value.flt)
			nCompareResult = -1;
		else
			nCompareResult = 1;
		break;
	case PT_BOOLEAN:
		nCompareResult = lpProp1->Value.b - lpProp2->Value.b;
		break;
	case PT_DOUBLE:
	case PT_APPTIME:
		if(lpProp1->Value.dbl == lpProp2->Value.dbl)
			nCompareResult = 0;
		else if(lpProp1->Value.dbl < lpProp2->Value.dbl)
			nCompareResult = -1;
		else
			nCompareResult = 1;
		break;
	case PT_I8:
		if(lpProp1->Value.li == lpProp2->Value.li)
			nCompareResult = 0;
		else if(lpProp1->Value.li < lpProp2->Value.li)
			nCompareResult = -1;
		else
			nCompareResult = 1;
		break;
	case PT_UNICODE:
	case PT_STRING8:
		if (lpProp1->Value.lpszA && lpProp2->Value.lpszA)
			if(PROP_ID(lpProp2->ulPropTag) == PROP_ID(PR_ANR))
				nCompareResult = u8_istartswith(lpProp1->Value.lpszA, lpProp2->Value.lpszA, locale);
			else
				nCompareResult = u8_icompare(lpProp1->Value.lpszA, lpProp2->Value.lpszA, locale);
		else
			nCompareResult = lpProp1->Value.lpszA != lpProp2->Value.lpszA;
		break;
	case PT_SYSTIME:
	case PT_CURRENCY:
		if(lpProp1->Value.hilo->hi == lpProp2->Value.hilo->hi && lpProp1->Value.hilo->lo < lpProp2->Value.hilo->lo)
			nCompareResult = -1;
		else if(lpProp1->Value.hilo->hi == lpProp2->Value.hilo->hi && lpProp1->Value.hilo->lo > lpProp2->Value.hilo->lo)
			nCompareResult = 1;
		else
			nCompareResult = lpProp1->Value.hilo->hi - lpProp2->Value.hilo->hi;
		break;
	case PT_BINARY:
	case PT_CLSID:
		if (lpProp1->Value.bin->__ptr && lpProp2->Value.bin->__ptr &&
			lpProp1->Value.bin->__size && lpProp2->Value.bin->__size &&
			lpProp1->Value.bin->__size == lpProp2->Value.bin->__size)
			nCompareResult = memcmp(lpProp1->Value.bin->__ptr, lpProp2->Value.bin->__ptr, lpProp1->Value.bin->__size);
		else
			nCompareResult = lpProp1->Value.bin->__size - lpProp2->Value.bin->__size;
		break;

	case PT_MV_I2:
		if (lpProp1->Value.mvi.__size == lpProp2->Value.mvi.__size) {
			for(i=0; i < lpProp1->Value.mvi.__size; i++) {
				nCompareResult = lpProp1->Value.mvi.__ptr[i] - lpProp2->Value.mvi.__ptr[i];
				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.mvi.__size - lpProp2->Value.mvi.__size;
		break;
	case PT_MV_LONG:
		if (lpProp1->Value.mvl.__size == lpProp2->Value.mvl.__size) {
			for(i=0; i < lpProp1->Value.mvl.__size; i++) {
				if(lpProp1->Value.mvl.__ptr[i] == lpProp2->Value.mvl.__ptr[i])
                    nCompareResult = 0;
				else if(lpProp1->Value.mvl.__ptr[i] < lpProp2->Value.mvl.__ptr[i])
					nCompareResult = -1;
				else
					nCompareResult = 1;

				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.mvl.__size - lpProp2->Value.mvl.__size;
		break;
	case PT_MV_R4:
		if (lpProp1->Value.mvflt.__size == lpProp2->Value.mvflt.__size) {
			for(i=0; i < lpProp1->Value.mvflt.__size; i++) {
				if(lpProp1->Value.mvflt.__ptr[i] == lpProp2->Value.mvflt.__ptr[i])
					nCompareResult = 0;
				else if(lpProp1->Value.mvflt.__ptr[i] < lpProp2->Value.mvflt.__ptr[i])
					nCompareResult = -1;
				else
					nCompareResult = 1;

				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.mvflt.__size - lpProp2->Value.mvflt.__size;
		break;
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		if (lpProp1->Value.mvdbl.__size == lpProp2->Value.mvdbl.__size) {
			for(i=0; i < lpProp1->Value.mvdbl.__size; i++) {
				if(lpProp1->Value.mvdbl.__ptr[i] == lpProp2->Value.mvdbl.__ptr[i])
					nCompareResult = 0;
				else if(lpProp1->Value.mvdbl.__ptr[i] < lpProp2->Value.mvdbl.__ptr[i])
					nCompareResult = -1;
				else
					nCompareResult = 1;

				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.mvdbl.__size - lpProp2->Value.mvdbl.__size;
		break;
	case PT_MV_I8:
		if (lpProp1->Value.mvli.__size == lpProp2->Value.mvli.__size) {
			for(i=0; i < lpProp1->Value.mvli.__size; i++) {
				if(lpProp1->Value.mvli.__ptr[i] == lpProp2->Value.mvli.__ptr[i])
					nCompareResult = 0;
				else if(lpProp1->Value.mvli.__ptr[i] < lpProp2->Value.mvli.__ptr[i])
					nCompareResult = -1;
				else
					nCompareResult = 1;
				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.mvli.__size - lpProp2->Value.mvli.__size;
		break;
	case PT_MV_SYSTIME:
	case PT_MV_CURRENCY:
		if (lpProp1->Value.mvhilo.__size == lpProp2->Value.mvhilo.__size) {
			for(i=0; i < lpProp1->Value.mvhilo.__size; i++) {
				if(lpProp1->Value.mvhilo.__ptr[i].hi == lpProp2->Value.mvhilo.__ptr[i].hi && lpProp1->Value.mvhilo.__ptr[i].lo < lpProp2->Value.mvhilo.__ptr[i].lo)
					nCompareResult = -1;
				else if(lpProp1->Value.mvhilo.__ptr[i].hi == lpProp2->Value.mvhilo.__ptr[i].hi && lpProp1->Value.mvhilo.__ptr[i].lo > lpProp2->Value.mvhilo.__ptr[i].lo)
					nCompareResult = 1;
				else
					nCompareResult = lpProp1->Value.mvhilo.__ptr[i].hi - lpProp2->Value.mvhilo.__ptr[i].hi;

				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.mvhilo.__size == lpProp2->Value.mvhilo.__size;
		break;
	case PT_MV_CLSID:
	case PT_MV_BINARY:
		if (lpProp1->Value.mvbin.__size == lpProp2->Value.mvbin.__size) {
			for(i=0; i < lpProp1->Value.mvbin.__size; i++) {
				if(lpProp1->Value.mvbin.__ptr[i].__ptr && lpProp2->Value.mvbin.__ptr[i].__ptr &&
				   lpProp1->Value.mvbin.__ptr[i].__size && lpProp2->Value.mvbin.__ptr[i].__size &&
				   lpProp1->Value.mvbin.__ptr[i].__size - lpProp2->Value.mvbin.__ptr[i].__size == 0)
					nCompareResult = memcmp(lpProp1->Value.mvbin.__ptr[i].__ptr, lpProp2->Value.mvbin.__ptr[i].__ptr, lpProp1->Value.mvbin.__ptr[i].__size);
				else
					nCompareResult = lpProp1->Value.mvbin.__ptr[i].__size - lpProp2->Value.mvbin.__ptr[i].__size;

				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.mvbin.__size - lpProp2->Value.mvbin.__size;
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		if (lpProp1->Value.mvszA.__size == lpProp2->Value.mvszA.__size) {
			for(i=0; i < lpProp1->Value.mvszA.__size; i++) {
				if (lpProp1->Value.mvszA.__ptr[i] && lpProp2->Value.mvszA.__ptr[i])
					nCompareResult =u8_icompare(lpProp1->Value.mvszA.__ptr[i], lpProp2->Value.mvszA.__ptr[i], locale);
				else
					nCompareResult = lpProp1->Value.mvszA.__ptr[i] != lpProp2->Value.mvszA.__ptr[i];

				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.mvszA.__size - lpProp2->Value.mvszA.__size;
		break;
	default:
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
		break;
	}

skip_check:
	*lpCompareResult = nCompareResult;

exit:
	return er;
}


/**
 * ulType is one of the RELOP_xx types. The result returned will indicate that at least one of the values in lpMVProp positively 
 * matched the RELOP_xx comparison with lpProp2.
 **/
ECRESULT CompareMVPropWithProp(struct propVal *lpMVProp1,
    const struct propVal *lpProp2, unsigned int ulType, const ECLocale &locale,
    bool *lpfMatch)
{
	ECRESULT	er = erSuccess;
	int			nCompareResult = -1; // Default, Don't change this to 0
	bool		fMatch = false;
	MVPropProxy pxyMVProp1(lpMVProp1);

	if(lpMVProp1 == NULL || lpProp2 == NULL || lpfMatch == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if((PROP_TYPE(lpMVProp1->ulPropTag)&~MV_FLAG) != PROP_TYPE(lpProp2->ulPropTag)) {
		// Treat this as equal
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// check soap union types and null pointers
	if(PropCheck(lpMVProp1) != erSuccess || PropCheck(lpProp2) != erSuccess){
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	for (unsigned int i = 0; !fMatch && i < pxyMVProp1.size(); ++i) {
		er = pxyMVProp1.compare(i, lpProp2, locale, &nCompareResult);
		if (er != erSuccess)
			goto exit;

		switch(ulType) {
		case RELOP_GE:
			fMatch = nCompareResult >= 0;
			break;
		case RELOP_GT:
			fMatch = nCompareResult > 0;
			break;
		case RELOP_LE:
			fMatch = nCompareResult <= 0;
			break;
		case RELOP_LT:
			fMatch = nCompareResult < 0;
			break;
		case RELOP_NE:
			fMatch = nCompareResult != 0;
			break;
		case RELOP_RE:
			fMatch = false; // FIXME ?? how should this work ??
			break;
		case RELOP_EQ:
			fMatch = nCompareResult == 0;
			break;
		}
	}

	*lpfMatch = fMatch;

exit:
	return er;
}

unsigned int PropSize(struct propVal *lpProp)
{
	unsigned int	ulSize;
	int				i;

	if(lpProp == NULL)
		return 0;

	switch(PROP_TYPE(lpProp->ulPropTag)) {
	case PT_I2:
		return 2;
	case PT_BOOLEAN:
	case PT_R4:
	case PT_LONG:
		return 4;
	case PT_APPTIME:
	case PT_DOUBLE:
	case PT_I8:
		return 8;
	case PT_UNICODE:
	case PT_STRING8:
		return lpProp->Value.lpszA ? (unsigned int)strlen(lpProp->Value.lpszA) : 0;
	case PT_SYSTIME:
	case PT_CURRENCY:
		return 8;
	case PT_BINARY:
	case PT_CLSID:
		return lpProp->Value.bin ? lpProp->Value.bin->__size : 0;
	case PT_MV_I2:
		return 2 * lpProp->Value.mvi.__size;
	case PT_MV_R4:
		return 4 * lpProp->Value.mvflt.__size;
	case PT_MV_LONG:
		return 4 * lpProp->Value.mvl.__size;
	case PT_MV_APPTIME:
	case PT_MV_DOUBLE:
		return 8 * lpProp->Value.mvdbl.__size;
	case PT_MV_I8:
		return 8 * lpProp->Value.mvli.__size;
	case PT_MV_UNICODE:
	case PT_MV_STRING8:
		ulSize = 0;
		for(i=0; i < lpProp->Value.mvszA.__size; i++)
		{
			ulSize+= (lpProp->Value.mvszA.__ptr[i])? (unsigned int)strlen(lpProp->Value.mvszA.__ptr[i]) : 0;
		}
		return ulSize;
	case PT_MV_SYSTIME:
	case PT_MV_CURRENCY:
		return 8 * lpProp->Value.mvhilo.__size;
	case PT_MV_BINARY:
	case PT_MV_CLSID:
		ulSize = 0;
		for(i=0; i < lpProp->Value.mvbin.__size; i++)
		{
			ulSize+= lpProp->Value.mvbin.__ptr[i].__size;
		}
		return ulSize;
	default:
		return 0;
	}
}

ECRESULT FreePropVal(struct propVal *lpProp, bool bBasePointerDel)
{
	ECRESULT er = erSuccess;

	if(lpProp == NULL)
		return er;

	switch(PROP_TYPE(lpProp->ulPropTag)) {
	case PT_I2:
	case PT_LONG:
	case PT_R4:
	case PT_BOOLEAN:
	case PT_DOUBLE:
	case PT_APPTIME:
	case PT_I8:
		// no extra cleanup needed
		break;
	case PT_SYSTIME:
	case PT_CURRENCY:
		if(lpProp->Value.hilo)
			delete lpProp->Value.hilo;
		break;
	case PT_STRING8:
	case PT_UNICODE:
		if(lpProp->Value.lpszA)
			delete [] lpProp->Value.lpszA;
		break;
	case PT_CLSID:
	case PT_BINARY:
		if(lpProp->Value.bin && lpProp->Value.bin->__ptr)
			delete [] lpProp->Value.bin->__ptr;
		if(lpProp->Value.bin)
			delete lpProp->Value.bin;
		break;
	case PT_MV_I2:
		if(lpProp->Value.mvi.__ptr)
			delete []lpProp->Value.mvi.__ptr;
		break;
	case PT_MV_LONG:
		if(lpProp->Value.mvl.__ptr)
			delete []lpProp->Value.mvl.__ptr;
		break;
	case PT_MV_R4:
		if(lpProp->Value.mvflt.__ptr)
			delete []lpProp->Value.mvflt.__ptr;
		break;
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		if(lpProp->Value.mvdbl.__ptr)
			delete []lpProp->Value.mvdbl.__ptr;
		break;
	case PT_MV_I8:
		if(lpProp->Value.mvli.__ptr)
			delete []lpProp->Value.mvli.__ptr;
		break;
	case PT_MV_SYSTIME:
	case PT_MV_CURRENCY:
		if(lpProp->Value.mvhilo.__ptr)
			delete[] lpProp->Value.mvhilo.__ptr;
		break;
	case PT_MV_CLSID:
	case PT_MV_BINARY:
		if(lpProp->Value.mvbin.__ptr)
		{
			for(int i=0; i < lpProp->Value.mvbin.__size; i++) {
				if(lpProp->Value.mvbin.__ptr[i].__ptr)
					delete[] lpProp->Value.mvbin.__ptr[i].__ptr;
			}
			delete[] lpProp->Value.mvbin.__ptr;
		}
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		if(lpProp->Value.mvszA.__ptr)
		{
			for(int i=0; i < lpProp->Value.mvszA.__size; i++) {
				if(lpProp->Value.mvszA.__ptr[i])
					delete[] lpProp->Value.mvszA.__ptr[i];
			}
			delete [] lpProp->Value.mvszA.__ptr;
		}
		break;
	case PT_SRESTRICTION:
		if(lpProp->Value.res)
			FreeRestrictTable(lpProp->Value.res);
		break;
	case PT_ACTIONS:
		if(lpProp->Value.actions) {
			struct actions *lpActions = lpProp->Value.actions;

			for(int i=0;i<lpActions->__size;i++) {
				struct action *lpAction = &lpActions->__ptr[i];

				switch(lpAction->acttype) {
					case OP_COPY:
					case OP_MOVE:
						delete [] lpAction->act.moveCopy.store.__ptr;
						delete [] lpAction->act.moveCopy.folder.__ptr;
						break;
					case OP_REPLY:
					case OP_OOF_REPLY:
						delete [] lpAction->act.reply.message.__ptr;
						delete [] lpAction->act.reply.guid.__ptr;
						break;
					case OP_DEFER_ACTION:
						delete [] lpAction->act.defer.bin.__ptr;
						break;
					case OP_BOUNCE:
						break;
					case OP_FORWARD:
					case OP_DELEGATE:
						FreeRowSet(lpAction->act.adrlist, true);
						break;
					case OP_TAG:
						FreePropVal(lpAction->act.prop, true);
						break;
				}
			}

			if(lpActions->__ptr)
				delete [] lpActions->__ptr;

			delete lpProp->Value.actions;
		}
		break;
	default:
		er = ZARAFA_E_INVALID_TYPE;
	}

	if(bBasePointerDel)
		delete lpProp;

	return er;
}

void FreeRowSet(struct rowSet *lpRowSet, bool bBasePointerDel)
{
	if(lpRowSet == NULL)
		return;

	for(int i=0; i<lpRowSet->__size; i++) {

		FreePropValArray(&lpRowSet->__ptr[i]);

	}

	if(lpRowSet->__size > 0)
		delete []lpRowSet->__ptr;

	if(bBasePointerDel)
		delete lpRowSet;
}

/** 
 * Frees a soap restriction table
 * 
 * @param[in] lpRestrict the soap restriction table to free and everything below it
 * @param[in] base always true, except when you know what you're doing (aka restriction optimizer for the zarafa-search)
 * 
 * @return 
 */
ECRESULT FreeRestrictTable(struct restrictTable *lpRestrict, bool base)
{
	ECRESULT er = erSuccess;
	unsigned int i = 0;

	if(lpRestrict == NULL)
		goto exit;

	switch(lpRestrict->ulType) {
	case RES_OR:
		if(lpRestrict->lpOr && lpRestrict->lpOr->__ptr) {

			for(i=0;i<lpRestrict->lpOr->__size;i++) {
				er = FreeRestrictTable(lpRestrict->lpOr->__ptr[i]);

				if(er != erSuccess)
					goto exit;
			}
			delete [] lpRestrict->lpOr->__ptr;
		}
		if(lpRestrict->lpOr)
			delete lpRestrict->lpOr;
		break;
	case RES_AND:
		if(lpRestrict->lpAnd && lpRestrict->lpAnd->__ptr) {
			for(i=0;i<lpRestrict->lpAnd->__size;i++) {
				er = FreeRestrictTable(lpRestrict->lpAnd->__ptr[i]);

				if(er != erSuccess)
					goto exit;
			}
			delete [] lpRestrict->lpAnd->__ptr;
		}
		if(lpRestrict->lpAnd)
			delete lpRestrict->lpAnd;
		break;

	case RES_NOT:
		if(lpRestrict->lpNot && lpRestrict->lpNot->lpNot)
			FreeRestrictTable(lpRestrict->lpNot->lpNot);

		if(lpRestrict->lpNot)
			delete lpRestrict->lpNot;
		break;
	case RES_CONTENT:
		if(lpRestrict->lpContent && lpRestrict->lpContent->lpProp)
			FreePropVal(lpRestrict->lpContent->lpProp, true);

		if(lpRestrict->lpContent)
			delete lpRestrict->lpContent;

		break;
	case RES_PROPERTY:
		if(lpRestrict->lpProp && lpRestrict->lpProp->lpProp)
			FreePropVal(lpRestrict->lpProp->lpProp, true);

		if(lpRestrict->lpProp)
			delete lpRestrict->lpProp;

		break;

	case RES_COMPAREPROPS:
		if(lpRestrict->lpCompare)
			delete lpRestrict->lpCompare;

		break;

	case RES_BITMASK:
		if(lpRestrict->lpBitmask)
			delete lpRestrict->lpBitmask;
		break;

	case RES_SIZE:
		if(lpRestrict->lpSize)
			delete lpRestrict->lpSize;
		break;

	case RES_EXIST:
		if(lpRestrict->lpExist)
			delete lpRestrict->lpExist;
		break;

	case RES_COMMENT:
		if (lpRestrict->lpComment) {
			if (lpRestrict->lpComment->lpResTable)
				FreeRestrictTable(lpRestrict->lpComment->lpResTable);

			FreePropValArray(&lpRestrict->lpComment->sProps);
			delete lpRestrict->lpComment;
		}
		break;

	case RES_SUBRESTRICTION:
	    if(lpRestrict->lpSub && lpRestrict->lpSub->lpSubObject)
	        FreeRestrictTable(lpRestrict->lpSub->lpSubObject);

        if(lpRestrict->lpSub)
            delete lpRestrict->lpSub;
        break;

	default:
		er = ZARAFA_E_INVALID_TYPE;
		// NOTE: don't exit here, delete lpRestrict
		break;
	}

	// only when we're optimizing restrictions we must keep the base pointer, so we can replace it with new content
	if (base)
		delete lpRestrict;

exit:
	return er;
}

ECRESULT CopyPropVal(const struct propVal *lpSrc, struct propVal *lpDst,
    struct soap *soap, bool bTruncate)
{
	ECRESULT er = erSuccess;
	int i;

	er = PropCheck(lpSrc);
	if(er != erSuccess)
		goto exit;

	lpDst->ulPropTag = lpSrc->ulPropTag;
	lpDst->__union = lpSrc->__union;

	switch(PROP_TYPE(lpSrc->ulPropTag)) {
	case PT_I2:
		lpDst->Value.i = lpSrc->Value.i;
		break;
	case PT_NULL:
	case PT_ERROR:
	case PT_LONG:
		lpDst->Value.ul = lpSrc->Value.ul;
		break;
	case PT_R4:
		lpDst->Value.flt = lpSrc->Value.flt;
		break;
	case PT_BOOLEAN:
		lpDst->Value.b = lpSrc->Value.b;
		break;
	case PT_DOUBLE:
	case PT_APPTIME:
		lpDst->Value.dbl = lpSrc->Value.dbl;
		break;
	case PT_I8:
		lpDst->Value.li = lpSrc->Value.li;
		break;
	case PT_CURRENCY:
	case PT_SYSTIME:
		if(lpSrc->Value.hilo == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->Value.hilo = s_alloc<hiloLong>(soap);
		lpDst->Value.hilo->hi = lpSrc->Value.hilo->hi;
		lpDst->Value.hilo->lo = lpSrc->Value.hilo->lo;
		break;
	case PT_UNICODE:
	case PT_STRING8: {
		size_t len;
		
		if(lpSrc->Value.lpszA == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		if (bTruncate)
			len = u8_cappedbytes(lpSrc->Value.lpszA, TABLE_CAP_STRING);
		else
			len = strlen(lpSrc->Value.lpszA);
		
		lpDst->Value.lpszA = s_alloc<char>(soap, len+1);
		strncpy(lpDst->Value.lpszA, lpSrc->Value.lpszA, len);
		*(lpDst->Value.lpszA+len) = 0; // null terminate after strncpy

		break;
	}
	case PT_BINARY:
	case PT_CLSID:
		if(lpSrc->Value.bin == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->Value.bin = s_alloc<struct xsd__base64Binary>(soap);
		lpDst->Value.bin->__size = lpSrc->Value.bin->__size;
		
		if(bTruncate) {
			if(lpDst->Value.bin->__size > TABLE_CAP_BINARY)
				lpDst->Value.bin->__size = TABLE_CAP_BINARY;
		}
		
		lpDst->Value.bin->__ptr = s_alloc<unsigned char>(soap, lpSrc->Value.bin->__size);
		memcpy(lpDst->Value.bin->__ptr, lpSrc->Value.bin->__ptr, lpDst->Value.bin->__size);
		break;
	case PT_MV_I2:
		if(lpSrc->Value.mvi.__ptr == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->Value.mvi.__size = lpSrc->Value.mvi.__size;
		lpDst->Value.mvi.__ptr = s_alloc<short int>(soap, lpSrc->Value.mvi.__size);
		memcpy(lpDst->Value.mvi.__ptr, lpSrc->Value.mvi.__ptr, sizeof(short int) * lpDst->Value.mvi.__size);
		break;
	case PT_MV_LONG:
		if(lpSrc->Value.mvl.__ptr == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->Value.mvl.__size = lpSrc->Value.mvl.__size;
		lpDst->Value.mvl.__ptr = s_alloc<unsigned int>(soap, lpSrc->Value.mvl.__size);
		memcpy(lpDst->Value.mvl.__ptr, lpSrc->Value.mvl.__ptr, sizeof(unsigned int) * lpDst->Value.mvl.__size);
		break;
	case PT_MV_R4:
		if(lpSrc->Value.mvflt.__ptr == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->Value.mvflt.__size = lpSrc->Value.mvflt.__size;
		lpDst->Value.mvflt.__ptr = s_alloc<float>(soap, lpSrc->Value.mvflt.__size);
		memcpy(lpDst->Value.mvflt.__ptr, lpSrc->Value.mvflt.__ptr, sizeof(float) * lpDst->Value.mvflt.__size);
		break;
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		if(lpSrc->Value.mvdbl.__ptr == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->Value.mvdbl.__size = lpSrc->Value.mvdbl.__size;
		lpDst->Value.mvdbl.__ptr = s_alloc<double>(soap, lpSrc->Value.mvdbl.__size);
		memcpy(lpDst->Value.mvdbl.__ptr, lpSrc->Value.mvdbl.__ptr, sizeof(double) * lpDst->Value.mvdbl.__size);
		break;
	case PT_MV_I8:
		if(lpSrc->Value.mvli.__ptr == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->Value.mvli.__size = lpSrc->Value.mvli.__size;
		lpDst->Value.mvli.__ptr = s_alloc<LONG64>(soap, lpSrc->Value.mvli.__size);
		memcpy(lpDst->Value.mvli.__ptr, lpSrc->Value.mvli.__ptr, sizeof(LONG64) * lpDst->Value.mvli.__size);
		break;
	case PT_MV_CURRENCY:
	case PT_MV_SYSTIME:
		if(lpSrc->Value.mvhilo.__ptr == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->Value.mvhilo.__size = lpSrc->Value.mvhilo.__size;
		lpDst->Value.mvhilo.__ptr = s_alloc<hiloLong>(soap, lpSrc->Value.mvhilo.__size);
		memcpy(lpDst->Value.mvhilo.__ptr, lpSrc->Value.mvhilo.__ptr, sizeof(hiloLong) * lpDst->Value.mvhilo.__size);
		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		if(lpSrc->Value.mvszA.__ptr == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->Value.mvszA.__size = lpSrc->Value.mvszA.__size;
		lpDst->Value.mvszA.__ptr = s_alloc<char*>(soap, lpSrc->Value.mvszA.__size);
		for(i=0; i < lpSrc->Value.mvszA.__size; i++)
		{
			lpDst->Value.mvszA.__ptr[i] = s_alloc<char>(soap, strlen(lpSrc->Value.mvszA.__ptr[i])+1);
			if(lpSrc->Value.mvszA.__ptr[i] == NULL)
			    strcpy(lpDst->Value.mvszA.__ptr[i], "");
			else
				strcpy(lpDst->Value.mvszA.__ptr[i], lpSrc->Value.mvszA.__ptr[i]);
		}
		break;
	case PT_MV_BINARY:
	case PT_MV_CLSID:
		if(lpSrc->Value.mvbin.__ptr == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->Value.mvbin.__size = lpSrc->Value.mvbin.__size;
		lpDst->Value.mvbin.__ptr = s_alloc<struct xsd__base64Binary>(soap, lpSrc->Value.mvbin.__size);
		for(i=0; i < lpSrc->Value.mvbin.__size; i++)
		{
			lpDst->Value.mvbin.__ptr[i].__ptr = s_alloc<unsigned char>(soap, lpSrc->Value.mvbin.__ptr[i].__size);
			if(lpSrc->Value.mvbin.__ptr[i].__ptr == NULL) {
				lpDst->Value.mvbin.__ptr[i].__size = 0;
			} else {
				memcpy(lpDst->Value.mvbin.__ptr[i].__ptr, lpSrc->Value.mvbin.__ptr[i].__ptr, lpSrc->Value.mvbin.__ptr[i].__size);
				lpDst->Value.mvbin.__ptr[i].__size = lpSrc->Value.mvbin.__ptr[i].__size;
			}
		}
		break;
	default:
		er = ZARAFA_E_INVALID_TYPE;
	}

exit:
	return er;
}

ECRESULT CopyPropVal(const struct propVal *lpSrc, struct propVal **lppDst,
    struct soap *soap, bool bTruncate)
{
	ECRESULT er = erSuccess;
	struct propVal *lpDst;

	lpDst = s_alloc<struct propVal>(soap);

	er = CopyPropVal(lpSrc, lpDst, soap);
	if (er != erSuccess) {
		// there is no sub-alloc when there's an error, so we can remove lpDst
		if (!soap)
			delete lpDst;		// maybe create s_free() ?
		goto exit;
	}

	*lppDst = lpDst;

exit:
	return er;
}

ECRESULT CopyPropValArray(const struct propValArray *lpSrc,
    struct propValArray **lppDst, struct soap *soap)
{
	ECRESULT er = erSuccess;
	struct propValArray *lpDst = NULL;

	if(lpSrc == NULL || lppDst == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpDst = s_alloc<struct propValArray>(soap);

	if(lpSrc->__size > 0) {
		er = CopyPropValArray(lpSrc, lpDst, soap);
		if(er != erSuccess)
			goto exit;
	}else {
		lpDst->__ptr = NULL;
		lpDst->__size = 0;
	}

	*lppDst = lpDst;

exit:
	return er;
}

ECRESULT CopyPropValArray(const struct propValArray *lpSrc,
    struct propValArray *lpDst, struct soap *soap)
{
	ECRESULT er = erSuccess;
	int i;

	if(lpSrc == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpDst->__ptr = s_alloc<struct propVal>(soap, lpSrc->__size);
	lpDst->__size = lpSrc->__size;
	memset(lpDst->__ptr, 0, sizeof(propVal)*lpDst->__size);

	for (i=0; i<lpSrc->__size; i++) {
		er = CopyPropVal(&lpSrc->__ptr[i], &lpDst->__ptr[i], soap);
		if(er != erSuccess) {
			if (!soap) {
				delete [] lpDst->__ptr;	// maybe create s_free() ?
				lpDst->__ptr = NULL;
			}
			lpDst->__size = 0;
			goto exit;
		}
	}

exit:
	return er;
}


ECRESULT CopyRestrictTable(struct soap *soap,
    const struct restrictTable *lpSrc, struct restrictTable **lppDst)
{
	ECRESULT er = erSuccess;
	struct restrictTable *lpDst = NULL;
	unsigned int i = 0;

	if(lpSrc == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpDst = s_alloc<struct restrictTable>(soap);
	memset(lpDst, 0, sizeof(restrictTable));

	lpDst->ulType = lpSrc->ulType;

	switch(lpSrc->ulType) {
	case RES_OR:
		if(lpSrc->lpOr == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		lpDst->lpOr = s_alloc<restrictOr>(soap);

		lpDst->lpOr->__ptr = s_alloc<restrictTable *>(soap, lpSrc->lpOr->__size);
		lpDst->lpOr->__size = lpSrc->lpOr->__size;
		memset(lpDst->lpOr->__ptr, 0, sizeof(restrictTable *) * lpSrc->lpOr->__size);

		for(i=0;i<lpSrc->lpOr->__size;i++) {
			er = CopyRestrictTable(soap, lpSrc->lpOr->__ptr[i], &lpDst->lpOr->__ptr[i]);

			if(er != erSuccess)
				goto exit;
		}

		break;
	case RES_AND:
		if(lpSrc->lpAnd == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		lpDst->lpAnd = s_alloc<restrictAnd>(soap);

		lpDst->lpAnd->__ptr = s_alloc<restrictTable *>(soap, lpSrc->lpAnd->__size);
		lpDst->lpAnd->__size = lpSrc->lpAnd->__size;
		memset(lpDst->lpAnd->__ptr, 0, sizeof(restrictTable *) * lpSrc->lpAnd->__size);

		for(i=0;i<lpSrc->lpAnd->__size;i++) {
			er = CopyRestrictTable(soap, lpSrc->lpAnd->__ptr[i], &lpDst->lpAnd->__ptr[i]);

			if(er != erSuccess)
				goto exit;
		}
		break;

	case RES_NOT:
		lpDst->lpNot = s_alloc<restrictNot>(soap);
		memset(lpDst->lpNot, 0, sizeof(restrictNot));

		er = CopyRestrictTable(soap, lpSrc->lpNot->lpNot, &lpDst->lpNot->lpNot);

		if(er != erSuccess)
			goto exit;

		break;
	case RES_CONTENT:
		lpDst->lpContent = s_alloc<restrictContent>(soap);
		memset(lpDst->lpContent, 0, sizeof(restrictContent));

		lpDst->lpContent->ulFuzzyLevel = lpSrc->lpContent->ulFuzzyLevel;
		lpDst->lpContent->ulPropTag = lpSrc->lpContent->ulPropTag;

		if(lpSrc->lpContent->lpProp) {
			er = CopyPropVal(lpSrc->lpContent->lpProp, &lpDst->lpContent->lpProp, soap);
			if(er != erSuccess)
				goto exit;
		}

		break;
	case RES_PROPERTY:
		lpDst->lpProp = s_alloc<restrictProp>(soap);
		memset(lpDst->lpProp, 0, sizeof(restrictProp));

		lpDst->lpProp->ulType = lpSrc->lpProp->ulType;
		lpDst->lpProp->ulPropTag = lpSrc->lpProp->ulPropTag;

		er = CopyPropVal(lpSrc->lpProp->lpProp, &lpDst->lpProp->lpProp, soap);

		if(er != erSuccess)
			goto exit;

		break;

	case RES_COMPAREPROPS:
		lpDst->lpCompare = s_alloc<restrictCompare>(soap);
		memset(lpDst->lpCompare, 0 , sizeof(restrictCompare));

		lpDst->lpCompare->ulType = lpSrc->lpCompare->ulType;
		lpDst->lpCompare->ulPropTag1 = lpSrc->lpCompare->ulPropTag1;
		lpDst->lpCompare->ulPropTag2 = lpSrc->lpCompare->ulPropTag2;
		break;

	case RES_BITMASK:
		lpDst->lpBitmask = s_alloc<restrictBitmask>(soap);
		memset(lpDst->lpBitmask, 0, sizeof(restrictBitmask));

		lpDst->lpBitmask->ulMask = lpSrc->lpBitmask->ulMask;
		lpDst->lpBitmask->ulPropTag = lpSrc->lpBitmask->ulPropTag;
		lpDst->lpBitmask->ulType = lpSrc->lpBitmask->ulType;
		break;

	case RES_SIZE:
		lpDst->lpSize = s_alloc<restrictSize>(soap);
		memset(lpDst->lpSize, 0, sizeof(restrictSize));

		lpDst->lpSize->ulPropTag = lpSrc->lpSize->ulPropTag;
		lpDst->lpSize->ulType = lpSrc->lpSize->ulType;
		lpDst->lpSize->cb = lpSrc->lpSize->cb;
		break;

	case RES_EXIST:
		lpDst->lpExist = s_alloc<restrictExist>(soap);
		memset(lpDst->lpExist, 0, sizeof(restrictExist));

		lpDst->lpExist->ulPropTag = lpSrc->lpExist->ulPropTag;
		break;

	case RES_COMMENT:
		lpDst->lpComment = s_alloc<restrictComment>(soap);
		memset(lpDst->lpComment, 0, sizeof(restrictComment));

		er = CopyPropValArray(&lpSrc->lpComment->sProps, &lpDst->lpComment->sProps, soap);
		if (er != erSuccess)
			goto exit;

		er = CopyRestrictTable(soap, lpSrc->lpComment->lpResTable, &lpDst->lpComment->lpResTable);
		if(er != erSuccess)
			goto exit;

		break;

	case RES_SUBRESTRICTION:
	    lpDst->lpSub = s_alloc<restrictSub>(soap);
	    memset(lpDst->lpSub, 0, sizeof(restrictSub));

	    lpDst->lpSub->ulSubObject = lpSrc->lpSub->ulSubObject;

		er = CopyRestrictTable(soap, lpSrc->lpSub->lpSubObject, &lpDst->lpSub->lpSubObject);

		if(er != erSuccess)
			goto exit;

        break;
	default:
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	*lppDst = lpDst;

exit:
	return er;
}

ECRESULT FreePropValArray(struct propValArray *lpPropValArray, bool bFreeBase)
{
	int i = 0;

	if(lpPropValArray) {
		for(i=0; i<lpPropValArray->__size; i++) {
			FreePropVal(&(lpPropValArray->__ptr[i]), false);
		}

		delete [] lpPropValArray->__ptr;

		if(bFreeBase)
			delete lpPropValArray;
	}

	return erSuccess;
}

ECRESULT CopyEntryId(struct soap *soap, entryId* lpSrc, entryId** lppDst)
{
	ECRESULT er = erSuccess;
	entryId* lpDst = NULL;

	if(lpSrc == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpDst = s_alloc<entryId>(soap);
	lpDst->__size = lpSrc->__size;

	if(lpSrc->__size > 0) {
		lpDst->__ptr = s_alloc<unsigned char>(soap, lpSrc->__size);

		memcpy(lpDst->__ptr, lpSrc->__ptr, sizeof(unsigned char) * lpSrc->__size);
	} else {
		lpDst->__ptr = NULL;
	}

	*lppDst = lpDst;
exit:
	return er;
}

ECRESULT CopyEntryList(struct soap *soap, struct entryList *lpSrc, struct entryList **lppDst)
{
	ECRESULT er = erSuccess;
	struct entryList *lpDst = NULL;

	if(lpSrc == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpDst = s_alloc<entryList>(soap);
	lpDst->__size = lpSrc->__size;
	if(lpSrc->__size > 0)
		lpDst->__ptr = s_alloc<entryId>(soap, lpSrc->__size);
	else
		lpDst->__ptr = NULL;

	for(unsigned int i=0; i<lpSrc->__size; i++) {
		lpDst->__ptr[i].__size = lpSrc->__ptr[i].__size;
		lpDst->__ptr[i].__ptr = s_alloc<unsigned char>(soap, lpSrc->__ptr[i].__size);
		memcpy(lpDst->__ptr[i].__ptr, lpSrc->__ptr[i].__ptr, sizeof(unsigned char) * lpSrc->__ptr[i].__size);
	}


	*lppDst = lpDst;
exit:
	return er;
}

ECRESULT FreeEntryList(struct entryList *lpEntryList, bool bFreeBase)
{
	ECRESULT er = erSuccess;

	if(lpEntryList == NULL)
		goto exit;

	if(lpEntryList->__ptr) {

		for(unsigned int i=0; i < lpEntryList->__size; i++) {
			if(lpEntryList->__ptr[i].__ptr)
				delete [] lpEntryList->__ptr[i].__ptr;
		}

		delete [] lpEntryList->__ptr;
	}

	if(bFreeBase) {
		delete lpEntryList;
	}

exit:

	return er;
}

ECRESULT FreeNotificationStruct(notification *lpNotification, bool bFreeBase)
{
	ECRESULT er = erSuccess;

	if(lpNotification == NULL)
		return er;

	if(lpNotification->obj != NULL){

		FreePropTagArray(lpNotification->obj->pPropTagArray);

		FreeEntryId(lpNotification->obj->pEntryId, true);
		FreeEntryId(lpNotification->obj->pOldId, true);
		FreeEntryId(lpNotification->obj->pOldParentId, true);
		FreeEntryId(lpNotification->obj->pParentId, true);

		delete lpNotification->obj;
	}

	if(lpNotification->tab != NULL) {
		if(lpNotification->tab->pRow != NULL)
			FreePropValArray(lpNotification->tab->pRow, true);

		if(lpNotification->tab->propIndex.Value.bin != NULL) {
			if(lpNotification->tab->propIndex.Value.bin->__size > 0)
				delete []lpNotification->tab->propIndex.Value.bin->__ptr;

			delete lpNotification->tab->propIndex.Value.bin;
		}

		if(lpNotification->tab->propPrior.Value.bin != NULL) {
			if(lpNotification->tab->propPrior.Value.bin->__size > 0)
				delete []lpNotification->tab->propPrior.Value.bin->__ptr;

			delete lpNotification->tab->propPrior.Value.bin;
		}

		delete lpNotification->tab;
	}

	if(lpNotification->newmail != NULL) {

		if(lpNotification->newmail->lpszMessageClass != NULL)
			delete []lpNotification->newmail->lpszMessageClass;

		FreeEntryId(lpNotification->newmail->pEntryId, true);
		FreeEntryId(lpNotification->newmail->pParentId, true);

		delete lpNotification->newmail;
	}

	if(lpNotification->ics != NULL) {
		FreeEntryId(lpNotification->ics->pSyncState, true);

		delete lpNotification->ics;
	}

	if(bFreeBase)
		delete lpNotification;

	return er;
}

// Make a copy of the struct notification.
ECRESULT CopyNotificationStruct(struct soap *soap, notification *lpNotification, notification &rNotifyTo)
{
	ECRESULT er = erSuccess;
	int nLen;

	if(lpNotification == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	memset(&rNotifyTo, 0, sizeof(rNotifyTo));

	rNotifyTo.ulEventType	= lpNotification->ulEventType;
	rNotifyTo.ulConnection	= lpNotification->ulConnection;

	if(lpNotification->tab != NULL) {

		rNotifyTo.tab =	s_alloc<notificationTable>(soap);

		memset(rNotifyTo.tab, 0, sizeof(notificationTable));

		rNotifyTo.tab->hResult = lpNotification->tab->hResult;
		rNotifyTo.tab->ulTableEvent = lpNotification->tab->ulTableEvent;

		CopyPropVal(&lpNotification->tab->propIndex, &rNotifyTo.tab->propIndex, soap);
		CopyPropVal(&lpNotification->tab->propPrior, &rNotifyTo.tab->propPrior, soap);

		// Ignore errors
		CopyPropValArray(lpNotification->tab->pRow, &rNotifyTo.tab->pRow, soap);

		rNotifyTo.tab->ulObjType = lpNotification->tab->ulObjType;

	}else if(lpNotification->obj != NULL) {

		rNotifyTo.obj = s_alloc<notificationObject>(soap);
		memset(rNotifyTo.obj, 0, sizeof(notificationObject));

		rNotifyTo.obj->ulObjType		= lpNotification->obj->ulObjType;

		// Ignore errors, sometimes nothing to copy
		CopyEntryId(soap, lpNotification->obj->pEntryId, &rNotifyTo.obj->pEntryId);
		CopyEntryId(soap, lpNotification->obj->pParentId, &rNotifyTo.obj->pParentId);
		CopyEntryId(soap, lpNotification->obj->pOldId, &rNotifyTo.obj->pOldId);
		CopyEntryId(soap, lpNotification->obj->pOldParentId, &rNotifyTo.obj->pOldParentId);
		CopyPropTagArray(soap, lpNotification->obj->pPropTagArray, &rNotifyTo.obj->pPropTagArray);

	}else if(lpNotification->newmail != NULL){
		rNotifyTo.newmail = s_alloc<notificationNewMail>(soap);

		memset(rNotifyTo.newmail, 0, sizeof(notificationNewMail));

		// Ignore errors, sometimes nothing to copy
		CopyEntryId(soap, lpNotification->newmail->pEntryId, &rNotifyTo.newmail->pEntryId);
		CopyEntryId(soap, lpNotification->newmail->pParentId, &rNotifyTo.newmail->pParentId);

		rNotifyTo.newmail->ulMessageFlags	= lpNotification->newmail->ulMessageFlags;

		if(lpNotification->newmail->lpszMessageClass) {
			nLen = (int)strlen(lpNotification->newmail->lpszMessageClass)+1;
			rNotifyTo.newmail->lpszMessageClass = s_alloc<char>(soap, nLen);
			memcpy(rNotifyTo.newmail->lpszMessageClass, lpNotification->newmail->lpszMessageClass, nLen);
		}
	}else if(lpNotification->ics != NULL){
		rNotifyTo.ics = s_alloc<notificationICS>(soap);
		memset(rNotifyTo.ics, 0, sizeof(notificationICS));

		// We use CopyEntryId as it just copied binary data
		CopyEntryId(soap, lpNotification->ics->pSyncState, &rNotifyTo.ics->pSyncState);
	}

exit:
	return er;
}

ECRESULT FreeNotificationArrayStruct(notificationArray *lpNotifyArray, bool bFreeBase)
{
	ECRESULT	er = erSuccess;
	unsigned int i;

	if(lpNotifyArray == NULL)
		goto exit;

	for(i = 0; i < lpNotifyArray->__size; i++)
		FreeNotificationStruct(&lpNotifyArray->__ptr[i], false);

	delete [] lpNotifyArray->__ptr;

	if(bFreeBase)
		delete lpNotifyArray;
	else {
		lpNotifyArray->__size = 0;
	}

exit:
	return er;
}

ECRESULT CopyNotificationArrayStruct(notificationArray *lpNotifyArrayFrom, notificationArray *lpNotifyArrayTo)
{
	ECRESULT	er = erSuccess;
	unsigned int i;

	if(lpNotifyArrayFrom == NULL){
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpNotifyArrayFrom->__size > 0)
		lpNotifyArrayTo->__ptr = new notification[lpNotifyArrayFrom->__size];
	else
		lpNotifyArrayTo->__ptr = NULL;

	lpNotifyArrayTo->__size = lpNotifyArrayFrom->__size;
	for(i = 0; i < lpNotifyArrayFrom->__size; i++)
	{
		CopyNotificationStruct(NULL, &lpNotifyArrayFrom->__ptr[i], lpNotifyArrayTo->__ptr[i]);
	}

exit:

	return er;
}

ECRESULT FreeUserObjectArray(struct userobjectArray *lpUserobjectArray, bool bFreeBase)
{
	ECRESULT	er = erSuccess;
	unsigned int i;

	if(lpUserobjectArray == NULL)
		goto exit;

	for(i = 0; i < lpUserobjectArray->__size; i++) {
		delete[] lpUserobjectArray->__ptr[i].lpszName;
	}

	delete [] lpUserobjectArray->__ptr;

	if(bFreeBase)
		delete lpUserobjectArray;
	else
		lpUserobjectArray->__size = 0;

exit:
	return er;
}

ECRESULT FreeEntryId(entryId* lpEntryId, bool bFreeBase)
{
	ECRESULT er = erSuccess;

	if(lpEntryId == NULL)
		goto exit;

	if(lpEntryId->__ptr)
		delete[] lpEntryId->__ptr;

	if(bFreeBase == true)
		delete lpEntryId;
	else
		lpEntryId->__size = 0;

exit:
	return er;
}

ECRESULT CopyRightsArrayToSoap(struct soap *soap, struct rightsArray *lpRightsArraySrc, struct rightsArray **lppRightsArrayDst)
{
	ECRESULT			er = erSuccess;
	struct rightsArray	*lpRightsArrayDst = NULL;

	if (soap == NULL || lpRightsArraySrc == NULL || lppRightsArrayDst == NULL)
	{
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpRightsArrayDst = s_alloc<struct rightsArray>(soap);
	memset(lpRightsArrayDst, 0, sizeof *lpRightsArrayDst);

	lpRightsArrayDst->__size = lpRightsArraySrc->__size;
	lpRightsArrayDst->__ptr = s_alloc<struct rights>(soap, lpRightsArraySrc->__size);

	for (unsigned int i = 0; i < lpRightsArraySrc->__size; ++i)
	{
		lpRightsArrayDst->__ptr[i] = lpRightsArraySrc->__ptr[i];

		lpRightsArrayDst->__ptr[i].sUserId.__ptr = s_alloc<unsigned char>(soap, lpRightsArrayDst->__ptr[i].sUserId.__size);
		memcpy(lpRightsArrayDst->__ptr[i].sUserId.__ptr, lpRightsArraySrc->__ptr[i].sUserId.__ptr, lpRightsArraySrc->__ptr[i].sUserId.__size);
	}
	
	*lppRightsArrayDst = lpRightsArrayDst;

exit:
	return er;
}

ECRESULT FreeRightsArray(struct rightsArray *lpRights)
{
	ECRESULT er = erSuccess;
	if(lpRights == NULL)
		goto exit;

	if(lpRights->__ptr)
	{
		if (lpRights->__ptr->sUserId.__ptr)
			delete [] lpRights->__ptr->sUserId.__ptr;
		delete [] lpRights->__ptr;
	}

    delete lpRights;

exit:
	return er;

}

static const struct propVal *
SpropValFindPropVal(const struct propValArray *lpsPropValArray,
    unsigned int ulPropTag)
{
	const struct propVal *lpPropVal = NULL;
	int i;

	if(PROP_TYPE(ulPropTag) == PT_ERROR)
		goto exit;

	for(i=0; i < lpsPropValArray->__size; i++)
	{
		if(lpsPropValArray->__ptr[i].ulPropTag == ulPropTag ||
			(PROP_ID(lpsPropValArray->__ptr[i].ulPropTag) == PROP_ID(ulPropTag) &&
			 PROP_TYPE(ulPropTag) == PT_UNSPECIFIED &&
			 PROP_TYPE(lpsPropValArray->__ptr[i].ulPropTag) != PT_ERROR) )
		{
			lpPropVal = &lpsPropValArray->__ptr[i];
			break;
		}
	}

exit:
	return lpPropVal;
}

// NOTE: PropValArray 2 overruled PropValArray 1, except if proptype is PT_ERROR
ECRESULT MergePropValArray(struct soap *soap,
    const struct propValArray *lpsPropValArray1,
    const struct propValArray *lpsPropValArray2,
    struct propValArray *lpPropValArrayNew)
{
	ECRESULT		er = erSuccess;
	int i;
	const struct propVal *lpsPropVal;

	lpPropValArrayNew->__ptr = s_alloc<struct propVal>(soap, lpsPropValArray1->__size + lpsPropValArray2->__size);
	lpPropValArrayNew->__size = 0;

	for(i=0; i < lpsPropValArray1->__size; i++)
	{
		lpsPropVal = SpropValFindPropVal(lpsPropValArray2, lpsPropValArray1->__ptr[i].ulPropTag);
		if(lpsPropVal == NULL)
			lpsPropVal = &lpsPropValArray1->__ptr[i];

		er = CopyPropVal(lpsPropVal, &lpPropValArrayNew->__ptr[lpPropValArrayNew->__size], soap);
		if(er != erSuccess)
			goto exit;

		lpPropValArrayNew->__size++;
	}

	//Merge items
	for(i=0; i < lpsPropValArray2->__size; i++)
	{
		lpsPropVal = SpropValFindPropVal(lpPropValArrayNew, lpsPropValArray2->__ptr[i].ulPropTag);
		if(lpsPropVal != NULL)
			continue; // Already exist

		er = CopyPropVal(&lpsPropValArray2->__ptr[i], &lpPropValArrayNew->__ptr[lpPropValArrayNew->__size], soap);
		if(er != erSuccess)
			goto exit;

		lpPropValArrayNew->__size++;
	}

exit:
	return er;
}

ECRESULT CopySearchCriteria(struct soap *soap,
    const struct searchCriteria *lpSrc, struct searchCriteria **lppDst)
{
	ECRESULT er = erSuccess;
	struct searchCriteria *lpDst = NULL;

	if(lpSrc == NULL) {
	    er = ZARAFA_E_NOT_FOUND;
	    goto exit;
    }

	lpDst = new searchCriteria;
	if(lpSrc->lpRestrict) {
    	er = CopyRestrictTable(soap, lpSrc->lpRestrict, &lpDst->lpRestrict);
    	if(er != erSuccess)
		    goto exit;
    } else {
        lpDst->lpRestrict = NULL;
    }

	if(lpSrc->lpFolders) {
    	er = CopyEntryList(soap, lpSrc->lpFolders, &lpDst->lpFolders);
    	if(er != erSuccess)
		    goto exit;
    } else {
        lpDst->lpFolders = NULL;
    }

	lpDst->ulFlags = lpSrc->ulFlags;

	*lppDst = lpDst;
exit:
	return er;
}

ECRESULT FreeSearchCriteria(struct searchCriteria *lpSearchCriteria)
{
	ECRESULT er = erSuccess;
	if(lpSearchCriteria->lpRestrict)
		FreeRestrictTable(lpSearchCriteria->lpRestrict);

	if(lpSearchCriteria->lpFolders)
		FreeEntryList(lpSearchCriteria->lpFolders);

	delete lpSearchCriteria;

	return er;
}

ECRESULT CopyUserObjectDetailsToSoap(unsigned int ulId, entryId *lpUserEid, const objectdetails_t &details, struct soap *soap, struct userobject *lpObject)
{
	ECRESULT er = erSuccess;

	lpObject->ulId = ulId;
	lpObject->lpszName = s_strcpy(soap, details.GetPropString(OB_PROP_S_FULLNAME).c_str());
	lpObject->ulType = details.GetClass();
	lpObject->sId.__size = lpUserEid->__size;
	lpObject->sId.__ptr = s_alloc<unsigned char>(soap, lpUserEid->__size);
	memcpy(lpObject->sId.__ptr, lpUserEid->__ptr, lpUserEid->__size);

	return er;
}


/**
 * Copy extra user details into propmap, (only the string values)
 *
 */
static ECRESULT CopyAnonymousDetailsToSoap(struct soap *soap,
    const objectdetails_t &details, bool bCopyBinary,
    struct propmapPairArray **lppsoapPropmap,
    struct propmapMVPairArray **lppsoapMVPropmap)
{
	ECRESULT er = erSuccess;
	struct propmapPairArray *lpsoapPropmap = NULL;
	struct propmapMVPairArray *lpsoapMVPropmap = NULL;
	property_map propmap = details.GetPropMapAnonymous();
	property_mv_map propmvmap = details.GetPropMapListAnonymous(); 
	unsigned int j = 0;

	if (!propmap.empty()) {
		lpsoapPropmap = s_alloc<struct propmapPairArray>(soap, 1);
		lpsoapPropmap->__size = 0;
		lpsoapPropmap->__ptr = s_alloc<struct propmapPair>(soap, propmap.size());
		for (property_map::iterator iter = propmap.begin(); iter != propmap.end(); iter++) {
			if (PROP_TYPE(iter->first) == PT_BINARY && bCopyBinary) {
				string strData = base64_encode((const unsigned char *)iter->second.data(), iter->second.size());
				lpsoapPropmap->__ptr[lpsoapPropmap->__size].ulPropId = iter->first;
				lpsoapPropmap->__ptr[lpsoapPropmap->__size++].lpszValue = s_strcpy(soap, strData.c_str());
				continue;
			}

			if (PROP_TYPE(iter->first) != PT_STRING8 && PROP_TYPE(iter->first) != PT_UNICODE)
				continue;
			lpsoapPropmap->__ptr[lpsoapPropmap->__size].ulPropId = iter->first;
			lpsoapPropmap->__ptr[lpsoapPropmap->__size++].lpszValue = s_strcpy(soap, iter->second.c_str());
		}
	}

	if (!propmvmap.empty()) {
		lpsoapMVPropmap = s_alloc<struct propmapMVPairArray>(soap, 1);
		lpsoapMVPropmap->__size = 0;
		lpsoapMVPropmap->__ptr = s_alloc<struct propmapMVPair>(soap, propmvmap.size());
		for (property_mv_map::iterator iter = propmvmap.begin(); iter != propmvmap.end(); iter++) {
			if (PROP_TYPE(iter->first) == PT_MV_BINARY && bCopyBinary) {
				j = 0;
				lpsoapMVPropmap->__ptr[lpsoapMVPropmap->__size].ulPropId = iter->first;
				lpsoapMVPropmap->__ptr[lpsoapMVPropmap->__size].sValues.__size = iter->second.size();
				lpsoapMVPropmap->__ptr[lpsoapMVPropmap->__size].sValues.__ptr = s_alloc<char *>(soap, lpsoapMVPropmap->__ptr[lpsoapMVPropmap->__size].sValues.__size);
				for (list<string>::iterator entry = iter->second.begin(); entry != iter->second.end(); entry++) {
					string strData = base64_encode((const unsigned char *)entry->data(), entry->size());
					lpsoapMVPropmap->__ptr[lpsoapMVPropmap->__size].sValues.__ptr[j] = s_strcpy(soap, strData.c_str());
					j++;
				}

				lpsoapMVPropmap->__size++;
				continue;
			}

			if (PROP_TYPE(iter->first) != PT_MV_STRING8 && PROP_TYPE(iter->first) != PT_MV_UNICODE)
				continue;

			j = 0;
			lpsoapMVPropmap->__ptr[lpsoapMVPropmap->__size].ulPropId = iter->first;
			lpsoapMVPropmap->__ptr[lpsoapMVPropmap->__size].sValues.__size = iter->second.size();
			lpsoapMVPropmap->__ptr[lpsoapMVPropmap->__size].sValues.__ptr = s_alloc<char *>(soap, lpsoapMVPropmap->__ptr[lpsoapMVPropmap->__size].sValues.__size);
			for (list<string>::iterator entry = iter->second.begin(); entry != iter->second.end(); entry++) {
				lpsoapMVPropmap->__ptr[lpsoapMVPropmap->__size].sValues.__ptr[j] = s_strcpy(soap, entry->c_str());
				j++;
			}

			lpsoapMVPropmap->__size++;
		}
	}

	if (lppsoapPropmap)
		*lppsoapPropmap = lpsoapPropmap;

	if (lppsoapMVPropmap)
		*lppsoapMVPropmap = lpsoapMVPropmap;

	return er;
}

static ECRESULT
CopyAnonymousDetailsFromSoap(struct propmapPairArray *lpsoapPropmap,
    struct propmapMVPairArray *lpsoapMVPropmap, objectdetails_t *details)
{
	if (lpsoapPropmap) {
		for (unsigned int i = 0; i < lpsoapPropmap->__size; i++) {
			if (PROP_TYPE(lpsoapPropmap->__ptr[i].ulPropId) == PT_BINARY) {
				string strData = base64_decode(lpsoapPropmap->__ptr[i].lpszValue);
				details->SetPropString((property_key_t)lpsoapPropmap->__ptr[i].ulPropId, strData);
			} else if (PROP_TYPE(lpsoapPropmap->__ptr[i].ulPropId) == PT_STRING8) {
				details->SetPropString((property_key_t)lpsoapPropmap->__ptr[i].ulPropId, lpsoapPropmap->__ptr[i].lpszValue);
			}
		}
	}

	if (lpsoapMVPropmap) {
		for (unsigned int i = 0; i < lpsoapMVPropmap->__size; i++) {
			details->SetPropListString((property_key_t)lpsoapMVPropmap->__ptr[i].ulPropId, list<string>());
			for (int j = 0; j < lpsoapMVPropmap->__ptr[i].sValues.__size; j++) {
				if (PROP_TYPE(lpsoapMVPropmap->__ptr[i].ulPropId) == PT_MV_BINARY) {
					string strData = base64_decode(lpsoapMVPropmap->__ptr[i].sValues.__ptr[j]);
					details->AddPropString((property_key_t)lpsoapMVPropmap->__ptr[i].ulPropId, strData);
				} else {
					details->AddPropString((property_key_t)lpsoapMVPropmap->__ptr[i].ulPropId, lpsoapMVPropmap->__ptr[i].sValues.__ptr[j]);
				}
			}
		}
	}

	return erSuccess;
}

ECRESULT CopyUserDetailsToSoap(unsigned int ulId, entryId *lpUserEid, const objectdetails_t &details, bool bCopyBinary, struct soap *soap, struct user *lpUser)
{
	ECRESULT er = erSuccess;
	const objectclass_t objClass = details.GetClass();

	// ASSERT(OBJECTCLASS_TYPE(objClass) == OBJECTTYPE_MAILUSER);

	lpUser->ulUserId = ulId;
	lpUser->lpszUsername = s_strcpy(soap, details.GetPropString(OB_PROP_S_LOGIN).c_str());
	lpUser->ulIsNonActive = (objClass == ACTIVE_USER ? 0 : 1);	// Needed for pre 6.40 clients
	lpUser->ulObjClass = objClass;
	lpUser->lpszMailAddress = s_strcpy(soap, details.GetPropString(OB_PROP_S_EMAIL).c_str());
	lpUser->lpszFullName = s_strcpy(soap, details.GetPropString(OB_PROP_S_FULLNAME).c_str());
	lpUser->ulIsAdmin = details.GetPropInt(OB_PROP_I_ADMINLEVEL);
	lpUser->lpszPassword = const_cast<char *>("");
	lpUser->lpszServername = s_strcpy(soap, details.GetPropString(OB_PROP_S_SERVERNAME).c_str());
	lpUser->ulIsABHidden = details.GetPropBool(OB_PROP_B_AB_HIDDEN);
	lpUser->ulCapacity = details.GetPropInt(OB_PROP_I_RESOURCE_CAPACITY);
	lpUser->lpsPropmap = NULL;
	lpUser->lpsMVPropmap = NULL;

	CopyAnonymousDetailsToSoap(soap, details, bCopyBinary, &lpUser->lpsPropmap, &lpUser->lpsMVPropmap);

	// Lazy copy
	lpUser->sUserId.__size = lpUserEid->__size;
	lpUser->sUserId.__ptr = lpUserEid->__ptr;

	return er;
}

ECRESULT CopyUserDetailsFromSoap(struct user *lpUser, string *lpstrExternId, objectdetails_t *details, struct soap *soap)
{
	ECRESULT er = erSuccess;

	if (lpUser->lpszUsername)
		details->SetPropString(OB_PROP_S_LOGIN, lpUser->lpszUsername);

	if (lpUser->lpszMailAddress)
		details->SetPropString(OB_PROP_S_EMAIL, lpUser->lpszMailAddress);

	if (lpUser->ulIsAdmin != (ULONG)-1)
		details->SetPropInt(OB_PROP_I_ADMINLEVEL, lpUser->ulIsAdmin);

	if (lpUser->ulObjClass != (ULONG)-1)
		details->SetClass((objectclass_t)lpUser->ulObjClass);

	if (lpUser->lpszFullName)
		details->SetPropString(OB_PROP_S_FULLNAME, lpUser->lpszFullName);

	if (lpUser->lpszPassword)
		details->SetPropString(OB_PROP_S_PASSWORD, lpUser->lpszPassword);

	if (lpstrExternId)
		details->SetPropObject(OB_PROP_O_EXTERNID, objectid_t(*lpstrExternId, details->GetClass()));

	if (lpUser->lpszServername)
		details->SetPropString(OB_PROP_S_SERVERNAME, lpUser->lpszServername);

	if (lpUser->ulIsABHidden != (ULONG)-1)
		details->SetPropBool(OB_PROP_B_AB_HIDDEN, !!lpUser->ulIsABHidden);

	if (lpUser->ulCapacity != (ULONG)-1)
		details->SetPropInt(OB_PROP_I_RESOURCE_CAPACITY, lpUser->ulCapacity);

	CopyAnonymousDetailsFromSoap(lpUser->lpsPropmap, lpUser->lpsMVPropmap, details);

	return er;
}

ECRESULT CopyGroupDetailsToSoap(unsigned int ulId, entryId *lpGroupEid, const objectdetails_t &details, bool bCopyBinary, struct soap *soap, struct group *lpGroup)
{
	ECRESULT er = erSuccess;

	// ASSERT(OBJECTCLASS_TYPE(details.GetClass()) == OBJECTTYPE_DISTLIST);

	lpGroup->ulGroupId = ulId;
	lpGroup->lpszGroupname = s_strcpy(soap, details.GetPropString(OB_PROP_S_LOGIN).c_str());
	lpGroup->lpszFullname = s_strcpy(soap, details.GetPropString(OB_PROP_S_FULLNAME).c_str());
	lpGroup->lpszFullEmail = s_strcpy(soap, details.GetPropString(OB_PROP_S_EMAIL).c_str());
	lpGroup->ulIsABHidden = details.GetPropBool(OB_PROP_B_AB_HIDDEN);
	lpGroup->lpsPropmap = NULL;
	lpGroup->lpsMVPropmap = NULL;

	CopyAnonymousDetailsToSoap(soap, details, bCopyBinary, &lpGroup->lpsPropmap, &lpGroup->lpsMVPropmap);

	// Lazy copy
	lpGroup->sGroupId.__size = lpGroupEid->__size;
	lpGroup->sGroupId.__ptr = lpGroupEid->__ptr;

	return er;
}

ECRESULT CopyGroupDetailsFromSoap(struct group *lpGroup, string *lpstrExternId, objectdetails_t *details, struct soap *soap)
{
	ECRESULT er = erSuccess;

	if (lpGroup->lpszGroupname)
		details->SetPropString(OB_PROP_S_LOGIN, lpGroup->lpszGroupname);

	if (lpGroup->lpszFullname)
		details->SetPropString(OB_PROP_S_FULLNAME, lpGroup->lpszFullname);

	if (lpGroup->lpszFullEmail)
		details->SetPropString(OB_PROP_S_EMAIL, lpGroup->lpszFullEmail);

	if (lpstrExternId)
		details->SetPropObject(OB_PROP_O_EXTERNID, objectid_t(*lpstrExternId, details->GetClass()));

	if (lpGroup->ulIsABHidden != (ULONG)-1)
		details->SetPropBool(OB_PROP_B_AB_HIDDEN, !!lpGroup->ulIsABHidden);

	CopyAnonymousDetailsFromSoap(lpGroup->lpsPropmap, lpGroup->lpsMVPropmap, details);

	return er;
}

ECRESULT CopyCompanyDetailsToSoap(unsigned int ulId, entryId *lpCompanyEid, unsigned int ulAdmin, entryId *lpAdminEid, const objectdetails_t &details, bool bCopyBinary, struct soap *soap, struct company *lpCompany)
{
	ECRESULT er = erSuccess;

	// ASSERT(details.GetClass() == CONTAINER_COMPANY);

	lpCompany->ulCompanyId = ulId;
	lpCompany->lpszCompanyname = s_strcpy(soap, details.GetPropString(OB_PROP_S_FULLNAME).c_str());
	lpCompany->ulAdministrator = ulAdmin;
	lpCompany->lpszServername = s_strcpy(soap, details.GetPropString(OB_PROP_S_SERVERNAME).c_str());
	lpCompany->ulIsABHidden = details.GetPropBool(OB_PROP_B_AB_HIDDEN);
	lpCompany->lpsPropmap = NULL;
	lpCompany->lpsMVPropmap = NULL;

	CopyAnonymousDetailsToSoap(soap, details, bCopyBinary, &lpCompany->lpsPropmap, &lpCompany->lpsMVPropmap);
	
	// Lazy copy
	lpCompany->sCompanyId.__size = lpCompanyEid->__size;
	lpCompany->sCompanyId.__ptr = lpCompanyEid->__ptr;
	
	// Lazy copy
	lpCompany->sAdministrator.__size = lpAdminEid->__size;
	lpCompany->sAdministrator.__ptr = lpAdminEid->__ptr;

	return er;
}

ECRESULT CopyCompanyDetailsFromSoap(struct company *lpCompany, string *lpstrExternId, unsigned int ulAdmin, objectdetails_t *details, struct soap *soap)
{
	ECRESULT er = erSuccess;

	if (lpCompany->lpszCompanyname)
		details->SetPropString(OB_PROP_S_FULLNAME, lpCompany->lpszCompanyname);

	if (lpCompany->lpszServername)
		details->SetPropString(OB_PROP_S_SERVERNAME, lpCompany->lpszServername);

	if (lpstrExternId)
		details->SetPropObject(OB_PROP_O_EXTERNID, objectid_t(*lpstrExternId, details->GetClass()));
 
	if (ulAdmin)
		details->SetPropInt(OB_PROP_I_SYSADMIN, ulAdmin);

	if (lpCompany->ulIsABHidden != (ULONG)-1)
		details->SetPropBool(OB_PROP_B_AB_HIDDEN, !!lpCompany->ulIsABHidden);

	CopyAnonymousDetailsFromSoap(lpCompany->lpsPropmap, lpCompany->lpsMVPropmap, details);

	return er;
}

DynamicPropValArray::DynamicPropValArray(struct soap *soap, unsigned int ulHint)
{
    m_ulCapacity = ulHint;
    m_ulPropCount = 0;
    m_soap = soap;

    m_lpPropVals = s_alloc<struct propVal>(m_soap, m_ulCapacity);
}

DynamicPropValArray::~DynamicPropValArray()
{
	if(m_lpPropVals && !m_soap) {
		for(unsigned int i=0; i < m_ulPropCount; i++) {
			FreePropVal(&m_lpPropVals[i], false);
		}
		delete [] m_lpPropVals;
	}
}
    
ECRESULT DynamicPropValArray::AddPropVal(struct propVal &propVal)
{
    ECRESULT er = erSuccess;
    
    if(m_ulCapacity == m_ulPropCount) {
        if(m_ulCapacity == 0)
            m_ulCapacity++;
        er = Resize(m_ulCapacity * 2);
        if(er != erSuccess)
            goto exit;
    }
    
    er = CopyPropVal(&propVal, &m_lpPropVals[m_ulPropCount], m_soap);
    if(er != erSuccess)
        goto exit;
        
    m_ulPropCount++;

exit:
    return er;
}

ECRESULT DynamicPropValArray::GetPropValArray(struct propValArray *lpPropValArray)
{
    ECRESULT er = erSuccess;
    
    lpPropValArray->__size = m_ulPropCount;
    lpPropValArray->__ptr = m_lpPropVals; // Transfer ownership to the caller
    
    m_lpPropVals = NULL;					// We don't own these anymore
    m_ulPropCount = 0;
    m_ulCapacity = 0;
    
    return er;
}

ECRESULT DynamicPropValArray::Resize(unsigned int ulSize)
{
    ECRESULT er = erSuccess;
    struct propVal *lpNew = NULL;
    
    if(ulSize < m_ulCapacity) {
        er = ZARAFA_E_INVALID_PARAMETER;
        goto exit;
    }
    
    lpNew = s_alloc<struct propVal>(m_soap, ulSize);
    if(lpNew == NULL) {
        er = ZARAFA_E_INVALID_PARAMETER;
        goto exit;
    }
    
    for(unsigned int i=0;i<m_ulPropCount;i++) {
        er = CopyPropVal(&m_lpPropVals[i], &lpNew[i], m_soap);
        if(er != erSuccess)
            goto exit;
    }
    
    if(!m_soap) {
		for(unsigned int i=0; i < m_ulPropCount; i++) {
			FreePropVal(&m_lpPropVals[i], false);
		}
		delete [] m_lpPropVals;
	}
	
    m_lpPropVals = lpNew;
    m_ulCapacity = ulSize;
    
exit:
    return er;
}		

DynamicPropTagArray::DynamicPropTagArray(struct soap *soap)
{
    m_soap = soap;
}

DynamicPropTagArray::~DynamicPropTagArray() { }

ECRESULT DynamicPropTagArray::AddPropTag(unsigned int ulPropTag) {
    m_lstPropTags.push_back(ulPropTag);
    
    return erSuccess;
}

BOOL DynamicPropTagArray::HasPropTag(unsigned int ulPropTag) const {
	return std::find(m_lstPropTags.begin(), m_lstPropTags.end(), ulPropTag) != m_lstPropTags.end();
}

ECRESULT DynamicPropTagArray::GetPropTagArray(struct propTagArray *lpsPropTagArray) {
    std::list<unsigned int>::iterator i;
    int n = 0;
    
    lpsPropTagArray->__size = m_lstPropTags.size();
    lpsPropTagArray->__ptr = s_alloc<unsigned int>(m_soap, lpsPropTagArray->__size);
    
    for(i=m_lstPropTags.begin(); i!=m_lstPropTags.end(); i++) {
        lpsPropTagArray->__ptr[n] = *i;
        n++;
    }
    
    return erSuccess;
}

/**
 * Calculate the propValArray size
 *
 * @param[in] lpSrc Pointer to a propVal array object
 *
 * @return the size of the object. If there is an error, object size is zero.
 */
unsigned int PropValArraySize(struct propValArray *lpSrc)
{
	unsigned int ulSize = 0;

	if(lpSrc == NULL) {
		goto exit;
	}

	ulSize = sizeof(struct propValArray) * lpSrc->__size;

	for(int i = 0; i < lpSrc->__size; i++) {
		ulSize += PropSize(&lpSrc->__ptr[i]);
	}

exit:
	return ulSize;
}

/**
 * Calculate the restrict table size
 *
 * @param[in] lpSrc Ponter to a restrict table object
 * @return the size of the object. If there is an error, object size is zero.
 */
unsigned int RestrictTableSize(struct restrictTable *lpSrc)
{
	unsigned int ulSize = 0;
	unsigned int i = 0;

	if(lpSrc == NULL) {
		goto exit;
	}

	switch(lpSrc->ulType) {
	case RES_OR:
		ulSize += sizeof(restrictOr);
		for(i=0; i<lpSrc->lpOr->__size; i++) {
			ulSize += RestrictTableSize(lpSrc->lpOr->__ptr[i]);
		}
		break;
	case RES_AND:
		ulSize += sizeof(restrictAnd);
		for(i=0;i<lpSrc->lpAnd->__size;i++) {
			ulSize += RestrictTableSize(lpSrc->lpAnd->__ptr[i]);
		}
		break;

	case RES_NOT:
		ulSize += sizeof(restrictNot);
		ulSize += RestrictTableSize(lpSrc->lpNot->lpNot);
		break;
	case RES_CONTENT:
		ulSize += sizeof(restrictContent);

		if(lpSrc->lpContent->lpProp) {
			ulSize += PropSize(lpSrc->lpContent->lpProp);
		}

		break;
	case RES_PROPERTY:
		ulSize += sizeof(restrictProp);
		ulSize += PropSize(lpSrc->lpProp->lpProp);

		break;

	case RES_COMPAREPROPS:
		ulSize += sizeof(restrictCompare);
		break;

	case RES_BITMASK:
		ulSize += sizeof(restrictBitmask);
		break;

	case RES_SIZE:
		ulSize += sizeof(restrictSize);
		break;

	case RES_EXIST:
		ulSize += sizeof(restrictExist);
		break;

	case RES_COMMENT:
		ulSize += sizeof(restrictComment) + sizeof(restrictTable);
		
		ulSize += PropValArraySize(&lpSrc->lpComment->sProps);
		ulSize += RestrictTableSize(lpSrc->lpComment->lpResTable);

		break;

	case RES_SUBRESTRICTION:
		ulSize += sizeof(restrictSub);
		ulSize += RestrictTableSize(lpSrc->lpSub->lpSubObject);

        break;
	default:
		break;
	}

exit:
	return ulSize;
}

/**
 * Calculate the size of a list of entries
 *
 * @param[in] lpSrc pointer to a list of entries
 * @return the size of the object. If there is an error, object size is zero.
 */
unsigned int EntryListSize(struct entryList *lpSrc)
{
	unsigned int ulSize = 0;

	if(lpSrc == NULL) {
		goto exit;
	}

	ulSize += sizeof(entryList);
	ulSize += sizeof(entryId) * lpSrc->__size;

	for(unsigned int i=0; i<lpSrc->__size; i++) {
		ulSize += lpSrc->__ptr[i].__size * sizeof(unsigned char);
	}

exit:
	return ulSize;
}

/**
 * Calculate the size of a proptag array
 *
 * @param[in] pPropTagArray Pointer to a array of property tags
 * @return the size of the object. If there is an error, object size is zero.
 */
unsigned int PropTagArraySize(struct propTagArray *pPropTagArray)
{
	return (pPropTagArray)?((sizeof(unsigned int) * pPropTagArray->__size) + sizeof(struct propTagArray)) : 0;
}

/**
 * Calculate the size of the search criteria object
 *
 * @param[in] lpSrc Pointer to a search criteria object.
 * @return the size of the object. If there is an error, object size is zero.
 */
unsigned int SearchCriteriaSize(struct searchCriteria *lpSrc)
{
	unsigned int ulSize = 0;

	if(lpSrc == NULL) {
	    goto exit;
    }

	ulSize += sizeof(struct searchCriteria);

	if(lpSrc->lpRestrict) {
		ulSize += RestrictTableSize(lpSrc->lpRestrict);
	}

	if(lpSrc->lpFolders) {
		ulSize += EntryListSize(lpSrc->lpFolders);
	}

exit:
	return ulSize;
}

/**
 * Calculate the size of a entryid
 *
 * @param[in] lpEntryid Pointer to an entryid object.
 * @return the size of the object. If there is an error, object size is zero.
 */
unsigned int EntryIdSize(entryId *lpEntryid)
{
	if(lpEntryid == NULL)
		return 0;
	
	return sizeof(entryId) + lpEntryid->__size;
}

/**
 * Calculate the size of a notification struct
 *
 * @param[in] lpNotification Pointer to a notification struct.
 * @return the size of the object. If there is an error, object size is zero.
 */
unsigned int NotificationStructSize(notification *lpNotification)
{
	unsigned int ulSize = 0;

	if(lpNotification == NULL) {
		goto exit;
	}

	ulSize += sizeof(notification);
	
	if(lpNotification->tab != NULL) {
		ulSize += sizeof(notificationTable);

		ulSize += PropSize(&lpNotification->tab->propIndex);
		ulSize += PropSize(&lpNotification->tab->propPrior);
		ulSize += PropValArraySize(lpNotification->tab->pRow);
	}else if(lpNotification->obj != NULL) {
		ulSize += sizeof(notificationObject);

		ulSize += EntryIdSize(lpNotification->obj->pEntryId);
		ulSize += EntryIdSize(lpNotification->obj->pParentId);
		ulSize += EntryIdSize(lpNotification->obj->pOldId);
		ulSize += EntryIdSize(lpNotification->obj->pOldParentId);
		ulSize += PropTagArraySize(lpNotification->obj->pPropTagArray);

	}else if(lpNotification->newmail != NULL){

		ulSize += sizeof(notificationNewMail);
		ulSize += EntryIdSize(lpNotification->newmail->pEntryId);
		ulSize += EntryIdSize(lpNotification->newmail->pParentId);
		
		if(lpNotification->newmail->lpszMessageClass) {
			ulSize += (unsigned int)strlen(lpNotification->newmail->lpszMessageClass)+1;
		}
	}else if(lpNotification->ics != NULL){
		ulSize += sizeof(notificationICS);
		ulSize += EntryIdSize(lpNotification->ics->pSyncState);
	}

exit:
	return ulSize;
}

/**
 * Calculate the size of a sort order array.
 *
 * @param[in] lpsSortOrder Pointer to a sort order array.
 * @return the size of the object. If there is an error, object size is zero.
 */
unsigned int SortOrderArraySize(struct sortOrderArray *lpsSortOrder)
{
	if (lpsSortOrder == NULL)
		return 0;	

	return sizeof(struct sortOrder) * lpsSortOrder->__size;
}

/**
 * Normalize the property tag to the local property type depending on -DUNICODE:
 *
 * With -DUNICODE, the function:
 * - replaces PT_STRING8 with PT_UNICODE
 * - replaces PT_MV_STRING8 with PT_MV_UNICODE
 * Without -DUNICODE, it:
 * - replaces PT_UNICODE with PT_STRING8
 * - replaces PT_MV_UNICODE with PT_MV_STRING8
 */
ULONG NormalizePropTag(ULONG ulPropTag)
{
	if((PROP_TYPE(ulPropTag) == PT_STRING8 || PROP_TYPE(ulPropTag) == PT_UNICODE) && PROP_TYPE(ulPropTag) != PT_TSTRING) {
		return CHANGE_PROP_TYPE(ulPropTag, PT_TSTRING);
	}
	if((PROP_TYPE(ulPropTag) == PT_MV_STRING8 || PROP_TYPE(ulPropTag) == PT_MV_UNICODE) && PROP_TYPE(ulPropTag) != PT_MV_TSTRING) {
		return CHANGE_PROP_TYPE(ulPropTag, PT_MV_TSTRING);
	}
	return ulPropTag;
}

/**
 * Frees a namedPropArray struct
 *
 * @param array Struct to free
 * @param bFreeBase Free passed pointer too
 */
ECRESULT FreeNamedPropArray(struct namedPropArray *array, bool bFreeBase)
{
	for(unsigned int i = 0; i < array->__size; i++) {
		if(array->__ptr[i].lpId)
			delete array->__ptr[i].lpId;
		if(array->__ptr[i].lpString)
			delete array->__ptr[i].lpString;
		if(array->__ptr[i].lpguid) {
			if(array->__ptr[i].lpguid->__ptr)
				delete [] array->__ptr[i].lpguid->__ptr;
				
			delete array->__ptr[i].lpguid;
		}
	}
	
	if (array->__ptr)
		delete [] array->__ptr;
		
	if(bFreeBase)
		delete array;

	return erSuccess;
}

/** 
 * Get logical source address for a request
 *
 * Normally returns the string representation of the IP address of the connected client. However,
 * when a proxy is detected (bProxy is true) and an X-Forwarded-From header is available, returns
 * the contents of that header
 *
 * @param[in] soap Soap object of the request
 * @result String representation of the requester's source address
 */
std::string GetSourceAddr(struct soap *soap)
{
	if( ((SOAPINFO *)soap->user)->bProxy && soap->proxy_from)
		return soap->proxy_from;
	else
		return PrettyIP(soap->ip);
}

