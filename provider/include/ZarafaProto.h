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

//gsoap ns service name:	ZarafaCmd
//gsoap ns service name:	ZarafaCmd
//gsoap ns service style:	rpc
//gsoap ns service encoding:	encoded
//gsoap ns service location:	http://localhost:236/zarafa
//gsoap ns service namespace: urn:zarafa
//gsoap ns service method-action: ZarafaCmd ""

#import "xop.h"
#import "xmlmime.h"

struct hiloLong {
	int hi;
	unsigned int lo;
};

// This is actually implemented in SOAP as a base64, not as an array of unsigned bytes
struct xsd__base64Binary {
	unsigned char *__ptr;
	int __size;
};

struct xsd__Binary {
	_xop__Include xop__Include; // attachment
	@char *xmlmime__contentType; // and its contentType 
};

typedef struct xsd__base64Binary entryId;

struct mv_i2 {
	short int *__ptr;
	int __size;
};

struct mv_long {
	unsigned int *__ptr;
	int __size;
};

struct mv_r4 {
	float *__ptr;
	int __size;
};

struct mv_double {
	double *__ptr;
	int __size;
};

struct mv_string8 {
	char**__ptr;
	int __size;
};

struct mv_hiloLong {
	struct hiloLong *__ptr;
	int __size;
};

struct mv_binary {
	struct xsd__base64Binary *__ptr;
	int __size;
};

struct mv_i8 {
	LONG64 *__ptr;
	int __size;
};

struct restrictTable;

union propValData {
    short int           i;          /* case PT_I2 */
    unsigned int		ul;			/* case PT_ULONG */
    float               flt;        /* case PT_R4 */
    double              dbl;        /* case PT_DOUBLE */
    bool				b;          /* case PT_BOOLEAN */
    char*               lpszA;      /* case PT_STRING8 */
	struct hiloLong *	hilo;
	struct xsd__base64Binary *	bin;
    LONG64				li;         /* case PT_I8 */
	struct mv_i2			mvi;		/* case PT_MV_I2 */
	struct mv_long			mvl;		/* case PT_MV_LONG */
	struct mv_r4			mvflt;		/* case PT_MV_R4 */
	struct mv_double		mvdbl;		/* case PT_MV_DOUBLE */
    struct mv_string8		mvszA;		/* case PT_MV_STRING8 */
	struct mv_hiloLong		mvhilo;
	struct mv_binary		mvbin;
	struct mv_i8			mvli;		/* case PT_MV_I8 */
	struct restrictTable	*res;
	struct actions			*actions;
};

struct propVal {
	unsigned int ulPropTag;
	int __union;
	union propValData Value;
};

struct propValArray {
	struct propVal *__ptr;
	int __size;
};

struct propTagArray {
	unsigned int *__ptr;
	int __size;
};

struct entryList {
	unsigned int __size;
	entryId *__ptr;
};

struct saveObject {
	int __size;					/* # children */
	struct saveObject *__ptr;	/* child objects */

	struct propTagArray delProps;
	struct propValArray modProps;
	bool bDelete;				/* delete this object completely */
	unsigned int ulClientId;		/* id for the client (PR_ROWID or PR_ATTACH_NUM, otherwise unused) */
	unsigned int ulServerId;		/* hierarchyid of the server (0 for new item) */
	unsigned int ulObjType;
	struct entryList *lpInstanceIds;	/* Single Instance Id (NULL for new item, or if Single Instancing is unknown) */
};

struct loadObjectResponse {
	unsigned int er;
	struct saveObject sSaveObject;
};

struct logonResponse {
	unsigned int	er;
	ULONG64 ulSessionId;
	char			*lpszVersion;
	unsigned int	ulCapabilities;
	struct xsd__base64Binary sLicenseResponse;
	struct xsd__base64Binary sServerGuid;
};

struct ssoLogonResponse {
	unsigned int	er;
	ULONG64 ulSessionId;
	char			*lpszVersion;
	unsigned int	ulCapabilities;
	struct xsd__base64Binary *lpOutput;
	struct xsd__base64Binary sLicenseResponse;
	struct xsd__base64Binary sServerGuid;
};

struct getStoreResponse {
	unsigned int				er;
	entryId						sStoreId;	// id of store
	entryId						sRootId;	// root folder id of store
	struct xsd__base64Binary	guid;		// guid of store
	char						*lpszServerPath;
};

struct getStoreNameResponse {
	char			*lpszStoreName;
	unsigned int	er;
};

struct getStoreTypeResponse {
	unsigned int	ulStoreType;
	unsigned int	er;
};


// Warning, this is synched with MAPI's types!
enum SortOrderType { EC_TABLE_SORT_ASCEND=0, EC_TABLE_SORT_DESCEND, EC_TABLE_SORT_COMBINE, EC_TABLE_SORT_CATEG_MAX = 4, EC_TABLE_SORT_CATEG_MIN = 8};

struct sortOrder {
	unsigned int ulPropTag;
	unsigned int ulOrder;
};

struct sortOrderArray {
	struct sortOrder *__ptr;
	int __size;
};

struct readPropsResponse {
	unsigned int er;
	struct propTagArray aPropTag;
	struct propValArray aPropVal;
};

struct loadPropResponse {
	unsigned int er;
	struct propVal *lpPropVal;
};

struct createFolderResponse {
	unsigned int er;
	entryId	sEntryId;
};

struct tableOpenResponse {
	unsigned int er;
	unsigned int ulTableId;
};

struct tableOpenRequest {
    entryId sEntryId;
    unsigned int ulTableType;
    unsigned int ulType;
    unsigned int ulFlags;
};

struct tableSortRequest {
    struct sortOrderArray sSortOrder;
    unsigned int ulCategories;
    unsigned int ulExpanded;
};

struct tableQueryRowsRequest {
    unsigned int ulCount;
    unsigned int ulFlags;
};

struct rowSet {
	struct propValArray *__ptr;
	int __size;
};

struct tableQueryRowsResponse {
	unsigned int er;
	struct rowSet sRowSet;
};

struct tableQueryColumnsResponse {
	unsigned int er;
	struct propTagArray sPropTagArray;
};

struct tableGetRowCountResponse {
	unsigned int er;
	unsigned int ulCount;
	unsigned int ulRow;
};

struct tableSeekRowResponse {
	unsigned int er;
	int lRowsSought; // may be negative
};

struct tableBookmarkResponse {
	unsigned int er;
	unsigned int ulbkPosition;
};

struct tableExpandRowResponse {
    unsigned int er;
    struct rowSet rowSet;
    unsigned int ulMoreRows;
};

struct tableCollapseRowResponse {
    unsigned int er;
    unsigned int ulRows;
};

struct tableGetCollapseStateResponse {
    struct xsd__base64Binary sCollapseState;
    unsigned int er;
};

struct tableSetCollapseStateResponse {
    unsigned int ulBookmark;
    unsigned int er;
};

struct tableMultiRequest {
    unsigned int ulTableId;
	unsigned int ulFlags;
    struct tableOpenRequest *lpOpen; 			// Open
    struct propTagArray *lpSetColumns;			// SetColumns
    struct restrictTable *lpRestrict;			// Restrict
    struct tableSortRequest *lpSort;			// Sort
    struct tableQueryRowsRequest *lpQueryRows; 	// QueryRows
};

struct tableMultiResponse {
    unsigned int er;
    unsigned int ulTableId;
    struct rowSet sRowSet; 						// QueryRows
};

struct categoryState {
    struct propValArray sProps;
    unsigned int fExpanded;
};

struct categoryStateArray {
    unsigned int __size;
    struct categoryState* __ptr;
};

struct collapseState {
    struct categoryStateArray sCategoryStates;
    struct propValArray sBookMarkProps;
};
   
struct notificationObject {
	entryId* pEntryId;
	unsigned int ulObjType;
	entryId* pParentId;
	entryId* pOldId;
	entryId* pOldParentId;
	struct propTagArray* pPropTagArray;
};

struct notificationTable{
	unsigned int ulTableEvent;
	unsigned int ulObjType;
	unsigned int hResult;
	struct propVal propIndex;
	struct propVal propPrior;
	struct propValArray* pRow;
};

struct notificationNewMail {
	entryId* pEntryId;
	entryId* pParentId;
	char* lpszMessageClass;
	unsigned int ulMessageFlags;
};

struct notificationICS {
	struct xsd__base64Binary *pSyncState;
	unsigned int ulChangeType;
};

struct notification {
	unsigned int ulConnection;
	unsigned int ulEventType;
	struct notificationObject *obj;
	struct notificationTable *tab;
	struct notificationNewMail *newmail;
	struct notificationICS *ics;
};

struct notificationArray {
	unsigned int __size;
	struct notification *__ptr;
};

struct notifyResponse {
	struct notificationArray	*pNotificationArray;
	unsigned int er;
};

struct notifySyncState {
	unsigned int ulSyncId;
	unsigned int ulChangeId;
};

struct notifySubscribe {
	unsigned int ulConnection;
	struct xsd__base64Binary sKey;
	unsigned int ulEventMask;
	struct notifySyncState sSyncState;
};

struct notifySubscribeArray {
	unsigned int __size;
	struct notifySubscribe *__ptr;
};

#define TABLE_NOADVANCE 1

struct rights {
	unsigned int ulUserid;
	unsigned int ulType;
	unsigned int ulRights;
	unsigned int ulState;
	entryId		 sUserId;
};

struct rightsArray {
	unsigned int __size;
	struct rights *__ptr;
};

struct rightsResponse {
	struct rightsArray	*pRightsArray;
	unsigned int er;
};

struct userobject {
	char* lpszName;
	unsigned int ulId;
	entryId		 sId;
	unsigned int ulType;
};

struct userobjectArray {
	unsigned int __size;
	struct userobject *__ptr;
};

struct userobjectResponse {
	struct userobjectArray *pUserObjectArray;
	unsigned int er;
};

struct getOwnerResponse {
	unsigned int ulOwner;
	entryId sOwner;
	unsigned int er;
};

struct statObjectResponse {
	unsigned int ulSize;
	unsigned int ftCreated;
	unsigned int ftModified;
	unsigned int er;
};

struct namedProp {
	unsigned int *lpId;
	char *lpString;
	struct xsd__base64Binary *lpguid;
};

struct namedPropArray {
	unsigned int __size;
	struct namedProp * __ptr;
};

struct getIDsFromNamesResponse {
	struct propTagArray lpsPropTags;
	unsigned int er;
};

struct getNamesFromIDsResponse {
	struct namedPropArray lpsNames;
	unsigned int er;
};

struct restrictTable;

struct restrictAnd {
	unsigned int __size;
	struct restrictTable **__ptr;
};

struct restrictBitmask {
	unsigned int ulMask;
	unsigned int ulPropTag;
	unsigned int ulType;
};

struct restrictCompare {
	unsigned int ulPropTag1;
	unsigned int ulPropTag2;
	unsigned int ulType;
};

struct restrictContent {
	unsigned int ulFuzzyLevel;
	unsigned int ulPropTag;
	struct propVal *lpProp;
};

struct restrictExist {
	unsigned int ulPropTag;
};

struct restrictComment {
	struct restrictTable *lpResTable;
	struct propValArray sProps;
};

struct restrictNot {
	struct restrictTable *lpNot;
};

struct restrictOr {
	unsigned int __size;
	struct restrictTable **__ptr;
};

struct restrictProp {
	unsigned int ulType;
	unsigned int ulPropTag;
	struct propVal *lpProp;
};

struct restrictSize {
	unsigned int ulType;
	unsigned int ulPropTag;
	unsigned int cb;
};

struct restrictSub {
	unsigned int ulSubObject;
	struct restrictTable *lpSubObject;
};

struct restrictTable {
	unsigned int ulType;
	struct restrictAnd *lpAnd;
	struct restrictBitmask *lpBitmask;
	struct restrictCompare *lpCompare;
	struct restrictContent *lpContent;
	struct restrictExist *lpExist;
	struct restrictNot *lpNot;
	struct restrictOr *lpOr;
	struct restrictProp *lpProp;
	struct restrictSize *lpSize;
	struct restrictComment *lpComment;
	struct restrictSub *lpSub;
};

struct tableGetSearchCriteriaResponse {
	struct restrictTable *lpRestrict;
	struct entryList *lpFolderIDs;
	unsigned int ulFlags;
	unsigned int er;
};

struct receiveFolder {
	entryId sEntryId;
	char* lpszAExplicitClass;
};

struct receiveFolderResponse {
	struct receiveFolder sReceiveFolder;
	unsigned int er;
};

struct receiveFoldersArray {
	unsigned int __size;
	struct receiveFolder * __ptr;
};

struct receiveFolderTableResponse {
	struct receiveFoldersArray sFolderArray;
	unsigned int er;
};

struct searchCriteria {
	struct restrictTable *lpRestrict;
	struct entryList *lpFolders;
	unsigned int ulFlags;
};

struct propmapPair {
	unsigned int ulPropId;
	char *lpszValue;
};

struct propmapPairArray {
	unsigned int __size;
	struct propmapPair *__ptr;
};

struct propmapMVPair {
	unsigned int ulPropId;
	struct mv_string8 sValues;
};

struct propmapMVPairArray {
	unsigned int __size;
	struct propmapMVPair *__ptr;
};

struct user {
	unsigned int ulUserId;
	char		*lpszUsername;
	char		*lpszPassword;
	char		*lpszMailAddress;
	char		*lpszFullName;
	char		*lpszServername;
	unsigned int 	ulIsNonActive;
	unsigned int 	ulIsAdmin;
	unsigned int	ulIsABHidden;
	unsigned int	ulCapacity;
	unsigned int	ulObjClass;
	struct propmapPairArray *lpsPropmap;
	struct propmapMVPairArray *lpsMVPropmap;
	entryId		sUserId;
};

struct userArray {
	unsigned int __size;
	struct user *__ptr;
};

struct userListResponse {
	struct userArray sUserArray;
	unsigned int er;
};

struct getUserResponse {
	struct user  *lpsUser;
	unsigned int er;
};

struct setUserResponse {
	unsigned int ulUserId;
	entryId		 sUserId;
	unsigned int er;
};

struct group {
	unsigned int ulGroupId;
	entryId		 sGroupId;
	char		*lpszGroupname;
	char		*lpszFullname;
	char		*lpszFullEmail;
	unsigned int	ulIsABHidden;
	struct propmapPairArray *lpsPropmap;
	struct propmapMVPairArray *lpsMVPropmap;
};

struct groupArray {
	unsigned int __size;
	struct group *__ptr;
};

struct groupListResponse {
	struct groupArray sGroupArray;
	unsigned int er;
};

struct getGroupResponse {
	struct group *lpsGroup;
	unsigned int er;
};

struct setGroupResponse {
	unsigned int ulGroupId;
	entryId		 sGroupId;
	unsigned int er;
};

struct company {
	unsigned int ulCompanyId;
	unsigned int ulAdministrator;
	entryId sCompanyId;
	entryId sAdministrator;
	char *lpszCompanyname;
	char *lpszServername;
	unsigned int	ulIsABHidden;
	struct propmapPairArray *lpsPropmap;
	struct propmapMVPairArray *lpsMVPropmap;
};

struct companyArray {
	unsigned int __size;
	struct company *__ptr;
};

struct companyListResponse {
	struct companyArray sCompanyArray;
	unsigned int er;
};

struct getCompanyResponse {
	struct company *lpsCompany;
	unsigned int er;
};

struct setCompanyResponse {
	unsigned int ulCompanyId;
	entryId sCompanyId;
	unsigned int er;
};

struct resolveUserStoreResponse {
	unsigned int ulUserId;
	entryId	sUserId;
	entryId	sStoreId;
	struct xsd__base64Binary guid;
	unsigned int er;
	char *lpszServerPath;
};

struct querySubMessageResponse {
	entryId sEntryId;
	unsigned int er;
};

struct userProfileResponse {
	char *szProfileName;
	char *szProfileAddress;
	unsigned int er;
};

struct resolveCompanyResponse {
	unsigned int ulCompanyId;
	entryId sCompanyId;
	unsigned int er;
};

struct resolveGroupResponse {
	unsigned int ulGroupId;
	entryId sGroupId;
	unsigned int er;
};

struct resolveUserResponse {
	unsigned int ulUserId;
	entryId sUserId;
	unsigned int er;
};

struct readChunkResponse {
	struct xsd__base64Binary data;
	unsigned int er;
};

struct flagArray {
	unsigned int __size;
	unsigned int *__ptr;
};

struct abResolveNamesResponse {
	struct rowSet sRowSet;
	struct flagArray aFlags;
	unsigned int er;
};

struct action {
	unsigned int acttype;
	unsigned int flavor;
	unsigned int flags;
	int __union;
	union _act {
		struct _moveCopy {
			struct xsd__base64Binary store;
			struct xsd__base64Binary folder;
		} moveCopy;
		struct _reply {
			struct xsd__base64Binary message;
			struct xsd__base64Binary guid;
		} reply;
		struct _defer {
			struct xsd__base64Binary bin;
		} defer;
		unsigned int bouncecode;
		struct rowSet *adrlist;
		struct propVal *prop;
	} act;
};

struct actions {
	struct action *__ptr;
	int __size;
};

struct quota {
	bool bUseDefaultQuota;
	bool bIsUserDefaultQuota;
	LONG64 llWarnSize;
	LONG64 llSoftSize;
	LONG64 llHardSize;
};

struct quotaResponse {
	struct quota sQuota;
	unsigned int er;
};

struct quotaStatus {
	LONG64 llStoreSize;
	unsigned int ulQuotaStatus;
	unsigned int er;
};

struct messageStatus {
	unsigned int ulMessageStatus;
	unsigned int er;
};

struct icsChange {
	unsigned int ulChangeId;
    struct xsd__base64Binary sSourceKey;
    struct xsd__base64Binary sParentSourceKey;
    unsigned int ulChangeType;
	unsigned int ulFlags;
};

struct icsChangesArray {
	unsigned int __size;
	struct icsChange *__ptr;
};

struct icsChangeResponse {
	struct icsChangesArray sChangesArray;
	unsigned int ulMaxChangeId;
	unsigned int er;
};

struct setSyncStatusResponse {
	unsigned int ulSyncId;
	unsigned int er;
};

struct getEntryIDFromSourceKeyResponse {
	entryId	sEntryId;
	unsigned int er;
};

struct getLicenseAuthResponse {
    struct xsd__base64Binary sAuthResponse;
    unsigned int er;
};
struct resolvePseudoUrlResponse {
	char *lpszServerPath;
	bool bIsPeer;
	unsigned int er;
};

struct licenseCapabilities {
    unsigned int __size;
    char **__ptr;
};

struct getLicenseCapaResponse {
    struct licenseCapabilities sCapabilities;
    unsigned int er;
};

struct getLicenseUsersResponse {
	unsigned int ulUsers;
	unsigned int er;
};

struct server {
	char *lpszName;
	char *lpszFilePath;
	char *lpszHttpPath;
	char *lpszSslPath;
	char *lpszPreferedPath;
	unsigned int ulFlags;
};

struct serverList {
	unsigned int __size;
	struct server *__ptr;
};

struct getServerDetailsResponse {
	struct serverList sServerList;
	unsigned int er;
};

struct getServerBehaviorResponse {
	unsigned int ulBehavior;
	unsigned int er;
};

struct sourceKeyPair {
	struct xsd__base64Binary sParentKey;
	struct xsd__base64Binary sObjectKey;
};

struct sourceKeyPairArray {
	unsigned int __size;
	struct sourceKeyPair *__ptr;
};

struct messageStream {
	unsigned int ulStep;
	struct propValArray sPropVals;
	struct xsd__Binary sStreamData;
};

struct messageStreamArray {
	unsigned int __size;
	struct messageStream *__ptr;
};

struct exportMessageChangesAsStreamResponse {
	struct messageStreamArray sMsgStreams;
	unsigned int er;
};

struct getChangeInfoResponse {
	struct propVal sPropPCL;
	struct propVal sPropCK;
	unsigned int er;
};

struct syncState {
	unsigned int ulSyncId;
	unsigned int ulChangeId;
};

struct syncStateArray {
	unsigned int __size;
	struct syncState *__ptr;
};

struct getSyncStatesReponse {
	struct syncStateArray sSyncStates;
	unsigned int er;
};

struct purgeDeferredUpdatesResponse {
    unsigned int ulDeferredRemaining;
    unsigned int er;
};

struct userClientUpdateStatusResponse {
	unsigned int ulTrackId;
	time_t tUpdatetime;
	char *lpszCurrentversion;
	char *lpszLatestversion;
	char *lpszComputername;
	unsigned int ulStatus;
	unsigned int er;
};

struct resetFolderCountResponse {
	unsigned int ulUpdates;
	unsigned int er;
};

//TableType flags for function ns__tableOpen
#define TABLETYPE_MS				1	// MessageStore tables
#define TABLETYPE_AB				2	// Addressbook tables
#define TABLETYPE_SPOOLER			3	// Spooler tables
#define TABLETYPE_MULTISTORE		4	// Multistore tables
#define TABLETYPE_STATS_SYSTEM		5	// System stats
#define TABLETYPE_STATS_SESSIONS	6	// Session stats
#define TABLETYPE_STATS_USERS		7	// User stats
#define TABLETYPE_STATS_COMPANY		8	// Company stats (hosted only)
#define TABLETYPE_USERSTORES		9	// UserStore tables
#define TABLETYPE_MAILBOX			10	// Mailbox Table

// Flags for struct tableMultiRequest
#define TABLE_MULTI_CLEAR_RESTRICTION	0x1	// Clear table restriction

#define fnevZarafaIcsChange			(fnevExtended | 0x00000001)

int ns__logon(char * szUsername, char * szPassword, char * szVersion, unsigned int ulCapabilities, struct xsd__base64Binary sLicenseReq, ULONG64 ullSessionGroup, char *szClientApp, struct logonResponse *lpsLogonResponse);
int ns__ssoLogon(ULONG64 ulSessionId, char *szUsername, struct xsd__base64Binary *lpInput, char *clientVersion, unsigned int clientCaps, struct xsd__base64Binary sLicenseReq, ULONG64 ullSessionGroup, char *szClientApp, struct ssoLogonResponse *lpsResponse);

int ns__getStore(ULONG64 ulSessionId, entryId* lpsEntryId, struct getStoreResponse *lpsResponse);
int ns__getStoreName(ULONG64 ulSessionId, entryId sEntryId, struct getStoreNameResponse* lpsResponse);
int ns__getStoreType(ULONG64 ulSessionId, entryId sEntryId, struct getStoreTypeResponse* lpsResponse);
int ns__getPublicStore(ULONG64 ulSessionId, unsigned int ulFlags, struct getStoreResponse *lpsResponse);
int ns__logoff(ULONG64 ulSessionId, unsigned int *result);

int ns__getRights(ULONG64 ulSessionId, entryId sEntryId, int ulType, struct rightsResponse *lpsRightResponse);
int ns__setRights(ULONG64 ulSessionId, entryId sEntryId, struct rightsArray *lpsrightsArray, unsigned int *result);
int ns__getUserObjectList(ULONG64 ulSessionId, unsigned int ulCompanyId, entryId sCompanyId, int ulType, struct userobjectResponse *lpsUserObjectResponse);

/* loads a big prop from an object */
int ns__loadProp(ULONG64 ulSessionId, entryId sEntryId, unsigned int ulObjId, unsigned int ulPropTag, struct loadPropResponse *lpsResponse);
int ns__saveObject(ULONG64 ulSessionId, entryId sParentEntryId, entryId sEntryId, struct saveObject *lpsSaveObj, unsigned int ulFlags, unsigned int ulSyncId, struct loadObjectResponse *lpsLoadObjectResponse);
int ns__loadObject(ULONG64 ulSessionId, entryId sEntryId, struct notifySubscribe *lpsNotSubscribe, unsigned int ulFlags, struct loadObjectResponse *lpsLoadObjectResponse);

int ns__createFolder(ULONG64 ulSessionId, entryId sParentId, entryId* lpsNewEntryId, unsigned int ulType, char *szName, char *szComment, bool fOpenIfExists, unsigned int ulSyncId, struct xsd__base64Binary sOrigSourceKey, struct createFolderResponse *lpsCreateFolderResponse);
int ns__deleteObjects(ULONG64 ulSessionId, unsigned int ulFlags, struct entryList *aMessages, unsigned int ulSyncId, unsigned int *result);
int ns__copyObjects(ULONG64 ulSessionId, struct entryList *aMessages, entryId sDestFolderId, unsigned int ulFlags, unsigned int ulSyncId, unsigned int *result);
int ns__emptyFolder(ULONG64 ulSessionId, entryId sEntryId,  unsigned int ulFlags, unsigned int ulSyncId, unsigned int *result);
int ns__deleteFolder(ULONG64 ulSessionId, entryId sEntryId, unsigned int ulFlags, unsigned int ulSyncId, unsigned int *result);
int ns__copyFolder(ULONG64 ulSessionId, entryId sEntryId, entryId sDestFolderId, char *lpszNewFolderName, unsigned int ulFlags, unsigned int ulSyncId, unsigned int *result);
int ns__setReadFlags(ULONG64 ulSessionId, unsigned int ulFlags, entryId* lpsEntryId, struct entryList *lpMessages, unsigned int ulSyncId, unsigned int *result);
int ns__setReceiveFolder(ULONG64 ulSessionId, entryId sStoreId, entryId* lpsEntryId, char* lpszMessageClass, unsigned int *result);
int ns__getReceiveFolder(ULONG64 ulSessionId, entryId sStoreId, char* lpszMessageClass, struct receiveFolderResponse *lpsReceiveFolder);
int ns__getReceiveFolderTable(ULONG64 ulSessionId, entryId sStoreId, struct receiveFolderTableResponse *lpsReceiveFolderTable);

int ns__getMessageStatus(ULONG64 ulSessionId, entryId sEntryId, unsigned int ulFlags, struct messageStatus* lpsStatus);
int ns__setMessageStatus(ULONG64 ulSessionId, entryId sEntryId, unsigned int ulNewStatus, unsigned int ulNewStatusMask, unsigned int ulSyncId, struct messageStatus* lpsOldStatus);

int ns__getIDsFromNames(ULONG64 ulSessionId, struct namedPropArray *lpsNamedProps, unsigned int ulFlags, struct getIDsFromNamesResponse *lpsResponse);
int ns__getNamesFromIDs(ULONG64 ulSessionId, struct propTagArray *lpsPropTags, struct getNamesFromIDsResponse *lpsResponse);

int ns__notify(ULONG64 ulSessionId, struct notification sNotification, unsigned int *er);
int ns__notifySubscribe(ULONG64 ulSessionId, struct notifySubscribe *notifySubscribe, unsigned int *result);
int ns__notifySubscribeMulti(ULONG64 ulSessionId, struct notifySubscribeArray *notifySubscribeArray, unsigned int *result);
int ns__notifyUnSubscribe(ULONG64 ulSessionId, unsigned int ulConnection, unsigned int *result);
int ns__notifyUnSubscribeMulti(ULONG64 ulSessionId, struct mv_long *ulConnectionArray, unsigned int *result);
int ns__notifyGetItems(ULONG64 ulSessionId, struct notifyResponse *notifications);

int ns__tableOpen(ULONG64 ulSessionId, entryId sEntryId, unsigned int ulTableType, unsigned int ulType, unsigned int ulFlags, struct tableOpenResponse *lpsTableOpenResponse);
int ns__tableClose(ULONG64 ulSessionId, unsigned int ulTableId, unsigned int *result);
int ns__tableSetColumns(ULONG64 ulSessionId, unsigned int ulTableId, struct propTagArray *aPropTag, unsigned int *result);
int ns__tableQueryColumns(ULONG64 ulSessionId, unsigned int ulTableId, unsigned int ulFlags, struct tableQueryColumnsResponse *lpsTableQueryColumnsResponse);
int ns__tableSort(ULONG64 ulSessionId, unsigned int ulTableId, struct sortOrderArray *aSortOrder, unsigned int ulCategories, unsigned int ulExpanded, unsigned int *result);
int ns__tableRestrict(ULONG64 ulSessionId, unsigned int ulTableId, struct restrictTable *lpRestrict, unsigned int *result);
int ns__tableGetRowCount(ULONG64 ulSessionId, unsigned int ulTableId, struct tableGetRowCountResponse *lpsTableGetRowCountResponse);
int ns__tableQueryRows(ULONG64 ulSessionId, unsigned int ulTableId, unsigned int ulRowCount, unsigned int ulFlags, struct tableQueryRowsResponse *lpsQueryRowsResponse);
int ns__tableFindRow(ULONG64 ulSessionId, unsigned int ulTableId, unsigned int ulBookmark, unsigned int ulFlags, struct restrictTable *lpsRestrict, unsigned int *result);
int ns__tableSeekRow(ULONG64 ulSessionId, unsigned int ulTableId, unsigned int ulBookmark, int lRowCount, struct tableSeekRowResponse *lpsResponse);
int ns__tableCreateBookmark(ULONG64 ulSessionId, unsigned int ulTableId, struct tableBookmarkResponse *lpsResponse);
int ns__tableFreeBookmark(ULONG64 ulSessionId, unsigned int ulTableId, unsigned int ulbkPosition, unsigned int *result);
int ns__tableSetSearchCriteria(ULONG64 ulSessionId, entryId sEntryId, struct restrictTable *lpRestrict, struct entryList *lpFolders, unsigned int ulFlags, unsigned int *result);
int ns__tableGetSearchCriteria(ULONG64 ulSessionId, entryId sEntryId, struct tableGetSearchCriteriaResponse *lpsResponse);
int ns__tableSetMultiStoreEntryIDs(ULONG64 ulSessionId, unsigned int ulTableId, struct entryList *aMessages, unsigned int *result);
int ns__tableExpandRow(ULONG64 ulSessionId, unsigned int ulTableId, struct xsd__base64Binary sInstanceKey, unsigned int ulRowCount, unsigned int ulFlags, struct tableExpandRowResponse *lpsTableExpandRowResponse);
int ns__tableCollapseRow(ULONG64 ulSessionId, unsigned int ulTableId, struct xsd__base64Binary sInstanceKey, unsigned int ulFlags, struct tableCollapseRowResponse *lpsTableCollapseRowResponse);
int ns__tableGetCollapseState(ULONG64 ulSessionId, unsigned int ulTableId, struct xsd__base64Binary sBookmark, struct tableGetCollapseStateResponse *lpsResponse);
int ns__tableSetCollapseState(ULONG64 ulSessionId, unsigned int ulTableId, struct xsd__base64Binary sCollapseState, struct tableSetCollapseStateResponse *lpsResponse);
int ns__tableMulti(ULONG64 ulSessionId, struct tableMultiRequest sRequest, struct tableMultiResponse *lpsResponse);

int ns__submitMessage(ULONG64 ulSessionId, entryId sEntryId, unsigned int ulFlags, unsigned int *result);
int ns__finishedMessage(ULONG64 ulSessionId, entryId sEntryId, unsigned int ulFlags, unsigned int *result);
int ns__abortSubmit(ULONG64 ulSessionId, entryId sEntryId, unsigned int *result);
int ns__isMessageInQueue(ULONG64 ulSessionId, entryId sEntryId, unsigned int *result);

// Get user ID / store for username (username == NULL for current user)
int ns__resolveStore(ULONG64 ulSessionId, struct xsd__base64Binary sStoreGuid, struct resolveUserStoreResponse *lpsResponse);
int ns__resolveUserStore(ULONG64 ulSessionId, char *szUserName, unsigned int ulStoreTypeMask, unsigned int ulFlags, struct resolveUserStoreResponse *lpsResponse);

// Actual user creation/deletion in the external user source
int ns__createUser(ULONG64 ulSessionId, struct user *lpsUser, struct setUserResponse *lpsUserSetResponse);
int ns__deleteUser(ULONG64 ulSessionId, unsigned int ulUserId, entryId sUserId, unsigned int *result);
int ns__removeAllObjects(ULONG64 ulSessionId, entryId sExceptUserId, unsigned int *result);

// Get user fullname/name/emailaddress/etc for specific user id (userid = 0 for current user)
int ns__getUser(ULONG64 ulSessionId, unsigned int ulUserId, entryId sUserId, struct getUserResponse *lpsUserGetResponse);
int ns__setUser(ULONG64 ulSessionId, struct user *lpsUser, unsigned int *result);
int ns__getUserList(ULONG64 ulSessionId, unsigned int ulCompanyId, entryId sCompanyId, struct userListResponse *lpsUserList);
int ns__getSendAsList(ULONG64 ulSessionId, unsigned int ulUserId, entryId sUserId, struct userListResponse *lpsUserList);
int ns__addSendAsUser(ULONG64 ulSessionId, unsigned int ulUserId, entryId sUserId, unsigned int ulSenderId, entryId sSenderId, unsigned int *result);
int ns__delSendAsUser(ULONG64 ulSessionId, unsigned int ulUserId, entryId sUserId, unsigned int ulSenderId, entryId sSenderId, unsigned int *result);
int ns__getUserClientUpdateStatus(ULONG64 ulSessionId, entryId sUserId, struct userClientUpdateStatusResponse *lpsResponse);

// Start softdelete purge
int ns__purgeSoftDelete(ULONG64 ulSessionId, unsigned int ulDays, unsigned int *result);
// Do deferred purge
int ns__purgeDeferredUpdates(ULONG64 ulSessionId, struct purgeDeferredUpdatesResponse *lpsResponse);
// Clear the cache
int ns__purgeCache(ULONG64 ulSessionId, unsigned int ulFlags, unsigned int *result);

// Create store for a user
int ns__createStore(ULONG64 ulSessionId, unsigned int ulStoreType, unsigned int ulUserId, entryId sUserId, entryId sStoreId, entryId sRootId, unsigned int ulFlags, unsigned int *result);
// Disabled, returns error, use ns__removeStore
int ns__deleteStore(ULONG64 ulSessionId, unsigned int ulStoreId, unsigned int ulSyncId, unsigned int *result);
// Mark store deleted for softdelete to purge from database
int ns__removeStore(ULONG64 ulSessionId, struct xsd__base64Binary sStoreGuid, unsigned int ulSyncId, unsigned int *result);
// Hook a store to a specified user (overrides previous hooked store)
int ns__hookStore(ULONG64 ulSessionId, unsigned int ulStoreType, entryId sUserId, struct xsd__base64Binary sStoreGuid, unsigned int ulSyncId, unsigned int *result);
// Unhook a store from a specific user
int ns__unhookStore(ULONG64 ulSessionId, unsigned int ulStoreType, entryId sUserId, unsigned int ulSyncId, unsigned int *result);

int ns__getOwner(ULONG64 ulSessionId, entryId sEntryId, struct getOwnerResponse *lpsResponse);
int ns__resolveUsername(ULONG64 ulSessionId,  char *lpszUsername, struct resolveUserResponse *lpsResponse);

int ns__createGroup(ULONG64 ulSessionId, struct group *lpsGroup, struct setGroupResponse *lpsSetGroupResponse);
int ns__setGroup(ULONG64 ulSessionId, struct group *lpsGroup, unsigned int *result);
int ns__getGroup(ULONG64 ulSessionId, unsigned int ulGroupId, entryId sGroupId, struct getGroupResponse *lpsReponse);
int ns__getGroupList(ULONG64 ulSessionId,  unsigned int ulCompanyId, entryId sCompanyId, struct groupListResponse *lpsGroupList);
int ns__groupDelete(ULONG64 ulSessionId, unsigned int ulGroupId, entryId sGroupId, unsigned int *result);
int ns__resolveGroupname(ULONG64 ulSessionId,  char *lpszGroupname, struct resolveGroupResponse *lpsResponse);

int ns__deleteGroupUser(ULONG64 ulSessionId, unsigned int ulGroupId, entryId sGroupId, unsigned int ulUserId, entryId sUserId, unsigned int *result);
int ns__addGroupUser(ULONG64 ulSessionId, unsigned int ulGroupId, entryId sGroupId, unsigned int ulUserId, entryId sUserId, unsigned int *result);
int ns__getUserListOfGroup(ULONG64 ulSessionId, unsigned int ulGroupId, entryId sGroupId, struct userListResponse *lpsUserList);
int ns__getGroupListOfUser(ULONG64 ulSessionId, unsigned int ulUserId, entryId sUserId, struct groupListResponse *lpsGroupList);

int ns__createCompany(ULONG64 ulSessionId, struct company *lpsCompany, struct setCompanyResponse *lpsResponse);
int ns__deleteCompany(ULONG64 ulSessionId, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result);
int ns__setCompany(ULONG64 ulSessionId, struct company *lpsCompany, unsigned int *result);
int ns__getCompany(ULONG64 ulSessionId, unsigned int ulCompanyId, entryId sCompanyId, struct getCompanyResponse *lpsResponse);
int ns__resolveCompanyname(ULONG64 ulSessionId, char *lpszCompanyname, struct resolveCompanyResponse *lpsResponse);
int ns__getCompanyList(ULONG64 ulSessionId, struct companyListResponse *lpsCompanyList);

int ns__addCompanyToRemoteViewList(ULONG64 ecSessionId, unsigned int ulSetCompanyId, entryId sSetCompanyId, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result);
int ns__delCompanyFromRemoteViewList(ULONG64 ecSessionId, unsigned int ulSetCompanyId, entryId sSetCompanyId, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result);
int ns__getRemoteViewList(ULONG64 ecSessionId, unsigned int ulCompanyId, entryId sCompanyId, struct companyListResponse *lpsCompanyList);
int ns__addUserToRemoteAdminList(ULONG64 ecSessionId, unsigned int ulUserId, entryId sUserId, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result);
int ns__delUserFromRemoteAdminList(ULONG64 ecSessionId, unsigned int ulUserId, entryId sUserId, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result);
int ns__getRemoteAdminList(ULONG64 ecSessionId, unsigned int ulCompanyId, entryId sCompanyId, struct userListResponse *lpsUserList);

int ns__checkExistObject(ULONG64 ulSessionId, entryId sEntryId, unsigned int ulFlags, unsigned int *result);

int ns__readABProps(ULONG64 ulSessionId, entryId sEntryId, struct readPropsResponse *readPropsResponse);
int ns__writeABProps(ULONG64 ulSessionId, entryId sEntryId, struct propValArray *aPropVal, unsigned int *result);
int ns__deleteABProps(ULONG64 ulSessionId, entryId sEntryId, struct propTagArray *lpsPropTags, unsigned int *result);
int ns__loadABProp(ULONG64 ulSessionId, entryId sEntryId, unsigned int ulPropTag, struct loadPropResponse *lpsResponse);

int ns__abResolveNames(ULONG64 ulSessionId, struct propTagArray* lpaPropTag, struct rowSet* lpsRowSet, struct flagArray* lpaFlags, unsigned int ulFlags, struct abResolveNamesResponse* lpsABResolveNames);

int ns__syncUsers(ULONG64 ulSessionId, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result);

int ns__setLockState(ULONG64 ulSessionId, entryId sEntryId, bool bLocked, unsigned int *result); 

int ns__resetFolderCount(ULONG64 ulSessionId, entryId sEntryId, struct resetFolderCountResponse *lpsResponse);

// Quota
int ns__GetQuota(ULONG64 ulSessionId, unsigned int ulUserid, entryId sUserId, bool bGetUserDefault, struct quotaResponse* lpsQuota);
int ns__SetQuota(ULONG64 ulSessionId, unsigned int ulUserid, entryId sUserId, struct quota* lpsQuota, unsigned int *result);
int ns__AddQuotaRecipient(ULONG64 ulSessionId, unsigned int ulCompanyid, entryId sCompanyId, unsigned int ulRecipientId, entryId sRecipientId, unsigned int ulType, unsigned int *result);
int ns__DeleteQuotaRecipient(ULONG64 ulSessionId, unsigned int ulCompanyid, entryId sCompanyId, unsigned int ulRecipientId, entryId sRecipientId, unsigned int ulType, unsigned int *result);
int ns__GetQuotaRecipients(ULONG64 ulSessionId, unsigned int ulUserid, entryId sUserId, struct userListResponse *lpsResponse);
int ns__GetQuotaStatus(ULONG64 ulSessionId, unsigned int ulUserid, entryId sUserId, struct quotaStatus* lpsQuotaStatus);

// Incremental Change Synchronization
int ns__getChanges(ULONG64 ulSessionId, struct xsd__base64Binary sSourceKeyFolder, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulChangeType, unsigned int ulFlags, struct restrictTable *lpsRestrict, struct icsChangeResponse* lpsChanges);
int ns__setSyncStatus(ULONG64 ulSessionId, struct xsd__base64Binary sSourceKeyFolder, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulChangeType, unsigned int ulFlags, struct setSyncStatusResponse *lpsResponse);

int ns__getEntryIDFromSourceKey(ULONG64 ulSessionId, entryId sStoreId, struct xsd__base64Binary folderSourceKey, struct xsd__base64Binary messageSourceKey, struct getEntryIDFromSourceKeyResponse *lpsResponse);
int ns__getSyncStates(ULONG64 ulSessionId, struct mv_long ulaSyncId, struct getSyncStatesReponse *lpsResponse);

// Licensing
int ns__getLicenseAuth(ULONG64 ulSessionId, struct xsd__base64Binary sAuthData, struct getLicenseAuthResponse *lpsResponse);
int ns__getLicenseCapa(ULONG64 ulSessionId, unsigned int ulServiceType, struct getLicenseCapaResponse *lpsResponse);
int ns__getLicenseUsers(ULONG64 ulSessionId, unsigned int ulServiceType, struct getLicenseUsersResponse *lpsResponse);

// Multi Server
int ns__resolvePseudoUrl(ULONG64 ulSessionId, char *lpszPseudoUrl, struct resolvePseudoUrlResponse* lpsResponse);
int ns__getServerDetails(ULONG64 ulSessionId, struct mv_string8 szaSvrNameList, unsigned int ulFlags, struct getServerDetailsResponse* lpsResponse);

// Server Behavior, legacy calls for 6.30 clients, unused and may be removed in the future
int ns__getServerBehavior(ULONG64 ulSessionId, struct getServerBehaviorResponse* lpsResponse);
int ns__setServerBehavior(ULONG64 ulSessionId, unsigned int ulBehavior, unsigned int *result);

// Streaming
int ns__exportMessageChangesAsStream(ULONG64 ulSessionId, unsigned int ulFlags, struct propTagArray sPropTags, struct sourceKeyPairArray, struct exportMessageChangesAsStreamResponse *lpsResponse);
int ns__importMessageFromStream(ULONG64 ulSessionId, unsigned int ulFlags, unsigned int ulSyncId, entryId sParentEntryId, entryId sEntryId, bool bIsNew, struct propVal *lpsConflictItems, struct xsd__Binary sStreamData, unsigned int *result);
int ns__getChangeInfo(ULONG64 ulSessionId, entryId sEntryId, struct getChangeInfoResponse *lpsResponse);

// Debug
struct testPerformArgs {
    int __size;
    char *__ptr[];
};

struct testGetResponse {
    char *szValue;    
    unsigned int er;
};

int ns__testPerform(ULONG64 ulSessionId, char *szCommand, struct testPerformArgs sPerform, unsigned int *result);
int ns__testSet(ULONG64 ulSessionId, char *szVarName, char *szValue, unsigned int *result);
int ns__testGet(ULONG64 ulSessionId, char *szVarName, struct testGetResponse *lpsResponse);

struct attachment {
	char	*lpszAttachmentName;
	struct xsd__Binary sData;
};

struct attachmentArray {
	int __size;
	struct attachment *__ptr;
};

struct clientUpdateResponse {
	unsigned int ulLogLevel;
	char *lpszServerPath;
	struct xsd__base64Binary sLicenseResponse;
	struct xsd__Binary sStreamData;
	unsigned int er;
};

struct clientUpdateInfoRequest {
	unsigned int ulTrackId;
	char *szUsername;
	char *szClientIPList;
	char *szClientVersion;
	char *szWindowsVersion;
	char *szComputerName;

	struct xsd__base64Binary sLicenseReq;
};

struct clientUpdateStatusRequest {
	unsigned int ulTrackId;
	unsigned int ulLastErrorCode;
	unsigned int ulLastErrorAction;
	struct attachmentArray sFiles;
};

struct clientUpdateStatusResponse {
	unsigned int er;
};

int ns__getClientUpdate(struct clientUpdateInfoRequest sClientUpdateInfo, struct clientUpdateResponse* lpsResponse);
int ns__setClientUpdateStatus(struct clientUpdateStatusRequest sClientUpdateStatus, struct clientUpdateStatusResponse* lpsResponse);
