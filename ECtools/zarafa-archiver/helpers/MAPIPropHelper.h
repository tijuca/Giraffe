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

#ifndef MAPIPROPHELPER_INCLUDED
#define MAPIPROPHELPER_INCLUDED

#include <memory>

#include <mapix.h>
#include <mapi_ptr.h>

#include <CommonUtil.h>

#include "archiver-common.h"
#include "ArchiverSessionPtr.h"     // For ArchiverSessionPtr

namespace za { namespace helpers {

class MAPIPropHelper;
typedef std::auto_ptr<MAPIPropHelper> MAPIPropHelperPtr;

class MessageState;

/**
 * The MAPIPropHelper class provides some common utility functions that relate to IMAPIProp
 * objects in the archiver context.
 */
class MAPIPropHelper
{
public:
	static HRESULT Create(MAPIPropPtr ptrMapiProp, MAPIPropHelperPtr *lpptrMAPIPropHelper);
	virtual ~MAPIPropHelper();

	HRESULT GetMessageState(ArchiverSessionPtr ptrSession, MessageState *lpState);
	HRESULT GetArchiveList(ObjectEntryList *lplstArchives, bool bIgnoreSourceKey = false);
	HRESULT SetArchiveList(const ObjectEntryList &lstArchives, bool bExplicitCommit = false);
	HRESULT SetReference(const SObjectEntry &sEntry, bool bExplicitCommit = false);
	HRESULT GetReference(SObjectEntry *lpEntry);
	HRESULT ClearReference(bool bExplicitCommit = false);
	HRESULT ReferencePrevious(const SObjectEntry &sEntry);
	HRESULT OpenPrevious(ArchiverSessionPtr ptrSession, LPMESSAGE *lppMessage);
	HRESULT RemoveStub();
	HRESULT SetClean();
	HRESULT DetachFromArchives();
	virtual HRESULT GetParentFolder(ArchiverSessionPtr ptrSession, LPMAPIFOLDER *lppFolder);

	static HRESULT GetArchiverProps(MAPIPropPtr ptrMapiProp, LPSPropTagArray lpExtra, LPSPropTagArray *lppProps);
	static HRESULT IsStubbed(MAPIPropPtr ptrMapiProp, LPSPropValue lpProps, ULONG cbProps, bool *lpbResult);
	static HRESULT GetArchiveList(MAPIPropPtr ptrMapiProp, LPSPropValue lpProps, ULONG cbProps, ObjectEntryList *lplstArchives);

protected:
	MAPIPropHelper(MAPIPropPtr ptrMapiProp);
	HRESULT Init();

private:
	MAPIPropPtr m_ptrMapiProp;

	PROPMAP_START
	PROPMAP_DEF_NAMED_ID(ARCHIVE_STORE_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(ARCHIVE_ITEM_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(ORIGINAL_SOURCEKEY)
	PROPMAP_DEF_NAMED_ID(STUBBED)
	PROPMAP_DEF_NAMED_ID(DIRTY)
	PROPMAP_DEF_NAMED_ID(REF_STORE_ENTRYID)
	PROPMAP_DEF_NAMED_ID(REF_ITEM_ENTRYID)
	PROPMAP_DEF_NAMED_ID(REF_PREV_ENTRYID)
};


class MessageState
{
public:
	MessageState(): m_ulState(0) {}

	bool isStubbed() const { return (m_ulState & msStubbed) != 0; }
	bool isDirty() const { return (m_ulState & msDirty) != 0; }
	bool isCopy() const { return (m_ulState & msCopy) != 0; }
	bool isMove() const { return (m_ulState & msMove) != 0; }

private:
	enum msFlags {
		msStubbed	= 0x01,	//<	The message is stubbed, mutual exlusive with msDirty
		msDirty		= 0x02,	//<	The message is dirty, mutual exclusive with msStubbed
		msCopy		= 0x04,	//< The message is copied, mutual exclusive with msMove
		msMove		= 0x08	//< The message is moved, mutual exclusive with msCopy
	};

	ULONG m_ulState;

friend class MAPIPropHelper;
};

}} // namespaces

#endif // !defined MAPIPROPHELPER_INCLUDED
