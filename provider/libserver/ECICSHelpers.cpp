/*
 * Copyright 2005 - 2009  Zarafa B.V.
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
#include <stringutil.h>
#include <ZarafaICS.h>

#include "ECStoreObjectTable.h"
#include "ECICSHelpers.h"
#include "ECSessionManager.h"
#include "ECMAPI.h"

#include <mapidefs.h>
#include <edkmdb.h>

#include <string>
#include <algorithm>

#include "ECLogger.h"
extern ECLogger* g_lpLogger;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern ECSessionManager*	g_lpSessionManager;



/**
 * IDbQueryCreator: Interface to the database query creators
 **/
class IDbQueryCreator
{
public:
	virtual ~IDbQueryCreator() {};
	virtual std::string CreateQuery() = 0;
};


/**
 * CommonQueryCreator: Abstract implementation of IDBQueryCreator that handles the
 *                     common part of all queries.
 **/
class CommonQueryCreator : public IDbQueryCreator
{
public:
	CommonQueryCreator(unsigned int ulFlags);
	
	// IDbQueryCreator
	std::string CreateQuery();
	
private:
	virtual std::string CreateBaseQuery() = 0;
	virtual std::string CreateOrderQuery() = 0;
	
private:
	unsigned int m_ulFlags;
};

CommonQueryCreator::CommonQueryCreator(unsigned int ulFlags)
	: m_ulFlags(ulFlags)
{ }

std::string CommonQueryCreator::CreateQuery()
{
	std::string strQuery = CreateBaseQuery();
	
	if ((m_ulFlags & SYNC_ASSOCIATED) == 0)
		strQuery += " AND (ISNULL(hierarchy.flags) OR hierarchy.flags & " + stringify(MSGFLAG_ASSOCIATED) + " = 0) ";

	if ((m_ulFlags & SYNC_NORMAL) == 0)
		strQuery += " AND (ISNULL(hierarchy.flags) OR hierarchy.flags & " + stringify(MSGFLAG_ASSOCIATED) + " = " + stringify(MSGFLAG_ASSOCIATED) + ") ";

	strQuery += CreateOrderQuery();
	
	return strQuery;
}


/**
 * IncrementalQueryCreator: Creates an incremental query. In other words only messages
 *                          that are new or have changed since the last check will be
 *                          returned (deleted is a change is this context).
 **/
class IncrementalQueryCreator : public CommonQueryCreator
{
public:
	IncrementalQueryCreator(ECDatabase *lpDatabase, unsigned int ulSyncId, unsigned int ulChangeId, const SOURCEKEY &sFolderSourceKey, unsigned int ulFlags);
	
private:
	std::string CreateBaseQuery();
	std::string CreateOrderQuery();
	
private:
	ECDatabase		*m_lpDatabase;
	unsigned int	m_ulSyncId;
	unsigned int	m_ulChangeId;
	const SOURCEKEY	&m_sFolderSourceKey;
	unsigned int	m_ulFlags;
};

IncrementalQueryCreator::IncrementalQueryCreator(ECDatabase *lpDatabase, unsigned int ulSyncId, unsigned int ulChangeId, const SOURCEKEY &sFolderSourceKey, unsigned int ulFlags)
	: CommonQueryCreator(ulFlags)
	, m_lpDatabase(lpDatabase)
	, m_ulSyncId(ulSyncId)
	, m_ulChangeId(ulChangeId)
	, m_sFolderSourceKey(sFolderSourceKey)
	, m_ulFlags(ulFlags)
{ }
	
std::string IncrementalQueryCreator::CreateBaseQuery()
{
	std::string strQuery;

	strQuery =  "SELECT changes.id, changes.sourcekey, changes.parentsourcekey, changes.change_type, changes.flags, NULL, changes.sourcesync "
				"FROM changes ";
	if ((m_ulFlags & (SYNC_ASSOCIATED | SYNC_NORMAL)) != (SYNC_ASSOCIATED | SYNC_NORMAL)) {
		strQuery +=	"LEFT JOIN indexedproperties ON indexedproperties.val_binary = changes.sourcekey AND indexedproperties.tag = " + stringify(PROP_ID(PR_SOURCE_KEY)) + " " +
					"LEFT JOIN hierarchy ON hierarchy.id = indexedproperties.hierarchyid ";
	}
	strQuery +=	"WHERE changes.id > " + stringify(m_ulChangeId) + 																	/* Get changes from change ID N onwards */
				"  AND changes.parentsourcekey = " + m_lpDatabase->EscapeBinary(m_sFolderSourceKey, m_sFolderSourceKey.size()) + 	/* Where change took place in Folder X */
				"  AND changes.change_type & " + stringify(ICS_MESSAGE) +															/* And change type is message */
				"  AND changes.sourcesync != " + stringify(m_ulSyncId);																/* And we didn't generate this change ourselves */

	if (m_ulFlags & SYNC_NO_DELETIONS) {
		strQuery += " AND changes.change_type & " + stringify(ICS_ACTION_MASK) + " != " + stringify(ICS_SOFT_DELETE) +
					" AND changes.change_type & " + stringify(ICS_ACTION_MASK) + " != " + stringify(ICS_HARD_DELETE);
	} else if (m_ulFlags & SYNC_NO_SOFT_DELETIONS) {
		strQuery += " AND changes.change_type & " + stringify(ICS_ACTION_MASK) + " != " + stringify(ICS_SOFT_DELETE);
	}

	if ((m_ulFlags & SYNC_READ_STATE) == 0)
		strQuery += " AND changes.change_type & " + stringify(ICS_ACTION_MASK) + " != " + stringify(ICS_FLAG);
		
	return strQuery;
}

std::string IncrementalQueryCreator::CreateOrderQuery()
{
	return " ORDER BY changes.id";
}


/**
 * FullQueryCreator: Create a query that will return all messages for a sync id. The
 *                   messages need to be processed afterwards to see what needs to be
 *                   send to the client.
 **/
class FullQueryCreator : public CommonQueryCreator
{
public:
	FullQueryCreator(ECDatabase *lpDatabase, const SOURCEKEY &sFolderSourceKey, unsigned int ulFlags, unsigned int ulFilteredSourceSync = 0);
	
private:
	std::string CreateBaseQuery();
	std::string CreateOrderQuery();
	
private:
	ECDatabase		*m_lpDatabase;
	const SOURCEKEY	&m_sFolderSourceKey;
	unsigned int	m_ulFilteredSourceSync;
};

FullQueryCreator::FullQueryCreator(ECDatabase *lpDatabase, const SOURCEKEY &sFolderSourceKey, unsigned int ulFlags, unsigned int ulFilteredSourceSync)
	: CommonQueryCreator(ulFlags)
	, m_lpDatabase(lpDatabase)
	, m_sFolderSourceKey(sFolderSourceKey)
	, m_ulFilteredSourceSync(ulFilteredSourceSync)
{ }
	
std::string FullQueryCreator::CreateBaseQuery()
{
	std::string strQuery;

	strQuery =  "SELECT 0 as id, sourcekey.val_binary as sourcekey, parentsourcekey.val_binary, " + stringify(ICS_MESSAGE_NEW) + ", NULL, hierarchy.flags, changes.sourcesync "
				"FROM hierarchy "
				"JOIN indexedproperties as sourcekey ON sourcekey.hierarchyid = hierarchy.id AND sourcekey.tag=" + stringify(PROP_ID(PR_SOURCE_KEY)) + " "
				"JOIN indexedproperties as parentsourcekey ON parentsourcekey.hierarchyid = hierarchy.parent AND parentsourcekey.tag=" + stringify(PROP_ID(PR_SOURCE_KEY)) +
				" LEFT JOIN changes on changes.sourcekey=sourcekey.val_binary AND changes.parentsourcekey=parentsourcekey.val_binary AND changes.change_type=" + stringify(ICS_MESSAGE_NEW) + " ";
				
	strQuery +=	"WHERE parentsourcekey.val_binary = " + m_lpDatabase->EscapeBinary(m_sFolderSourceKey, m_sFolderSourceKey.size()) +
				"  AND hierarchy.type=" + stringify(MAPI_MESSAGE) + " AND hierarchy.flags & 1024 = 0";
	
	if (m_ulFilteredSourceSync)
		strQuery += " AND (changes.sourcesync is NULL OR changes.sourcesync!=" + stringify(m_ulFilteredSourceSync) + ")";
		
	return strQuery;
}

std::string FullQueryCreator::CreateOrderQuery()
{
	return " ORDER BY hierarchy.id DESC";
}


/**
 * IMessageProcessor: Interface to the message processors.
 **/
class IMessageProcessor
{
public:
	virtual ~IMessageProcessor() {};
	virtual ECRESULT ProcessAccepted(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType, unsigned int *lpulFlags) = 0;
	virtual ECRESULT ProcessRejected(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType) = 0;
	virtual ECRESULT GetResidualMessages(LPMESSAGESET lpsetResiduals) = 0;
	virtual unsigned int GetMaxChangeId() const = 0;
};


/**
 * NonLegacyIncrementalProcessor: Processes accepted and rejected messages without the burden of tracking
 *                                legacy or checking for presence of messages.
 *                                This processor expects to be used in conjunction with the IncrementalQueryCreator,
 *                                which implies that all changes are genuin changes and no messages will be
 *                                rejected through a restriction.
 **/
class NonLegacyIncrementalProcessor : public IMessageProcessor
{
public:
	NonLegacyIncrementalProcessor(unsigned int ulChangeId);
	ECRESULT ProcessAccepted(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType, unsigned int *lpulFlags);
	ECRESULT ProcessRejected(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType);
	ECRESULT GetResidualMessages(LPMESSAGESET lpsetResiduals);
	unsigned int GetMaxChangeId() const;
	
private:
	unsigned int m_ulMaxChangeId;
};

NonLegacyIncrementalProcessor::NonLegacyIncrementalProcessor(unsigned int ulChangeId)
	: m_ulMaxChangeId(ulChangeId)
{ }

ECRESULT NonLegacyIncrementalProcessor::ProcessAccepted(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType, unsigned int *lpulFlags)
{
	// Since all changes are truly new changes, we'll just set the changetype to whatever we receive
	ASSERT(lpulChangeType);
	ASSERT(lpDBRow && lpDBRow[icsChangeType]);
	ASSERT(lpDBRow && lpDBRow[icsID]);
	
	*lpulChangeType = atoui(lpDBRow[icsChangeType]);
	*lpulFlags = lpDBRow[icsFlags] ? atoui(lpDBRow[icsFlags]) : 0;
	m_ulMaxChangeId = std::max(m_ulMaxChangeId, lpDBRow[icsID] ? atoui(lpDBRow[icsID]) : 0);
	return erSuccess;
}

ECRESULT NonLegacyIncrementalProcessor::ProcessRejected(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType)
{
	// Since no restriction can be applied when using this processor, we'll never get a reject.
	// We'll set the changetype to 0 anyway.
	ASSERT(FALSE);
	
	*lpulChangeType = 0;
	return erSuccess;
}

ECRESULT NonLegacyIncrementalProcessor::GetResidualMessages(LPMESSAGESET lpsetResiduals)
{
	// No legacy, so no residuals.
	return erSuccess;
}

unsigned int NonLegacyIncrementalProcessor::GetMaxChangeId() const
{
	return m_ulMaxChangeId;
}


/**
 * NonLegacyFullProcessor: Processes accepted and rejected messages without the burden of tracking
 *                         legacy, but allowing messages to be processed that were synced to the 
 *                         client previously. Since we don't have legacy, we assume all messages
 *                         up to the current changeId are on the client.
 **/
class NonLegacyFullProcessor : public IMessageProcessor
{
public:
	NonLegacyFullProcessor(unsigned int ulChangeId, unsigned int ulSyncId);
	ECRESULT ProcessAccepted(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType, unsigned int *lpulFlags);
	ECRESULT ProcessRejected(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType);
	ECRESULT GetResidualMessages(LPMESSAGESET lpsetResiduals);
	unsigned int GetMaxChangeId() const;
	
private:
	unsigned int m_ulChangeId;
	unsigned int m_ulSyncId;
	unsigned int m_ulMaxChangeId;
};

NonLegacyFullProcessor::NonLegacyFullProcessor(unsigned int ulChangeId, unsigned int ulSyncId)
	: m_ulChangeId(ulChangeId)
	, m_ulSyncId(ulSyncId)
	, m_ulMaxChangeId(ulChangeId)
{ }

ECRESULT NonLegacyFullProcessor::ProcessAccepted(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType, unsigned int *lpulFlags)
{
	// This processor will allways be used with the FullQueryGenerator, which means that the provided
	// changetype is allways ICS_MESSAGE_NEW. However, we do have the message flags so we can see if
	// a message is deleted.
	ASSERT(lpulChangeType);
	ASSERT(lpDBRow && lpDBRow[icsChangeType] && lpDBRow[icsMsgFlags]);
	ASSERT(atoui(lpDBRow[icsChangeType]) == ICS_MESSAGE_NEW);

	unsigned int ulChange = (lpDBRow[icsID] ? atoui(lpDBRow[icsID]) : 0);
	if (atoui(lpDBRow[icsMsgFlags]) & MSGFLAG_DELETED) {
		if (ulChange <= m_ulChangeId)	// Only delete if present remotely.
			*lpulChangeType = ICS_HARD_DELETE;
	} else {
		unsigned int ulSourceSync = (lpDBRow[icsSourceSync] ? atoui(lpDBRow[icsSourceSync]) : 0);
		// Only add if not present remotely and not created by the current client
		if (ulChange > m_ulChangeId && (ulSourceSync == 0 || ulSourceSync != m_ulSyncId))	
			*lpulChangeType = ICS_MESSAGE_NEW;
	}
	
	*lpulFlags = 0; // Flags are only useful for ICS_FLAG

	if (ulChange > m_ulMaxChangeId)
		m_ulMaxChangeId = ulChange;
		
	return erSuccess;
}

ECRESULT NonLegacyFullProcessor::ProcessRejected(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType)
{
	// We assume the client has all messages, so we need to send a delete for any non-matching message.
	ASSERT(lpulChangeType);

	unsigned int ulChange = (lpDBRow[icsID] ? atoui(lpDBRow[icsID]) : 0);
	if (ulChange <= m_ulChangeId)
		*lpulChangeType = ICS_HARD_DELETE;

	if (ulChange > m_ulMaxChangeId)
		m_ulMaxChangeId = ulChange;

	return erSuccess;
}

ECRESULT NonLegacyFullProcessor::GetResidualMessages(LPMESSAGESET lpsetResiduals)
{
	// No legacy, so no residuals.
	return erSuccess;
}

unsigned int NonLegacyFullProcessor::GetMaxChangeId() const
{
	return m_ulMaxChangeId;
}


/**
 * LegacyProcessor: Processes accepted and rejected messages while keeping track of legacy messages.
 **/
class LegacyProcessor : public IMessageProcessor
{
public:
	LegacyProcessor(unsigned int ulChangeId, unsigned int ulSyncId, const MESSAGESET &setMessages, unsigned int ulMaxFolderChange);
	ECRESULT ProcessAccepted(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType, unsigned int *lpulFlags);
	ECRESULT ProcessRejected(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType);
	ECRESULT GetResidualMessages(LPMESSAGESET lpsetResiduals);
	unsigned int GetMaxChangeId() const;
	
private:
	unsigned int	m_ulChangeId;
	unsigned int	m_ulSyncId;
	MESSAGESET		m_setMessages;
	unsigned int	m_ulMaxFolderChange;
	unsigned int	m_ulMaxChangeId;
};

LegacyProcessor::LegacyProcessor(unsigned int ulChangeId, unsigned int ulSyncId, const MESSAGESET &setMessages, unsigned int ulMaxFolderChange)
	: m_ulChangeId(ulChangeId)
	, m_ulSyncId(ulSyncId)
	, m_setMessages(setMessages)
	, m_ulMaxFolderChange(ulMaxFolderChange)
	, m_ulMaxChangeId(ulChangeId)
{ 
	/**
	 * We'll never get an empty set when a restriction was used in the previous run. However it is
	 * possible that the previous run returned an empty set. In that case setMessages contains exactly
	 * one entry with the sourcekey set to 0x00. If that's the case we'll just empty the set and
	 * continue as usual.
	 **/
	if (m_setMessages.size() == 1 && m_setMessages.find(SOURCEKEY(1, "\x00")) != m_setMessages.end())
		m_setMessages.clear();
}

ECRESULT LegacyProcessor::ProcessAccepted(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType, unsigned int *lpulFlags)
{
	unsigned int			ulMsgFlags = 0;
	MESSAGESET::iterator	iterMessage;

	// When we get here we're accepting a message that has matched the restriction (or if there was no
	// restriction). However since we have legacy, this messages might be present allready, in which
	// case we need to do nothing unless its deleted or changed since the last check.
	ASSERT(lpulChangeType);
	ASSERT(lpDBRow && lpDBRow[icsSourceKey] && lpDBRow[icsChangeType] && lpDBRow[icsMsgFlags]);
	ASSERT(atoui(lpDBRow[icsChangeType]) == ICS_MESSAGE_NEW);
	
	*lpulFlags = 0;
	ulMsgFlags = atoui(lpDBRow[icsMsgFlags]);
	iterMessage = m_setMessages.find(SOURCEKEY(lpDBLen[icsSourceKey], lpDBRow[icsSourceKey]));
	if (iterMessage == m_setMessages.end()) {
		// The message is not synced yet!
		unsigned int ulSourceSync = (lpDBRow[icsSourceSync] ? atoui(lpDBRow[icsSourceSync]) : 0);
		if (ulMsgFlags & MSGFLAG_DELETED || (ulSourceSync != 0 && ulSourceSync == m_ulSyncId))		// Deleted or created by current client
			*lpulChangeType = 0;	// Ignore
		else
			*lpulChangeType = ICS_MESSAGE_NEW;
	} else {
		// The message is synced!
		if (ulMsgFlags & MSGFLAG_DELETED)		// Deleted
			*lpulChangeType = ICS_HARD_DELETE;
		else if (iterMessage->second.ulChangeTypes) {		// Modified
			if(iterMessage->second.ulChangeTypes & (ICS_CHANGE_FLAG_NEW | ICS_CHANGE_FLAG_CHANGE))
				*lpulChangeType = ICS_MESSAGE_CHANGE;
			else if(iterMessage->second.ulChangeTypes & ICS_CHANGE_FLAG_FLAG) {
				*lpulChangeType = ICS_MESSAGE_FLAG;
				*lpulFlags = iterMessage->second.ulFlags;
			}
		}
		else
			*lpulChangeType = 0;	// Ignore
		
		m_setMessages.erase(iterMessage);
	}
	
	if (*lpulChangeType != 0)
		m_ulMaxChangeId = m_ulMaxFolderChange;
	
	return erSuccess;
}

ECRESULT LegacyProcessor::ProcessRejected(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType)
{
	unsigned int			ulMsgFlags = 0;
	MESSAGESET::iterator	iterMessage;

	// When we get here we're rejecting a message that has not-matched the restriction. 
	// However since we have legacy, this messages might not be present anyway, in which
	// case we need to do nothing.
	ASSERT(lpulChangeType);
	ASSERT(lpDBRow && lpDBRow[icsSourceKey] && lpDBRow[icsChangeType] && lpDBRow[icsMsgFlags]);
	ASSERT(atoui(lpDBRow[icsChangeType]) == ICS_MESSAGE_NEW);
	
	ulMsgFlags = atoui(lpDBRow[icsMsgFlags]);
	iterMessage = m_setMessages.find(SOURCEKEY(lpDBLen[icsSourceKey], lpDBRow[icsSourceKey]));
	if (iterMessage == m_setMessages.end()) {
		// The message is not synced yet!
		*lpulChangeType = 0;	// Ignore
	} else {
		// The message is synced!
		*lpulChangeType = ICS_HARD_DELETE;
		m_setMessages.erase(iterMessage);
	}
	
	if (*lpulChangeType != 0)
		m_ulMaxChangeId = m_ulMaxFolderChange;
	
	return erSuccess;
}

ECRESULT LegacyProcessor::GetResidualMessages(LPMESSAGESET lpsetResiduals)
{
	ASSERT(lpsetResiduals);
	std::copy(m_setMessages.begin(), m_setMessages.end(), std::inserter(*lpsetResiduals, lpsetResiduals->begin()));
	return erSuccess;
}

unsigned int LegacyProcessor::GetMaxChangeId() const
{
	return m_ulMaxChangeId;
}


/**
 * FirstSyncProcessor: Processes accepted and rejected messages for initial syncs. And because
 *                     it is the first sync we assume there are no messages on the device yet.
 **/
class FirstSyncProcessor : public IMessageProcessor
{
public:
	FirstSyncProcessor(unsigned int ulMaxFolderChange);
	ECRESULT ProcessAccepted(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType, unsigned int *lpulFlags);
	ECRESULT ProcessRejected(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType);
	ECRESULT GetResidualMessages(LPMESSAGESET lpsetResiduals);
	unsigned int GetMaxChangeId() const;
	
private:
	unsigned int m_ulMaxFolderChange;
};

FirstSyncProcessor::FirstSyncProcessor(unsigned int ulMaxFolderChange)
	: m_ulMaxFolderChange(ulMaxFolderChange)
{ }

ECRESULT FirstSyncProcessor::ProcessAccepted(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType, unsigned int *lpulFlags)
{
	// This processor will allways be used with the FullQueryGenerator, which means that the provided
	// changetype is allways ICS_MESSAGE_NEW. However, we do have the message flags so we can see if
	// a message is deleted.
	ASSERT(lpulChangeType);
	ASSERT(lpDBRow && lpDBRow[icsChangeType] && lpDBRow[icsMsgFlags]);
	ASSERT(atoui(lpDBRow[icsChangeType]) == ICS_MESSAGE_NEW);
	
	*lpulFlags = 0; // Only useful for ICS_FLAG type changes
	if (atoui(lpDBRow[icsMsgFlags]) & MSGFLAG_DELETED)
		*lpulChangeType = 0;	// Ignore
	else
		*lpulChangeType = ICS_MESSAGE_NEW;
		
	return erSuccess;
}

ECRESULT FirstSyncProcessor::ProcessRejected(DB_ROW lpDBRow, DB_LENGTHS lpDBLen, unsigned int *lpulChangeType)
{
	ASSERT(lpulChangeType);
	
	*lpulChangeType = 0;	// Ignore
	return erSuccess;
}

ECRESULT FirstSyncProcessor::GetResidualMessages(LPMESSAGESET lpsetResiduals)
{
	// No legacy, so no residuals.
	return erSuccess;
}

unsigned int FirstSyncProcessor::GetMaxChangeId() const
{
	return m_ulMaxFolderChange;
}


/**
 * ECGetContentChangesHelper definitions
 **/
ECRESULT ECGetContentChangesHelper::Create(struct soap *soap, ECSession *lpSession, ECDatabase *lpDatabase, const SOURCEKEY &sFolderSourceKey, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulFlags, struct restrictTable *lpsRestrict, ECGetContentChangesHelper **lppHelper)
{
	ECRESULT					er = erSuccess;
	ECGetContentChangesHelper	*lpHelper = new ECGetContentChangesHelper(soap, lpSession, lpDatabase, sFolderSourceKey, ulSyncId, ulChangeId, ulFlags, lpsRestrict);
	
	er = lpHelper->Init();
	if (er != erSuccess)
		goto exit;
		
	ASSERT(lppHelper);
	*lppHelper = lpHelper;
	lpHelper = NULL;
	
exit:
	if (lpHelper)
		delete lpHelper;
		
	return er;
}

ECGetContentChangesHelper::ECGetContentChangesHelper(struct soap *soap, ECSession *lpSession, ECDatabase *lpDatabase, const SOURCEKEY &sFolderSourceKey, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulFlags, struct restrictTable *lpsRestrict)
	: m_lpQueryCreator(NULL)
	, m_lpMsgProcessor(NULL)
	, m_soap(soap)
	, m_lpSession(lpSession)
	, m_lpDatabase(lpDatabase)
	, m_lpsRestrict(lpsRestrict)
	, m_lpChanges(NULL)
	, m_sFolderSourceKey(sFolderSourceKey)
	, m_ulSyncId(ulSyncId)
	, m_ulChangeId(ulChangeId)
	, m_ulChangeCnt(0)
	, m_ulMaxFolderChange(0)
	, m_ulFlags(ulFlags)
{ }

ECRESULT ECGetContentChangesHelper::Init()
{
	ECRESULT	er = erSuccess;
	DB_RESULT	lpDBResult = NULL;
	DB_ROW		lpDBRow;
	std::string	strQuery;

	ASSERT(m_lpDatabase);

	strQuery = "SELECT MAX(id) FROM changes WHERE parentsourcekey=" + m_lpDatabase->EscapeBinary(m_sFolderSourceKey, m_sFolderSourceKey.size());
	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;
		
	if ((lpDBRow = m_lpDatabase->FetchRow(lpDBResult)) == NULL || lpDBRow == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
	
	if (lpDBRow[0])
		m_ulMaxFolderChange = atoui(lpDBRow[0]);
	
	m_lpDatabase->FreeResult(lpDBResult);
	lpDBResult = NULL;

	// Here we setup the classes to delegate specific work to	
	if (m_ulChangeId == 0) {
		/*
		 * Initial sync
		 * We want all message that were not created by the current client (m_ulSyncId).
		 */
		m_lpQueryCreator = new FullQueryCreator(m_lpDatabase, m_sFolderSourceKey, m_ulFlags, m_ulSyncId);
		m_lpMsgProcessor = new FirstSyncProcessor(m_ulMaxFolderChange);
	} else {
		/*
		 * Incremental sync
		 * We first need to determine if the previous sync was with or without
		 * restriction and if a restriction is requested now.
		 */
		er = GetSyncedMessages(m_ulSyncId, m_ulChangeId, &m_setLegacyMessages);
		if (er != erSuccess)
			goto exit;
			
		if (m_setLegacyMessages.empty()) {
			/*
			 * Previous request was without restriction.
			 */
			if (m_lpsRestrict == NULL) {
				/*
				 * This request is also without a restriction. We can use an
				 * incremental query.
				 */
				m_lpQueryCreator = new IncrementalQueryCreator(m_lpDatabase, m_ulSyncId, m_ulChangeId, m_sFolderSourceKey, m_ulFlags);
				m_lpMsgProcessor = new NonLegacyIncrementalProcessor(m_ulChangeId);
			} else {
				/*
				 * This request is WITH a restriction. This means the client
				 * switched from using no restriction to using a restriction.
				 * Note: In practice this won't happen very often.
				 * We need to perform a full query to be able te decide which
				 * messages match the restriction and which don't.
				 * Since the previous request was without a restriction, we
				 * assume all messages that were present during the last sync
				 * are on the device.
				 * We do want to filter all messages that were created since
				 * the last sync and were created by the current client. The
				 * processor should do that because that's too complex for the
				 * query creator to do.
				 */
				m_lpQueryCreator = new FullQueryCreator(m_lpDatabase, m_sFolderSourceKey, m_ulFlags);
				m_lpMsgProcessor = new NonLegacyFullProcessor(m_ulChangeId, m_ulSyncId);
			}
		} else {
			/*
			 * The previous request was with a restriction, so we can't do an
			 * incremental sync in any case, as that will only get us add's and
			 * deletes for changes that happened after the last sync. But we
			 * can also have adds because certain older messages might not have
			 * matched the previous restriction, but do match the current (where
			 * no restriction is seen as a match-all restriction).
			 * We do want to filter all messages that were created since
			 * the last sync and were created by the current client. The
			 * processor should do that because that's too complex for the
			 * query creator to do.
			 */
			m_lpQueryCreator = new FullQueryCreator(m_lpDatabase, m_sFolderSourceKey, m_ulFlags);
			m_lpMsgProcessor = new LegacyProcessor(m_ulChangeId, m_ulSyncId, m_setLegacyMessages, m_ulMaxFolderChange);
		}
	}
		
exit:
	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);

	return er;
}
 
ECGetContentChangesHelper::~ECGetContentChangesHelper()
{
	if (m_lpQueryCreator)
		delete m_lpQueryCreator;
		
	if(m_lpMsgProcessor)
		delete m_lpMsgProcessor;
}
	
ECRESULT ECGetContentChangesHelper::QueryDatabase(DB_RESULT *lppDBResult)
{
	ECRESULT		er = erSuccess;
	DB_RESULT		lpDBResult = NULL;
	std::string		strQuery;
	unsigned int	ulChanges = 0;

	ASSERT(m_lpQueryCreator);
	strQuery = m_lpQueryCreator->CreateQuery();
	
	ASSERT(m_lpDatabase);
	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;
		
	ulChanges = m_lpDatabase->GetNumRows(lpDBResult) + m_setLegacyMessages.size();
	m_lpChanges = (icsChangesArray*)soap_malloc(m_soap, sizeof *m_lpChanges);
	m_lpChanges->__ptr = (icsChange*)soap_malloc(m_soap, sizeof *m_lpChanges->__ptr * ulChanges);
	m_lpChanges->__size = 0;
	
	ASSERT(lppDBResult);
	*lppDBResult = lpDBResult;
	
exit:
	return er;
}

ECRESULT ECGetContentChangesHelper::ProcessRow(DB_ROW lpDBRow, DB_LENGTHS lpDBLen)
{
	ECRESULT		er = erSuccess;
	bool			fMatch = true;
	unsigned int	ulChangeType = 0;
	unsigned int	ulFlags = 0;
	
	ASSERT(lpDBRow);
	ASSERT(lpDBLen);
	
	if (lpDBRow[icsSourceKey] == NULL || lpDBRow[icsParentSourceKey] == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if (m_lpsRestrict) {
		ASSERT(m_lpSession);
		er = MatchRestriction(SOURCEKEY(lpDBLen[icsSourceKey], lpDBRow[icsSourceKey]), m_lpsRestrict, &fMatch);
		if (er == ZARAFA_E_NOT_FOUND) {
			er = erSuccess;
			fMatch = false;
		} else if (er != erSuccess)
			goto exit;
	}
	
	ASSERT(m_lpMsgProcessor);
	if (fMatch) {
		er = m_lpMsgProcessor->ProcessAccepted(lpDBRow, lpDBLen, &ulChangeType, &ulFlags);
		if (m_lpsRestrict)
			m_setNewMessages.insert(MESSAGESET::value_type(SOURCEKEY(lpDBLen[icsSourceKey], lpDBRow[icsSourceKey]), SAuxMessageData(SOURCEKEY(lpDBLen[icsParentSourceKey], lpDBRow[icsParentSourceKey]), ICS_CHANGE_FLAG_NEW, ulFlags)));
	} else
		er = m_lpMsgProcessor->ProcessRejected(lpDBRow, lpDBLen, &ulChangeType);
	
	if (er != erSuccess)
		goto exit;
		
	g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "%s %s message, sourcekey=%s", 
		(ulChangeType == 0 ? "Ignoring" : ((ulChangeType & ICS_ACTION_MASK) == ICS_NEW ? "Creating" : 
										  ((ulChangeType & ICS_ACTION_MASK) == ICS_CHANGE ? "Changing" :
										  ((ulChangeType & ICS_ACTION_MASK) == ICS_FLAG ? "Changing" :
										  ((ulChangeType & ICS_ACTION_MASK) == ICS_SOFT_DELETE ? "Soft deleting" :
										  ((ulChangeType & ICS_ACTION_MASK) == ICS_HARD_DELETE ? "Hard deleting" : "Wondering about")))))),
		(fMatch ? "matching" : "non-matching"),
		bin2hex(lpDBLen[icsSourceKey], (unsigned char*)lpDBRow[icsSourceKey]).c_str());
		
	// If ulChangeType equals 0 we can skip this message
	if (ulChangeType == 0)
		goto exit;
		
	m_lpChanges->__ptr[m_ulChangeCnt].ulChangeId = lpDBRow[icsID] ? atoui(lpDBRow[icsID]) : 0;
	
	m_lpChanges->__ptr[m_ulChangeCnt].sSourceKey.__ptr = (unsigned char *)soap_malloc(m_soap, lpDBLen[icsSourceKey]);
	m_lpChanges->__ptr[m_ulChangeCnt].sSourceKey.__size = lpDBLen[icsSourceKey];
	memcpy(m_lpChanges->__ptr[m_ulChangeCnt].sSourceKey.__ptr, lpDBRow[icsSourceKey], lpDBLen[icsSourceKey]);

	m_lpChanges->__ptr[m_ulChangeCnt].sParentSourceKey.__ptr = (unsigned char *)soap_malloc(m_soap, lpDBLen[icsParentSourceKey]);
	m_lpChanges->__ptr[m_ulChangeCnt].sParentSourceKey.__size = lpDBLen[icsParentSourceKey];
	memcpy(m_lpChanges->__ptr[m_ulChangeCnt].sParentSourceKey.__ptr, lpDBRow[icsParentSourceKey], lpDBLen[icsParentSourceKey]);

	m_lpChanges->__ptr[m_ulChangeCnt].ulChangeType = ulChangeType;

	m_lpChanges->__ptr[m_ulChangeCnt].ulFlags = ulFlags;

	m_ulChangeCnt++;
	
exit:
	return er;
}

ECRESULT ECGetContentChangesHelper::ProcessResidualMessages()
{
	ECRESULT				er = erSuccess;
	MESSAGESET				setResiduals;
	MESSAGESET::iterator	iterMessage;

	ASSERT(m_lpMsgProcessor);
	er = m_lpMsgProcessor->GetResidualMessages(&setResiduals);
	if (er != erSuccess)
		goto exit;
	
	for (iterMessage = setResiduals.begin(); iterMessage != setResiduals.end(); ++iterMessage) {
		if (iterMessage->first.size() == 1 && memcmp(iterMessage->first, "\0", 1) == 0)
			continue;	// Skip empty restricted set marker,
	
		m_lpChanges->__ptr[m_ulChangeCnt].ulChangeId = 0;
		
		m_lpChanges->__ptr[m_ulChangeCnt].sSourceKey.__ptr = (unsigned char *)soap_malloc(m_soap, iterMessage->first.size());
		m_lpChanges->__ptr[m_ulChangeCnt].sSourceKey.__size = iterMessage->first.size();
		memcpy(m_lpChanges->__ptr[m_ulChangeCnt].sSourceKey.__ptr, iterMessage->first, iterMessage->first.size());

		m_lpChanges->__ptr[m_ulChangeCnt].sParentSourceKey.__ptr = (unsigned char *)soap_malloc(m_soap, iterMessage->second.sParentSourceKey.size());
		m_lpChanges->__ptr[m_ulChangeCnt].sParentSourceKey.__size = iterMessage->second.sParentSourceKey.size();
		memcpy(m_lpChanges->__ptr[m_ulChangeCnt].sParentSourceKey.__ptr, iterMessage->second.sParentSourceKey, iterMessage->second.sParentSourceKey.size());
		
		m_lpChanges->__ptr[m_ulChangeCnt].ulChangeType = ICS_HARD_DELETE;
		
		m_lpChanges->__ptr[m_ulChangeCnt].ulFlags = 0;
		
		m_ulChangeCnt++;
	}	
	
exit:
	return er;
}

ECRESULT ECGetContentChangesHelper::Finalize(unsigned int *lpulMaxChange, icsChangesArray **lppChanges)
{
	ECRESULT					er = erSuccess;
	std::string					strQuery;
	unsigned int				ulMaxChange = 0;
	unsigned int				ulNewChange = 0;
	MESSAGESET::const_iterator	iterMessage;
	DB_RESULT					lpDBResult	= NULL;
	DB_ROW						lpDBRow;
	
	ASSERT(lppChanges);
	ASSERT(lpulMaxChange);
	
	m_lpChanges->__size = m_ulChangeCnt;
	*lppChanges = m_lpChanges;
	
	ASSERT(m_lpMsgProcessor != NULL);
	ulMaxChange = m_lpMsgProcessor->GetMaxChangeId();
	
	// If there were no changes and this was not the initial sync, we only need to purge all too-new-syncedmessages.
	// If this is the initial sync, we might need to write the empty restricted set marker, so we can't
	// stop doing work here. Also, if we have converted from a non-restricted to a restricted set, we have to write
	// the new set of messages, even if there are no changes.
	if (m_ulChangeCnt == 0 && m_ulChangeId > 0 && !(m_setLegacyMessages.empty() && m_lpsRestrict) ) {
		ASSERT(ulMaxChange >= m_ulChangeId);
		*lpulMaxChange = ulMaxChange;
		
		// Delete all entries that have a changeid that are greater to the new change id.
		strQuery = "DELETE FROM syncedmessages WHERE sync_id=" + stringify(m_ulSyncId) + " AND change_id>" + stringify(ulMaxChange);
		er = m_lpDatabase->DoDelete(strQuery);
		goto exit;
	}
	
	if (ulMaxChange == m_ulChangeId) {
		/**
		 * If we get here, we had at least one change but the max changeid for the server is the
		 * same as the changeid in the request. This means the change was caused by either a modified
		 * restriction.
		 * When this happens a new changeid must be generated in order to return a unique state to the
		 * client that can be used in subsequent requests. We do this by creating a dummy change in the
		 * changes table.
		 */
		// Bump the changeid
		strQuery = "REPLACE INTO changes (sourcekey,parentsourcekey,sourcesync) VALUES (0, " + m_lpDatabase->EscapeBinary(m_sFolderSourceKey, m_sFolderSourceKey.size()) + "," + stringify(m_ulSyncId) + ")";
		er = m_lpDatabase->DoInsert(strQuery, &ulNewChange);
		if (er != erSuccess)
			goto exit;
			
		ASSERT(ulNewChange > ulMaxChange);
		ulMaxChange = ulNewChange;
		ASSERT(ulMaxChange > m_ulChangeId);
	}
	
	/**
	 * If a restriction is set, but the set of synced messages is empty we'll make a placeholder entry
	 * so we can differentiate between having all messages and having no messages on the client.
	 *
	 * It's actually backwards to put in a placeholder when we have no message and put in nothing when
	 * we have all messages, but having all message (because no restriction was set) never stores anything
	 * in the syncedmessages table, so this scheme is compatible. On top of that, having no messages synced
	 * at all is rare, having all messages isn't.
	 **/
	if (m_lpsRestrict && m_setNewMessages.empty())
		m_setNewMessages.insert(MESSAGESET::value_type(SOURCEKEY(1, "\x00"), SAuxMessageData(m_sFolderSourceKey, 0, 0)));


	if (!m_setNewMessages.empty()) {
		std::set<unsigned int> setChangeIds;
		
		strQuery = "SELECT DISTINCT change_id FROM syncedmessages WHERE sync_id=" + stringify(m_ulSyncId);
		er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
		if (er != erSuccess)
			goto exit;

		while ((lpDBRow = m_lpDatabase->FetchRow(lpDBResult))) {
			if (lpDBRow == NULL || lpDBRow[0] == NULL) {
				er = ZARAFA_E_DATABASE_ERROR; // this should never happen
				goto exit;
			}
			setChangeIds.insert(atoui(lpDBRow[0]));
		}

		m_lpDatabase->FreeResult(lpDBResult);
		lpDBResult = NULL;

		if (!setChangeIds.empty()) {
			std::set<unsigned int> setDeleteIds;
			std::set<unsigned int>::iterator iter;
			
			/* Remove obsolete states
			 *
			 * rules:
			 * 1) Remove any states that are newer than the state that was requested
			 *    We do this since if the client requests state X, it can never request state X+1
			 *    later unless X+1 is the state that was generated from this request. We can therefore
			 *    remove any state > X at this point, since state X+1 will be inserted later
			 * 2) Remove any states that are older than the state that was requested minus two
			 *    We cannot remove state X since the client may re-request this state (eg if the export
			 *    failed due to network error, or if the export is interrupted before ending). We also
			 *    do not remove state X-1 and X-2 so that we support some sort of rollback of the client.
			 *    This may happen if the client is restored to an old state. In practice removing X-1 and
			 *    X-2 will probably not cause any real problems though, and the number 2 is pretty
			 *    arbitrary.
			 */

			// Delete any message state that is higher than the changeset that changes were
			// requested from (rule 1)
			iter = setChangeIds.upper_bound(m_ulChangeId);
			if (iter != setChangeIds.end()) {
				std::copy(iter, setChangeIds.end(), std::inserter(setDeleteIds, setDeleteIds.begin()));
			}

			// Find all message states that are equal or lower than the changeset that changes were requested from
			iter = setChangeIds.lower_bound(m_ulChangeId);
			// Reverse up to two message states (less if they do not exist)
			for (int i = 0; iter != setChangeIds.begin() && i < 2; ++i, --iter);
			// Remove message states that are older than X-2 (rule 2)
			std::copy(setChangeIds.begin(), iter, std::inserter(setDeleteIds, setDeleteIds.begin()));

			if (!setDeleteIds.empty()) {
				ASSERT(setChangeIds.size() - setDeleteIds.size() <= 2);
				
				strQuery = "DELETE FROM syncedmessages WHERE sync_id=" + stringify(m_ulSyncId) + " AND change_id IN (";
				for (iter = setDeleteIds.begin(); iter != setDeleteIds.end(); ++iter) {
					strQuery.append(stringify(*iter));
					strQuery.append(1, ',');
				}
				strQuery.resize(strQuery.size() - 1);	// Remove trailing ','
				strQuery.append(1, ')');

				er = m_lpDatabase->DoDelete(strQuery);
				if (er != erSuccess)
					goto exit;
			}
		}
	
		// Create the insert query
		strQuery = "INSERT INTO syncedmessages (sync_id,change_id,sourcekey,parentsourcekey) VALUES ";
		for (iterMessage = m_setNewMessages.begin(); iterMessage != m_setNewMessages.end(); ++iterMessage)
			strQuery += "(" + stringify(m_ulSyncId) + "," + stringify(ulMaxChange) + "," + 
						m_lpDatabase->EscapeBinary(iterMessage->first, iterMessage->first.size()) + "," + 
						m_lpDatabase->EscapeBinary(iterMessage->second.sParentSourceKey, iterMessage->second.sParentSourceKey.size()) + "),";

		strQuery.resize(strQuery.size() - 1);
		er = m_lpDatabase->DoInsert(strQuery);
		if (er != erSuccess)
			goto exit;
	}
	
	*lpulMaxChange = ulMaxChange;

exit:
	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);
		
	return er;
}

ECRESULT ECGetContentChangesHelper::MatchRestriction(const SOURCEKEY &sSourceKey, struct restrictTable *lpsRestrict, bool *lpfMatch)
{
    ECRESULT er = erSuccess;
    unsigned int ulObjId = 0;
    ECObjectTableList lstRows;
    ECObjectTableList::value_type sRow;
    ECODStore sODStore;
    bool fMatch = false;
    struct propTagArray *lpPropTags = NULL;
    struct rowSet *lpRowSet = NULL;

    memset(&sODStore, 0, sizeof(sODStore));

	// Add change key and predecessor change list
	er = g_lpSessionManager->GetCacheManager()->GetObjectFromProp(PROP_ID(PR_SOURCE_KEY), sSourceKey.size(), sSourceKey, &ulObjId);
    if(er != erSuccess)
        goto exit;

    er = g_lpSessionManager->GetCacheManager()->GetObject(ulObjId, NULL, NULL, NULL, &sODStore.ulObjType);
    if(er != erSuccess)
    	goto exit;

	sODStore.lpGuid = new GUID;

	er = g_lpSessionManager->GetCacheManager()->GetStore(ulObjId, &sODStore.ulStoreId, sODStore.lpGuid);
	if(er != erSuccess)
		goto exit;

	er = ECGenericObjectTable::GetRestrictPropTags(lpsRestrict, NULL, &lpPropTags);
	if(er != erSuccess)
		goto exit;

	sRow.ulObjId = ulObjId;
	sRow.ulOrderId = 0;

	lstRows.push_back(sRow);

	ASSERT(m_lpSession);
	// NULL for soap, not m_soap. We'll free this ourselves
	er = ECStoreObjectTable::QueryRowData(NULL, NULL, m_lpSession, &lstRows, lpPropTags, &sODStore, &lpRowSet, false, false);
	if(er != erSuccess)
		goto exit;

	if(lpRowSet->__size != 1) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	// @todo: Get a proper locale for the case insensitive comparisons inside MatchRowRestrict
	er = ECGenericObjectTable::MatchRowRestrict(g_lpSessionManager->GetCacheManager(), &lpRowSet->__ptr[0], lpsRestrict, NULL, createLocaleFromName(""), &fMatch);
	if(er != erSuccess)
		goto exit;

    *lpfMatch = fMatch;

exit:
	if(sODStore.lpGuid)
		delete sODStore.lpGuid;
    if(lpPropTags)
        FreePropTagArray(lpPropTags);

    if(lpRowSet)
        FreeRowSet(lpRowSet, true);

    return er;
}

ECRESULT ECGetContentChangesHelper::GetSyncedMessages(unsigned int ulSyncId, unsigned int ulChangeId, LPMESSAGESET lpsetMessages)
{
	typedef std::pair<MESSAGESET::iterator, bool> insert_result;

	ECRESULT		er = erSuccess;
	std::string		strSubQuery;
	std::string		strQuery;
	DB_RESULT		lpDBResult	= NULL;
	DB_ROW			lpDBRow;
	DB_LENGTHS		lpDBLen;
	
	strQuery = 
		"SELECT m.sourcekey, m.parentsourcekey, c.change_type, c.flags "
		"FROM syncedmessages as m "
			"LEFT JOIN changes as c "
				"ON m.sourcekey=c.sourcekey AND m.parentsourcekey=c.parentsourcekey AND c.id > " + stringify(ulChangeId) + " AND c.sourcesync != " + stringify(ulSyncId) + " "
		"WHERE sync_id=" + stringify(ulSyncId) + " AND change_id=" + stringify(ulChangeId);
	ASSERT(m_lpDatabase);
	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;
		
	while ((lpDBRow = m_lpDatabase->FetchRow(lpDBResult))) {
		insert_result	iResult;

		lpDBLen = m_lpDatabase->FetchRowLengths(lpDBResult);
		if (lpDBRow == NULL || lpDBLen == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL) {
			er = ZARAFA_E_DATABASE_ERROR; // this should never happen
			goto exit;
		}

		iResult = lpsetMessages->insert(MESSAGESET::value_type(SOURCEKEY(lpDBLen[0], lpDBRow[0]), SAuxMessageData(SOURCEKEY(lpDBLen[1], lpDBRow[1]), 1 << (lpDBRow[2]?atoui(lpDBRow[2]):0), lpDBRow[3]?atoui(lpDBRow[3]):0)));
		if (iResult.second == false && lpDBRow[2]) {
			iResult.first->second.ulChangeTypes |= 1 << (lpDBRow[2]?atoui(lpDBRow[2]):0);
		}
	}
	
exit:
	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);

	return er;
}

bool ECGetContentChangesHelper::CompareMessageEntry(const MESSAGESET::value_type &lhs, const MESSAGESET::value_type &rhs)
{
	return lhs.first == rhs.first;
}

bool ECGetContentChangesHelper::MessageSetsDiffer() const
{
	if (m_setLegacyMessages.size() != m_setNewMessages.size())
		return true;
	
	return !std::equal(m_setLegacyMessages.begin(), m_setLegacyMessages.end(), m_setNewMessages.begin(), &CompareMessageEntry);
}
