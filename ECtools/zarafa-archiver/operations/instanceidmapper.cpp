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

#include "platform.h"
#include "instanceidmapper.h"
#include "archiver.h"
#include "ECDatabaseMySQL.h"
#include "stringutil.h"

namespace za { namespace operations {

HRESULT InstanceIdMapper::Create(ECLogger *lpLogger, ECConfig *lpConfig, InstanceIdMapperPtr *lpptrMapper)
{
	HRESULT hr = hrSuccess;
	InstanceIdMapper *lpMapper = NULL;
	ECConfig *lpLocalConfig = lpConfig;

	// Get config if required.
	if (lpLocalConfig == NULL) {
		lpLocalConfig = ECConfig::Create(Archiver::GetConfigDefaults());
		if (!lpLocalConfig->LoadSettings(Archiver::GetConfigPath())) {
			// Just log warnings and errors and continue with default.
			LogConfigErrors(lpLocalConfig, lpLogger);
		}
	}

	try {
		lpMapper = new InstanceIdMapper(lpLogger);
	} catch (const std::bad_alloc &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	hr = lpMapper->Init(lpLocalConfig);
	if (hr != hrSuccess)
		goto exit;
		
	lpptrMapper->reset(lpMapper, boost::checked_deleter<InstanceIdMapper>());
	lpMapper = NULL;

exit:
	delete lpMapper;
	if (lpConfig == NULL) {
		ASSERT(lpLocalConfig != NULL);
		delete lpLocalConfig;
	}

	return hr;
}

InstanceIdMapper::InstanceIdMapper(ECLogger *lpLogger)
: m_ptrDatabase(new ECDatabaseMySQL(lpLogger))
{ }

HRESULT InstanceIdMapper::Init(ECConfig *lpConfig)
{
	ECRESULT er = erSuccess;
	
	er = m_ptrDatabase->Connect(lpConfig);
	if (er == ZARAFA_E_DATABASE_NOT_FOUND) {
		m_ptrDatabase->GetLogger()->Log(EC_LOGLEVEL_INFO, "Database not found, creating database.");
		er = m_ptrDatabase->CreateDatabase(lpConfig);
	}
	
	if (er != erSuccess)
		m_ptrDatabase->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Database connection failed: " + m_ptrDatabase->GetError());

	return ZarafaErrorToMAPIError(er);
}

HRESULT InstanceIdMapper::GetMappedInstanceId(const SBinary &sourceServerUID, ULONG cbSourceInstanceID, LPENTRYID lpSourceInstanceID, const SBinary &destServerUID, ULONG *lpcbDestInstanceID, LPENTRYID *lppDestInstanceID)
{
	HRESULT hr = hrSuccess;
	ECRESULT er = erSuccess;
	string strQuery;
	DB_RESULT lpResult = NULL;
	DB_ROW lpDBRow = NULL;
	DB_LENGTHS lpLengths = NULL;

	if (cbSourceInstanceID == 0 || lpSourceInstanceID == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	strQuery =
		"SELECT m_dst.val_binary FROM za_mappings AS m_dst "
			"JOIN za_mappings AS m_src ON m_dst.instance_id = m_src.instance_id AND m_dst.tag = m_src.tag AND m_src.val_binary = " + m_ptrDatabase->EscapeBinary((LPBYTE)lpSourceInstanceID, cbSourceInstanceID) + " "
			"JOIN za_servers AS s_dst ON m_dst.server_id = s_dst.id AND s_dst.guid = " + m_ptrDatabase->EscapeBinary(destServerUID.lpb, destServerUID.cb) + " "
			"JOIN za_servers AS s_src ON m_src.server_id = s_src.id AND s_src.guid = " + m_ptrDatabase->EscapeBinary(sourceServerUID.lpb, sourceServerUID.cb);

	er = m_ptrDatabase->DoSelect(strQuery, &lpResult);
	if (er != erSuccess) {
		hr = ZarafaErrorToMAPIError(er);
		goto exit;
	}

	switch (m_ptrDatabase->GetNumRows(lpResult)) {
		case 0:
			hr = MAPI_E_NOT_FOUND;
			goto exit;

		case 1:
			break;

		default:	// This should be impossible.
			hr = MAPI_E_DISK_ERROR;	// MAPI version of ZARAFA_E_DATABASE_ERROR
			goto exit;
	}

	lpDBRow = m_ptrDatabase->FetchRow(lpResult);
	if (lpDBRow == NULL || lpDBRow[0] == NULL) {
		hr = MAPI_E_DISK_ERROR;	// MAPI version of ZARAFA_E_DATABASE_ERROR
		goto exit;
	}

	lpLengths = m_ptrDatabase->FetchRowLengths(lpResult);
	if (lpLengths == NULL || lpLengths[0] == 0) {
		hr = MAPI_E_DISK_ERROR;	// MAPI version of ZARAFA_E_DATABASE_ERROR
		goto exit;
	}

	hr = MAPIAllocateBuffer(lpLengths[0], (LPVOID*)lppDestInstanceID);
	if (hr != hrSuccess)
		goto exit;

	memcpy(*lppDestInstanceID, lpDBRow[0], lpLengths[0]);
	*lpcbDestInstanceID = lpLengths[0];

exit:
	if (lpResult)
		m_ptrDatabase->FreeResult(lpResult);

	return hr;
}

HRESULT InstanceIdMapper::SetMappedInstances(ULONG ulPropTag, const SBinary &sourceServerUID, ULONG cbSourceInstanceID, LPENTRYID lpSourceInstanceID, const SBinary &destServerUID, ULONG cbDestInstanceID, LPENTRYID lpDestInstanceID)
{
	ECRESULT er = erSuccess;
	string strQuery;
	DB_RESULT lpResult = NULL;
	DB_ROW lpDBRow = NULL;

	if (cbSourceInstanceID == 0 || lpSourceInstanceID == NULL || cbDestInstanceID == 0 || lpDestInstanceID == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = m_ptrDatabase->Begin();
	if (er != erSuccess)
		goto exit;

	// Make sure the server entries exist.
	strQuery = "INSERT IGNORE INTO za_servers (guid) VALUES (" + m_ptrDatabase->EscapeBinary(sourceServerUID.lpb, sourceServerUID.cb) + "),(" +  m_ptrDatabase->EscapeBinary(destServerUID.lpb, destServerUID.cb) + ")";
	er = m_ptrDatabase->DoInsert(strQuery, NULL, NULL);
	if (er != erSuccess)
		goto exit;

	// Now first see if the source instance is available.
	strQuery = "SELECT instance_id FROM za_mappings AS m JOIN za_servers AS s ON m.server_id = s.id AND s.guid = " + m_ptrDatabase->EscapeBinary(sourceServerUID.lpb, sourceServerUID.cb) + " "
					"WHERE m.val_binary = " + m_ptrDatabase->EscapeBinary((LPBYTE)lpSourceInstanceID, cbSourceInstanceID) + " AND tag = " + stringify(PROP_ID(ulPropTag));
	er = m_ptrDatabase->DoSelect(strQuery, &lpResult);
	if (er != erSuccess)
		goto exit;

	lpDBRow = m_ptrDatabase->FetchRow(lpResult);
	if (lpDBRow == NULL) {
		unsigned int ulNewId;

		m_ptrDatabase->FreeResult(lpResult);
		lpResult = NULL;
		
		strQuery = "INSERT INTO za_instances (tag) VALUES (" + stringify(PROP_ID(ulPropTag)) + ")";
		er = m_ptrDatabase->DoInsert(strQuery, &ulNewId, NULL);
		if (er != erSuccess)
			goto exit;

		strQuery = "INSERT IGNORE INTO za_mappings (server_id, val_binary, tag, instance_id) VALUES "
						"((SELECT id FROM za_servers WHERE guid = " + m_ptrDatabase->EscapeBinary(sourceServerUID.lpb, sourceServerUID.cb) + ")," + m_ptrDatabase->EscapeBinary((LPBYTE)lpSourceInstanceID, cbSourceInstanceID) + "," + stringify(PROP_ID(ulPropTag)) + "," + stringify(ulNewId) + "),"
						"((SELECT id FROM za_servers WHERE guid = " + m_ptrDatabase->EscapeBinary(destServerUID.lpb, destServerUID.cb) + ")," + m_ptrDatabase->EscapeBinary((LPBYTE)lpDestInstanceID, cbDestInstanceID) + "," + stringify(PROP_ID(ulPropTag)) + "," + stringify(ulNewId) + ")";
	} else {	// Source instance id is known
		strQuery = "REPLACE INTO za_mappings (server_id, val_binary, tag, instance_id) VALUES "
						"((SELECT id FROM za_servers WHERE guid = " + m_ptrDatabase->EscapeBinary(destServerUID.lpb, destServerUID.cb) + ")," + m_ptrDatabase->EscapeBinary((LPBYTE)lpDestInstanceID, cbDestInstanceID) + "," + stringify(PROP_ID(ulPropTag)) + "," + lpDBRow[0] + ")";

		m_ptrDatabase->FreeResult(lpResult);
		lpResult = NULL;
	}
	er = m_ptrDatabase->DoInsert(strQuery, NULL, NULL);
	if (er != erSuccess)
		goto exit;

	er = m_ptrDatabase->Commit();

exit:
	if (er != erSuccess)
		m_ptrDatabase->Rollback();

	return ZarafaErrorToMAPIError(er);
}

}} // namespaces operations, za
