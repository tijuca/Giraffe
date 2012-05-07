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

#ifndef storehelper_INCLUDED
#define storehelper_INCLUDED

#include "archivehelper.h"
#include "mapiprophelper.h"

#include <memory>
#include <string>
#include <list>

#include <CommonUtil.h>
#include <mapi_ptr.h>
#include "tstring.h"

class ECRestriction;
class ECAndRestriction;
class ECOrRestriction;

namespace za { namespace helpers {

class StoreHelper;
typedef std::auto_ptr<StoreHelper> StoreHelperPtr;

/**
 * The StoreHelper class provides some common utility functions that relate to IMsgStore
 * objects in the archiver context.
 */
class StoreHelper : public MAPIPropHelper
{
public:
	static HRESULT Create(MsgStorePtr &ptrMsgStore, StoreHelperPtr *lpptrStoreHelper);
	~StoreHelper();
	
	HRESULT GetFolder(const tstring &strFolder, bool bCreate, LPMAPIFOLDER *lppFolder);
	HRESULT UpdateSearchFolders();
	
	HRESULT GetIpmSubtree(LPMAPIFOLDER *lppFolder);
	HRESULT GetSearchFolders(LPMAPIFOLDER *lppSearchArchiveFolder, LPMAPIFOLDER *lppSearchDeleteFolder, LPMAPIFOLDER *lppSearchStubFolder);
	
private:
	StoreHelper(MsgStorePtr &ptrMsgStore);
	HRESULT Init();
	
	HRESULT GetSubFolder(MAPIFolderPtr &ptrFolder, const tstring &strFolder, bool bCreate, LPMAPIFOLDER *lppFolder);

	enum eSearchFolder {esfArchive = 0, esfDelete, esfStub, esfMax};
	
	HRESULT CheckAndUpdateSearchFolder(LPMAPIFOLDER lpSearchFolder, eSearchFolder esfWhich);
	HRESULT CreateSearchFolder(eSearchFolder esfWhich, LPMAPIFOLDER *lppSearchFolder);
	HRESULT CreateSearchFolders(LPMAPIFOLDER *lppSearchArchiveFolder, LPMAPIFOLDER *lppSearchDeleteFolder, LPMAPIFOLDER *lppSearchStubFolder);
	HRESULT DoCreateSearchFolder(LPMAPIFOLDER lpParent, eSearchFolder esfWhich, LPMAPIFOLDER *lppSearchFolder);

	HRESULT SetupSearchArchiveFolder(LPMAPIFOLDER lpSearchFolder, const ECRestriction *lpresClassCheck, const ECRestriction *lpresArchiveCheck);
	HRESULT SetupSearchDeleteFolder(LPMAPIFOLDER lpSearchFolder, const ECRestriction *lpresClassCheck, const ECRestriction *lpresArchiveCheck);
	HRESULT SetupSearchStubFolder(LPMAPIFOLDER lpSearchFolder, const ECRestriction *lpresClassCheck, const ECRestriction *lpresArchiveCheck);

	HRESULT GetClassCheckRestriction(ECOrRestriction *lpresClassCheck);
	HRESULT GetArchiveCheckRestriction(ECAndRestriction *lpresArchiveCheck);

private:
	typedef HRESULT(StoreHelper::*fn_setup_t)(LPMAPIFOLDER, const ECRestriction *, const ECRestriction *);
	struct search_folder_info_t {
		LPCTSTR		lpszName;
		LPCTSTR		lpszDescription;
		fn_setup_t	fnSetup;
	};

	static search_folder_info_t s_infoSearchFolders[];

	MsgStorePtr	m_ptrMsgStore;
	MAPIFolderPtr m_ptrIpmSubtree;
	
	PROPMAP_START
	PROPMAP_DEF_NAMED_ID(ARCHIVE_STORE_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(ARCHIVE_ITEM_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(ORIGINAL_SOURCEKEY)
	PROPMAP_DEF_NAMED_ID(SEARCH_FOLDER_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(STUBBED)
	PROPMAP_DEF_NAMED_ID(DIRTY)
	PROPMAP_DEF_NAMED_ID(FLAGS)
	PROPMAP_DEF_NAMED_ID(VERSION)
};

}} // namespaces

#endif // ndef storehelper_INCLUDED
