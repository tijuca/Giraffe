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

#include <sys/un.h>
#include <sys/socket.h>

#include <base64.h>
#include <ECChannel.h>
#include <ECDefs.h>
#include <stringutil.h>

#include "ECSearchClient.h"

ECSearchClient::ECSearchClient(const char *szIndexerPath, unsigned int ulTimeOut)
	: ECChannelClient(szIndexerPath, ":;")
{
	m_ulTimeout = ulTimeOut;
}

ECSearchClient::~ECSearchClient()
{
}

ECRESULT ECSearchClient::GetProperties(mapindexprops_t &mapProps)
{
	ECRESULT er = erSuccess;
	std::vector<std::string> lstProps;
	std::vector<std::string>::iterator iter;

	er = DoCmd("PROPS", lstProps);
	if (er != erSuccess)
		goto exit;

	for (iter = lstProps.begin(); iter != lstProps.end(); iter++) {

		std::vector<std::string> tmp = tokenize(*iter, " ");
		if (tmp.size() != 2)
			continue;

		mapProps.insert(make_pair(xtoi(tmp[1].c_str()), tmp[0]));
	}

exit:
	return er;
}

ECRESULT ECSearchClient::Scope(std::string &strServerMapping, entryId *lpsEntryId, entryList *lpsFolders)
{
	ECRESULT er = erSuccess;
	std::vector<std::string> lstResponse;
	std::string strScope;

	er = Connect();
	if (er != erSuccess)
		goto exit;

	strScope = "SCOPE " + strServerMapping + " " + bin2hex(lpsEntryId->__size, lpsEntryId->__ptr);
	for (unsigned int i = 0; i < lpsFolders->__size; i++)
		strScope += " " + bin2hex(lpsFolders->__ptr[i].__size, lpsFolders->__ptr[i].__ptr);

	er = DoCmd(strScope, lstResponse);
	if (er != erSuccess)
		goto exit;

	if (!lstResponse.empty()) {
		er = ZARAFA_E_BAD_VALUE;
		goto exit;
	}

exit:
	return er;
}

ECRESULT ECSearchClient::Query(std::string &strQuery, ECSearchResultArray **lppsResults)
{
	ECRESULT er = erSuccess;
	ECSearchResultArray *lpResults = NULL;
	std::vector<std::string> lstResponse;
	locale_t loc = createlocale(LC_NUMERIC, "C");

	if (!lppsResults) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = DoCmd("QUERY " + strQuery, lstResponse);
	if (er != erSuccess)
		goto exit;

	lpResults = new ECSearchResultArray();
	if (!lpResults) {
		er = ZARAFA_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpResults->__ptr = new ECSearchResult[lstResponse.size()];
	if (!lpResults->__ptr) {
		er = ZARAFA_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpResults->__size = 0;
	for (unsigned int i = 0; i < lstResponse.size(); i++) {
		std::vector<std::string> tmp = tokenize(lstResponse[i], " ");
		std::string strEntryId;

		if (tmp.size() > 2) {
			er = ZARAFA_E_INVALID_PARAMETER;
			goto exit;
		}

		strEntryId = hex2bin(tmp[0]);
		if (strEntryId.empty()) {
			er = ZARAFA_E_CALL_FAILED;
			goto exit;
		}

		lpResults->__ptr[i].sEntryId.__size = strEntryId.size();
		lpResults->__ptr[i].sEntryId.__ptr = new unsigned char[strEntryId.size()];
		if (!lpResults->__ptr[i].sEntryId.__ptr) {
			er = ZARAFA_E_NOT_ENOUGH_MEMORY;
			goto exit;
		}

		memcpy(lpResults->__ptr[i].sEntryId.__ptr, strEntryId.c_str(), strEntryId.size());

		lpResults->__ptr[i].fScore = strtod_l(tmp[1].c_str(), NULL, loc);
		lpResults->__size++;
	}

	if (lppsResults)
		*lppsResults = lpResults;

exit:
	if ((er != erSuccess) && lpResults)
		FreeSearchResults(lpResults);

	freelocale(loc);

	return er;
}

ECRESULT ECSearchClient::Query(std::string &strServerMapping, entryId *lpsEntryId, entryList *lpsFolders, std::string &strQuery, ECSearchResultArray **lppsResults)
{
	ECRESULT er = erSuccess;

	er = Scope(strServerMapping, lpsEntryId, lpsFolders);
	if (er != erSuccess)
		goto exit;

	er = Query(strQuery, lppsResults);
	if (er != erSuccess)
		goto exit;

exit:
	return er;
}

ECRESULT ECSearchClient::SyncRun()
{
	std::vector<std::string> lstVoid;
	return DoCmd("SYNCRUN", lstVoid);
}

void FreeSearchResults(ECSearchResultArray *lpResults, bool bFreeBase)
{
	if (!lpResults)
		return;

	if (lpResults->__ptr) {
		for (unsigned int i = 0; i < lpResults->__size; i++) {
			if (lpResults->__ptr[i].sEntryId.__ptr)
				delete [] lpResults->__ptr[i].sEntryId.__ptr;
		}

		delete [] lpResults->__ptr;
	}

	if (bFreeBase)
		delete lpResults;
}
