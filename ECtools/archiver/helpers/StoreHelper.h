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

#ifndef STOREHELPER_H_INCLUDED
#define STOREHELPER_H_INCLUDED

#include <memory>
#include <kopano/zcdefs.h>
#include "MAPIPropHelper.h"

namespace KC {

class ECRestriction;
class ECAndRestriction;
class ECOrRestriction;

namespace helpers {

class StoreHelper;
typedef std::unique_ptr<StoreHelper> StoreHelperPtr;

/**
 * The StoreHelper class provides some common utility functions that relate to IMsgStore
 * objects in the archiver context.
 */
class _kc_export StoreHelper _kc_final : public MAPIPropHelper {
public:
	static HRESULT Create(MsgStorePtr &ptrMsgStore, StoreHelperPtr *lpptrStoreHelper);
	_kc_hidden HRESULT GetFolder(const tstring &name, bool create, LPMAPIFOLDER *ret);
	_kc_hidden HRESULT UpdateSearchFolders(void);
	_kc_hidden HRESULT GetIpmSubtree(LPMAPIFOLDER *);
	HRESULT GetSearchFolders(LPMAPIFOLDER *lppSearchArchiveFolder, LPMAPIFOLDER *lppSearchDeleteFolder, LPMAPIFOLDER *lppSearchStubFolder);
	
private:
	_kc_hidden StoreHelper(MsgStorePtr &);
	_kc_hidden HRESULT Init(void);
	_kc_hidden HRESULT GetSubFolder(MAPIFolderPtr &, const tstring &name, bool create, LPMAPIFOLDER *ret);
	enum eSearchFolder {esfArchive = 0, esfDelete, esfStub, esfMax};
	_kc_hidden HRESULT CheckAndUpdateSearchFolder(LPMAPIFOLDER folder, eSearchFolder which);
	_kc_hidden HRESULT CreateSearchFolder(eSearchFolder which, LPMAPIFOLDER *ret);
	_kc_hidden HRESULT CreateSearchFolders(LPMAPIFOLDER *archive_folder, LPMAPIFOLDER *delete_folder, LPMAPIFOLDER *stub_folder);
	_kc_hidden HRESULT DoCreateSearchFolder(LPMAPIFOLDER parent, eSearchFolder which, LPMAPIFOLDER *retsf);
	_kc_hidden HRESULT SetupSearchArchiveFolder(LPMAPIFOLDER folder, const ECRestriction *class_chk, const ECRestriction *arc_chk);
	_kc_hidden HRESULT SetupSearchDeleteFolder(LPMAPIFOLDER folder, const ECRestriction *class_chk, const ECRestriction *arc_chk);
	_kc_hidden HRESULT SetupSearchStubFolder(LPMAPIFOLDER folder, const ECRestriction *class_chk, const ECRestriction *arc_chk);
	_kc_hidden HRESULT GetClassCheckRestriction(ECOrRestriction *class_chk);
	_kc_hidden HRESULT GetArchiveCheckRestriction(ECAndRestriction *arc_chk);

	typedef HRESULT(StoreHelper::*fn_setup_t)(LPMAPIFOLDER, const ECRestriction *, const ECRestriction *);
	struct search_folder_info_t {
		LPCTSTR		lpszName;
		LPCTSTR		lpszDescription;
		fn_setup_t	fnSetup;
	};

	static const search_folder_info_t s_infoSearchFolders[];
	MsgStorePtr	m_ptrMsgStore;
	MAPIFolderPtr m_ptrIpmSubtree;
	
	PROPMAP_DECL()
	PROPMAP_DEF_NAMED_ID(ARCHIVE_STORE_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(ARCHIVE_ITEM_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(ORIGINAL_SOURCEKEY)
	PROPMAP_DEF_NAMED_ID(SEARCH_FOLDER_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(STUBBED)
	PROPMAP_DEF_NAMED_ID(DIRTY)
	PROPMAP_DEF_NAMED_ID(FLAGS)
	PROPMAP_DEF_NAMED_ID(VERSION)
};

}} /* namespace */

#endif // !defined STOREHELPER_H_INCLUDED
