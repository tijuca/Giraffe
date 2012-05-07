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

#include "ECOfflineABImporter.h"
#include "ECSyncLog.h"

#include <ECLogger.h>
#include <ECABEntryID.h>
#include <stringutil.h>

#include <mapix.h>
#include <edkmdb.h>

#include <list>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static inline bool operator==(const ECENTRYID& lhs, const ECENTRYID& rhs)
{
	if (lhs.cb == rhs.cb)
		return memcmp(lhs.lpb, rhs.lpb, lhs.cb) == 0;
	else 
		return false;
}

static inline bool operator<(const ECENTRYID& lhs, const ECENTRYID& rhs)
{
	return memcmp(lhs.lpb, rhs.lpb, ((lhs.cb <= rhs.cb) ? lhs.cb : rhs.cb)) < 0;
}

OfflineABImporter::OfflineABImporter(IECServiceAdmin *lpDstServiceAdmin, IECServiceAdmin *lpSrcServiceAdmin)
{
	ECSyncLog::GetLogger(&m_lpLogger);

	m_lpDstServiceAdmin = lpDstServiceAdmin;
	m_lpDstServiceAdmin->AddRef();
	m_lpSrcServiceAdmin = lpSrcServiceAdmin;
	m_lpSrcServiceAdmin->AddRef();
}
	
OfflineABImporter::~OfflineABImporter()
{
	if(m_lpDstServiceAdmin)
		m_lpDstServiceAdmin->Release();
	if(m_lpSrcServiceAdmin)
		m_lpSrcServiceAdmin->Release();
	if (m_lpLogger)
		m_lpLogger->Release();
}
	
ULONG __stdcall OfflineABImporter::AddRef() { return 0; }
ULONG __stdcall OfflineABImporter::Release() { return 0; }
HRESULT __stdcall OfflineABImporter::QueryInterface(REFIID iid, void **lpvoid) { return MAPI_E_INTERFACE_NOT_SUPPORTED; }
HRESULT OfflineABImporter::GetLastError(HRESULT hr, ULONG ulFlags, LPMAPIERROR *lppMAPIError) { return MAPI_E_NO_SUPPORT; }
HRESULT OfflineABImporter::Config(LPSTREAM lpState, ULONG ulFlags) { return MAPI_E_NO_SUPPORT; }
HRESULT OfflineABImporter::UpdateState(LPSTREAM lpState) { return hrSuccess; }
			
HRESULT OfflineABImporter::ImportABChange(ULONG ulObjType, ULONG cbObjId, LPENTRYID lpObjId)
{
	HRESULT hr = SYNC_E_IGNORE;
	ECUSER *lpsSrcUser = NULL;
	ECGROUP *lpsSrcGroup = NULL;
	ECCOMPANY *lpsSrcCompany = NULL;
	ECCOMPANY *lpsCheckDstCompany = NULL;
	std::list<ECENTRYID> lstSrcMembers;
	std::list<ECENTRYID> lstDstMembers;
	std::list<ECENTRYID>::iterator iterSrcMembers;
	std::list<ECENTRYID>::iterator iterDstMembers;
	ULONG cDstUsers = 0;		
	LPECUSER lpDstUsers = NULL;
	ULONG cSrcUsers = 0;
	LPECUSER lpSrcUsers = NULL;
	ULONG cDstGroups = 0;		
	LPECGROUP lpDstGroups = NULL;
	ULONG cSrcGroups = 0;		
	LPECGROUP lpSrcGroups = NULL;
	ULONG ulMemberObjType = 0;

	if(ulObjType == MAPI_MAILUSER) {
		// Get the source data
		LOG_DEBUG(m_lpLogger, "ImportABChange: GetUser type=MAPI_MAILUSER, id=%s", bin2hex(cbObjId, (LPBYTE)lpObjId).c_str());
		hr = m_lpSrcServiceAdmin->GetUser(cbObjId, lpObjId, MAPI_UNICODE, &lpsSrcUser);
		if (hr == MAPI_E_NOT_FOUND){
			LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: hr=MAPI_E_NOT_FOUND (deleted, so OK)");
			hr = hrSuccess; // we'll get a delete in a later change
			goto exit;
		}
		if(hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
			goto exit;
		}

		lpsSrcUser->lpszPassword = (LPTSTR)L"Dummy";

		// FIXME admin/nonactive
		LOG_DEBUG(m_lpLogger, "ImportABChange: SetUser username=%S, id=%s", lpsSrcUser->lpszUsername, bin2hex(lpsSrcUser->sUserId.cb, lpsSrcUser->sUserId.lpb).c_str());
		hr = m_lpDstServiceAdmin->SetUser(lpsSrcUser, MAPI_UNICODE);
		if(hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
			goto exit;
		}
			
	} else if (ulObjType == MAPI_DISTLIST) {
		LOG_DEBUG(m_lpLogger, "ImportABChange: GetGroup type=MAPI_DISTLIST, id=%s", bin2hex(cbObjId, (LPBYTE)lpObjId).c_str());
		hr = m_lpSrcServiceAdmin->GetGroup(cbObjId, lpObjId, MAPI_UNICODE, &lpsSrcGroup);
		if (hr == MAPI_E_NOT_FOUND){
			LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: hr=MAPI_E_NOT_FOUND (deleted, so OK)");
			hr = hrSuccess; // we'll get a delete in a later change
			goto exit;
		}
		if (hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
			goto exit;
		}

		LOG_DEBUG(m_lpLogger, "ImportABChange: SetGroup groupname=%S, id=%s", (LPWSTR)lpsSrcGroup->lpszGroupname, bin2hex(lpsSrcGroup->sGroupId.cb, lpsSrcGroup->sGroupId.lpb).c_str());
		hr = m_lpDstServiceAdmin->SetGroup(lpsSrcGroup, MAPI_UNICODE);
		if(hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
			goto exit;
		}
			
		/* Sync group members */
		
		/* Get source members */
		LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: GetUserListOfGroup (from source)");
		hr = m_lpSrcServiceAdmin->GetUserListOfGroup(cbObjId, lpObjId, MAPI_UNICODE, &cSrcUsers, &lpSrcUsers);
		if(hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
			goto exit;
		}
		LOG_DEBUG(m_lpLogger, "ImportABChange: count=%u", cSrcUsers);
			
		for (unsigned int i = 0; i < cSrcUsers; i++) {
			if (GetNonPortableObjectType(lpSrcUsers[i].sUserId.cb, (LPENTRYID)lpSrcUsers[i].sUserId.lpb, &ulMemberObjType) != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: GetNonPortableObjectType failed");
				continue;
			}
			if (ulMemberObjType != MAPI_MAILUSER) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: objecttype=%u, not MAPI_MAILUSER(6)", ulMemberObjType);
				continue;		// skip group-in-group
			}
			GeneralizeEntryIdInPlace(lpSrcUsers[i].sUserId.cb, (LPENTRYID)lpSrcUsers[i].sUserId.lpb);
			lstSrcMembers.push_back(lpSrcUsers[i].sUserId);
		}
		
		/* Get destination members */
		LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: GetUserListOfGroup (to destination)");
		hr = m_lpDstServiceAdmin->GetUserListOfGroup(lpsSrcGroup->sGroupId.cb, (LPENTRYID)lpsSrcGroup->sGroupId.lpb, MAPI_UNICODE, &cDstUsers, &lpDstUsers);
		if(hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
			goto exit;
		}
		LOG_DEBUG(m_lpLogger, "ImportABChange: count=%u", cDstUsers);
		
		for (unsigned int i = 0; i < cDstUsers; i++) {
			if (GetNonPortableObjectType(lpDstUsers[i].sUserId.cb, (LPENTRYID)lpDstUsers[i].sUserId.lpb, &ulMemberObjType) != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: GetNonPortableObjectType failed");
				continue;
			}
			if (ulMemberObjType != MAPI_MAILUSER) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: objecttype=%u, not MAPI_MAILUSER(6)", ulMemberObjType);
				continue;		// skip group-in-group
			}
			GeneralizeEntryIdInPlace(lpDstUsers[i].sUserId.cb, (LPENTRYID)lpDstUsers[i].sUserId.lpb);
			lstDstMembers.push_back(lpDstUsers[i].sUserId);
		}
		
		lstDstMembers.sort();
		lstDstMembers.unique();
		lstSrcMembers.sort();
		lstSrcMembers.unique();
		
		/* We now have lstDstMembers, and lstSrcMembers. We now have to bring lstDstMembers into
		 * sync with lstSrcMembers */
		iterDstMembers = lstDstMembers.begin();
		iterSrcMembers = lstSrcMembers.begin();
		while (iterDstMembers != lstDstMembers.end() && iterSrcMembers != lstSrcMembers.end()) {
			if(*iterSrcMembers == *iterDstMembers) {
				++iterDstMembers;
				++iterSrcMembers;
				continue;
			}
				
			if(*iterSrcMembers < *iterDstMembers) {
				/* The item in iterSrcMembers is not in iterDstMembers, add it */
				LOG_DEBUG(m_lpLogger, "ImportABChange: AddGroupUser (to destination) id=%s", bin2hex(iterSrcMembers->cb, (LPBYTE)iterSrcMembers->lpb).c_str());
				hr = m_lpDstServiceAdmin->AddGroupUser(lpsSrcGroup->sGroupId.cb, (LPENTRYID)lpsSrcGroup->sGroupId.lpb, iterSrcMembers->cb, (LPENTRYID)iterSrcMembers->lpb);
				if(hr != hrSuccess) {
					LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
					goto exit;
				}
				iterSrcMembers++;
			} else {
				/* The item in iterDstMembers is not in iterSrcMembers, remove it */
				LOG_DEBUG(m_lpLogger, "ImportABChange: AddGroupUser (from destination) id=%s", bin2hex(iterDstMembers->cb, (LPBYTE)iterDstMembers->lpb).c_str());
				hr = m_lpDstServiceAdmin->DeleteGroupUser(lpsSrcGroup->sGroupId.cb, (LPENTRYID)lpsSrcGroup->sGroupId.lpb, iterDstMembers->cb, (LPENTRYID)iterDstMembers->lpb);
				if(hr != hrSuccess) {
					LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
					goto exit;
				}
				iterDstMembers++;
			}
		}
		
		for(;iterSrcMembers != lstSrcMembers.end(); iterSrcMembers++) {
			/* Anything left in 'src' shoul de added to 'dst' */
			LOG_DEBUG(m_lpLogger, "ImportABChange: AddGroupUser (to destination) id=%s", bin2hex(iterSrcMembers->cb, (LPBYTE)iterSrcMembers->lpb).c_str());
			hr = m_lpDstServiceAdmin->AddGroupUser(lpsSrcGroup->sGroupId.cb, (LPENTRYID)lpsSrcGroup->sGroupId.lpb, iterSrcMembers->cb, (LPENTRYID)iterSrcMembers->lpb);
			if(hr != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
				goto exit; 
			}
		}
		
		for(;iterDstMembers != lstDstMembers.end(); iterDstMembers++) {
			/* Anything left in 'dst should be deleted */
			LOG_DEBUG(m_lpLogger, "ImportABChange: AddGroupUser (from destination) id=%s", bin2hex(iterDstMembers->cb, (LPBYTE)iterDstMembers->lpb).c_str());
			hr = m_lpDstServiceAdmin->DeleteGroupUser(lpsSrcGroup->sGroupId.cb, (LPENTRYID)lpsSrcGroup->sGroupId.lpb, iterDstMembers->cb, (LPENTRYID)iterDstMembers->lpb);
			if(hr != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
				goto exit;
			}
		}
		
		/* Group member lists are now sync'ed */
					
		/* FIXME: Sync groups in groups */
	} else if (ulObjType == MAPI_ABCONT) {
		LOG_DEBUG(m_lpLogger, "ImportABChange: GetCompany (from source) type=MAPI_ABCONT, id=%s", bin2hex(cbObjId, (LPBYTE)lpObjId).c_str());
		hr = m_lpSrcServiceAdmin->GetCompany(cbObjId, lpObjId, MAPI_UNICODE, &lpsSrcCompany);
		if (hr == MAPI_E_NO_SUPPORT) {
			LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: hr=MAPI_E_NO_SUPPORT. The container is an addresslist, not a company. Ignoring");
			hr = hrSuccess;
			goto exit;
		} else if (hr == MAPI_E_NOT_FOUND) {
			LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: hr=MAPI_E_NOT_FOUND. Checking destination.");
			hr = m_lpDstServiceAdmin->GetCompany(cbObjId, lpObjId, MAPI_UNICODE, &lpsSrcCompany);
			if (hr == MAPI_E_NOT_FOUND) {
				LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: hr=MAPI_E_NOT_FOUND. So it doesn't exist on either side.");
				hr = hrSuccess;
				goto exit; /* No need to sync anything */
			} else if (hr != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
				goto exit;
			}

			/* Company is present in offline database, but we were not allowed to
			 * view it in the online database. Permissions have changed and we should
			 * remove the company from the offline database.
			 * Go through the user and grouplist to delete all members of this company
			 * unfortunately we don't know which users belong to which company
			 * so we must check all users */
			LOG_DEBUG(m_lpLogger, "ImportABChange: ImportABDeletion type=MAPI_ABCONT, id=%s", bin2hex(cbObjId, (LPBYTE)lpObjId).c_str());
			hr = ImportABDeletion(MAPI_ABCONT, cbObjId, lpObjId);
			if (hr != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
				goto exit;
			}

			LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: GetGroupList (from destination)");
			hr = m_lpDstServiceAdmin->GetGroupList(0, NULL, MAPI_UNICODE, &cDstGroups, &lpDstGroups);
			if (hr != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
				goto exit;
			}
			LOG_DEBUG(m_lpLogger, "ImportABChange: count=%u", cDstGroups);

			for (unsigned int i = 0; i < cDstGroups; i++) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: GetGroup (from source) id=%s", bin2hex(lpDstGroups[i].sGroupId.cb, (LPBYTE)lpDstGroups[i].sGroupId.lpb).c_str());
				hr = m_lpSrcServiceAdmin->GetGroup(lpDstGroups[i].sGroupId.cb, (LPENTRYID)lpDstGroups[i].sGroupId.lpb, MAPI_UNICODE, &lpsSrcGroup);
				if (hr == MAPI_E_NOT_FOUND) {
					LOG_DEBUG(m_lpLogger, "ImportABChange: ImportABDeletion type=MAPI_ABCONT, id=%s", bin2hex(lpDstGroups[i].sGroupId.cb, (LPBYTE)lpDstGroups[i].sGroupId.lpb).c_str());
					hr = ImportABDeletion(MAPI_DISTLIST, lpDstGroups[i].sGroupId.cb, (LPENTRYID)lpDstGroups[i].sGroupId.lpb);
				}
				if (hr != hrSuccess) {
					LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
					goto exit;
				}
			}
	
			LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: GetUserList (from destination)");
			hr = m_lpDstServiceAdmin->GetUserList(0, NULL, MAPI_UNICODE, &cDstUsers, &lpDstUsers);
			if (hr != hrSuccess)
				goto exit;
			LOG_DEBUG(m_lpLogger, "ImportABChange: count=%u", cDstUsers);

			for (unsigned int i = 0; i < cDstUsers; i++) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: GetUser (from source) id=%s", bin2hex(lpDstUsers[i].sUserId.cb, (LPBYTE)lpDstUsers[i].sUserId.lpb).c_str());
				hr = m_lpSrcServiceAdmin->GetUser(lpDstUsers[i].sUserId.cb, (LPENTRYID)lpDstUsers[i].sUserId.lpb, MAPI_UNICODE, &lpsSrcUser);
				if (hr == MAPI_E_NOT_FOUND) {
					LOG_DEBUG(m_lpLogger, "ImportABChange: ImportABDeletion type=MAPI_ABCONT, id=%s", bin2hex(lpDstUsers[i].sUserId.cb, (LPBYTE)lpDstUsers[i].sUserId.lpb).c_str());
					hr = ImportABDeletion(MAPI_MAILUSER, lpDstUsers[i].sUserId.cb, (LPENTRYID)lpDstUsers[i].sUserId.lpb);
				}
				if (hr != hrSuccess) {
					LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
					goto exit;
				}
			}

			// Company, users and/or groups are deleted.
			goto exit;
		} else if (hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
			goto exit;
		}

		/*
		 * If the company is already present in the offline database,
		 * then the rights or the name was changed and we don't need
		 * to sync the user/grouplist.
		 */
		LOG_DEBUG(m_lpLogger, "ImportABChange: GetCompany (from destination) type=MAPI_ABCONT, id=%s", bin2hex(lpsSrcCompany->sCompanyId.cb, (LPBYTE)lpsSrcCompany->sCompanyId.lpb).c_str());
		hr = m_lpDstServiceAdmin->GetCompany(lpsSrcCompany->sCompanyId.cb, (LPENTRYID)lpsSrcCompany->sCompanyId.lpb, MAPI_UNICODE, &lpsCheckDstCompany);
		if (hr != hrSuccess && hr != MAPI_E_NOT_FOUND) {
			LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
			goto exit;
		}

		/* Sync company name */
		LOG_DEBUG(m_lpLogger, "ImportABChange: SetCompany comapnyname=%S, id=%s", (LPWSTR)lpsSrcCompany->lpszCompanyname, bin2hex(lpsSrcCompany->sCompanyId.cb, lpsSrcCompany->sCompanyId.lpb).c_str());
		hr = m_lpDstServiceAdmin->SetCompany(lpsSrcCompany, MAPI_UNICODE);
		if (hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
			goto exit;
		}

		if (lpsCheckDstCompany == NULL) {
			/*
			* Sync users/groups by requesting the members of the company
			* and calling ImportABChange recurively to add the members
			*/
			LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: GetUserList (from source)");
			hr = m_lpSrcServiceAdmin->GetUserList(cbObjId, lpObjId, MAPI_UNICODE, &cSrcUsers, &lpSrcUsers);
			if (hr != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
				goto exit;
			}
			LOG_DEBUG(m_lpLogger, "ImportABChange: count=%u", cDstUsers);

			for (unsigned int i = 0; i < cSrcUsers; i++) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: ImportABChange type=MAPI_MAILUSER, id=%s", bin2hex(lpSrcUsers[i].sUserId.cb, (LPBYTE)lpSrcUsers[i].sUserId.lpb).c_str());
				hr = ImportABChange(MAPI_MAILUSER, lpSrcUsers[i].sUserId.cb, (LPENTRYID)lpSrcUsers[i].sUserId.lpb);
				if (hr != hrSuccess) {
					LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
					goto exit;
				}
			}

			LOG_DEBUG(m_lpLogger, "%s", "ImportABChange: GetGroupList (from source)");
			hr = m_lpSrcServiceAdmin->GetGroupList(cbObjId, lpObjId, MAPI_UNICODE, &cSrcGroups, &lpSrcGroups);
			if (hr != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "ImportABChange: hr=%s", stringify(hr, true).c_str());
				goto exit;
			}

			for (unsigned int i = 0; i < cSrcGroups; i++) {
				bool bEveryone = false;
				hr = EntryIdIsEveryone(lpSrcGroups[i].sGroupId.cb, (LPENTRYID)lpSrcGroups[i].sGroupId.lpb, &bEveryone);
				if (hr != hrSuccess) {
					LOG_DEBUG(m_lpLogger, "ImportABChange: EntryIdIsEveryone failed, hr=%s", stringify(hr, true).c_str());
					goto exit;
				}

				if (!bEveryone) {
					LOG_DEBUG(m_lpLogger, "ImportABChange: ImportABChange type=MAPI_DISTLIST, id=%s", bin2hex(lpSrcGroups[i].sGroupId.cb, (LPBYTE)lpSrcGroups[i].sGroupId.lpb).c_str());
					hr = ImportABChange(MAPI_DISTLIST, lpSrcGroups[i].sGroupId.cb, (LPENTRYID)lpSrcGroups[i].sGroupId.lpb);
					if (hr != hrSuccess)
						goto exit;
				}
			}
		}
	} else {
		LOG_DEBUG(m_lpLogger, "ImportABChange: Invalid type=%u", ulObjType);
		hr = MAPI_E_INVALID_TYPE;
	}

exit:
	if(lpsSrcUser)
		MAPIFreeBuffer(lpsSrcUser);
		
	if(lpsSrcGroup)
		MAPIFreeBuffer(lpsSrcGroup);
		
	if(lpsSrcCompany)
		MAPIFreeBuffer(lpsSrcCompany);

	if(lpsCheckDstCompany)
		MAPIFreeBuffer(lpsCheckDstCompany);

	if(lpDstUsers)
		MAPIFreeBuffer(lpDstUsers);

	if(lpSrcUsers)
		MAPIFreeBuffer(lpSrcUsers);
		
	if(lpDstGroups)
		MAPIFreeBuffer(lpDstGroups);

	if(lpSrcGroups)
		MAPIFreeBuffer(lpSrcGroups);

	return hr;
}
	
HRESULT OfflineABImporter::ImportABDeletion(ULONG ulType, ULONG cbObjId, LPENTRYID lpObjId)
{
	HRESULT hr = hrSuccess;

	if(ulType == MAPI_MAILUSER) {
		hr = m_lpDstServiceAdmin->DeleteUser(cbObjId, lpObjId);
	} else if(ulType == MAPI_DISTLIST) {
		hr = m_lpDstServiceAdmin->DeleteGroup(cbObjId, lpObjId);
	} else if (ulType == MAPI_ABCONT) {
		hr = m_lpDstServiceAdmin->DeleteCompany(cbObjId, lpObjId);
	}

	if(hr == MAPI_E_NOT_FOUND) {
		// Deleting nonexistent user: success
		hr = hrSuccess;
	}
	
	return hr;
}
