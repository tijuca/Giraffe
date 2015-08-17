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

#ifndef ECICS_H
#define ECICS_H

#include <string>
#include <list>
#include <mapidefs.h>	// SBinary

typedef ULONG syncid_t;
typedef ULONG changeid_t;
typedef ULONG connection_t;

// Client-side type definitions for ICS
typedef struct {
    unsigned int ulChangeId;
    SBinary sSourceKey;
    SBinary sParentSourceKey;
    SBinary sMovedFromSourceKey;
    unsigned int ulChangeType;
    unsigned int ulFlags;
} ICSCHANGE;

/**
 * SSyncState: This structure uniquely defines a sync state.
 */
typedef struct {
	syncid_t	ulSyncId;		//!< The sync id uniquely specifies a folder in a syncronization context.
	changeid_t	ulChangeId;		//!< The change id specifies the syncronization state for a specific sync id.
} SSyncState;

/**
 * SSyncAdvise: This structure combines a sync state with a notification connection.
 */
typedef struct {
	SSyncState		sSyncState;	//!< The sync state that's for which a change notifications have been registered.
	connection_t	ulConnection;		//!< The connection on which notifications for the folder specified by the sync state are received.
} SSyncAdvise;

/**
 * Extract the sync id from binary data that is known to be a valid sync state.
 */
#define SYNCID(lpb) (((SSyncState*)(lpb))->ulSyncId)

/**
 * Extract the change id from binary data that is known to be a valid sync state.
 */
#define CHANGEID(lpb) (((SSyncState*)(lpb))->ulChangeId)

typedef std::list<syncid_t> ECLISTSYNCID;
typedef std::list<SSyncState> ECLISTSYNCSTATE;
typedef std::list<SSyncAdvise> ECLISTSYNCADVISE;

#endif
