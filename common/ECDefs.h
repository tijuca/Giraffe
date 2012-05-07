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

#ifndef ECDEFS_H
#define ECDEFS_H

// Get permission type
#define ACCESS_TYPE_DENIED		1
#define ACCESS_TYPE_GRANT		2
#define ACCESS_TYPE_BOTH		3

#define	ecRightsNone			0x00000000L
#define ecRightsReadAny			0x00000001L
#define	ecRightsCreate			0x00000002L
#define	ecRightsEditOwned		0x00000008L
#define	ecRightsDeleteOwned		0x00000010L
#define	ecRightsEditAny			0x00000020L
#define	ecRightsDeleteAny		0x00000040L
#define	ecRightsCreateSubfolder	0x00000080L
#define	ecRightsFolderAccess	0x00000100L
//#define	ecrightsContact			0x00000200L
#define	ecRightsFolderVisible	0x00000400L

#define ecRightsTemplateNoRights	ecRightsFolderVisible
#define ecRightsTemplateReadOnly	ecRightsTemplateNoRights | ecRightsReadAny
#define ecRightsTemplateSecretary	ecRightsTemplateReadOnly | ecRightsCreate | ecRightsEditOwned | ecRightsDeleteOwned | ecRightsEditAny | ecRightsDeleteAny
#define ecRightsTemplateOwner		ecRightsTemplateSecretary | ecRightsCreateSubfolder | ecRightsFolderAccess

/* #define ecRightsTemplateReviewer	ecRightsTemplateReadOnly */
/* #define ecRightsTemplateAuthor		ecRightsTemplateReadOnly | ecRightsCreate | ecRightsEditOwned | ecRightsDeleteOwned */
/* #define ecRightsTemplateEditor		ecRightsTemplateReadOnly | ecRightsCreate | ecRightsEditAny | ecRightsDeleteAny */

#define ecRightsAll				0x000005FBL
#define ecRightsFullControl		0x000004FBL
#define ecRightsDefault			ecRightsNone | ecRightsFolderVisible
#define ecRightsDefaultPublic	ecRightsReadAny | ecRightsFolderVisible
#define	ecRightsAdmin			0x00001000L

#define	ecRightsAllMask			0x000015FBL

// Right change indication (state field in struct)
#define RIGHT_NORMAL				0x00
#define RIGHT_NEW					0x01
#define RIGHT_MODIFY				0x02
#define RIGHT_DELETED				0x04
#define RIGHT_AUTOUPDATE_DENIED		0x08

#define OBJECTCLASS(__type, __class) \
	( (((__type) << 16) | ((__class) & 0xffff)) )
#define OBJECTCLASS_CLASSTYPE(__class) \
	( ((__class) & 0xffff0000) )
#define OBJECTCLASS_TYPE(__class) \
	( (objecttype_t)(((__class) >> 16) & 0xffff) )
#define OBJECTCLASS_ISTYPE(__class) \
	( (((__class) & 0xffff) == 0) && (((__class) >> 16) != 0) )
#define OBJECTCLASS_FIELD_COMPARE(__left, __right) \
	( !(__left) || !(__right) || (__left) == (__right) )
#define OBJECTCLASS_COMPARE(__left, __right) \
	( OBJECTCLASS_FIELD_COMPARE(OBJECTCLASS_TYPE(__left), OBJECTCLASS_TYPE(__right)) && \
	  OBJECTCLASS_FIELD_COMPARE((__left) & 0xffff, (__right) & 0xffff) )
#define OBJECTCLASS_COMPARE_SQL(__column, __objclass) \
	string((!(__objclass)) ? \
			"TRUE" : \
			((__objclass) & 0xffff) ? \
				__column " = " + stringify(__objclass) : \
				"(" __column " & 0xffff0000) = " + stringify((__objclass) & 0xffff0000))

enum objecttype_t {
	OBJECTTYPE_UNKNOWN		= 0,
	OBJECTTYPE_MAILUSER		= 1,
	OBJECTTYPE_DISTLIST		= 3,
	OBJECTTYPE_CONTAINER	= 4
};

enum objectclass_t {
	OBJECTCLASS_UNKNOWN		= OBJECTCLASS(OBJECTTYPE_UNKNOWN, 0),

	/* All User (active and nonactive) objectclasses */
	OBJECTCLASS_USER		= OBJECTCLASS(OBJECTTYPE_MAILUSER, 0),
	ACTIVE_USER				= OBJECTCLASS(OBJECTTYPE_MAILUSER, 1),
	NONACTIVE_USER			= OBJECTCLASS(OBJECTTYPE_MAILUSER, 2),
	NONACTIVE_ROOM			= OBJECTCLASS(OBJECTTYPE_MAILUSER, 3),
	NONACTIVE_EQUIPMENT		= OBJECTCLASS(OBJECTTYPE_MAILUSER, 4),
	NONACTIVE_CONTACT		= OBJECTCLASS(OBJECTTYPE_MAILUSER, 5),

	/* All distribution lists */
	OBJECTCLASS_DISTLIST	= OBJECTCLASS(OBJECTTYPE_DISTLIST, 0),
	DISTLIST_GROUP			= OBJECTCLASS(OBJECTTYPE_DISTLIST, 1),
	DISTLIST_SECURITY		= OBJECTCLASS(OBJECTTYPE_DISTLIST, 2),
	DISTLIST_DYNAMIC		= OBJECTCLASS(OBJECTTYPE_DISTLIST, 3),

	/* All container objects */
	OBJECTCLASS_CONTAINER	= OBJECTCLASS(OBJECTTYPE_CONTAINER, 0),
	CONTAINER_COMPANY		= OBJECTCLASS(OBJECTTYPE_CONTAINER, 1),
	CONTAINER_ADDRESSLIST	= OBJECTCLASS(OBJECTTYPE_CONTAINER, 2)
};

enum userobject_relation_t {
	OBJECTRELATION_GROUP_MEMBER = 1,
	OBJECTRELATION_COMPANY_VIEW = 2,
	OBJECTRELATION_COMPANY_ADMIN = 3,
	OBJECTRELATION_QUOTA_USERRECIPIENT = 4,
	OBJECTRELATION_QUOTA_COMPANYRECIPIENT = 5,
	OBJECTRELATION_USER_SENDAS = 6,
	OBJECTRELATION_ADDRESSLIST_MEMBER = 7
};

// Warning, those values are the same as ECSecurity::eQuotaStatus
enum eQuotaStatus{ QUOTA_OK, QUOTA_WARN, QUOTA_SOFTLIMIT, QUOTA_HARDLIMIT};

enum userobject_admin_level_t {
	ADMIN_LEVEL_ADMIN = 1,		/* Administrator over user's own company. */
	ADMIN_LEVEL_SYSADMIN = 2		/* System administrator (same rights as SYSTEM). */
};

typedef struct _sECEntryId
{
	unsigned int	cb;
	unsigned char*	lpb;
} ECENTRYID, *LPECENTRYID;

typedef struct _sECServerNameList
{
	unsigned int	cServers;
	LPTSTR*			lpszaServer;
} ECSVRNAMELIST, *LPECSVRNAMELIST;

typedef struct _sPropmapEntry {
	unsigned int	ulPropId;
	LPTSTR			lpszValue;
} SPROPMAPENTRY, *LPSPROPMAPENTRY;

typedef struct _sPropmap {
	unsigned int		cEntries;
	LPSPROPMAPENTRY		lpEntries;
} SPROPMAP, *LPSPROPMAP;

typedef struct _sMVPropmapEntry {
	unsigned int	ulPropId;
	int				cValues;
	LPTSTR*			lpszValues;
} MVPROPMAPENTRY, *LPMVPROPMAPENTRY;

typedef struct _sMVPropmap {
	unsigned int		cEntries;
	LPMVPROPMAPENTRY	lpEntries;
} MVPROPMAP, *LPMVPROPMAP;

typedef struct _sECUser {
	LPTSTR			lpszUsername;	// username@companyname
	LPTSTR			lpszPassword;
	LPTSTR			lpszMailAddress;
	LPTSTR			lpszFullName;
	LPTSTR			lpszServername;
	objectclass_t	ulObjClass;
	unsigned int	ulIsAdmin;		// See userobject_admin_level_t
	unsigned int	ulIsABHidden;	// Is user hidden from address book
	unsigned int	ulCapacity;		// Resource capacity
	SPROPMAP		sPropmap;		// Extra anonymous properties for addressbook
	MVPROPMAP		sMVPropmap;		// Extra anonymous MV properties for addressbook
	ECENTRYID		sUserId;
} ECUSER, *LPECUSER;

typedef struct _sECGroup {
	LPTSTR			lpszGroupname; // groupname@companyname
	LPTSTR			lpszFullname;
	LPTSTR			lpszFullEmail;
	ECENTRYID		sGroupId;
	unsigned int	ulIsABHidden;	// Is group hidden from address book
	SPROPMAP		sPropmap;		// Extra anonymous properties for addressbook
	MVPROPMAP		sMVPropmap;		// Extra anonymous MV properties for addressbook
}ECGROUP, *LPECGROUP;

typedef struct _sECCompany {
	ECENTRYID		sAdministrator; // userid of the administrator
	LPTSTR			lpszCompanyname;
	LPTSTR			lpszServername;
	ECENTRYID		sCompanyId;
	unsigned int	ulIsABHidden;	// Is company hidden from address book
	SPROPMAP		sPropmap;		// Extra anonymous properties for addressbook
	MVPROPMAP		sMVPropmap;		// Extra anonymous MV properties for addressbook
} ECCOMPANY, *LPECCOMPANY;


typedef struct _sUserClientUpdateStatus {
	unsigned int	ulTrackId;
	time_t			tUpdatetime;
	LPTSTR			lpszCurrentversion;
	LPTSTR			lpszLatestversion;
	LPTSTR			lpszComputername;
	unsigned int 	ulStatus;
}ECUSERCLIENTUPDATESTATUS, *LPECUSERCLIENTUPDATESTATUS;

#define UPDATE_STATUS_UNKNOWN	0
#define UPDATE_STATUS_SUCCESS   1
#define UPDATE_STATUS_PENDING   2
#define UPDATE_STATUS_FAILED    3

typedef struct _sECPermission {
	unsigned int	ulType;
	unsigned int	ulRights;
	unsigned int	ulState;
	ECENTRYID		sUserId;
}ECPERMISSION, *LPECPERMISSION;

typedef struct _sECQuota {
	bool			bUseDefaultQuota;
	bool			bIsUserDefaultQuota; // Default quota for users within company
	long long		llWarnSize;
	long long		llSoftSize;
	long long		llHardSize;
}ECQUOTA, *LPECQUOTA;

typedef struct _sECQuotaStatus {
	long long		llStoreSize;
	eQuotaStatus	quotaStatus;
}ECQUOTASTATUS, *LPECQUOTASTATUS;

typedef struct _sECServer {
	LPTSTR	lpszName;
	LPTSTR	lpszFilePath;
	LPTSTR	lpszHttpPath;
	LPTSTR	lpszSslPath;
	LPTSTR	lpszPreferedPath;
	ULONG	ulFlags;
}ECSERVER, *LPECSERVER;

typedef struct _sECServerList {
	unsigned int	cServers;
	LPECSERVER		lpsaServer;
}ECSERVERLIST, *LPECSERVERLIST;

// Flags for ns__submitMessage
#define EC_SUBMIT_LOCAL			0x00000000
#define EC_SUBMIT_MASTER		0x00000001

#define EC_SUBMIT_DOSENTMAIL	0x00000002

// GetServerDetails
#define EC_SERVERDETAIL_NO_NAME			0x00000001
#define EC_SERVERDETAIL_FILEPATH		0x00000002
#define EC_SERVERDETAIL_HTTPPATH		0x00000004
#define EC_SERVERDETAIL_SSLPATH			0x00000008
#define EC_SERVERDETAIL_PREFEREDPATH	0x00000010

#define EC_SDFLAG_IS_PEER		0x00000001
#define EC_SDFLAG_HAS_PUBLIC	0x00000002

// CreateStore flag(s)
#define EC_OVERRIDE_HOMESERVER			0x00000001

#endif
