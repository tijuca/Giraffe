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

#ifndef ECICSHELPERS_H
#define ECICSHELPERS_H

#include "ECICS.h"
#include "ECDatabase.h"

// Indexes into the database rows.
enum {
	icsID				= 0,
	icsSourceKey		= 1,
	icsParentSourceKey	= 2,
	icsChangeType		= 3,
	icsFlags			= 4,
	icsMsgFlags			= 5,
	icsSourceSync		= 6
};


// Auxiliary Message Data (ParentSourceKey, Last ChangeId)
struct SAuxMessageData {
	SAuxMessageData(const SOURCEKEY &ps, unsigned int ct, unsigned int flags): sParentSourceKey(ps), ulChangeTypes(ct), ulFlags(flags) {}
	SOURCEKEY		sParentSourceKey;
	unsigned int	ulChangeTypes;
	unsigned int	ulFlags; // For readstate change
};
typedef std::map<SOURCEKEY,SAuxMessageData>	MESSAGESET, *LPMESSAGESET;


// Forward declarations of interfaces used by ECGetContentChangesHelper
class IDbQueryCreator;
class IMessageProcessor;


class ECGetContentChangesHelper
{
public:
	static ECRESULT Create(struct soap *soap, ECSession *lpSession, ECDatabase *lpDatabase, const SOURCEKEY &sFolderSourceKey, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulFlags, struct restrictTable *lpsRestrict, ECGetContentChangesHelper **lppHelper);
	~ECGetContentChangesHelper();
	
	ECRESULT QueryDatabase(DB_RESULT *lppDBResult);
	ECRESULT ProcessRow(DB_ROW lpDBRow, DB_LENGTHS lpDBLen);
	ECRESULT ProcessResidualMessages();
	ECRESULT Finalize(unsigned int *lpulMaxChange, icsChangesArray **lppChanges);
	
private:
	ECGetContentChangesHelper(struct soap *soap, ECSession *lpSession, ECDatabase *lpDatabase, const SOURCEKEY &sFolderSourceKey, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulFlags, struct restrictTable *lpsRestrict);
	ECRESULT Init();
	
	ECRESULT MatchRestriction(const SOURCEKEY &sSourceKey, struct restrictTable *lpsRestrict, bool *lpfMatch);
	ECRESULT GetSyncedMessages(unsigned int ulSyncId, unsigned int ulChangeId, LPMESSAGESET lpsetMessages);
	static bool CompareMessageEntry(const MESSAGESET::value_type &lhs, const MESSAGESET::value_type &rhs);
	bool MessageSetsDiffer() const;
	
private:
	// Interfaces for delegated processing
	IDbQueryCreator		*m_lpQueryCreator;
	IMessageProcessor	*m_lpMsgProcessor;
	
	// Internal variables
	soap			*m_soap;
	ECSession		*m_lpSession;
	ECDatabase		*m_lpDatabase;
	restrictTable	*m_lpsRestrict;
	icsChangesArray *m_lpChanges;
	const SOURCEKEY	&m_sFolderSourceKey;
	unsigned int	m_ulSyncId;
	unsigned int	m_ulChangeId;
	unsigned int	m_ulChangeCnt;
	unsigned int	m_ulMaxFolderChange;
	unsigned int	m_ulFlags;
	MESSAGESET		m_setLegacyMessages;
	MESSAGESET		m_setNewMessages;
};


#endif // ndef ECICSHELPERS_H
