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
#include <initguid.h>

/*
 * All the IID_* symbols are created here
 */

#define USES_IID_IMSProvider
#define USES_IID_IXPProvider
#define USES_IID_IABProvider
#define USES_IID_IMsgStore
#define USES_IID_IMSLogon
#define USES_IID_IXPLogon
#define USES_IID_IABLogon
#define USES_IID_IMAPIFolder
#define USES_IID_IMessage
#define USES_IID_IExchangeManageStore
#define USES_IID_IAttachment
#define USES_IID_IMAPIContainer
#define USES_IID_IMAPIProp
#define USES_IID_IMAPITable
#define USES_IID_IMAPITableData
#define USES_IID_ISequentialStream
#define USES_IID_IUnknown
#define USES_IID_IStream
#define USES_IID_IStorage
#define USES_IID_IMessageRaw

// quick linux hack
#define ECDEBUGCLIENT_USES_UIDS

//Trace info
#define USES_IID_IMAPISession
#define USES_IID_IMAPIAdviseSink
#define USES_IID_IProfSect
#define USES_IID_IMAPIStatus
#define USES_IID_IAddrBook
#define USES_IID_IMailUser
#define USES_IID_IMAPIContainer
#define USES_IID_IABContainer
#define USES_IID_IDistList
#define USES_IID_IMAPISup
#define USES_IID_IMAPITableData
#define USES_IID_IMAPISpoolerInit
#define USES_IID_IMAPISpoolerSession
#define USES_IID_ITNEF
#define USES_IID_IMAPIPropData
#define USES_IID_IMAPIControl
#define USES_IID_IProfAdmin
#define USES_IID_IMsgServiceAdmin
#define USES_IID_IMAPISpoolerService
#define USES_IID_IMAPIProgress
#define USES_IID_ISpoolerHook
#define USES_IID_IMAPIViewContext
#define USES_IID_IMAPIFormMgr
#define USES_IID_IProviderAdmin
#define USES_IID_IMAPIForm
#define USES_PS_MAPI
#define USES_PS_PUBLIC_STRINGS
#define USES_IID_IPersistMessage
#define USES_IID_IMAPIViewAdviseSink
#define USES_IID_IStreamDocfile
#define USES_IID_IMAPIFormProp
#define USES_IID_IMAPIFormContainer
#define USES_IID_IMAPIFormAdviseSink
#define USES_IID_IStreamTnef
#define USES_IID_IMAPIFormFactory
#define USES_IID_IMAPIMessageSite
#define USES_PS_ROUTING_EMAIL_ADDRESSES
#define USES_PS_ROUTING_ADDRTYPE
#define USES_PS_ROUTING_DISPLAY_NAME
#define USES_PS_ROUTING_ENTRYID
#define USES_PS_ROUTING_SEARCH_KEY
#define USES_MUID_PROFILE_INSTANCE
#define USES_IID_IMAPIFormInfo
#define USES_IID_IEnumMAPIFormProp
#define USES_IID_IExchangeModifyTable
//#endif

// mapiguidext.h
#define USES_muidStoreWrap
#define USES_PS_INTERNET_HEADERS
#define USES_PSETID_Appointment
#define USES_PSETID_Task
#define USES_PSETID_Address
#define USES_PSETID_Common
#define USES_PSETID_Log
#define USES_PSETID_Meeting
#define USES_PSETID_Sharing
#define USES_PSETID_PostRss
#define USES_PSETID_UnifiedMessaging
#define USES_PSETID_AirSync
#define USES_PSETID_Note
#define USES_PSETID_CONTACT_FOLDER_RECIPIENT
#define USES_PSETID_Zarafa_CalDav	//used in caldav
#define USES_PSETID_Archive
#define USES_PSETID_CalendarAssistant
#define USES_GUID_Dilkie
#define USES_IID_IMAPIClientShutdown
#define USES_IID_IMAPIProviderShutdown
#define USES_IID_ISharedFolderEntryId
#define USES_WAB_GUID

#define USES_IID_IPRProvider
#define USES_IID_IMAPIProfile

#define USES_PSETID_ZMT

#define USES_IID_IMAPIWrappedObject
#define USES_IID_IMAPISessionUnknown
#define USES_IID_IMAPISupportUnknown
#define USES_IID_IMsgServiceAdmin2
#define USES_IID_IAddrBookSession
#define USES_IID_CAPONE_PROF
#define USES_IID_IMAPISync
#define USES_IID_IMAPISyncProgressCallback
#define USES_IID_IMAPISecureMessage
#define USES_IID_IMAPIGetSession

// freebusy guids
#define USES_IID_IEnumFBBlock
#define USES_IID_IFreeBusyData
#define USES_IID_IFreeBusySupport
#define USES_IID_IFreeBusyUpdate

// used in ECSync
#define USES_IID_IDispatch
#define USES_IID_ISensNetwork

#include <mapiguid.h>
#include "edkguid.h"
#include "ECGuid.h"
#include "freebusyguid.h"
#include "mapiguidext.h"
