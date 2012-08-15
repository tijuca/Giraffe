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

#ifndef mapi_ptr_INCLUDED
#define mapi_ptr_INCLUDED

#include "mapi_ptr/mapi_object_ptr.h"
#include "mapi_ptr/mapi_memory_ptr.h"
#include "mapi_ptr/mapi_array_ptr.h"
#include "mapi_ptr/mapi_rowset_ptr.h"

#include <mapix.h>
#include <mapispi.h>
#include <edkmdb.h>
#include <edkguid.h>

#include "IECServiceAdmin.h"
#include "IECSecurity.h"
#include "IECSingleInstance.h"
#include "ECGuid.h"
#include "mapiguidext.h"

DEFINEMAPIPTR(ABContainer);
DEFINEMAPIPTR(AddrBook);
DEFINEMAPIPTR(DistList);
DEFINEMAPIPTR(ECSecurity);
DEFINEMAPIPTR(ECServiceAdmin);
DEFINEMAPIPTR(ECSingleInstance);
DEFINEMAPIPTR(ExchangeManageStore);
DEFINEMAPIPTR(ExchangeModifyTable);
DEFINEMAPIPTR(ExchangeExportChanges);
DEFINEMAPIPTR(MAPIAdviseSink);
DEFINEMAPIPTR(MAPIContainer);
DEFINEMAPIPTR(MAPIFolder);
DEFINEMAPIPTR(MAPIProp);
DEFINEMAPIPTR(MAPISession);
DEFINEMAPIPTR(MAPITable);
DEFINEMAPIPTR(MailUser);
DEFINEMAPIPTR(Message);
DEFINEMAPIPTR(MsgServiceAdmin);
DEFINEMAPIPTR(MsgStore);
DEFINEMAPIPTR(ProfAdmin);
DEFINEMAPIPTR(ProfSect);
DEFINEMAPIPTR(ProviderAdmin);
DEFINEMAPIPTR(Unknown);
DEFINEMAPIPTR(Stream);
typedef mapi_object_ptr<IAttach, IID_IAttachment> AttachPtr;	// Nice... MS (not Mark S) is a bit inconsistent here.
DEFINEMAPIPTR(MAPIGetSession);


typedef mapi_memory_ptr<ECPERMISSION> ECPermissionPtr;
typedef mapi_memory_ptr<ENTRYID> EntryIdPtr;
typedef mapi_memory_ptr<ENTRYLIST> EntryListPtr;
typedef mapi_memory_ptr<MAPIERROR> MAPIErrorPtr;
typedef mapi_memory_ptr<ROWLIST> RowListPtr;
typedef mapi_memory_ptr<SPropValue> SPropValuePtr;
typedef mapi_memory_ptr<SPropTagArray> SPropTagArrayPtr;
typedef mapi_memory_ptr<SRestriction> SRestrictionPtr;
typedef mapi_memory_ptr<SSortOrderSet> SSortOrderSetPtr;
typedef mapi_memory_ptr<WCHAR> WStringPtr;
typedef mapi_memory_ptr<FlagList> FlagListPtr;

typedef mapi_array_ptr<ECPERMISSION> ECPermissionArrayPtr;
typedef mapi_array_ptr<SPropValue> SPropArrayPtr;

typedef mapi_rowset_ptr<SRow> SRowSetPtr;
typedef mapi_rowset_ptr<ADRENTRY> AdrListPtr;


#endif // ndef mapi_ptr_INCLUDED
