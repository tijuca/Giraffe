/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
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

#ifndef ZARAFACODE_H
#define ZARAFACODE_H

#include <zarafa/platform.h>

#define MAKE_ZARAFA_SCODE(sev,code) ( (((unsigned int)(sev)<<31) | ((unsigned int)(code))) )
#define MAKE_ZARAFA_E( err ) (MAKE_ZARAFA_SCODE( 1, err ))
#define MAKE_ZARAFA_W( warn ) (MAKE_ZARAFA_SCODE( 1, warn )) // This macro is broken, should be 0

#define ZARAFA_E_NONE					0
#define ZARAFA_E_UNKNOWN				MAKE_ZARAFA_E( 1 )
#define ZARAFA_E_NOT_FOUND				MAKE_ZARAFA_E( 2 )
#define ZARAFA_E_NO_ACCESS				MAKE_ZARAFA_E( 3 )
#define ZARAFA_E_NETWORK_ERROR			MAKE_ZARAFA_E( 4 )
#define ZARAFA_E_SERVER_NOT_RESPONDING	MAKE_ZARAFA_E( 5 )
#define ZARAFA_E_INVALID_TYPE			MAKE_ZARAFA_E( 6 )
#define ZARAFA_E_DATABASE_ERROR			MAKE_ZARAFA_E( 7 )
#define ZARAFA_E_COLLISION				MAKE_ZARAFA_E( 8 )
#define ZARAFA_E_LOGON_FAILED			MAKE_ZARAFA_E( 9 )
#define ZARAFA_E_HAS_MESSAGES			MAKE_ZARAFA_E( 10 )
#define ZARAFA_E_HAS_FOLDERS			MAKE_ZARAFA_E( 11 )
#define ZARAFA_E_HAS_RECIPIENTS			MAKE_ZARAFA_E( 12 )
#define ZARAFA_E_HAS_ATTACHMENTS		MAKE_ZARAFA_E( 13 )
#define ZARAFA_E_NOT_ENOUGH_MEMORY		MAKE_ZARAFA_E( 14 )
#define ZARAFA_E_TOO_COMPLEX			MAKE_ZARAFA_E( 15 )
#define ZARAFA_E_END_OF_SESSION			MAKE_ZARAFA_E( 16 )
#define ZARAFA_W_CALL_KEEPALIVE			MAKE_ZARAFA_W( 17 )
#define ZARAFA_E_UNABLE_TO_ABORT		MAKE_ZARAFA_E( 18 )
#define ZARAFA_E_NOT_IN_QUEUE			MAKE_ZARAFA_E( 19 )
#define ZARAFA_E_INVALID_PARAMETER		MAKE_ZARAFA_E( 20 )
#define ZARAFA_W_PARTIAL_COMPLETION		MAKE_ZARAFA_W( 21 )
#define ZARAFA_E_INVALID_ENTRYID		MAKE_ZARAFA_E( 22 )
#define ZARAFA_E_BAD_VALUE				MAKE_ZARAFA_E( 23 )
#define ZARAFA_E_NO_SUPPORT				MAKE_ZARAFA_E( 24 )
#define ZARAFA_E_TOO_BIG				MAKE_ZARAFA_E( 25 )
#define ZARAFA_W_POSITION_CHANGED		MAKE_ZARAFA_W( 26 )
#define ZARAFA_E_FOLDER_CYCLE			MAKE_ZARAFA_E( 27 )
#define ZARAFA_E_STORE_FULL				MAKE_ZARAFA_E( 28 )
#define ZARAFA_E_PLUGIN_ERROR			MAKE_ZARAFA_E( 29 )
#define ZARAFA_E_UNKNOWN_OBJECT			MAKE_ZARAFA_E( 30 )
#define ZARAFA_E_NOT_IMPLEMENTED		MAKE_ZARAFA_E( 31 )
#define ZARAFA_E_DATABASE_NOT_FOUND		MAKE_ZARAFA_E( 32 )
#define ZARAFA_E_INVALID_VERSION		MAKE_ZARAFA_E( 33 )
#define ZARAFA_E_UNKNOWN_DATABASE		MAKE_ZARAFA_E( 34 )
#define ZARAFA_E_NOT_INITIALIZED		MAKE_ZARAFA_E( 35 )
#define ZARAFA_E_CALL_FAILED			MAKE_ZARAFA_E( 36 )
#define ZARAFA_E_SSO_CONTINUE			MAKE_ZARAFA_E( 37 )
#define ZARAFA_E_TIMEOUT				MAKE_ZARAFA_E( 38 )
#define ZARAFA_E_INVALID_BOOKMARK		MAKE_ZARAFA_E( 39 )
#define ZARAFA_E_UNABLE_TO_COMPLETE		MAKE_ZARAFA_E( 40 )
#define ZARAFA_E_UNKNOWN_INSTANCE_ID	MAKE_ZARAFA_E( 41 )
#define ZARAFA_E_IGNORE_ME				MAKE_ZARAFA_E( 42 )
#define ZARAFA_E_BUSY					MAKE_ZARAFA_E( 43 )
#define ZARAFA_E_OBJECT_DELETED			MAKE_ZARAFA_E( 44 )
#define ZARAFA_E_USER_CANCEL			MAKE_ZARAFA_E( 45 )
#define ZARAFA_E_UNKNOWN_FLAGS			MAKE_ZARAFA_E( 46 )
#define ZARAFA_E_SUBMITTED				MAKE_ZARAFA_E( 47 )

#define erSuccess	ZARAFA_E_NONE

typedef unsigned int ECRESULT;

// FIXME which of these numbers are mapped 1-to-1 with actual
// MAPI values ? Move all fixed values to ECMAPI.h

#define ZARAFA_TAG_DISPLAY_NAME		0x3001

// These must match MAPI types !
#define ZARAFA_OBJTYPE_FOLDER			3
#define ZARAFA_OBJTYPE_MESSAGE			5

// Sessions
typedef uint64_t ECSESSIONID;
typedef uint64_t ECSESSIONGROUPID;

#define EC_NOTIFICATION_CHECK_FREQUENTY		(1000*2)
#define EC_NOTIFICATION_CLOSE_TIMEOUT		(1000)
#define EC_SESSION_TIMEOUT					(60*5)		// In seconds
#define EC_SESSION_KEEPALIVE_TIME			(60)	// In seconds
#define EC_SESSION_TIMEOUT_CHECK			(1000*60*5)	// In microsecconds

/* the same definetions as MAPI */
#define EC_ACCESS_MODIFY                ((unsigned int) 0x00000001)
#define EC_ACCESS_READ					((unsigned int) 0x00000002)
#define EC_ACCESS_DELETE				((unsigned int) 0x00000004)
#define EC_ACCESS_CREATE_HIERARCHY		((unsigned int) 0x00000008)
#define EC_ACCESS_CREATE_CONTENTS		((unsigned int) 0x00000010)
#define EC_ACCESS_CREATE_ASSOCIATED		((unsigned int) 0x00000020)

#define EC_ACCESS_OWNER					((unsigned int) 0x0000003F)

#define ecSecurityRead			1
#define ecSecurityCreate		2
#define ecSecurityEdit			3
#define ecSecurityDelete		4
#define ecSecurityCreateFolder	5
#define ecSecurityFolderVisible	6
#define ecSecurityFolderAccess	7
#define ecSecurityOwner			8
#define ecSecurityAdmin			9

// Property under which to store the search criteria for search folders
#define PR_EC_SEARCHCRIT	PROP_TAG(PT_STRING8, 0x6706)
#define PR_EC_SUGGESTION	PROP_TAG(PT_UNICODE, 0x6707)

// create 200 directories for non-database attachments
#define ATTACH_PATHDEPTH_LEVEL1 10
#define ATTACH_PATHDEPTH_LEVEL2 20

typedef enum { CONNECTION_TYPE_TCP, CONNECTION_TYPE_SSL, CONNECTION_TYPE_NAMED_PIPE, CONNECTION_TYPE_NAMED_PIPE_PRIORITY } CONNECTION_TYPE;

//Functions
HRESULT ZarafaErrorToMAPIError(ECRESULT ecResult, HRESULT hrDefault = 0x80070005 /*MAPI_E_NO_ACCESS*/);

#endif // ZARAFACODE_H
