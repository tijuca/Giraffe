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

#ifndef transaction_INCLUDED
#define transaction_INCLUDED

#include "transaction_fwd.h"
#include "archiver-session_fwd.h"
#include "archiver-common.h"
#include "postsaveaction.h"

#include "mapi_ptr.h"

namespace za { namespace operations {

class Transaction {
public:
	Transaction(const SObjectEntry &objectEntry);
	HRESULT SaveChanges(SessionPtr ptrSession, RollbackPtr *lpptrRollback);
	HRESULT PurgeDeletes(SessionPtr ptrSession, TransactionPtr ptrDeferredTransaction = TransactionPtr());
	const SObjectEntry& GetObjectEntry() const;

	HRESULT Save(IMessage *lpMessage, bool bDeleteOnFailure, const PostSaveActionPtr &ptrPSAction = PostSaveActionPtr());
	HRESULT Delete(const SObjectEntry &objectEntry, bool bDeferredDelete = false);

private:
	struct SaveEntry {
		MessagePtr	ptrMessage;
		bool bDeleteOnFailure;
		PostSaveActionPtr ptrPSAction;
	};
	typedef std::list<SaveEntry>	MessageList;

	struct DelEntry {
		SObjectEntry objectEntry;
		bool bDeferredDelete;
	};
	typedef std::list<DelEntry>	ObjectList;

	const SObjectEntry	m_objectEntry;
	MessageList m_lstSave;
	ObjectList m_lstDelete;
};

inline const SObjectEntry& Transaction::GetObjectEntry() const
{
	return m_objectEntry;
}




class Rollback {
public:
	HRESULT Delete(SessionPtr ptrSession, IMessage *lpMessage);
	HRESULT Execute(SessionPtr ptrSession);

private:
	struct DelEntry {
		MAPIFolderPtr ptrFolder;
		entryid_t eidMessage;
	};
	typedef std::list<DelEntry>	MessageList;
	MessageList	m_lstDelete;
};

}} // namespace operations, za

#endif // ndef transaction_INCLUDED
