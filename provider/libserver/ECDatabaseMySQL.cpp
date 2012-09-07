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

#include <iostream>
#include <errmsg.h>
#include "ECDatabaseMySQL.h"
#include "mysqld_error.h"

#include "stringutil.h"

#include "ECDefs.h"
#include "ECDBDef.h"
#include "ECUserManagement.h"
#include "ECConfig.h"
#include "ECLogger.h"
#include "ZarafaCode.h"

#include "ecversion.h"

#include "mapidefs.h"
#include "ECConversion.h"
#include "SOAPUtils.h"
#include "ECSearchFolders.h"

#include "ECDatabaseUpdate.h"
#include "ECStatsCollector.h"


using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#ifdef DEBUG
#define DEBUG_SQL 0
#define DEBUG_TRANSACTION 0
#endif

#define LOG_SQL_DEBUG(_plog, _msg, ...) if ((_plog)->Log(EC_LOGLEVEL_DEBUG|EC_LOGLEVEL_SQL)) (_plog)->Log(EC_LOGLEVEL_DEBUG|EC_LOGLEVEL_SQL, _msg, ##__VA_ARGS__)

// The maximum packet size. This is automatically also the maximum
// size of a single entry in the database. This means that PR_BODY, PR_COMPRESSED_RTF
// etc. cannot grow larger than 16M. This shouldn't be such a problem in practice.

// In debian lenny, setting your max_allowed_packet to 16M actually gives this value.... Unknown
// why.
#define MAX_ALLOWED_PACKET			16776192

sUpdateList_t	sUpdateList[] = {
	// Updates from version 5.02 to 5.10
	{ Z_UPDATE_CREATE_VERSIONS_TABLE, 0, "Create table: versions", UpdateDatabaseCreateVersionsTable },
	{ Z_UPDATE_CREATE_SEARCHFOLDERS_TABLE, 0, "Create table: searchresults", UpdateDatabaseCreateSearchFolders },
	{ Z_UPDATE_FIX_USERTABLE_NONACTIVE, 0, "Update table: users, field: nonactive", UpdateDatabaseFixUserNonActive },
	// Only update if previous revision was after Z_UPDATE_CREATE_SEARCHFOLDERS_TABLE, because the definition has changed
	{ Z_UPDATE_ADD_FLAGS_TO_SEARCHRESULTS, Z_UPDATE_CREATE_SEARCHFOLDERS_TABLE, "Update table: searchresults", UpdateDatabaseCreateSearchFoldersFlags },
	{ Z_UPDATE_POPULATE_SEARCHFOLDERS, 0, "Populate search folders", UpdateDatabasePopulateSearchFolders },

	// Updates from version 5.10 to 5.20
	{ Z_UPDATE_CREATE_CHANGES_TABLE, 0, "Create table: changes", UpdateDatabaseCreateChangesTable },
	{ Z_UPDATE_CREATE_SYNCS_TABLE, 0, "Create table: syncs", UpdateDatabaseCreateSyncsTable },
	{ Z_UPDATE_CREATE_INDEXEDPROPS_TABLE, 0, "Create table: indexedproperties", UpdateDatabaseCreateIndexedPropertiesTable },
	{ Z_UPDATE_CREATE_SETTINGS_TABLE, 0, "Create table: settings", UpdateDatabaseCreateSettingsTable},
	{ Z_UPDATE_CREATE_SERVER_GUID, 0, "Insert server GUID into settings", UpdateDatabaseCreateServerGUID },
	{ Z_UPDATE_CREATE_SOURCE_KEYS, 0, "Insert source keys into indexedproperties", UpdateDatabaseCreateSourceKeys },

	// Updates from version 5.20 to 6.00
	{ Z_UPDATE_CONVERT_ENTRYIDS, 0, "Convert entryids: indexedproperties", UpdateDatabaseConvertEntryIDs },
	{ Z_UPDATE_CONVERT_SC_ENTRYIDLIST, 0, "Update entrylist searchcriteria", UpdateDatabaseSearchCriteria },
	{ Z_UPDATE_CONVERT_USER_OBJECT_TYPE, 0, "Add Object type to 'users' table", UpdateDatabaseAddUserObjectType },
	{ Z_UPDATE_ADD_USER_SIGNATURE, 0, "Add signature to 'users' table", UpdateDatabaseAddUserSignature },
	{ Z_UPDATE_ADD_SOURCE_KEY_SETTING, 0, "Add setting 'source_key_auto_increment'", UpdateDatabaseAddSourceKeySetting },
	{ Z_UPDATE_FIX_USERS_RESTRICTIONS, 0, "Add restriction to 'users' table", UpdateDatabaseRestrictExternId },

	// Update from version 6.00 to 6.10
	{ Z_UPDATE_ADD_USER_COMPANY, 0, "Add company column to 'users' table", UpdateDatabaseAddUserCompany },
	{ Z_UPDATE_ADD_OBJECT_RELATION_TYPE, 0, "Add Object relation type to 'objectrelation' table", UpdateDatabaseAddObjectRelationType },
	{ Z_UPDATE_DEL_DEFAULT_COMPANY, 0, "Delete default company from 'users' table", UpdateDatabaseDelUserCompany},
	{ Z_UPDATE_ADD_COMPANY_TO_STORES, 0, "Adding company to 'stores' table", UpdateDatabaseAddCompanyToStore},

	// Update from version x to x
	{ Z_UPDATE_ADD_IMAP_SEQ, 0, "Add IMAP sequence number in 'settings' table", UpdateDatabaseAddIMAPSequenceNumber},
	{ Z_UPDATE_KEYS_CHANGES, Z_UPDATE_CREATE_CHANGES_TABLE, "Update keys in 'changes' table", UpdateDatabaseKeysChanges},

	// Update from version 6.1x to 6.20
	{ Z_UPDATE_MOVE_PUBLICFOLDERS, 0, "Moving publicfolders and favorites", UpdateDatabaseMoveFoldersInPublicFolder},

	// Update from version 6.2x to 6.30
	{ Z_UPDATE_ADD_EXTERNID_TO_OBJECT, 0, "Adding externid to 'object' table", UpdateDatabaseAddExternIdToObject}, 
	{ Z_UPDATE_CREATE_REFERENCES, 0, "Creating Single Instance Attachment table", UpdateDatabaseCreateReferences},
	{ Z_UPDATE_LOCK_DISTRIBUTED, 0, "Locking multiserver capability", UpdateDatabaseLockDistributed},
	{ Z_UPDATE_CREATE_ABCHANGES_TABLE, 0, "Creating Addressbook Changes table", UpdateDatabaseCreateABChangesTable},
	{ Z_UPDATE_SINGLEINSTANCE_TAG, 0, "Updating 'singleinstances' table to correct tag value", UpdateDatabaseSetSingleinstanceTag},

	// Update from version 6.3x to 6.40
	{ Z_UPDATE_CREATE_SYNCEDMESSAGES_TABLE, 0, "Create table: synced messages", UpdateDatabaseCreateSyncedMessagesTable},
	
	// Update from < 6.30 to >= 6.30
	{ Z_UPDATE_FORCE_AB_RESYNC, 0, "Force Addressbook Resync", UpdateDatabaseForceAbResync},
	
	// Update from version 6.3x to 6.40
	{ Z_UPDATE_RENAME_OBJECT_TYPE_TO_CLASS, 0, "Rename objecttype columns to objectclass", UpdateDatabaseRenameObjectTypeToObjectClass},
	{ Z_UPDATE_CONVERT_OBJECT_TYPE_TO_CLASS, 0, "Convert objecttype columns to objectclass values", UpdateDatabaseConvertObjectTypeToObjectClass},
	{ Z_UPDATE_ADD_OBJECT_MVPROPERTY_TABLE, 0, "Add object MV property table", UpdateDatabaseAddMVPropertyTable},
	{ Z_UPDATE_COMPANYNAME_TO_COMPANYID, 0, "Link objects in DB plugin through companyid", UpdateDatabaseCompanyNameToCompanyId},
	{ Z_UPDATE_OUTGOINGQUEUE_PRIMARY_KEY, 0, "Update outgoingqueue key", UpdateDatabaseOutgoingQueuePrimarykey},
	{ Z_UPDATE_ACL_PRIMARY_KEY, 0, "Update acl key", UpdateDatabaseACLPrimarykey},
	{ Z_UPDATE_BLOB_EXTERNID, 0, "Update externid in object table", UpdateDatabaseBlobExternId}, // Avoid MySQL 4.x traling spaces quirk
	{ Z_UPDATE_KEYS_CHANGES_2, 0, "Update keys in 'changes' table", UpdateDatabaseKeysChanges2},
	{ Z_UPDATE_MVPROPERTIES_PRIMARY_KEY, 0, "Update mvproperties key", UpdateDatabaseMVPropertiesPrimarykey},
	{ Z_UPDATE_FIX_SECURITYGROUP_DBPLUGIN, 0, "Update DB plugin group to security groups", UpdateDatabaseFixDBPluginGroups},
	{ Z_UPDATE_CONVERT_SENDAS_DBPLUGIN, 0, "Update DB/Unix plugin sendas settings", UpdateDatabaseFixDBPluginSendAs},

	{ Z_UPDATE_MOVE_IMAP_SUBSCRIBES, 0, "Move IMAP subscribed list from store to inbox", UpdateDatabaseMoveSubscribedList},
	{ Z_UPDATE_SYNC_TIME_KEY, 0, "Update sync table time index", UpdateDatabaseSyncTimeIndex },

	// Update within the 6.40
	{ Z_UPDATE_ADD_STATE_KEY, 0, "Update changes table state key", UpdateDatabaseAddStateKey },

	// Blocking upgrade from 6.40 to 7.00, tables are not unicode compatible.
	{ Z_UPDATE_CONVERT_TO_UNICODE, 0, "Converting database to Unicode", UpdateDatabaseConvertToUnicode },

	// Update from version 6.4x to 7.00
	{ Z_UPDATE_CONVERT_STORE_USERNAME, 0, "Update stores table usernames", UpdateDatabaseConvertStoreUsername },
	{ Z_UPDATE_CONVERT_RULES, 0, "Converting rules to Unicode", UpdateDatabaseConvertRules },
	{ Z_UPDATE_CONVERT_SEARCH_FOLDERS, 0, "Converting search folders to Unicode", UpdateDatabaseConvertSearchFolders },
	{ Z_UPDATE_CONVERT_PROPERTIES, 0, "Converting properties for IO performance", UpdateDatabaseConvertProperties },
	{ Z_UPDATE_CREATE_COUNTERS, 0, "Creating counters for IO performance", UpdateDatabaseCreateCounters },
	{ Z_UPDATE_CREATE_COMMON_PROPS, 0, "Creating common properties for IO performance", UpdateDatabaseCreateCommonProps },
	{ Z_UPDATE_CHECK_ATTACHMENTS, 0, "Checking message attachment properties for IO performance", UpdateDatabaseCheckAttachments },
	{ Z_UPDATE_CREATE_TPROPERTIES, 0, "Creating tproperties for IO performance", UpdateDatabaseCreateTProperties },
	{ Z_UPDATE_CONVERT_HIERARCHY, 0, "Converting hierarchy for IO performance", UpdateDatabaseConvertHierarchy },
	{ Z_UPDATE_CREATE_DEFERRED, 0, "Creating deferred table for IO performance", UpdateDatabaseCreateDeferred },
	{ Z_UPDATE_CONVERT_CHANGES, 0, "Converting changes for IO performance", UpdateDatabaseConvertChanges },
	{ Z_UPDATE_CONVERT_NAMES, 0, "Converting names table to Unicode", UpdateDatabaseConvertNames },

	// Update from version 7.00 to 7.0.1
	{ Z_UPDATE_CONVERT_RF_TOUNICODE, 0, "Converting receivefolder table to Unicode", UpdateDatabaseReceiveFolderToUnicode },

	// Update from 6.40.13 / 7.0.3
	{ Z_UPDATE_CREATE_CLIENTUPDATE_TABLE, 0, "Creating client update status table", UpdateDatabaseClientUpdateStatus },

	{ Z_UPDATE_CONVERT_STORES, 0, "Converting stores table", UpdateDatabaseConvertStores },
	{ Z_UPDATE_UPDATE_STORES, 0, "Updating stores table", UpdateDatabaseUpdateStores },
	
	// Update from 7.0 to 7.1
	{ Z_UPDATE_UPDATE_WLINK_RECKEY, 0, "Updating wunderbar record keys", UpdateWLinkRecordKeys },
};

static char *server_groups[] = {
  "zarafa",
  (char *)NULL
};

typedef struct {
	const char *szName;
	const char *szSQL;
} STOREDPROCS;

/**
 * Mode 0 = All bodies
 * Mode 1 = Best body only (RTF better than HTML) + plaintext
 * Mode 2 = Plaintext only
 */
 
const char *szGetProps =
"CREATE PROCEDURE GetProps(IN hid integer, IN mode integer)\n"
"BEGIN\n"
"  DECLARE bestbody INT;\n"

"  IF mode = 1 THEN\n"
"  	 call GetBestBody(hid, bestbody);\n"
"  END IF;\n"
  
"  SELECT 0, tag, properties.type, val_ulong, val_string, val_binary, val_double, val_longint, val_hi, val_lo, 0, names.nameid, names.namestring, names.guid\n"
"    FROM properties LEFT JOIN names ON (properties.tag-0x8501)=names.id WHERE hierarchyid=hid AND (tag <= 0x8500 OR names.id IS NOT NULL) AND (tag NOT IN (0x1009, 0x1013) OR mode = 0 OR (mode = 1 AND tag = bestbody) )\n"
"  UNION\n"
"  SELECT count(*), tag, mvproperties.type, \n"
"          group_concat(length(mvproperties.val_ulong),':', mvproperties.val_ulong ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_string),':', mvproperties.val_string ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_binary),':', mvproperties.val_binary ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_double),':', mvproperties.val_double ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_longint),':', mvproperties.val_longint ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_hi),':', mvproperties.val_hi ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_lo),':', mvproperties.val_lo ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          0, names.nameid, names.namestring, names.guid \n"
"    FROM mvproperties LEFT JOIN names ON (mvproperties.tag-0x8501)=names.id WHERE hierarchyid=hid AND (tag <= 0x8500 OR names.id IS NOT NULL) GROUP BY tag, mvproperties.type; \n"
"END;\n";

const char *szPrepareGetProps =
"CREATE PROCEDURE PrepareGetProps(IN hid integer)\n"
"BEGIN\n"
"  SELECT 0, tag, properties.type, val_ulong, val_string, val_binary, val_double, val_longint, val_hi, val_lo, hierarchy.id, names.nameid, names.namestring, names.guid\n"
"    FROM properties JOIN hierarchy ON properties.hierarchyid=hierarchy.id LEFT JOIN names ON (properties.tag-0x8501)=names.id WHERE hierarchy.parent=hid AND (tag <= 0x8500 OR names.id IS NOT NULL);\n"
"  SELECT count(*), tag, mvproperties.type, \n"
"          group_concat(length(mvproperties.val_ulong),':', mvproperties.val_ulong ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_string),':', mvproperties.val_string ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_binary),':', mvproperties.val_binary ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_double),':', mvproperties.val_double ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_longint),':', mvproperties.val_longint ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_hi),':', mvproperties.val_hi ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          group_concat(length(mvproperties.val_lo),':', mvproperties.val_lo ORDER BY mvproperties.orderid SEPARATOR ''), \n"
"          hierarchy.id, names.nameid, names.namestring, names.guid \n"
"    FROM mvproperties JOIN hierarchy ON mvproperties.hierarchyid=hierarchy.id LEFT JOIN names ON (mvproperties.tag-0x8501)=names.id WHERE hierarchy.parent=hid AND (tag <= 0x8500 OR names.id IS NOT NULL) GROUP BY tag, mvproperties.type; \n"
"END;\n";


const char *szGetBestBody = 
"CREATE PROCEDURE GetBestBody(hid integer, OUT bestbody integer)\n"
"DETERMINISTIC\n"
"BEGIN\n"
"  DECLARE best INT;\n"
"  DECLARE CONTINUE HANDLER FOR NOT FOUND\n"
"    SET bestbody = 0 ;\n"
"  \n"
"  # Get body with lowest id (RTF before HTML)\n"
"  SELECT tag INTO bestbody FROM properties WHERE hierarchyid=hid AND tag IN (0x1009, 0x1013) ORDER BY tag LIMIT 1;\n"
"END;\n";

const char *szStreamObj = 
"# Read a type-5 (Message) item from the database, output properties and subobjects\n"
"CREATE PROCEDURE StreamObj(IN rootid integer, IN maxdepth integer, IN mode integer)\n"
"BEGIN\n"
"DECLARE no_more_rows BOOLEAN;\n"
"DECLARE subid INT;\n"
"DECLARE subsubid INT;\n"
"DECLARE subtype INT;\n"
"DECLARE cur_hierarchy CURSOR FOR\n"
"	SELECT id,hierarchy.type FROM hierarchy WHERE parent=rootid AND type=7; \n"
"DECLARE CONTINUE HANDLER FOR NOT FOUND\n"
"    SET no_more_rows = TRUE;\n"

"  call GetProps(rootid, mode);\n"

"  call PrepareGetProps(rootid);\n"
 
"  SELECT id,hierarchy.type FROM hierarchy WHERE parent=rootid;\n"

"  OPEN cur_hierarchy;\n"

"  the_loop: LOOP\n"
"    FETCH cur_hierarchy INTO subid, subtype;\n"

"    IF no_more_rows THEN\n"
"      CLOSE cur_hierarchy;\n"
"      LEAVE the_loop;\n"
"    END IF;\n"

"    IF subtype = 7 THEN\n"
"      BEGIN\n"
"        DECLARE CONTINUE HANDLER FOR NOT FOUND set subsubid = 0;\n"

"        IF maxdepth > 0 THEN\n"
"          SELECT id INTO subsubid FROM hierarchy WHERE parent=subid LIMIT 1;\n"
"          SELECT id, hierarchy.type FROM hierarchy WHERE parent = subid LIMIT 1;\n"

"          IF subsubid != 0 THEN\n"
"            # Recurse into submessage (must be type 5 since attachments can only contain nested messages)\n"
"            call StreamObj(subsubid, maxdepth-1, mode);\n"
"          END IF;\n"
"        ELSE\n"
"          # Maximum depth reached. Output a zero-length subset to indicate that we're\n"
"          # not recursing further.\n"
"          SELECT id, hierarchy.type FROM hierarchy WHERE parent=subid LIMIT 0;\n"
"        END IF;\n"
"      END;\n"

"    END IF;\n"
"  END LOOP the_loop;\n"

"END\n";

STOREDPROCS stored_procedures[] = {
	{ "GetProps", szGetProps },
	{ "PrepareGetProps", szPrepareGetProps },
	{ "GetBestBody", szGetBestBody },
	{ "StreamObj", szStreamObj }
};

std::string	ECDatabaseMySQL::m_strDatabaseDir;

ECDatabaseMySQL::ECDatabaseMySQL(ECLogger *lpLogger, ECConfig *lpConfig)
{
	m_bMysqlInitialize	= false;
	m_bConnected		= false;
	m_bLocked			= false;
	m_bAutoLock			= true;
	m_lpLogger			= lpLogger;
	m_lpConfig			= lpConfig;
	m_bSuppressLockErrorLogging = false;

	// Create a mutex handle for mysql
	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_hMutexMySql, &mattr);

#ifdef DEBUG
	m_ulTransactionState = 0;
#endif	

}

ECDatabaseMySQL::~ECDatabaseMySQL()
{
	Close();

	// Close the mutex handle of mysql
	pthread_mutex_destroy(&m_hMutexMySql);
}

ECRESULT ECDatabaseMySQL::InitLibrary(char* lpDatabaseDir, char* lpConfigFile, ECLogger *lpLogger)
{
	ECRESULT	er = erSuccess;
	string		strDatabaseDir;
	string		strConfigFile;
	int			ret = 0;

	if(lpDatabaseDir) {
    	strDatabaseDir = "--datadir=";
    	strDatabaseDir+= lpDatabaseDir;

		m_strDatabaseDir = lpDatabaseDir;
    }

    if(lpConfigFile) {
    	strConfigFile = "--defaults-file=";
    	strConfigFile+= lpConfigFile;
    }
	
	char *server_args[] = {
		"zarafa",		/* this string is not used */
		(char*)strConfigFile.c_str(),
		(char*)strDatabaseDir.c_str(),
	};

	if((ret = mysql_library_init(arraySize(server_args), server_args, server_groups)) != 0) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to initialize mysql: error 0x%08X", ret);
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

exit:
	return er;
}

/**
 * Initialize anything that has to be initialized at server startup.
 *
 * Currently this means we're updating all the stored procedure definitions
 */
ECRESULT ECDatabaseMySQL::InitializeDBState()
{
	ECRESULT er = erSuccess;

	
	for(unsigned int i = 0; i < arraySize(stored_procedures); i++) {
		er = DoUpdate(std::string("DROP PROCEDURE IF EXISTS ") + stored_procedures[i].szName);
		if(er != erSuccess)
			goto exit;
			
		er = DoUpdate(stored_procedures[i].szSQL);
		if(er != erSuccess) {
			int err = mysql_errno(&m_lpMySQL);
			if (err == ER_DBACCESS_DENIED_ERROR) {
				m_lpLogger->Log(EC_LOGLEVEL_FATAL, "zarafa-server is not allowed to create stored procedures");
				m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Please grant CREATE ROUTINE permissions to the mysql user '%s' on the '%s' database",
								m_lpConfig->GetSetting("mysql_user"), m_lpConfig->GetSetting("mysql_database"));
			} else {
				m_lpLogger->Log(EC_LOGLEVEL_FATAL, "zarafa-server is unable to create stored procedures, error %d", err);
			}
			goto exit;
		}
	}

exit:	
	return er;
}

void ECDatabaseMySQL::UnloadLibrary() 
{
	mysql_server_end();// mysql > 4.1.10 = mysql_library_end();
}

ECRESULT ECDatabaseMySQL::InitEngine()
{
	ECRESULT er = erSuccess;

	_ASSERT(m_bMysqlInitialize == false);

	//Init mysql and make a connection
	if(mysql_init(&m_lpMySQL) == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	m_bMysqlInitialize = true; 

	// Set auto reconnect OFF
	// mysql < 5.0.4 default on, mysql 5.0.4 > reconnection default off
	// Zarafa always wants reconnect OFF, because we want to know when the connection
	// is broken since this creates a new MySQL session, and we want to set some session
	// variables
	m_lpMySQL.reconnect = 0;

exit:
	return er;
}

ECLogger* ECDatabaseMySQL::GetLogger()
{
	ASSERT(m_lpLogger);
	return m_lpLogger;
}

std::string ECDatabaseMySQL::GetDatabaseDir()
{
	ASSERT(!m_strDatabaseDir.empty());
	return m_strDatabaseDir;
}

ECRESULT ECDatabaseMySQL::CheckExistColumn(const std::string &strTable, const std::string &strColumn, bool *lpbExist)
{
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	DB_RESULT		lpDBResult = NULL;

	strQuery = "SELECT 1 FROM information_schema.COLUMNS "
				"WHERE TABLE_SCHEMA = '" + string(m_lpConfig->GetSetting("mysql_database")) + "' "
				"AND TABLE_NAME = '" + strTable + "' "
				"AND COLUMN_NAME = '" + strColumn + "'";
				
	er = DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;
	
	*lpbExist = (FetchRow(lpDBResult) != NULL);
	
exit:
	if (lpDBResult)
		FreeResult(lpDBResult);
	
	return er;
}

ECRESULT ECDatabaseMySQL::CheckExistIndex(const std::string strTable, const std::string &strKey, bool *lpbExist)
{
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	DB_RESULT		lpDBResult = NULL;
	
	strQuery = "SHOW INDEXES FROM " + strTable + " WHERE Key_name='" + strKey + "'";

	er = DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	*lpbExist = (FetchRow(lpDBResult) != NULL);

exit:
	if (lpDBResult)
		FreeResult(lpDBResult);
	
	return er;
}

ECRESULT ECDatabaseMySQL::Connect()
{
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	char*			lpMysqlPort = m_lpConfig->GetSetting("mysql_port");
	char*			lpMysqlSocket = m_lpConfig->GetSetting("mysql_socket");
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	unsigned int	gcm = 0;

	if (*lpMysqlSocket == '\0')
		lpMysqlSocket = NULL;
	
	er = InitEngine();
	if(er != erSuccess)
		goto exit;

	if(mysql_real_connect(&m_lpMySQL, 
			m_lpConfig->GetSetting("mysql_host"), 
			m_lpConfig->GetSetting("mysql_user"), 
			m_lpConfig->GetSetting("mysql_password"), 
			m_lpConfig->GetSetting("mysql_database"), 
			(lpMysqlPort)?atoi(lpMysqlPort):0, 
			lpMysqlSocket, CLIENT_MULTI_STATEMENTS) == NULL)
	{
		if(mysql_errno(&m_lpMySQL) == ER_BAD_DB_ERROR){ // Database does not exist
			er = ZARAFA_E_DATABASE_NOT_FOUND;
		}else
			er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
	
	// Check if the database is available, but empty
	strQuery = "SHOW tables";
	er = DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;
		
	if(GetNumRows(lpDBResult) == 0) {
		er = ZARAFA_E_DATABASE_NOT_FOUND;
		goto exit;
	}
	
	if(lpDBResult) {
		FreeResult(lpDBResult);
		lpDBResult = NULL;
	}
	
	strQuery = "SHOW variables LIKE 'max_allowed_packet'";
	er = DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
	    goto exit;
	
	lpDBRow = FetchRow(lpDBResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL) {
	    m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to retrieve max_allowed_packet value. Assuming 16M");
	    m_ulMaxAllowedPacket = (unsigned int)MAX_ALLOWED_PACKET;
    } else {
        m_ulMaxAllowedPacket = atoui(lpDBRow[1]);
    }

	m_bConnected = true;

#if HAVE_MYSQL_SET_CHARACTER_SET
	// function since mysql 5.0.7
	if (mysql_set_character_set(&m_lpMySQL, "utf8")) {
	    m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to set character set to 'utf8'");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
#else
	if (Query("set character_set_client = 'utf8'") != 0) {
		m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to set character_set_client value");
		goto exit;
	}
	if (Query("set character_set_connection = 'utf8'") != 0) {
		m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to set character_set_connection value");
		goto exit;
	}
	if (Query("set character_set_results = 'utf8'") != 0) {
		m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to set character_set_results value");
		goto exit;
	}
#endif
	if (Query("set max_sp_recursion_depth = 255") != 0) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to set recursion depth");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	// Force to a sane value
	gcm = atoui(m_lpConfig->GetSetting("mysql_group_concat_max_len"));
	if(gcm < 1024)
		gcm = 1024;
		
	strQuery = (string)"SET SESSION group_concat_max_len = " + stringify(gcm);
	if(Query(strQuery) != erSuccess ) {
	    m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to set group_concat_max_len value");
	}

	// changing the SESSION max_allowed_packet is removed since mysql 5.1, and GLOBAL is for SUPER users only, so just give a warning
	if (m_ulMaxAllowedPacket < MAX_ALLOWED_PACKET)
		m_lpLogger->Log(EC_LOGLEVEL_WARNING, "max_allowed_packet is smaller than 16M (%d). You are advised to increase this value by adding max_allowed_packet=16M in the [mysqld] section of my.cnf.", m_ulMaxAllowedPacket);

	if (m_lpMySQL.server_version && m_lpMySQL.server_version[0] >= '5') {
		// this option was introduced in mysql 5.0, so let's not even try on 4.1 servers
		strQuery = "SET SESSION sql_mode = 'STRICT_ALL_TABLES'";
		Query(strQuery); // ignore error
	}

exit:
	if(lpDBResult)
		FreeResult(lpDBResult);

	if (er == erSuccess)
		g_lpStatsCollector->Increment(SCN_DATABASE_CONNECTS);
	else
		g_lpStatsCollector->Increment(SCN_DATABASE_FAILED_CONNECTS);
		
	return er;
}

ECRESULT ECDatabaseMySQL::Close()
{
	ECRESULT er = erSuccess;

	//INFO: No locking here

	m_bConnected = false;

	// Close mysql data connection and deallocate data
	if(m_bMysqlInitialize)
		mysql_close(&m_lpMySQL);

	m_bMysqlInitialize = false;

	return er;
}

// Get database ownership
bool ECDatabaseMySQL::Lock()
{

	pthread_mutex_lock(&m_hMutexMySql);

	m_bLocked = true;

	return m_bLocked;
}

// Release the database ownership
bool ECDatabaseMySQL::UnLock()
{
	pthread_mutex_unlock(&m_hMutexMySql);

	m_bLocked = false;
		
	return m_bLocked;
}

bool ECDatabaseMySQL::isConnected() {

	return m_bConnected;
}

/**
 * Perform an SQL query on MySQL
 *
 * Sends a query to the MySQL server, and does a reconnect if the server connection is lost before or during
 * the SQL query. The reconnect is done only once. If the query fails after the reconnect, the entire call
 * fails.
 * 
 * It is up to the caller to get any result information from the query.
 *
 * @param[in] strQuery SQL query to perform
 * @return result (erSuccess or ZARAFA_E_DATABASE_ERROR)
 */
ECRESULT ECDatabaseMySQL::Query(const string &strQuery) {
	ECRESULT er = erSuccess;
	int err;
	
	LOG_SQL_DEBUG(m_lpLogger, "SQL [%08lu]: \"%s;\"", m_lpMySQL.thread_id, strQuery.c_str()); 

	// use mysql_real_query to be binary safe ( http://dev.mysql.com/doc/mysql/en/mysql-real-query.html )
	err = mysql_real_query( &m_lpMySQL, strQuery.c_str(), strQuery.length() );

	if(err && (mysql_errno(&m_lpMySQL) == CR_SERVER_LOST || mysql_errno(&m_lpMySQL) == CR_SERVER_GONE_ERROR)) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "SQL [%08lu] info: Try to reconnect", m_lpMySQL.thread_id);
			
		er = Close();
		if(er != erSuccess)
			goto exit;
			
		er = Connect();
		if(er != erSuccess)
			goto exit;
			
		// Try again
		err = mysql_real_query( &m_lpMySQL, strQuery.c_str(), strQuery.length() );
	}

	if(err) {
		if (!m_bSuppressLockErrorLogging || GetLastError() == DB_E_UNKNOWN)
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "SQL [%08lu] Failed: %s, Query Size: %lu, Query: \"%s\"", m_lpMySQL.thread_id, mysql_error(&m_lpMySQL), (long unsigned int)strQuery.size(), strQuery.c_str()); 
		er = ZARAFA_E_DATABASE_ERROR;
		// Don't assert on ER_NO_SUCH_TABLE because it's an anticipated error in the db upgrade code.
		if (mysql_errno(&m_lpMySQL) != ER_NO_SUCH_TABLE)
			ASSERT(false);
	}

exit:
	return er;
}

/**
 * Perform a SELECT operation on the database
 *
 * Sends the passed SELECT-like (any operation that outputs a result set) query to the MySQL server and retrieves
 * the result.
 *
 * Setting fStreamResult will delay retrieving data from the network until FetchRow() is called. The only
 * drawback is that GetRowCount() can therefore not be used unless all rows are fetched first. The main reason to
 * use this is to conserve memory and increase pipelining (the client can start processing data before the server
 * has completed the query)
 *
 * @param[in] strQuery SELECT query string
 * @param[out] Result output
 * @param[in] fStreamResult TRUE if data should be streamed instead of stored
 * @return result erSuccess or ZARAFA_E_DATABASE_ERROR
 */
ECRESULT ECDatabaseMySQL::DoSelect(const string &strQuery, DB_RESULT *lppResult, bool fStreamResult) {

	ECRESULT er = erSuccess;
	DB_RESULT lpResult = NULL;

	_ASSERT(strQuery.length()!= 0);

	// Autolock, lock data
	if(m_bAutoLock)
		Lock();
		
	if( Query(strQuery) != erSuccess ) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if(fStreamResult)
    	lpResult = mysql_use_result( &m_lpMySQL );
    else
    	lpResult = mysql_store_result( &m_lpMySQL );
    
	if( lpResult == NULL ) {
		er = ZARAFA_E_DATABASE_ERROR;
		if (!m_bSuppressLockErrorLogging || GetLastError() == DB_E_UNKNOWN)
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "SQL [%08lu] result failed: %s, Query: \"%s\"", m_lpMySQL.thread_id, mysql_error(&m_lpMySQL), strQuery.c_str()); 
	}

	g_lpStatsCollector->Increment(SCN_DATABASE_SELECTS);
	
	if (lppResult)
		*lppResult = lpResult;
	else {
		if(lpResult)
			FreeResult(lpResult);
	}

exit:
	if (er != erSuccess) {
		g_lpStatsCollector->Increment(SCN_DATABASE_FAILED_SELECTS);
		g_lpStatsCollector->SetTime(SCN_DATABASE_LAST_FAILED, time(NULL));
	}

	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

ECRESULT ECDatabaseMySQL::DoSelectMulti(const string &strQuery) {

	ECRESULT er = erSuccess;

	_ASSERT(strQuery.length()!= 0);

	// Autolock, lock data
	if(m_bAutoLock)
		Lock();
		
	if( Query(strQuery) != erSuccess ) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
	
	m_bFirstResult = true;

	g_lpStatsCollector->Increment(SCN_DATABASE_SELECTS);
	
exit:
	if (er != erSuccess) {
		g_lpStatsCollector->Increment(SCN_DATABASE_FAILED_SELECTS);
		g_lpStatsCollector->SetTime(SCN_DATABASE_LAST_FAILED, time(NULL));
	}

	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

/**
 * Get next resultset from a multi-resultset query
 * 
 * @param[out] lppResult Resultset
 * @return result
 */
ECRESULT ECDatabaseMySQL::GetNextResult(DB_RESULT *lppResult) {

	ECRESULT er = erSuccess;
	DB_RESULT lpResult = NULL;
	int ret = 0;

	// Autolock, lock data
	if(m_bAutoLock)
		Lock();
		
	if(!m_bFirstResult)
		ret = mysql_next_result( &m_lpMySQL );
		
	m_bFirstResult = false;
		
	if(ret < 0) {
		er = ZARAFA_E_DATABASE_ERROR;
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "SQL [%08lu] next_result failed: expected more results", m_lpMySQL.thread_id);
		goto exit;
	}
	
	if(ret > 0) {
		er = ZARAFA_E_DATABASE_ERROR;
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "SQL [%08lu] next_result of multi-resultset failed: %s", m_lpMySQL.thread_id, mysql_error(&m_lpMySQL)); 
		goto exit;
	}		

   	lpResult = mysql_store_result( &m_lpMySQL );
   	if(lpResult == NULL) {
   		// I think this can only happen on the first result set of a query since otherwise mysql_next_result() would already fail
		er = ZARAFA_E_DATABASE_ERROR;
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "SQL [%08lu] result failed: %s", m_lpMySQL.thread_id, mysql_error(&m_lpMySQL)); 
		goto exit;
   	}

	if (lppResult)
		*lppResult = lpResult;
	else {
		if(lpResult)
			FreeResult(lpResult);
	}

exit:
	if (er != erSuccess) {
		g_lpStatsCollector->Increment(SCN_DATABASE_FAILED_SELECTS);
		g_lpStatsCollector->SetTime(SCN_DATABASE_LAST_FAILED, time(NULL));
	}

	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

/**
 * Finalize a multi-SQL call
 *
 * A stored procedure will output a NULL result set at the end of the stored procedure to indicate
 * that the results have ended. This must be consumed here, otherwise the database will be left in a bad
 * state.
 */
ECRESULT ECDatabaseMySQL::FinalizeMulti() {

	ECRESULT er = erSuccess;
	DB_RESULT lpResult = NULL;

	mysql_next_result(&m_lpMySQL);
	
	lpResult = mysql_store_result(&m_lpMySQL);
	
	if(lpResult != NULL) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "SQL [%08lu] result failed: unexpected results received at end of batch", m_lpMySQL.thread_id);
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
exit:
	if(lpResult)
		FreeResult(lpResult);
		
	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

/**
 * Perform a UPDATE operation on the database
 *
 * Sends the passed UPDATE query to the MySQL server, and optionally returns the number of affected rows. The
 * affected rows is the number of rows that have been MODIFIED, which is not necessarily the number of rows that
 * MATCHED the WHERE-clause.
 *
 * @param[in] strQuery UPDATE query string
 * @param[out] lpulAffectedRows (optional) Receives the number of affected rows 
 * @return result erSuccess or ZARAFA_E_DATABASE_ERROR
 */
ECRESULT ECDatabaseMySQL::DoUpdate(const string &strQuery, unsigned int *lpulAffectedRows) {
	
	ECRESULT er = erSuccess;

	// Autolock, lock data
	if(m_bAutoLock)
		Lock();
	
	er = _Update(strQuery, lpulAffectedRows);

	if (er != erSuccess) {
		g_lpStatsCollector->Increment(SCN_DATABASE_FAILED_UPDATES);
		g_lpStatsCollector->SetTime(SCN_DATABASE_LAST_FAILED, time(NULL));
	}

	g_lpStatsCollector->Increment(SCN_DATABASE_UPDATES);

	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

ECRESULT ECDatabaseMySQL::_Update(const string &strQuery, unsigned int *lpulAffectedRows)
{
	ECRESULT er = erSuccess;
					
	if( Query(strQuery) != erSuccess ) {
		// FIXME: Add the mysql error system ?
		// er = nMysqlError;
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
	
	if(lpulAffectedRows)
		*lpulAffectedRows = GetAffectedRows();
	
exit:
	return er;
}

/**
 * Perform an INSERT operation on the database
 *
 * Sends the passed INSERT query to the MySQL server, and optionally returns the new insert ID and the number of inserted
 * rows.
 *
 * @param[in] strQuery INSERT query string
 * @param[out] lpulInsertId (optional) Receives the last insert id
 * @param[out] lpulAffectedRows (optional) Receives the number of inserted rows
 * @return result erSuccess or ZARAFA_E_DATABASE_ERROR
 */
ECRESULT ECDatabaseMySQL::DoInsert(const string &strQuery, unsigned int *lpulInsertId, unsigned int *lpulAffectedRows)
{
	ECRESULT er = erSuccess;

	// Autolock, lock data
	if(m_bAutoLock)
		Lock();
	
	er = _Update(strQuery, lpulAffectedRows);

	if (er == erSuccess) {
		if (lpulInsertId)
			*lpulInsertId = GetInsertId();
	} else {
		g_lpStatsCollector->Increment(SCN_DATABASE_FAILED_INSERTS);
		g_lpStatsCollector->SetTime(SCN_DATABASE_LAST_FAILED, time(NULL));
	}

	g_lpStatsCollector->Increment(SCN_DATABASE_INSERTS);

	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

/**
 * Perform a DELETE operation on the database
 *
 * Sends the passed DELETE query to the MySQL server, and optionally the number of deleted rows
 *
 * @param[in] strQuery INSERT query string
 * @param[out] lpulAffectedRows (optional) Receives the number of deleted rows
 * @return result erSuccess or ZARAFA_E_DATABASE_ERROR
 */
ECRESULT ECDatabaseMySQL::DoDelete(const string &strQuery, unsigned int *lpulAffectedRows) {

	ECRESULT er = erSuccess;

	// Autolock, lock data
	if(m_bAutoLock)
		Lock();
	
	er = _Update(strQuery, lpulAffectedRows);

	if (er != erSuccess) {
		g_lpStatsCollector->Increment(SCN_DATABASE_FAILED_DELETES);
		g_lpStatsCollector->SetTime(SCN_DATABASE_LAST_FAILED, time(NULL));
	}

	g_lpStatsCollector->Increment(SCN_DATABASE_DELETES);

	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

/*
 * This function updates a sequence in an atomic fashion - if called correctly;
 *
 * To make it work correctly, the state of the database connection should *NOT* be in a transaction; this would delay
 * committing of the data until a later time, causing other concurrent threads to possibly generate the same ID or lock
 * while waiting for this transaction to end. So, don't call Begin() before calling this function unless you really
 * know what you're doing.
 *
 * @todo measure sequence update calls, currently it is a update
 */
ECRESULT ECDatabaseMySQL::DoSequence(const std::string &strSeqName, unsigned int ulCount, unsigned long long *lpllFirstId) {
	ECRESULT er = erSuccess;
	unsigned int ulAffected = 0;

    // Autolock, lock data
	if(m_bAutoLock)
		Lock();

#ifdef DEBUG
#if DEBUG_TRANSACTION
	if(m_ulTransactionState != 0) {
		ASSERT(FALSE);
	}		
#endif
#endif

	// Attempt to update the sequence in an atomic fashion
	er = DoUpdate("UPDATE settings SET value=LAST_INSERT_ID(value+1)+" + stringify(ulCount-1) + " WHERE name = '" + strSeqName + "'", &ulAffected);
	if(er != erSuccess)
		goto exit;
	
	// If the setting was missing, insert it now, starting at sequence 1 (not 0 for safety - maybe there's some if(ulSequenceId) code somewhere)
	if(ulAffected == 0) {
		er = Query("INSERT INTO settings (name, value) VALUES('" + strSeqName + "',LAST_INSERT_ID(1)+" + stringify(ulCount-1) + ")");
		if(er != erSuccess)
			goto exit;
	}
			
	*lpllFirstId = mysql_insert_id(&m_lpMySQL);
	
exit:
	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

unsigned int ECDatabaseMySQL::GetAffectedRows() {

	return (unsigned int)mysql_affected_rows(&m_lpMySQL);
}

unsigned int ECDatabaseMySQL::GetInsertId() {

	return (unsigned int)mysql_insert_id(&m_lpMySQL);
}

void ECDatabaseMySQL::FreeResult(DB_RESULT sResult) {

	_ASSERT(sResult != NULL);

	if(sResult)
		mysql_free_result((MYSQL_RES *)sResult);
}

unsigned int ECDatabaseMySQL::GetNumRows(DB_RESULT sResult) {

	return (unsigned int)mysql_num_rows((MYSQL_RES *)sResult);
}

unsigned int ECDatabaseMySQL::GetNumRowFields(DB_RESULT sResult) {

	return mysql_num_fields((MYSQL_RES *)sResult);
}

unsigned int ECDatabaseMySQL::GetRowIndex(DB_RESULT sResult, const std::string &strFieldname) {

	MYSQL_FIELD *lpFields;
	unsigned int cbFields;
	unsigned int ulIndex = (unsigned int)-1;

	lpFields = mysql_fetch_fields((MYSQL_RES *)sResult);
	cbFields = mysql_field_count(&m_lpMySQL);

	for (unsigned int i = 0; i < cbFields && ulIndex == (unsigned int)-1; ++i)
		if (stricmp(lpFields[i].name, strFieldname.c_str()) == 0)
			ulIndex = i;
	
	return ulIndex;
}

DB_ROW ECDatabaseMySQL::FetchRow(DB_RESULT sResult) {

	return mysql_fetch_row((MYSQL_RES *)sResult);
}

DB_LENGTHS ECDatabaseMySQL::FetchRowLengths(DB_RESULT sResult) {

	return (DB_LENGTHS)mysql_fetch_lengths((MYSQL_RES *)sResult);
}

/** 
 * For some reason, MySQL only supports up to 3 bytes of utf-8 data. This means
 * that data outside the BMP is not supported. This function filters the passed utf-8 string
 * and removes the non-BMP characters. Since it should be extremely uncommon to have useful
 * data outside the BMP, this should be acceptable.
 *
 * Note: BMP stands for Basic Multilingual Plane (first 0x10000 code points in unicode)
 *
 * If somebody points out a useful use case for non-BMP characters in the future, then we'll
 * have to rethink this.
 *
 */
std::string ECDatabaseMySQL::FilterBMP(const std::string &strToFilter)
{
	const char *c = strToFilter.c_str();
	std::string::size_type pos = 0;
	std::string strFiltered;

	while(pos < strToFilter.size()) {
		// Copy 1, 2, and 3-byte utf-8 sequences
		int len;
		
		if((c[pos] & 0x80) == 0)
			len = 1;
		else if((c[pos] & 0xE0) == 0xC0)
			len = 2;
		else if((c[pos] & 0xF0) == 0xE0)
			len = 3;
		else if((c[pos] & 0xF8) == 0xF0)
			len = 4;
		else if((c[pos] & 0xFC) == 0xF8)
			len = 5;
		else if((c[pos] & 0xFE) == 0xFC)
			len = 6;
		else {
			// Invalid utf-8 ?
			len = 1;
		}
		
		if(len < 4) {
			strFiltered.append(&c[pos], len);
		}
		
		pos += len;
	}
	
	return strFiltered;
}

std::string ECDatabaseMySQL::Escape(const std::string &strToEscape)
{
	ULONG size = strToEscape.length()*2+1;
	char *szEscaped = new char[size];
	std::string escaped;

	memset(szEscaped, 0, size);

	mysql_real_escape_string(&this->m_lpMySQL, szEscaped, strToEscape.c_str(), strToEscape.length());

	escaped = szEscaped;

	delete [] szEscaped;

	return escaped;
}

std::string ECDatabaseMySQL::EscapeBinary(unsigned char *lpData, unsigned int ulLen)
{
	ULONG size = ulLen*2+1;
	char *szEscaped = new char[size];
	std::string escaped;
	
	memset(szEscaped, 0, size);

	mysql_real_escape_string(&this->m_lpMySQL, szEscaped, (const char *)lpData, ulLen);

	escaped = szEscaped;

	delete [] szEscaped;

	return "'" + escaped + "'";
}

std::string ECDatabaseMySQL::EscapeBinary(const std::string& strData)
{
	return EscapeBinary((unsigned char *)strData.c_str(), strData.size());
}

void ECDatabaseMySQL::ResetResult(DB_RESULT sResult) {

	mysql_data_seek((MYSQL_RES *)sResult, 0);
}

std::string ECDatabaseMySQL::GetError()
{
	if(m_bMysqlInitialize == false)
		return "MYSQL not initialized";

	return mysql_error(&m_lpMySQL);
}

DB_ERROR ECDatabaseMySQL::GetLastError()
{
	DB_ERROR dberr;
	
	switch (mysql_errno(&m_lpMySQL)) {
		case ER_LOCK_WAIT_TIMEOUT:
			dberr = DB_E_LOCK_WAIT_TIMEOUT;
			break;
		
		case ER_LOCK_DEADLOCK:
			dberr = DB_E_LOCK_DEADLOCK;
			break;
			
		default:
			dberr = DB_E_UNKNOWN;
			break;
	}
	
	return dberr;
}

bool ECDatabaseMySQL::SuppressLockErrorLogging(bool bSuppress)
{
	std::swap(bSuppress, m_bSuppressLockErrorLogging);
	return bSuppress;
}

ECRESULT ECDatabaseMySQL::Begin() {
	ECRESULT er = erSuccess;
	
	er = Query("BEGIN");

#ifdef DEBUG
#if DEBUG_TRANSACTION
	m_lpLogger->Log(EC_LOGLEVEL_FATAL,"%08X: BEGIN", &m_lpMySQL);
	if(m_ulTransactionState != 0) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"BEGIN ALREADY ISSUED");
		ASSERT(("BEGIN ALREADY ISSUED", FALSE));
	}
	m_ulTransactionState = 1;
#endif
#endif
	
	return er;
}

ECRESULT ECDatabaseMySQL::Commit() {
	ECRESULT er = erSuccess;
	er = Query("COMMIT");
	
#ifdef DEBUG
#if DEBUG_TRANSACTION
	m_lpLogger->Log(EC_LOGLEVEL_FATAL,"%08X: COMMIT", &m_lpMySQL);
	if(m_ulTransactionState != 1) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"NO BEGIN ISSUED");
		ASSERT(("NO BEGIN ISSUED", FALSE));
	}
	m_ulTransactionState = 0;
#endif
#endif

	return er;
}

ECRESULT ECDatabaseMySQL::Rollback() {
	ECRESULT er = erSuccess;

	er = Query("ROLLBACK");
	
#ifdef DEBUG
#if DEBUG_TRANSACTION
	m_lpLogger->Log(EC_LOGLEVEL_FATAL,"%08X: ROLLBACK", &m_lpMySQL);
	if(m_ulTransactionState != 1) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"NO BEGIN ISSUED");
		ASSERT(("NO BEGIN ISSUED", FALSE));
	}
	m_ulTransactionState = 0;
#endif
#endif

	return er;
}

unsigned int ECDatabaseMySQL::GetMaxAllowedPacket() {
    return m_ulMaxAllowedPacket;
}

void ECDatabaseMySQL::ThreadInit() {
	mysql_thread_init();
}

void ECDatabaseMySQL::ThreadEnd() {
	mysql_thread_end();
}

ECRESULT ECDatabaseMySQL::IsInnoDBSupported()
{
	ECRESULT	er = erSuccess;
	DB_RESULT	lpResult = NULL;
	DB_ROW		lpDBRow = NULL;

	er = DoSelect("SHOW VARIABLES LIKE \"have_innodb\"", &lpResult);
	if(er != erSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to get value 'have_innodb' from the mysql server. Probably INNODB is not supported. Error: %s", GetError().c_str());
		goto exit;
	}

	lpDBRow = FetchRow(lpResult);
	if (lpDBRow == NULL || lpDBRow[1] == NULL) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to get value 'have_innodb' from the mysql server. Probably INNODB is not supported.");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if (stricmp(lpDBRow[1], "DISABLED") == 0) {
		// mysql has run with innodb enabled once, but disabled this.. so check your log.
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"INNODB engine is disabled. Please re-enable the INNODB engine. Check your MySQL log for more information or comment out skip-innodb in the mysql configuration file.");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	} else if(stricmp(lpDBRow[1], "YES") != 0 && stricmp(lpDBRow[1], "DEFAULT") != 0) {
		// mysql is incorrectly configured or compiled.
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"INNODB engine is not supported. Please enable the INNODB engine in the mysql configuration file.");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

exit:
	if(lpResult)
		FreeResult(lpResult);

	return er;
}

ECRESULT ECDatabaseMySQL::CreateDatabase()
{
	ECRESULT	er = erSuccess;
	string		strQuery;
	char*		lpDatabase = m_lpConfig->GetSetting("mysql_database");
	char*		lpMysqlPort = m_lpConfig->GetSetting("mysql_port");
	char*		lpMysqlSocket = m_lpConfig->GetSetting("mysql_socket");

	if(*lpMysqlSocket == '\0')
		lpMysqlSocket = NULL;

	// Zarafa database tables
	sSQLDatabase_t sDatabaseTables[] = {
		{"acl", Z_TABLEDEF_ACL},
		{"hierarchy", Z_TABLEDEF_HIERARCHY},
		{"names", Z_TABLEDEF_NAMES},
		{"mvproperties", Z_TABLEDEF_MVPROPERTIES},
		{"tproperties", Z_TABLEDEF_TPROPERTIES},
		{"properties", Z_TABLEDEF_PROPERTIES},
		{"delayedupdate", Z_TABLEDEF_DELAYEDUPDATE},
		{"receivefolder", Z_TABLEDEF_RECEIVEFOLDER},

		{"stores", Z_TABLEDEF_STORES},
		{"users", Z_TABLEDEF_USERS},
		{"outgoingqueue", Z_TABLEDEF_OUTGOINGQUEUE},
		{"lob", Z_TABLEDEF_LOB},
		{"searchresults", Z_TABLEDEF_SEARCHRESULTS},
		{"changes", Z_TABLEDEF_CHANGES},
		{"syncs", Z_TABLEDEF_SYNCS},
		{"versions", Z_TABLEDEF_VERSIONS},
		{"indexedproperties", Z_TABLEDEF_INDEXED_PROPERTIES},
		{"settings", Z_TABLEDEF_SETTINGS},

		{"object", Z_TABLEDEF_OBJECT},
		{"objectproperty", Z_TABLEDEF_OBJECT_PROPERTY},
		{"objectmvproperty", Z_TABLEDEF_OBJECT_MVPROPERTY},
		{"objectrelation", Z_TABLEDEF_OBJECT_RELATION},

		{"singleinstances", Z_TABLEDEF_REFERENCES },
		{"abchanges", Z_TABLEDEF_ABCHANGES },
		{"syncedmessages", Z_TABLEDEFS_SYNCEDMESSAGES },
		{"clientupdatestatus", Z_TABLEDEF_CLIENTUPDATESTATUS },
	};

	// Zarafa database default data
	sSQLDatabase_t sDatabaseData[] = {
		{"users", Z_TABLEDATA_USERS},
		{"stores", Z_TABLEDATA_STORES},
		{"hierarchy", Z_TABLEDATA_HIERARCHY},
		{"properties", Z_TABLEDATA_PROPERTIES},
		{"acl", Z_TABLEDATA_ACL},
		{"settings", Z_TABLEDATA_SETTINGS},
		{"indexedproperties", Z_TABLEDATA_INDEXED_PROPERTIES},
	};

	er = InitEngine();
	if(er != erSuccess)
		goto exit;

	// Connect
	if(mysql_real_connect(&m_lpMySQL, 
			m_lpConfig->GetSetting("mysql_host"), 
			m_lpConfig->GetSetting("mysql_user"), 
			m_lpConfig->GetSetting("mysql_password"), 
			NULL, 
			(lpMysqlPort)?atoi(lpMysqlPort):0, 
			lpMysqlSocket, 0) == NULL)
	{
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if(lpDatabase == NULL) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to create database: Unknown database");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Create database %s", lpDatabase);

	er = IsInnoDBSupported();
	if(er != erSuccess)
		goto exit;

	strQuery = "CREATE DATABASE IF NOT EXISTS `"+std::string(m_lpConfig->GetSetting("mysql_database"))+"`";
	if(Query(strQuery) != erSuccess){
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to create database: %s", GetError().c_str());
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	strQuery = "USE `"+std::string(m_lpConfig->GetSetting("mysql_database"))+"`";
	er = DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

	// Database tables
	for (unsigned int i=0; i < (sizeof(sDatabaseTables) / sizeof(sSQLDatabase_t)); i++)
	{
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Create table: %s", sDatabaseTables[i].lpComment);
		er = DoInsert(sDatabaseTables[i].lpSQL);
		if(er != erSuccess)
			goto exit;	
	}

	// Add the default table data
	for (unsigned int i=0; i < (sizeof(sDatabaseData) / sizeof(sSQLDatabase_t)); i++)
	{
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Add table data for: %s", sDatabaseData[i].lpComment);
		er = DoInsert(sDatabaseData[i].lpSQL);
		if(er != erSuccess)
			goto exit;
	}

	er = InsertServerGUID(this);
	if(er != erSuccess)
		goto exit;

	// Add the release id in the database
	er = UpdateDatabaseVersion(Z_UPDATE_RELEASE_ID);
	if(er != erSuccess)
		goto exit;

	// Loop throught the update list
	for (unsigned int i=Z_UPDATE_RELEASE_ID; i < (sizeof(sUpdateList) / sizeof(sUpdateList_t)); i++)
	{
		er = UpdateDatabaseVersion(sUpdateList[i].ulVersion);
		if(er != erSuccess)
			goto exit;
	}

	
	m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Database is created");

exit:
	return er;
}

ECRESULT ECDatabaseMySQL::GetDatabaseVersion(unsigned int *lpulMajor, unsigned int *lpulMinor, unsigned int *lpulRevision, unsigned int *lpulDatabaseRevision)
{
	ECRESULT		er = erSuccess;
	string			strQuery;
	DB_RESULT		lpResult = NULL;
	DB_ROW			lpDBRow = NULL;

	strQuery = "SELECT major,minor,revision,databaserevision FROM versions ORDER BY major DESC, minor DESC, revision DESC, databaserevision DESC LIMIT 1";
	er = DoSelect(strQuery, &lpResult);
	if(er != erSuccess && mysql_errno(&m_lpMySQL) != ER_NO_SUCH_TABLE)
		goto exit;

	if(er != erSuccess || GetNumRows(lpResult) == 0) {
		// Ok, maybe < than version 5.10
		// check version

		strQuery = "SHOW COLUMNS FROM properties";
		er = DoSelect(strQuery, &lpResult);
		if(er != erSuccess)
			goto exit;

		lpDBRow = FetchRow(lpResult);
		er = ZARAFA_E_UNKNOWN_DATABASE;

		while(lpResult)
		{
			if(lpDBRow != NULL && lpDBRow[0] != NULL && stricmp(lpDBRow[0], "storeid") == 0)
			{
				*lpulMajor = 5;
				*lpulMinor = 0;
				*lpulRevision = 0;
				*lpulDatabaseRevision = 0;
				er = erSuccess;
				break;
			}
			lpDBRow = FetchRow(lpResult);
		}
		
		goto exit;
	}

	lpDBRow = FetchRow(lpResult);
	if( lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL || lpDBRow[3] == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	*lpulMajor = atoui(lpDBRow[0]);
	*lpulMinor = atoui(lpDBRow[1]);
	*lpulRevision = atoui(lpDBRow[2]);
	*lpulDatabaseRevision = atoui(lpDBRow[3]);

exit:
	if(lpResult)
		FreeResult(lpResult);

	return er;
}

ECRESULT ECDatabaseMySQL::IsUpdateDone(unsigned int ulDatabaseRevision, unsigned int ulRevision)
{
	ECRESULT		er = ZARAFA_E_NOT_FOUND;
	string			strQuery;
	DB_RESULT		lpResult = NULL;

	strQuery = "SELECT major,minor,revision,databaserevision FROM versions WHERE databaserevision = " + stringify(ulDatabaseRevision);
	if (ulRevision > 0)
		strQuery += " AND revision = " + stringify(ulRevision);

	strQuery += " ORDER BY major DESC, minor DESC, revision DESC, databaserevision DESC LIMIT 1";
	
	er = DoSelect(strQuery, &lpResult);
	if(er != erSuccess)
		goto exit;

	if(GetNumRows(lpResult) != 1)
		er = ZARAFA_E_NOT_FOUND;

exit:
    if(lpResult)
        FreeResult(lpResult);

	return er;
}

ECRESULT ECDatabaseMySQL::GetFirstUpdate(unsigned int *lpulDatabaseRevision)
{
	ECRESULT		er = erSuccess;
	DB_RESULT		lpResult = NULL;
	DB_ROW			lpDBRow = NULL;

	er = DoSelect("SELECT MIN(databaserevision) FROM versions", &lpResult);
	if(er != erSuccess && mysql_errno(&m_lpMySQL) != ER_NO_SUCH_TABLE)
		goto exit;
	else if(er == erSuccess)
		lpDBRow = FetchRow(lpResult);

	er = erSuccess;

	if (lpDBRow == NULL || lpDBRow[0] == NULL ) {
		*lpulDatabaseRevision = 0;
	}else
		*lpulDatabaseRevision = atoui(lpDBRow[0]);


exit:
    if(lpResult)
        FreeResult(lpResult);

	return er;
}

/** 
 * Update the database to the current version.
 * 
 * @param[in]  bForceUpdate possebly force upgrade
 * @param[out] strReport error message
 * 
 * @return Zarafa error code
 */
ECRESULT ECDatabaseMySQL::UpdateDatabase(bool bForceUpdate, std::string &strReport)
{
	ECRESULT		er = erSuccess;
	unsigned int	ulMajor = 0;
	unsigned int	ulMinor = 0;
	unsigned int	ulRevision = 0;
	unsigned int	ulDatabaseRevision = 0;
	bool			bUpdated = false;
	bool			bSkipped = false;
	unsigned int	ulDatabaseRevisionMin = 0;

	er = GetDatabaseVersion(&ulMajor, &ulMinor, &ulRevision, &ulDatabaseRevision);
	if(er != erSuccess)
		goto exit;

	
	er = GetFirstUpdate(&ulDatabaseRevisionMin);
	if(er != erSuccess)
		goto exit;

	//default error
	strReport = "Unable to upgrade zarafa from version ";
	strReport+= stringify(ulMajor) + std::string(".") + stringify(ulMinor) + std::string(".")+stringify(ulRevision);
	strReport+= std::string(" to ");
	strReport+= std::string(PROJECT_VERSION_DOT_STR) + std::string(".")+std::string(PROJECT_SVN_REV_STR);

	// Check version
	if(ulMajor == PROJECT_VERSION_MAJOR && ulMinor == PROJECT_VERSION_MINOR && ulRevision == PROJECT_VERSION_REVISION && ulDatabaseRevision == Z_UPDATE_LAST) {
		// up to date
		goto exit;
	}else if((ulMajor == PROJECT_VERSION_MAJOR && ulMinor == PROJECT_VERSION_MINOR && ulRevision > PROJECT_VERSION_REVISION) ||
			 (ulMajor == PROJECT_VERSION_MAJOR && ulMinor > PROJECT_VERSION_MINOR) || 
			 (ulMajor > PROJECT_VERSION_MAJOR)) {
		// Start a old server with a new database
		strReport = "Database version (" +stringify(ulRevision)+","+stringify(ulDatabaseRevision) +
			") is newer than the server version (" + stringify(PROJECT_VERSION_REVISION)+","+stringify(Z_UPDATE_LAST) + ")";
		er = ZARAFA_E_INVALID_VERSION;
		goto exit;
	}

	this->m_bForceUpdate = bForceUpdate;

	if (bForceUpdate)
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Manually forced the database upgrade; the option '--force-database-upgrade' was given.");

	// Loop throught the update list
	for (unsigned int i=ulDatabaseRevisionMin; i < (sizeof(sUpdateList) / sizeof(sUpdateList_t)); i++)
	{

		if ( (ulDatabaseRevisionMin > 0 && IsUpdateDone(sUpdateList[i].ulVersion) == hrSuccess) ||
			(sUpdateList[i].ulVersionMin != 0 && ulDatabaseRevision >= sUpdateList[i].ulVersion && 
			ulDatabaseRevision >= sUpdateList[i].ulVersionMin) ||
			(sUpdateList[i].ulVersionMin != 0 && IsUpdateDone(sUpdateList[i].ulVersionMin, PROJECT_VERSION_REVISION) == hrSuccess))
		{
			// Update already done, next
			continue;
		}

		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Start: %s", sUpdateList[i].lpszLogComment );

		er = Begin();
		if(er != erSuccess)
			goto exit;

		bSkipped = false;
		er = sUpdateList[i].lpFunction(this);
		if (er == ZARAFA_E_IGNORE_ME) {
			bSkipped = true;
			er = erSuccess;
		} else if (er == ZARAFA_E_USER_CANCEL) {
			goto exit; // Reason should be logged in the update itself.
		} else if (er != hrSuccess) {
			Rollback();
			m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Failed: Rollback database");
			goto exit;
		}

		er = UpdateDatabaseVersion(sUpdateList[i].ulVersion);
		if(er != erSuccess)
			goto exit;

		er = Commit();
		if(er != erSuccess)
			goto exit;

		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"%s: %s", bSkipped ? "Skipped" : "Done", sUpdateList[i].lpszLogComment);

		bUpdated = true;
	}

	// Ok, no changes for the database, but for update history we add a version record
	if(bUpdated == false) {
		// Update version table
		er = UpdateDatabaseVersion(Z_UPDATE_LAST);
		if(er != erSuccess)
			goto exit;
	}

exit:

	return er;
}

ECRESULT ECDatabaseMySQL::UpdateDatabaseVersion(unsigned int ulDatabaseRevision)
{
	ECRESULT	er = erSuccess;
	string		strQuery;

	// Insert version number
	strQuery = "INSERT INTO versions (major, minor, revision, databaserevision, updatetime) VALUES(";
	strQuery += stringify(PROJECT_VERSION_MAJOR) + std::string(", ") + stringify(PROJECT_VERSION_MINOR) + std::string(", ");
	strQuery += std::string("'") + std::string(PROJECT_SVN_REV_STR) +  std::string("', ") + stringify(ulDatabaseRevision) + ", FROM_UNIXTIME("+stringify(time(NULL))+") )";

	er = DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

exit:
	return er;
}
/**
 * Validate all database tables
*/
ECRESULT ECDatabaseMySQL::ValidateTables()
{
	ECRESULT	er = erSuccess;
	string		strQuery;
	list<std::string> listTables;
	list<std::string> listErrorTables;
	list<std::string>::iterator iterTables;
	DB_RESULT	lpResult = NULL;
	DB_ROW		lpDBRow = NULL;

	er = DoSelect("SHOW TABLES", &lpResult);
	if(er != erSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to get all tables from the mysql database. %s", GetError().c_str());
		goto exit;
	}

	// Get all tables of the database
	while( (lpDBRow = FetchRow(lpResult))) {
		if (lpDBRow == NULL || lpDBRow[0] == NULL) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Wrong table information.");
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		listTables.insert(listTables.end(), lpDBRow[0]);
	}
	if(lpResult) FreeResult(lpResult);
	lpResult = NULL;

	for(iterTables = listTables.begin(); iterTables != listTables.end(); iterTables++) {
		er = DoSelect("CHECK TABLE " + *iterTables, &lpResult);
		if(er != erSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to check table '%s'", iterTables->c_str());
			goto exit;
		}

		lpDBRow = FetchRow(lpResult);
		if (lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Wrong check table information.");
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		m_lpLogger->Log(EC_LOGLEVEL_INFO,"%30s | %15s | %s", lpDBRow[0], lpDBRow[2], lpDBRow[3]);
		if (strcmp(lpDBRow[2], "error") == 0)
			listErrorTables.insert(listErrorTables.end(), lpDBRow[0]);

		if(lpResult) FreeResult(lpResult);
		lpResult = NULL;
	}

	if (!listErrorTables.empty())
	{
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Rebuild tables.");
		for(iterTables = listErrorTables.begin(); iterTables != listErrorTables.end(); iterTables++) {


			er = DoUpdate("ALTER TABLE " + *iterTables + " FORCE");
			if(er != erSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to fix table '%s'", iterTables->c_str());
				break;
			}
		}
		if(er != erSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Rebuild tables failed. Error code 0x%08x", er);
		} else {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Rebuild tables done.");
		}
	}//	if (!listErrorTables.empty())

exit:
	if(lpResult)
		FreeResult(lpResult);

	return er;
}
