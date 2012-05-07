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
#include "ECABEntryID.h"

#include <mapicode.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/* This is a copy from the definition in Zarafa.h. It's for internal use only as we
 * don't want to expose the format of the entry id. */
typedef struct ABEID {
	BYTE	abFlags[4];
	GUID	guid;
	ULONG	ulVersion;
	ULONG	ulType;
	ULONG	ulId;
	char	szExId[1];
	char	szPadding[3];

	ABEID(ULONG ulType, GUID guid, ULONG ulId) {
		memset(this, 0, sizeof(ABEID));
		this->ulType = ulType;
		this->guid = guid;
		this->ulId = ulId;
	}
} ABEID, *PABEID;

static const		GUID MUIDECSAB = {0x50a921ac, 0xd340, 0x48ee, {0xb3, 0x19, 0xfb, 0xa7, 0x53, 0x30, 0x44, 0x25}};

static ABEID		g_sDefaultEid(MAPI_MAILUSER, MUIDECSAB, 0);
unsigned char		*g_lpDefaultEid = (unsigned char*)&g_sDefaultEid;
const unsigned int	g_cbDefaultEid = sizeof(g_sDefaultEid);

static ABEID		g_sEveryOneEid(MAPI_DISTLIST, MUIDECSAB, 1);
unsigned char		*g_lpEveryoneEid = (unsigned char*)&g_sEveryOneEid;
const unsigned int	g_cbEveryoneEid = sizeof(g_sEveryOneEid);

static ABEID		g_sSystemEid(MAPI_MAILUSER, MUIDECSAB, 2);
unsigned char		*g_lpSystemEid = (unsigned char*)&g_sSystemEid;
const unsigned int	g_cbSystemEid = sizeof(g_sSystemEid);

static HRESULT CheckEntryId(unsigned int cbEntryId, LPENTRYID lpEntryId, unsigned ulId, unsigned ulType, bool *lpbResult)
{
	HRESULT	hr = hrSuccess;
	bool	bResult = true;
	PABEID	lpEid = NULL;

	if (cbEntryId < sizeof(ABEID) || lpEntryId == NULL || lpbResult == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	lpEid = (PABEID)lpEntryId;
	if (lpEid->ulId != ulId)
		bResult = false;
		
	else if (lpEid->ulType != ulType)
		bResult = false;

	else if (lpEid->ulVersion == 1 && strlen(lpEid->szExId) > 0)
		bResult = false;

	*lpbResult = bResult;

exit:
	return hr;
}

HRESULT EntryIdIsDefault(unsigned int cbEntryId, LPENTRYID lpEntryId, bool *lpbResult)
{
	return CheckEntryId(cbEntryId, lpEntryId, 0, MAPI_MAILUSER, lpbResult);
}

HRESULT EntryIdIsSystem(unsigned int cbEntryId, LPENTRYID lpEntryId, bool *lpbResult)
{
	return CheckEntryId(cbEntryId, lpEntryId, 2, MAPI_MAILUSER, lpbResult);
}

HRESULT EntryIdIsEveryone(unsigned int cbEntryId, LPENTRYID lpEntryId, bool *lpbResult)
{
	return CheckEntryId(cbEntryId, lpEntryId, 1, MAPI_DISTLIST, lpbResult);
}

HRESULT GetNonPortableObjectId(unsigned int cbEntryId, LPENTRYID lpEntryId, unsigned int *lpulObjectId)
{
	HRESULT hr = hrSuccess;

	if (cbEntryId < sizeof(ABEID) || lpEntryId == NULL || lpulObjectId == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	*lpulObjectId = ((PABEID)lpEntryId)->ulId;

exit:
	return hr;
}

HRESULT GetNonPortableObjectType(unsigned int cbEntryId, LPENTRYID lpEntryId, ULONG *lpulObjectType)
{
	HRESULT hr = hrSuccess;

	if (cbEntryId < sizeof(ABEID) || lpEntryId == NULL || lpulObjectType == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	*lpulObjectType = ((PABEID)lpEntryId)->ulType;

exit:
	return hr;
}

HRESULT GeneralizeEntryIdInPlace(unsigned int cbEntryId, LPENTRYID lpEntryId)
{
	HRESULT	hr = hrSuccess;
	PABEID	lpAbeid = NULL;

	if (cbEntryId < sizeof(ABEID) || lpEntryId == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	lpAbeid = (PABEID)lpEntryId;
	switch (lpAbeid->ulVersion) {
		// A version 0 entry id is generalized by nature as it's not used to be shared
		// between servers.
		case 0:
			break;

		// A version 1 entry id can be understood by all servers in a cluster, but they
		// cannot be compared with memcpy because the ulId field is server specific.
		// However we can zero this field as the server doesn't need it to locate the
		// object referenced with the entry id.
		// An exception on this rule are SYSTEM en EVERYONE as they don't have an external
		// id. However their ulId fields are specified as 1 and 2 respectively. So every
		// server will understand the entry id, regardless of the version number. We will
		// downgrade anyway be as compatible as possible in that case.
		case 1:
			if (strlen(lpAbeid->szExId) > 0)	// remove the 'legacy ulId field'	
				lpAbeid->ulId = 0;			
			else {								// downgrade to version 0
				ASSERT(cbEntryId == sizeof(ABEID));
				lpAbeid->ulVersion = 0;
			}
			break;

		default:
			ASSERT(FALSE);
			break;
	}

exit:
	return hr;
}
