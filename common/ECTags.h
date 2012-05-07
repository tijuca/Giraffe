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

#ifndef ECTAGS_H
#define ECTAGS_H

// Public Zarafa properties
#define PR_EC_BASE			0x6700

////////////////////////////////////////////////////////////////////////////////
// Profile properties
//

#define PR_EC_PATH						PROP_TAG(PT_STRING8,	PR_EC_BASE+0x00)
#define PR_EC_USERNAME					PROP_TAG(PT_TSTRING,	PR_EC_BASE+0x01)
#define PR_EC_USERNAME_A				PROP_TAG(PT_STRING8,	PR_EC_BASE+0x01)
#define PR_EC_USERNAME_W				PROP_TAG(PT_UNICODE,	PR_EC_BASE+0x01)
#define PR_EC_USERPASSWORD				PROP_TAG(PT_TSTRING,	PR_EC_BASE+0x02)
#define PR_EC_USERPASSWORD_A			PROP_TAG(PT_STRING8,	PR_EC_BASE+0x02)
#define PR_EC_USERPASSWORD_W			PROP_TAG(PT_UNICODE,	PR_EC_BASE+0x02)
#define PR_EC_PORT						PROP_TAG(PT_STRING8,	PR_EC_BASE+0x03)
#define PR_EC_FLAGS						PROP_TAG(PT_LONG,		PR_EC_BASE+0x04)
#define PR_EC_SSLKEY_FILE				PROP_TAG(PT_STRING8,	PR_EC_BASE+0x05)
#define PR_EC_SSLKEY_PASS				PROP_TAG(PT_STRING8,	PR_EC_BASE+0x06)
#define PR_EC_LAST_CONNECTIONTYPE		PROP_TAG(PT_LONG,		PR_EC_BASE+0x09)
#define PR_EC_CONNECTION_TIMEOUT		PROP_TAG(PT_LONG,		PR_EC_BASE+0x0A)

// Used for proxy settings
#define PR_EC_PROXY_HOST				PROP_TAG(PT_STRING8,	PR_EC_BASE+0x0B)
#define PR_EC_PROXY_PORT				PROP_TAG(PT_LONG,		PR_EC_BASE+0x0C)
#define PR_EC_PROXY_USERNAME			PROP_TAG(PT_STRING8,	PR_EC_BASE+0x0D)
#define PR_EC_PROXY_PASSWORD			PROP_TAG(PT_STRING8,	PR_EC_BASE+0x0E)
#define PR_EC_PROXY_FLAGS				PROP_TAG(PT_LONG,		PR_EC_BASE+0x0F)

#define PR_EC_OFFLINE_PATH				PROP_TAG(PT_TSTRING,	PR_EC_BASE+0x10) /* same as PR_EC_CONTACT_ENTRYID, but on different objects! */
#define PR_EC_OFFLINE_PATH_A			PROP_TAG(PT_STRING8,	PR_EC_BASE+0x10) /* same as PR_EC_CONTACT_ENTRYID, but on different objects! */
#define PR_EC_OFFLINE_PATH_W			PROP_TAG(PT_UNICODE,	PR_EC_BASE+0x10) /* same as PR_EC_CONTACT_ENTRYID, but on different objects! */

#define PR_EC_SERVERNAME				PROP_TAG(PT_TSTRING,	PR_EC_BASE+0x11) /* same as PR_EC_HIERARCHYID, but on archive store profile sections! */
#define PR_EC_SERVERNAME_A				PROP_TAG(PT_STRING8,	PR_EC_BASE+0x11) /* same as PR_EC_HIERARCHYID, but on archive store profile sections! */
#define PR_EC_SERVERNAME_W				PROP_TAG(PT_UNICODE,	PR_EC_BASE+0x11) /* same as PR_EC_HIERARCHYID, but on archive store profile sections! */

/* same as properties below, but on different objects */
#define PR_ZC_CONTACT_STORE_ENTRYIDS	PROP_TAG(PT_MV_BINARY, PR_EC_BASE+0x11)
#define PR_ZC_CONTACT_FOLDER_ENTRYIDS	PROP_TAG(PT_MV_BINARY, PR_EC_BASE+0x12)
#define PR_ZC_CONTACT_FOLDER_NAMES		PROP_TAG(PT_MV_TSTRING, PR_EC_BASE+0x13)
#define PR_ZC_CONTACT_FOLDER_NAMES_A	PROP_TAG(PT_MV_STRING8, PR_EC_BASE+0x13)
#define PR_ZC_CONTACT_FOLDER_NAMES_W	PROP_TAG(PT_MV_UNICODE, PR_EC_BASE+0x13)

////////////////////////////////////////////////////////////////////////////////

// The property under which we actually save the PR_ENTRYID in recipient tables
#define PR_EC_CONTACT_ENTRYID			PROP_TAG(PT_BINARY,		PR_EC_BASE+0x10)
#define PR_EC_HIERARCHYID				PROP_TAG(PT_LONG,		PR_EC_BASE+0x11)
#define PR_EC_STOREGUID					PROP_TAG(PT_BINARY,		PR_EC_BASE+0x12)
/* 0x6712 == PR_RANK (edkmdb.h) */
#define PR_EC_COMPANYID					PROP_TAG(PT_LONG,		PR_EC_BASE+0x13)
#define PR_EC_STORETYPE					PROP_TAG(PT_LONG,		PR_EC_BASE+0x14)

#define PR_EC_QUOTA_MAIL_TIME			PROP_TAG(PT_SYSTIME,	PR_EC_BASE+0x20)
//NOTE:	The properties PR_QUOTA_WARNING_THRESHOLD, PR_QUOTA_SEND_THRESHOLD, PR_QUOTA_RECEIVE_THRESHOLD
//		are in the range of PR_EC_BASE+0x21 to PR_EC_BASE+0x23

#define PR_EC_STATSTABLE_SYSTEM			PROP_TAG(PT_OBJECT,		PR_EC_BASE+0x30)
#define PR_EC_STATSTABLE_SESSIONS		PROP_TAG(PT_OBJECT,		PR_EC_BASE+0x31)
#define PR_EC_STATSTABLE_USERS			PROP_TAG(PT_OBJECT,		PR_EC_BASE+0x32)
#define PR_EC_STATSTABLE_COMPANY		PROP_TAG(PT_OBJECT,		PR_EC_BASE+0x33)

/* system stats */
#define PR_EC_STATS_SYSTEM_DESCRIPTION	PROP_TAG(PT_STRING8,	PR_EC_BASE+0x40)
#define PR_EC_STATS_SYSTEM_VALUE		PROP_TAG(PT_STRING8,	PR_EC_BASE+0x41)
/* session stats */
#define PR_EC_STATS_SESSION_ID			PROP_TAG(PT_LONGLONG,	PR_EC_BASE+0x42)
#define PR_EC_STATS_SESSION_IPADDRESS	PROP_TAG(PT_STRING8,	PR_EC_BASE+0x43)
#define PR_EC_STATS_SESSION_IDLETIME	PROP_TAG(PT_LONG,		PR_EC_BASE+0x44)
#define PR_EC_STATS_SESSION_CAPABILITY	PROP_TAG(PT_LONG,		PR_EC_BASE+0x45)
#define PR_EC_STATS_SESSION_LOCKED		PROP_TAG(PT_BOOLEAN,	PR_EC_BASE+0x46)
#define PR_EC_STATS_SESSION_BUSYSTATES	PROP_TAG(PT_MV_STRING8,	PR_EC_BASE+0x47)

/* currently only used in user stats table */
#define PR_EC_COMPANY_NAME				PROP_TAG(PT_TSTRING,	PR_EC_BASE+0x48)
#define PR_EC_COMPANY_NAME_A			PROP_TAG(PT_STRING8,	PR_EC_BASE+0x48)
#define PR_EC_COMPANY_NAME_W			PROP_TAG(PT_UNICODE,	PR_EC_BASE+0x48)
#define PR_EC_COMPANY_ADMIN				PROP_TAG(PT_TSTRING,	PR_EC_BASE+0x49)
#define PR_EC_COMPANY_ADMIN_A			PROP_TAG(PT_STRING8,	PR_EC_BASE+0x49)
#define PR_EC_COMPANY_ADMIN_W			PROP_TAG(PT_UNICODE,	PR_EC_BASE+0x49)

#define PR_EC_STATS_SESSION_CPU_USER	PROP_TAG(PT_DOUBLE,		PR_EC_BASE+0x4a)
#define PR_EC_STATS_SESSION_CPU_SYSTEM	PROP_TAG(PT_DOUBLE,		PR_EC_BASE+0x4b)
#define PR_EC_STATS_SESSION_CPU_REAL	PROP_TAG(PT_DOUBLE,		PR_EC_BASE+0x4c)

#define PR_EC_STATS_SESSION_GROUP_ID	PROP_TAG(PT_LONGLONG,	PR_EC_BASE+0x4d)
#define PR_EC_STATS_SESSION_PEER_PID	PROP_TAG(PT_LONG,		PR_EC_BASE+0x4e)
#define PR_EC_STATS_SESSION_CLIENT_VERSION	PROP_TAG(PT_STRING8,	PR_EC_BASE+0x4f)
#define PR_EC_STATS_SESSION_CLIENT_APPLICATION PROP_TAG(PT_STRING8, PR_EC_BASE+0x50)
#define PR_EC_STATS_SESSION_REQUESTS	PROP_TAG(PT_LONG, 		PR_EC_BASE+0x51)

#define PR_EC_OUTOFOFFICE				PROP_TAG(PT_BOOLEAN,	PR_EC_BASE+0x60)
#define PR_EC_OUTOFOFFICE_MSG			PROP_TAG(PT_TSTRING,	PR_EC_BASE+0x61)
#define PR_EC_OUTOFOFFICE_MSG_A			PROP_TAG(PT_STRING8,	PR_EC_BASE+0x61)
#define PR_EC_OUTOFOFFICE_MSG_W			PROP_TAG(PT_UNICODE,	PR_EC_BASE+0x61)
#define PR_EC_OUTOFOFFICE_SUBJECT		PROP_TAG(PT_TSTRING, 	PR_EC_BASE+0x62)
#define PR_EC_OUTOFOFFICE_SUBJECT_A		PROP_TAG(PT_STRING8, 	PR_EC_BASE+0x62)
#define PR_EC_OUTOFOFFICE_SUBJECT_W		PROP_TAG(PT_UNICODE, 	PR_EC_BASE+0x62)

#define PR_EC_WEBACCESS_SETTINGS		PROP_TAG(PT_STRING8, PR_EC_BASE+0x70)
#define PR_EC_RECIPIENT_HISTORY			PROP_TAG(PT_STRING8, PR_EC_BASE+0x71)


// The hidden object property which can be used to access the underlying IECUnknown object though a pointer in lpszA
#define PR_EC_OBJECT					PROP_TAG(PT_OBJECT,		PR_EC_BASE+0x7f)

// Contains the 'flags' column in the outgoing queue (EC_SUBMIT_{MASTER,LOCAL,DOSENTMAIL})
#define PR_EC_OUTGOING_FLAGS			PROP_TAG(PT_LONG,		PR_EC_BASE+0x80)

// Contains the accountname for a store (to be used from for example the outgoing queue)
#define PR_EC_MAILBOX_OWNER_ACCOUNT		PROP_TAG(PT_TSTRING,	PR_EC_BASE+0x81)
#define PR_EC_MAILBOX_OWNER_ACCOUNT_A	PROP_TAG(PT_STRING8,	PR_EC_BASE+0x81)
#define PR_EC_MAILBOX_OWNER_ACCOUNT_W	PROP_TAG(PT_UNICODE,	PR_EC_BASE+0x81)

// Contains an IMAP-compatible UID for each message. It is hidden from GetPropList(), but gettable in GetProps and in tables
// Note that we save a 64-bit counter, but the property is only 32-bit because IMAP requires it to be 32-bit.
#define PR_EC_IMAP_ID					PROP_TAG(PT_LONG,	PR_EC_BASE+0x82)
// EntryIds of subscribed folders
// format: <ULONG:number_entries>[<ULONG:cb><BYTE*cb>]
#define PR_EC_IMAP_SUBSCRIBED			PROP_TAG(PT_BINARY,	PR_EC_BASE+0x84)
// Stores the last-read IMAP UID in a folder
#define PR_EC_IMAP_MAX_ID				PROP_TAG(PT_LONG, 	PR_EC_BASE+0x85)
// Stores the DATE part of an e-mail, disregarding timezone
#define PR_EC_CLIENT_SUBMIT_DATE		PROP_TAG(PT_SYSTIME, PR_EC_BASE+0x86)
// Stores the DATE part of an e-mail, disregarding timezone
#define PR_EC_MESSAGE_DELIVERY_DATE		PROP_TAG(PT_SYSTIME, PR_EC_BASE+0x87)
// Complete email for IMAP optimizations
#define PR_EC_IMAP_EMAIL				PROP_TAG(PT_BINARY,	PR_EC_BASE+0x8C)
#define PR_EC_IMAP_EMAIL_SIZE			PROP_TAG(PT_LONG,	PR_EC_BASE+0x8D)
#define PR_EC_IMAP_BODY					PROP_TAG(PT_STRING8,	PR_EC_BASE+0x8E)
#define PR_EC_IMAP_BODYSTRUCTURE		PROP_TAG(PT_STRING8,	PR_EC_BASE+0x8F)

// addressbook entryids of users which are allowed to send as
#define PR_EC_SENDAS_USER_ENTRYIDS		PROP_TAG(PT_MV_BINARY,	PR_EC_BASE+0x83)

// addressbook ADS legacyExchange DN
#define PR_EC_EXCHANGE_DN				PROP_TAG(PT_TSTRING,PR_EC_BASE+0x88)
#define PR_EC_EXCHANGE_DN_A				PROP_TAG(PT_STRING8,PR_EC_BASE+0x88)
#define PR_EC_EXCHANGE_DN_W				PROP_TAG(PT_UNICODE,PR_EC_BASE+0x88)

// notification based syncronization
#define PR_EC_CHANGE_ADVISOR			PROP_TAG(PT_OBJECT, PR_EC_BASE+0x89)
#define PR_EC_CHANGE_ONL_STATE			PROP_TAG(PT_BINARY, PR_EC_BASE+0x8A)
#define PR_EC_CHANGE_OFFL_STATE			PROP_TAG(PT_BINARY, PR_EC_BASE+0x8B)

// server path to home server of a user
#define PR_EC_SERVERPATH				PROP_TAG(PT_STRING8,	PR_EC_BASE+0xC0)
#define PR_EC_HOMESERVER_NAME			PROP_TAG(PT_TSTRING,	PR_EC_BASE+0xC1)
#define PR_EC_HOMESERVER_NAME_A			PROP_TAG(PT_STRING8,	PR_EC_BASE+0xC1)
#define PR_EC_HOMESERVER_NAME_W			PROP_TAG(PT_UNICODE,	PR_EC_BASE+0xC1)
#define PR_EC_SERVER_UID				PROP_TAG(PT_BINARY,		PR_EC_BASE+0xC2)
#define PR_EC_DELETED_STORE				PROP_TAG(PT_BOOLEAN,	PR_EC_BASE+0xC3)
#define PR_EC_ARCHIVE_SERVERS			PROP_TAG(PT_MV_TSTRING,	PR_EC_BASE+0xC4)
#define PR_EC_ARCHIVE_SERVERS_A			PROP_TAG(PT_MV_STRING8,	PR_EC_BASE+0xC4)
#define PR_EC_ARCHIVE_SERVERS_W			PROP_TAG(PT_MV_UNICODE,	PR_EC_BASE+0xC4)
#define PR_EC_ARCHIVE_COUPLINGS			PROP_TAG(PT_MV_TSTRING,	PR_EC_BASE+0xC5)
#define PR_EC_ARCHIVE_COUPLINGS_A		PROP_TAG(PT_MV_STRING8,	PR_EC_BASE+0xC5)
#define PR_EC_ARCHIVE_COUPLINGS_W		PROP_TAG(PT_MV_UNICODE,	PR_EC_BASE+0xC5)

#define PR_EC_SEARCHFOLDER_STATUS		PROP_TAG(PT_LONG,	PR_EC_BASE+0x90)

#define PR_EC_OFFLINE_SYNC_STATUS		PROP_TAG(PT_BINARY,	PR_EC_BASE+0xA0)
#define PR_EC_ONLINE_SYNC_STATUS		PROP_TAG(PT_BINARY,	PR_EC_BASE+0xA1)
#define PR_EC_AB_SYNC_STATUS			PROP_TAG(PT_BINARY,	PR_EC_BASE+0xA2)
#define PR_EC_SYNC_WAIT_TIME			PROP_TAG(PT_LONG,	PR_EC_BASE+0xA3)
#define PR_EC_SYNC_ON_NOTIFY			PROP_TAG(PT_LONG,	PR_EC_BASE+0xA4)
#define PR_EC_RESYNC_ID					PROP_TAG(PT_LONG,	PR_EC_BASE+0xA5)
#define PR_EC_STORED_SERVER_UID			PROP_TAG(PT_BINARY,	PR_EC_BASE+0xA6)


// Properties for IMailUsers
#define PR_EC_NONACTIVE					PROP_TAG(PT_BOOLEAN,	PR_EC_BASE+0xB0)
#define PR_EC_ADMINISTRATOR				PROP_TAG(PT_LONG,		PR_EC_BASE+0xB1) // Maps to 'ulIsAdmin' in ECUser

#define PR_EC_MGR_ORG_ENTRYID			PROP_TAG(PT_BINARY,		PR_EC_BASE+0xB2) //this property will store original entry id of a migrated message
#define PR_EC_ENABLED_FEATURES			PROP_TAG(PT_MV_TSTRING, PR_EC_BASE+0xB3)
#define PR_EC_ENABLED_FEATURES_A		PROP_TAG(PT_MV_STRING8, PR_EC_BASE+0xB3)
#define PR_EC_ENABLED_FEATURES_W		PROP_TAG(PT_MV_UNICODE, PR_EC_BASE+0xB3)
#define PR_EC_DISABLED_FEATURES			PROP_TAG(PT_MV_TSTRING, PR_EC_BASE+0xB4)
#define PR_EC_DISABLED_FEATURES_A		PROP_TAG(PT_MV_STRING8, PR_EC_BASE+0xB4)
#define PR_EC_DISABLED_FEATURES_W		PROP_TAG(PT_MV_UNICODE, PR_EC_BASE+0xB4)

#define PR_EC_PUBLIC_IPM_SUBTREE_ENTRYID	PROP_TAG(PT_BINARY,	PR_EC_BASE+0xD0)

// Special WA properties used between zarafa-dagent and WA
#define PR_EC_WA_ATTACHMENT_HIDDEN_OVERRIDE	PROP_TAG(PT_BOOLEAN,PR_EC_BASE+0xE0)

// Flags for PR_EC_FLAGS
#define EC_PROFILE_FLAGS_NO_NOTIFICATIONS		0x0000001
#define EC_PROFILE_FLAGS_NO_COMPRESSION			0x0000002
#define EC_PROFILE_FLAGS_NO_PUBLIC_STORE		0x0000004
#define EC_PROFILE_FLAGS_OFFLINE				0x0000080
#define EC_PROFILE_FLAGS_CACHE_PRIVATE			0x0000100
#define EC_PROFILE_FLAGS_CACHE_PUBLIC			0x0000400
#define EC_PROFILE_FLAGS_TRUNCATE_SOURCEKEY		0x0000800		// Truncate PR_SOURCE_KEY to 22 bytes (from 24 bytes)

// Zarafa internal flags
#define EC_PROVIDER_OFFLINE						0x0F00000

// Flags for PR_EC_PROXY_FLAGS
#define EC_PROFILE_PROXY_FLAGS_USE_PROXY		0x0000001

#define EC_SEARCHFOLDER_STATUS_RUNNING 0
#define EC_SEARCHFOLDER_STATUS_REBUILD 1
#define EC_SEARCHFOLDER_STATUS_STOPPED 2

// Type of store
#define ECSTORE_TYPE_PRIVATE	0
#define ECSTORE_TYPE_PUBLIC		1
#define ECSTORE_TYPE_ARCHIVE	2

#define ECSTORE_TYPE_MASK_PRIVATE (1 << ECSTORE_TYPE_PRIVATE)
#define ECSTORE_TYPE_MASK_PUBLIC (1 << ECSTORE_TYPE_PUBLIC)
#define ECSTORE_TYPE_MASK_ARCHIVE (1 << ECSTORE_TYPE_ARCHIVE)

#define ECSTORE_TYPE_ISVALID(ulStoreType)		\
	((ulStoreType) == ECSTORE_TYPE_PRIVATE ||	\
	 (ulStoreType) == ECSTORE_TYPE_PUBLIC ||	\
	 (ulStoreType) == ECSTORE_TYPE_ARCHIVE)

// Flags for IECServiceAdmin::PurgeCache()
#define PURGE_CACHE_QUOTA			0x0001
#define PURGE_CACHE_QUOTADEFAULT	0x0002	
#define PURGE_CACHE_OBJECTS			0x0004
#define PURGE_CACHE_STORES			0x0008
#define PURGE_CACHE_ACL				0x0010
#define PURGE_CACHE_CELL			0x0020
#define PURGE_CACHE_INDEX1			0x0040
#define PURGE_CACHE_INDEX2			0x0080
#define PURGE_CACHE_INDEXEDPROPERTIES 0x0100
#define PURGE_CACHE_USEROBJECT		0x0200
#define PURGE_CACHE_EXTERNID		0x0400
#define PURGE_CACHE_USERDETAILS		0x0800
#define PURGE_CACHE_SERVER			0x1000
#define PURGE_CACHE_ALL				0xFFFFFFFF

// Extra table flag, do not let the server cap contents on 255 characters
// 1 bit under the MAPI_UNICODE bit, hopefully we won't clash with Exchange bits later on
#define EC_TABLE_NOCAP			0x40000000

#endif
