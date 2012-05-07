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

#ifndef IECCHANGEADVISOR_H
#define IECCHANGEADVISOR_H

#include "IECChangeAdviseSink.h"

/**
 * IECChangeAdvisor: Interface for registering change notifications on folders.
 */
class IECChangeAdvisor : public IUnknown{
public:
	virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError) = 0;

	/**
	 * Configure the change change advisor based on a stream.
	 *
	 * @param[in]	lpStream
	 *					The stream containing the state of the state of the change advisor. This stream
	 *					is obtained by a call to UpdateState.
	 *					If lpStream is NULL, an empty state is assumed.
	 * @param[in]	lpGuid
	 *					Unused. Set to NULL.
	 * @param[in]	lpAdviseSink
	 *					The advise sink that will receive the change notifications.
	 * @param[in]	ulFlags
	 *					- SYNC_CATCHUP	Update the internal state, but don't perform any operations
	 *									on the server.
	 * @return HRESULT
	 */
	virtual HRESULT __stdcall Config(LPSTREAM lpStream, LPGUID lpGUID, LPECCHANGEADVISESINK lpAdviseSink, ULONG ulFlags) = 0;

	/**
	 * Store the current internal state in the stream pointed to by lpStream.
	 *
	 * @param[in]	lpStream
	 *					The stream in which the current state will be stored.
	 * @return HRESULT
	 */
	virtual HRESULT __stdcall UpdateState(LPSTREAM lpStream) = 0;

	/**
	 * Register one or more folders for change notifications through this change advisor.
	 *
	 * @param[in]	lpEntryList
	 *					A list of keys specifying the folders to monitor. A key is an 8 byte
	 *					block of data. The first 4 bytes specify the sync id of the folder to
	 *					monitor. The second 4 bytes apecify the change id of the folder to monitor.
	 *					Use the SSyncState structure to easily create and access this data.
	 * @return HRESULT
	 */
	virtual HRESULT __stdcall AddKeys(LPENTRYLIST lpEntryList) = 0;

	/**
	 * Unregister one or more folder for change notifications.
	 *
	 * @param[in]	lpEntryList
	 *					A list of keys specifying the folders to monitor. See AddKeys for
	 *					information about the key format.
	 * @return HRESULT
	 */
	virtual HRESULT __stdcall RemoveKeys(LPENTRYLIST lpEntryList) = 0;

	/**
	 * Check if the change advisor is monitoring the folder specified by a particular sync id.
	 *
	 * @param[in]	ulSyncId
	 *					The sync id of the folder.
	 * @return hrSuccess if the folder is being monitored.
	 */
    virtual HRESULT __stdcall IsMonitoringSyncId(ULONG ulSyncId) = 0;

	/**
	 * Update the changeid for a particular syncid.
	 *
	 * This is used to update the state of the changeadvisor. This is also vital information
	 * when a reconnect is required.
	 *
	 * @param[in]	ulSyncId
	 *					The syncid for which to update the changeid.
	 * @param[in]	ulChangeId
	 *					The new changeid for the specified syncid.
	 *
	 * @return HRESULT
	 */
	virtual HRESULT __stdcall UpdateSyncState(ULONG ulSyncId, ULONG ulChangeId) = 0;
};

typedef IECChangeAdvisor* LPECCHANGEADVISOR;

#endif
