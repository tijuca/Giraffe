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

#include <zarafa/platform.h>
#include <mapicode.h>
#include <zarafa/ZarafaCode.h>
#include <zarafa/mapiext.h>

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


/*
 * Some helper functions to convert the SOAP-style objects
 * to MAPI-style structs and vice-versa
 */
HRESULT ZarafaErrorToMAPIError(ECRESULT ecResult, HRESULT hrDefault)
{
	HRESULT hr = S_OK;

	switch(ecResult) {
		case ZARAFA_E_NONE:
			hr = S_OK;
			break;
		/*case ZARAFA_E_UNKNOWN://FIXME: No MAPI error?
			hr = MAPI_E_UNKNOWN;
			break;*/
		case ZARAFA_E_NOT_FOUND:
			hr = MAPI_E_NOT_FOUND;
			break;
		case ZARAFA_E_NO_ACCESS:
			hr = MAPI_E_NO_ACCESS;
			break;
		case ZARAFA_E_NETWORK_ERROR:
			hr = MAPI_E_NETWORK_ERROR;
			break;
		case ZARAFA_E_SERVER_NOT_RESPONDING:
			hr = MAPI_E_NETWORK_ERROR;
			break;
		case ZARAFA_E_INVALID_TYPE:
			hr = MAPI_E_INVALID_TYPE;
			break;
		case ZARAFA_E_DATABASE_ERROR:
			hr = MAPI_E_DISK_ERROR;
			break;
		case ZARAFA_E_COLLISION:
			hr = MAPI_E_COLLISION;
			break;
		case ZARAFA_E_LOGON_FAILED:
			hr = MAPI_E_LOGON_FAILED;
			break;
		case ZARAFA_E_HAS_MESSAGES:
			hr = MAPI_E_HAS_MESSAGES;
			break;
		case ZARAFA_E_HAS_FOLDERS:
			hr = MAPI_E_HAS_FOLDERS;
			break;
/*		case ZARAFA_E_HAS_RECIPIENTS://FIXME: No MAPI error?
			hr = ZARAFA_E_HAS_RECIPIENTS; 
			break;*/
/*		case ZARAFA_E_HAS_ATTACHMENTS://FIXME: No MAPI error?
			hr = ZARAFA_E_HAS_ATTACHMENTS;
			break;*/
		case ZARAFA_E_NOT_ENOUGH_MEMORY:
			hr = MAPI_E_NOT_ENOUGH_MEMORY;
			break;
		case ZARAFA_E_TOO_COMPLEX:
			hr = MAPI_E_TOO_COMPLEX;
			break;
		case ZARAFA_E_END_OF_SESSION:
			hr = MAPI_E_END_OF_SESSION;
			break;
		case ZARAFA_E_UNABLE_TO_ABORT:
			hr = MAPI_E_UNABLE_TO_ABORT;
			break;
		case ZARAFA_W_CALL_KEEPALIVE: //Internal information
			hr = ZARAFA_W_CALL_KEEPALIVE;
			break;
		case ZARAFA_E_NOT_IN_QUEUE:
			hr = MAPI_E_NOT_IN_QUEUE;
			break;
		case ZARAFA_E_INVALID_PARAMETER:
			hr = MAPI_E_INVALID_PARAMETER;
			break;
		case ZARAFA_W_PARTIAL_COMPLETION:
			hr = MAPI_W_PARTIAL_COMPLETION;
			break;
		case ZARAFA_E_INVALID_ENTRYID:
			hr = MAPI_E_INVALID_ENTRYID;
			break;
		case ZARAFA_E_NO_SUPPORT:
		case ZARAFA_E_NOT_IMPLEMENTED:
			hr = MAPI_E_NO_SUPPORT;
			break;
		case ZARAFA_E_TOO_BIG:
			hr = MAPI_E_TOO_BIG;
			break;
		case ZARAFA_W_POSITION_CHANGED:
			hr = MAPI_W_POSITION_CHANGED;
			break;
		case ZARAFA_E_FOLDER_CYCLE:
			hr = MAPI_E_FOLDER_CYCLE;
			break;
		case ZARAFA_E_STORE_FULL:
			hr = MAPI_E_STORE_FULL;
			break;
		case ZARAFA_E_NOT_INITIALIZED:
			hr = MAPI_E_NOT_INITIALIZED;
			break;
		case ZARAFA_E_CALL_FAILED:
			hr = MAPI_E_CALL_FAILED;
			break;
		case ZARAFA_E_TIMEOUT:
			hr = MAPI_E_TIMEOUT;
			break;
		case ZARAFA_E_INVALID_BOOKMARK:
			hr = MAPI_E_INVALID_BOOKMARK;
			break;
		case ZARAFA_E_UNABLE_TO_COMPLETE:
			hr = MAPI_E_UNABLE_TO_COMPLETE;
			break;
		case ZARAFA_E_INVALID_VERSION:
			hr = MAPI_E_VERSION;
			break;
		case ZARAFA_E_OBJECT_DELETED:
			hr = MAPI_E_OBJECT_DELETED;
			break;
		case ZARAFA_E_USER_CANCEL:
			hr = MAPI_E_USER_CANCEL;
			break;
		case ZARAFA_E_UNKNOWN_FLAGS:
			hr = MAPI_E_UNKNOWN_FLAGS;
			break;
		case ZARAFA_E_SUBMITTED:
			hr = MAPI_E_SUBMITTED;
			break;
		default:
			hr = hrDefault;
			break;
	}
	
	return hr;
}
