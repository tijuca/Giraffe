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

#include <mapidefs.h>
#include <mapiutil.h>
#include <mapitags.h>
#include "mapiext.h"

#include "ECMessage.h"
#include "ECAttach.h"
#include "ECMemTable.h"

#include "codepage.h"
#include "rtfutil.h"
#include "Util.h"
#include "Mem.h"

#include "ECGuid.h"
#include "edkguid.h"
#include "ECDebug.h"
#include "WSUtil.h"


#include "ClientUtil.h"
#include "ECMemStream.h"

#include <charset/utf32string.h>
#include <charset/convert.h>

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MAX_TABLE_PROPSIZE 8192

SizedSPropTagArray(15, sPropRecipColumns) = {15, { PR_7BIT_DISPLAY_NAME_W, PR_EMAIL_ADDRESS_W, PR_INSTANCE_KEY, PR_RECORD_KEY, PR_SEARCH_KEY, PR_SEND_RICH_INFO, PR_DISPLAY_NAME_W, PR_RECIPIENT_TYPE, PR_ROWID, PR_DISPLAY_TYPE, PR_ENTRYID, PR_SPOOLER_STATUS, PR_OBJECT_TYPE, PR_ADDRTYPE_W, PR_RESPONSIBILITY } };
SizedSPropTagArray(8, sPropAttachColumns) = {8, { PR_ATTACH_NUM, PR_INSTANCE_KEY, PR_RECORD_KEY, PR_RENDERING_POSITION, PR_ATTACH_FILENAME_W, PR_ATTACH_METHOD, PR_DISPLAY_NAME_W, PR_ATTACH_LONG_FILENAME_W } };

HRESULT ECMessageFactory::Create(ECMsgStore *lpMsgStore, BOOL fNew, BOOL fModify, ULONG ulFlags, BOOL bEmbedded, ECMAPIProp *lpRoot, ECMessage **lpMessage) const
{
	return ECMessage::Create(lpMsgStore, fNew, fModify, ulFlags, bEmbedded, lpRoot, lpMessage);
}

ECMessage::ECMessage(ECMsgStore *lpMsgStore, BOOL fNew, BOOL fModify, ULONG ulFlags, BOOL bEmbedded, ECMAPIProp *lpRoot) : ECMAPIProp(lpMsgStore, MAPI_MESSAGE, fModify, lpRoot, "IMessage")
{
	this->m_lpParentID = NULL;
	this->m_cbParentID = 0;
	this->ulObjFlags = ulFlags & MAPI_ASSOCIATED;
	this->lpRecips = NULL;
	this->lpAttachments = NULL;
	this->ulNextAttUniqueId = 0;
	this->ulNextRecipUniqueId = 0;
	this->fNew = fNew;
	this->m_bEmbedded = bEmbedded;
	this->m_bExplicitSubjectPrefix = FALSE;
	this->m_ulLastChange = syncChangeNone;
	this->m_bBusySyncRTF = FALSE;
	this->m_ulBodyType = bodyTypeUnknown;
	this->m_bRecipsDirty = FALSE;

	// proptag, getprop, setprops, class, bRemovable, bHidden

	this->HrAddPropHandlers(PR_RTF_IN_SYNC,				GetPropHandler       ,DefaultSetPropIgnore,		(void*) this, TRUE,  FALSE);
	this->HrAddPropHandlers(PR_HASATTACH,				GetPropHandler       ,DefaultSetPropComputed,	(void*) this, FALSE, FALSE);
	this->HrAddPropHandlers(PR_NORMALIZED_SUBJECT,		GetPropHandler		 ,DefaultSetPropIgnore,		(void*) this, FALSE, FALSE);
	this->HrAddPropHandlers(PR_PARENT_ENTRYID,			GetPropHandler       ,DefaultSetPropComputed,	(void*) this, FALSE, FALSE);
	this->HrAddPropHandlers(PR_MESSAGE_SIZE,			GetPropHandler       ,DefaultSetPropComputed,	(void*) this, FALSE, FALSE);
	this->HrAddPropHandlers(PR_DISPLAY_TO,				GetPropHandler       ,DefaultSetPropComputed,	(void*) this, FALSE, FALSE);
	this->HrAddPropHandlers(PR_DISPLAY_CC,				GetPropHandler       ,DefaultSetPropComputed,	(void*) this, FALSE, FALSE);
	this->HrAddPropHandlers(PR_DISPLAY_BCC,				GetPropHandler       ,DefaultSetPropComputed,	(void*) this, FALSE, FALSE);
	this->HrAddPropHandlers(PR_ACCESS,					GetPropHandler       ,DefaultSetPropComputed,	(void*) this, FALSE, FALSE);

	this->HrAddPropHandlers(PR_MESSAGE_ATTACHMENTS,		GetPropHandler       ,DefaultSetPropIgnore,	(void*) this, FALSE, FALSE);
	this->HrAddPropHandlers(PR_MESSAGE_RECIPIENTS,		GetPropHandler       ,DefaultSetPropIgnore,	(void*) this, FALSE, FALSE);

	// Workaround for support html in outlook 2000/xp
	this->HrAddPropHandlers(PR_BODY_HTML,				GetPropHandler       ,SetPropHandler,	(void*) this, FALSE, TRUE);

	// The property 0x10970003 is set by outlook when browsing in the 'unread mail' searchfolder. It is used to make sure
	// that a message that you just read is not removed directly from view. It is set for each message which should be in the view
	// even though it is 'read', and is removed when you leave the folder. When you try to export this property to a PST, you get
	// an access denied error. We therefore hide this property, ie you can GetProps/SetProps it and use it in a restriction, but
	// GetPropList will never list it (same as in a PST).
	this->HrAddPropHandlers(0x10970003,					DefaultGetPropGetReal,DefaultSetPropSetReal,	(void*) this, TRUE, TRUE);

	// Don't show the PR_EC_IMAP_ID, and mark it as computed and deletable. This makes sure that CopyTo() will not copy it to
	// the other message.
	this->HrAddPropHandlers(PR_EC_IMAP_ID,      		DefaultGetPropGetReal,DefaultSetPropComputed, 	(void*) this, TRUE, TRUE);

	// Make sure the MSGFLAG_HASATTACH flag gets added when needed.
	this->HrAddPropHandlers(PR_MESSAGE_FLAGS,      		GetPropHandler		,SetPropHandler,		 	(void*) this, FALSE, FALSE);
	
	// Make sure PR_SOURCE_KEY is available
	this->HrAddPropHandlers(PR_SOURCE_KEY,				GetPropHandler		,SetPropHandler,			(void*) this, TRUE, FALSE);

	// IMAP complete email, removable and hidden. setprop ignore? use interface for single-instancing
	this->HrAddPropHandlers(PR_EC_IMAP_EMAIL,			DefaultGetPropGetReal		,DefaultSetPropSetReal,			(void*) this, TRUE, TRUE);
	this->HrAddPropHandlers(PR_EC_IMAP_EMAIL_SIZE,		DefaultGetPropGetReal		,DefaultSetPropSetReal,			(void*) this, TRUE, TRUE);
	this->HrAddPropHandlers(CHANGE_PROP_TYPE(PR_EC_IMAP_BODY, PT_UNICODE),			DefaultGetPropGetReal		,DefaultSetPropSetReal,			(void*) this, TRUE, TRUE);
	this->HrAddPropHandlers(CHANGE_PROP_TYPE(PR_EC_IMAP_BODYSTRUCTURE, PT_UNICODE),	DefaultGetPropGetReal		,DefaultSetPropSetReal,			(void*) this, TRUE, TRUE);
}

ECMessage::~ECMessage()
{
	if(m_lpParentID)
		MAPIFreeBuffer(m_lpParentID);

	if(lpRecips)
		lpRecips->Release();

	if(lpAttachments)
		lpAttachments->Release();
}

HRESULT	ECMessage::Create(ECMsgStore *lpMsgStore, BOOL fNew, BOOL fModify, ULONG ulFlags, BOOL bEmbedded, ECMAPIProp *lpRoot, ECMessage **lppMessage)
{
	HRESULT hr = hrSuccess;
	ECMessage *lpMessage = NULL;

	lpMessage = new ECMessage(lpMsgStore, fNew, fModify, ulFlags, bEmbedded, lpRoot);

	hr = lpMessage->QueryInterface(IID_ECMessage, (void **)lppMessage);

	return hr;
}

HRESULT	ECMessage::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECMessage, this);
	REGISTER_INTERFACE(IID_ECMAPIProp, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IMessage, &this->m_xMessage);
	REGISTER_INTERFACE(IID_IMAPIProp, &this->m_xMessage);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xMessage);

	REGISTER_INTERFACE(IID_ISelectUnicode, &this->m_xUnknown);

	REGISTER_INTERFACE(IID_IECSingleInstance, &this->m_xECSingleInstance);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECMessage::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	HRESULT			hr = hrSuccess;
	ULONG			cValues = 0;
	LPSPropValue	lpPropArray = NULL;
	eBodyType		ulBodyType = bodyTypeUnknown;
	ULONG i;

	ULONG			ulBestMatchTable[4][3] = {
		/* unknown */ { PR_BODY_W, PR_RTF_COMPRESSED, PR_HTML },
		/* plain */   { PR_BODY_W, PR_RTF_COMPRESSED, PR_HTML },
		/* rtf */     { PR_RTF_COMPRESSED, PR_HTML, PR_BODY_W },
		/* html */    { PR_HTML, PR_RTF_COMPRESSED, PR_BODY_W }};

	ULONG	ulBestMatch = 0;
	bool	bBody, bRtf, bHtml;

	hr = GetPropsInternal(lpPropTagArray, ulFlags, &cValues, &lpPropArray);
	if (HR_FAILED(hr))
		goto exit;

	bBody = lpPropTagArray == NULL || Util::FindPropInArray(lpPropTagArray, CHANGE_PROP_TYPE(PR_BODY_W, PT_UNSPECIFIED)) >= 0;
	bRtf = lpPropTagArray == NULL || Util::FindPropInArray(lpPropTagArray, CHANGE_PROP_TYPE(PR_RTF_COMPRESSED, PT_UNSPECIFIED)) >= 0;
	bHtml = lpPropTagArray == NULL || Util::FindPropInArray(lpPropTagArray, CHANGE_PROP_TYPE(PR_HTML, PT_UNSPECIFIED)) >= 0;

	if((bBody || bRtf || bHtml) && GetBodyType(&ulBodyType) == hrSuccess) {
		/*
		 * Exchange has a particular way of handling requests for different types of body content. There are three:
		 *
		 * - RTF (PR_RTF_COMPRESSED)
		 * - HTML (PR_HTML or PR_BODY_HTML, these are interchangeable in all cases)
		 * - Plaintext (PR_BODY)
		 *
		 * All of these properties are available (or none at all if there is no body) via OpenProperty() AT ALL TIMES, even if the
		 * item itself was not saved as that specific type of message. However, the exchange follows the following rules
		 * when multiple body types are requested in a single GetProps() call:
		 *
		 * - Only the 'best fit' property is returned as an actual value, the other properties are returned with PT_ERROR
		 * - Best fit for plaintext is (in order): PR_BODY, PR_RTF, PR_HTML
		 * - For RTF messages: PR_RTF, PR_HTML, PR_BODY
		 * - For HTML messages: PR_HTML, PR_RTF, PR_BODY
		 * - When GetProps() is called with NULL as the property list, the error value is MAPI_E_NOT_ENOUGH_MEMORY
		 * - When GetProps() is called with a list of properties, the error value is MAPI_E_NOT_ENOUGH_MEMORY or MAPI_E_NOT_FOUND depending on the following:
		 *   - When the requested property ID is higher than the best-match property, the value is MAPI_E_NOT_FOUND
		 *   - When the requested property ID is lower than the best-match property, the value is MAPI_E_NOT_ENOUGH_MEMORY
		 *
		 * Additionally, the normal rules for returning MAPI_E_NOT_ENOUGH_MEMORY apply (ie for large properties).
		 *
		 * Example: RTF message, PR_BODY, PR_HTML and PR_RTF_COMPRESSED requested in single GetProps() call:
		 * returns: PR_BODY -> MAPI_E_NOT_ENOUGH_MEMORY, PR_HTML -> MAPI_E_NOT_FOUND, PR_RTF_COMPRESSED -> actual RTF content
		 *
		 * PR_RTF_IN_SYNC is normally always TRUE, EXCEPT if the following is true:
		 * - Both PR_RTF_COMPRESSED and PR_HTML are requested
		 * - Actual body type is HTML
		 *
		 * This is used to disambiguate the situation in which you request PR_RTF_COMPRESSED and PR_HTML and receive MAPI_E_NOT_ENOUGH_MEMORY for
		 * both properties (or both are OK but we never do that).
		 *
		 * Since the values of the properties depend on the requested property tag set, a property handler cannot be used in this
		 * case, and therefore the above logic is implemented here.
		 */

		// Find best match property in requested property set
		if(lpPropTagArray == NULL)
			// No properties specified, best match is always number one choice body property
			ulBestMatch = ulBestMatchTable[ulBodyType][0];
		else {
			// Find best match in requested set
			for (i = 0; i < 3; i++) {
				if (Util::FindPropInArray(lpPropTagArray, PROP_TAG(PT_UNSPECIFIED, PROP_ID(ulBestMatchTable[ulBodyType][i]))) >= 0) {
					ulBestMatch = ulBestMatchTable[ulBodyType][i];
					break;
				}
			}
		}

		for (i = 0; i < cValues; i++)
		{
			if( PROP_ID(lpPropArray[i].ulPropTag) == PROP_ID(PR_RTF_COMPRESSED) ||
				PROP_ID(lpPropArray[i].ulPropTag) == PROP_ID(PR_BODY_W) ||
				PROP_ID(lpPropArray[i].ulPropTag) == PROP_ID(PR_HTML)) 
			{
				// Override body property if it is NOT the best-match property
				if(PROP_ID(lpPropArray[i].ulPropTag) != PROP_ID(ulBestMatch)) {
					lpPropArray[i].ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropArray[i].ulPropTag));

					// Set the correct error according to above rules
					if(lpPropTagArray == NULL)
						lpPropArray[i].Value.ul = MAPI_E_NOT_ENOUGH_MEMORY;
					else
						lpPropArray[i].Value.ul = PROP_ID(lpPropArray[i].ulPropTag) < PROP_ID(ulBestMatch) ? MAPI_E_NOT_ENOUGH_MEMORY : MAPI_E_NOT_FOUND;
				}
			}

			// RTF_IN_SYNC should be false only if the message is actually HTML and both RTF and HTML are requested
			// (we are indicating that RTF should not be used in this case). Note that PR_RTF_IN_SYNC is normally
			// forced to TRUE in our property handler, so we only need to change it to FALSE if needed.
			if( PROP_ID(lpPropArray[i].ulPropTag) == PROP_ID(PR_RTF_IN_SYNC)) {
				if(bHtml && bRtf && ulBodyType == bodyTypeHTML) {
					lpPropArray[i].ulPropTag = PR_RTF_IN_SYNC;
					lpPropArray[i].Value.b = false;
				}
			}
		}
	}

	*lpcValues = cValues;
	*lppPropArray = lpPropArray;
	lpPropArray = NULL;

exit:
	if (lpPropArray)
		MAPIFreeBuffer(lpPropArray);

	return hr;
}

HRESULT ECMessage::GetPropsInternal(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	if(m_ulLastChange != syncChangeNone && m_bBusySyncRTF != TRUE) {
		// minimize SyncRTF call only when body props are requested
		LONG ulPos = Util::FindPropInArray(lpPropTagArray, PR_RTF_COMPRESSED);
		if (ulPos < 0) ulPos = Util::FindPropInArray(lpPropTagArray, PROP_TAG(PT_UNSPECIFIED, PROP_ID(PR_BODY_HTML)) );
		if (ulPos < 0) ulPos = Util::FindPropInArray(lpPropTagArray, PROP_TAG(PT_UNSPECIFIED, PROP_ID(PR_BODY)) );
		if (ulPos >= 0)
			SyncRTF();
	}

	return ECMAPIProp::GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
}

HRESULT ECMessage::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	SyncRTF();
	
	return ECMAPIProp::GetPropList(ulFlags, lppPropTagArray);
}

HRESULT ECMessage::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	HRESULT hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	if (lpiid == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

//FIXME: Support the flags ?
	if(ulPropTag == PR_MESSAGE_ATTACHMENTS) {
		if(*lpiid == IID_IMAPITable)
			hr = GetAttachmentTable(ulInterfaceOptions, (LPMAPITABLE*)lppUnk);
	} else if(ulPropTag == PR_MESSAGE_RECIPIENTS) {
		if (*lpiid == IID_IMAPITable)
			hr = GetRecipientTable(ulInterfaceOptions, (LPMAPITABLE*)lppUnk);
	} else {
		// Sync RTF if needed
		SyncRTF();

		// Workaround for support html in outlook 2000/xp
		if(ulPropTag == PR_BODY_HTML)
			ulPropTag = PR_HTML;

		hr = ECMAPIProp::OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
	}

exit:
	return hr;
}

HRESULT ECMessage::GetAttachmentTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT hr = hrSuccess;
	ECMemTableView *lpView = NULL;
	LPSPropValue lpPropID = NULL;
	LPSPropValue lpPropType = NULL;
	LPSPropTagArray lpPropTagArray = NULL;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	if(lstProps == NULL) {
		hr = HrLoadProps();
		if (hr != hrSuccess)
			goto exit;
		if(lstProps == NULL) {
			hr = MAPI_E_CALL_FAILED;
			goto exit;
		}
	}			

	if (this->lpAttachments == NULL) {
		hr = Util::HrCopyUnicodePropTagArray(ulFlags, (LPSPropTagArray)&sPropAttachColumns, &lpPropTagArray);
		if(hr != hrSuccess)
			goto exit;

		hr = ECMemTable::Create(lpPropTagArray, PR_ATTACH_NUM, &this->lpAttachments);
		if(hr != hrSuccess)
			goto exit;

		// This code is resembles the table-copying code in GetRecipientTable, but we do some slightly different
		// processing on the data that we receive from the table. Basically, data is copied to the attachment
		// table received from the server through m_sMapiObject, but the PR_ATTACH_NUM is re-generated locally
		if (!fNew) {
			// existing message has "table" in m_sMapiObject data
			ECMapiObjects::iterator iterObjects;
			std::list<ECProperty>::iterator iterPropVals;

			for (iterObjects = m_sMapiObject->lstChildren->begin(); iterObjects != m_sMapiObject->lstChildren->end(); iterObjects++) {

				if ((*iterObjects)->ulObjType != MAPI_ATTACH)
					continue;

				if ((*iterObjects)->bDelete)
					continue;

				this->ulNextAttUniqueId = (*iterObjects)->ulUniqueId > this->ulNextAttUniqueId ? (*iterObjects)->ulUniqueId : this->ulNextAttUniqueId;
				this->ulNextAttUniqueId++;

				{
					ULONG ulProps = (*iterObjects)->lstProperties->size();
					LPSPropValue lpProps = NULL;
					SPropValue sKeyProp;
					ULONG i;

					// +1 for maybe missing PR_ATTACH_NUM property
					// +1 for maybe missing PR_OBJECT_TYPE property
					ECAllocateBuffer(sizeof(SPropValue)*(ulProps+2), (void**)&lpProps);

					lpPropID = NULL;
					lpPropType = NULL;

					for (i = 0, iterPropVals = (*iterObjects)->lstProperties->begin(); iterPropVals != (*iterObjects)->lstProperties->end(); iterPropVals++, i++) {
						(*iterPropVals).CopyToByRef(&lpProps[i]);

						if (lpProps[i].ulPropTag == PR_ATTACH_NUM) {
							lpPropID = &lpProps[i];
						} else if (lpProps[i].ulPropTag == PR_OBJECT_TYPE) {
							lpPropType = &lpProps[i];
						} else if (PROP_ID(lpProps[i].ulPropTag) == PROP_ID(PR_ATTACH_DATA_OBJ)) {
							lpProps[i].ulPropTag = CHANGE_PROP_TYPE(lpProps[i].ulPropTag, PT_ERROR);
							lpProps[i].Value.err = MAPI_E_NOT_ENOUGH_MEMORY;
						} else if (PROP_TYPE(lpProps[i].ulPropTag) == PT_BINARY && lpProps[i].Value.bin.cb > MAX_TABLE_PROPSIZE) {
							lpProps[i].ulPropTag = CHANGE_PROP_TYPE(lpProps[i].ulPropTag, PT_ERROR);
							lpProps[i].Value.err = MAPI_E_NOT_ENOUGH_MEMORY;
						}
					}

					if (lpPropID == NULL) {
						ulProps++;
						lpPropID = &lpProps[i++];
					}
					lpPropID->ulPropTag = PR_ATTACH_NUM;
					lpPropID->Value.ul = (*iterObjects)->ulUniqueId;				// use uniqueid from "recount" code in WSMAPIPropStorage::desoapertize()
					
					if (lpPropType == NULL) {
						ulProps++;
						lpPropType = &lpProps[i++];
					}
					lpPropType->ulPropTag = PR_OBJECT_TYPE;
					lpPropType->Value.ul = MAPI_ATTACH;

					sKeyProp.ulPropTag = PR_EC_HIERARCHYID;
					sKeyProp.Value.ul = (*iterObjects)->ulObjId;

					hr = lpAttachments->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, &sKeyProp, lpProps, i);
					if (hr != hrSuccess)
						goto exit; // continue?

					ECFreeBuffer(lpProps);
					lpProps = NULL;
				}
			}

			// since we just loaded the table, all enties are clean (actually not required for attachments, but it doesn't hurt)
			hr = lpAttachments->HrSetClean();
			if (hr != hrSuccess)
				goto exit;
		} // !new == empty table
	}


	if(this->lpAttachments == NULL) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	hr = lpAttachments->HrGetView(createLocaleFromName(""), ulFlags & MAPI_UNICODE, &lpView);

	if(hr != hrSuccess)
		goto exit;

	hr = lpView->QueryInterface(IID_IMAPITable, (void **)lppTable);

	lpView->Release();

exit:
	if (lpPropTagArray)
		MAPIFreeBuffer(lpPropTagArray);

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

HRESULT ECMessage::OpenAttach(ULONG ulAttachmentNum, LPCIID lpInterface, ULONG ulFlags, LPATTACH *lppAttach)
{
	HRESULT				hr = hrSuccess;
	IMAPITable			*lpTable = NULL;
	ECAttach			*lpAttach = NULL;
	IECPropStorage		*lpParentStorage = NULL;
	SPropValue			sID;
	LPSPropValue		lpObjId = NULL;
	ULONG				ulObjId;

	if(this->lpAttachments == NULL) {
		hr = this->GetAttachmentTable(fMapiUnicode, &lpTable);

		if(hr != hrSuccess)
			goto exit;

		lpTable->Release();
	}

	if(this->lpAttachments == NULL) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	hr = ECAttach::Create(this->GetMsgStore(), MAPI_ATTACH, TRUE, ulAttachmentNum, m_lpRoot, &lpAttach);

	if(hr != hrSuccess)
		goto exit;

	sID.ulPropTag = PR_ATTACH_NUM;
	sID.Value.ul = ulAttachmentNum;

	if(lpAttachments->HrGetRowID(&sID, &lpObjId) == hrSuccess) {
		ulObjId = lpObjId->Value.ul;
	} else {
		ulObjId = 0;
	}

	hr = this->GetMsgStore()->lpTransport->HrOpenParentStorage(this, ulAttachmentNum, ulObjId, this->lpStorage->GetServerStorage(), &lpParentStorage);
	if(hr != hrSuccess)
		goto exit;

	hr = lpAttach->HrSetPropStorage(lpParentStorage, TRUE);
	if(hr != hrSuccess)
		goto exit;

	hr = lpAttach->QueryInterface(IID_IAttachment, (void **)lppAttach);

	// Register the object as a child of ours
	AddChild(lpAttach);

	lpAttach->Release();

exit:
    if(hr != hrSuccess) {
        if(lpAttach)
            lpAttach->Release();
    }

	if (lpParentStorage)
		lpParentStorage->Release();

	if(lpObjId)
		ECFreeBuffer(lpObjId);

	return hr;
}

HRESULT ECMessage::CreateAttach(LPCIID lpInterface, ULONG ulFlags, ULONG *lpulAttachmentNum, LPATTACH *lppAttach)
{
	return CreateAttach(lpInterface, ulFlags, ECAttachFactory(), lpulAttachmentNum, lppAttach);
}

HRESULT ECMessage::CreateAttach(LPCIID lpInterface, ULONG ulFlags, const IAttachFactory &refFactory, ULONG *lpulAttachmentNum, LPATTACH *lppAttach)
{
	HRESULT				hr = hrSuccess;
	IMAPITable*			lpTable = NULL;
	ECAttach*			lpAttach = NULL;
	SPropValue			sID;
	IECPropStorage*		lpStorage = NULL;

	if(this->lpAttachments == NULL) {
		hr = this->GetAttachmentTable(fMapiUnicode, &lpTable);

		if(hr != hrSuccess)
			goto exit;

		lpTable->Release();
	}

	if(this->lpAttachments == NULL) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	hr = refFactory.Create(this->GetMsgStore(), MAPI_ATTACH, TRUE, this->ulNextAttUniqueId, m_lpRoot, &lpAttach);

	if(hr != hrSuccess)
		goto exit;

	hr = lpAttach->HrLoadEmptyProps();

	if(hr != hrSuccess)
		goto exit;

	sID.ulPropTag = PR_ATTACH_NUM;
	sID.Value.ul = this->ulNextAttUniqueId;

	hr = this->GetMsgStore()->lpTransport->HrOpenParentStorage(this, this->ulNextAttUniqueId, 0, NULL, &lpStorage);
	if(hr != hrSuccess)
		goto exit;

	hr = lpAttach->HrSetPropStorage(lpStorage, FALSE);
	if(hr != hrSuccess)
		goto exit;

	hr = lpAttach->SetProps(1, &sID, NULL);
	if(hr != hrSuccess)
		goto exit;

	hr = lpAttach->QueryInterface(IID_IAttachment, (void **)lppAttach);

	AddChild(lpAttach);

	lpAttach->Release();

	*lpulAttachmentNum = sID.Value.ul;

	// successfully created attachment, so increment counter for the next
	this->ulNextAttUniqueId++;

exit:
	if(lpStorage)
		lpStorage->Release();

	return hr;
}

HRESULT ECMessage::DeleteAttach(ULONG ulAttachmentNum, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	IMAPITable *lpTable = NULL;
	SPropValue sPropID;

	if(this->lpAttachments == NULL) {
		hr = this->GetAttachmentTable(fMapiUnicode, &lpTable);

		if(hr != hrSuccess)
			goto exit;

		lpTable->Release();
	}

	if(this->lpAttachments == NULL) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	sPropID.ulPropTag = PR_ATTACH_NUM;
	sPropID.Value.ul = ulAttachmentNum;

	hr = this->lpAttachments->HrModifyRow(ECKeyTable::TABLE_ROW_DELETE, NULL, &sPropID, 1);
	if (hr !=hrSuccess)
		goto exit;

	// the object is deleted from the child list when SaveChanges is called, which calls SyncAttachments()

exit:
	return hr;
}

HRESULT ECMessage::GetRecipientTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT hr = hrSuccess;
	ECMemTableView *lpView = NULL;
	LPSPropTagArray lpPropTagArray = NULL;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	if(lstProps == NULL) {
		hr = HrLoadProps();
		if (hr != hrSuccess)
			goto exit;
		if(lstProps == NULL) {
			hr = MAPI_E_CALL_FAILED;
			goto exit;
		}
	}			

	if (this->lpRecips == NULL) {
		hr = Util::HrCopyUnicodePropTagArray(ulFlags, (LPSPropTagArray)&sPropRecipColumns, &lpPropTagArray);
		if(hr != hrSuccess)
			goto exit;

		hr = ECMemTable::Create(lpPropTagArray, PR_ROWID, &lpRecips);
		if(hr != hrSuccess)
			goto exit;
		
		// What we do here is that we reconstruct a recipient table from the m_sMapiObject data, and then process it in two ways:
		// 1. Remove PR_ROWID values and replace them with client-side values
		// 2. Replace PR_EC_CONTACT_ENTRYID with PR_ENTRYID in the table
		// This means that the PR_ENTRYID value from the original recipient is not actually in the table (only
		// in the lpID value passed to HrModifyRow)

		// Get the existing table for this message (there is none if the message is unsaved)
		if (!fNew) {
			// existing message has "table" in m_sMapiObject data
			ECMapiObjects::iterator iterObjects;
			std::list<ECProperty>::iterator iterPropVals;

			for (iterObjects = m_sMapiObject->lstChildren->begin(); iterObjects != m_sMapiObject->lstChildren->end(); iterObjects++) {

				// The only valid types are MAPI_MAILUSER and MAPI_DISTLIST. However some MAPI clients put in other
				// values as object type. We know about the existence of MAPI_ATTACH as another valid subtype for
				// Messages, so we'll skip those, treat MAPI_DISTLIST as MAPI_DISTLIST and anything else as
				// MAPI_MAILUSER.
				if ((*iterObjects)->ulObjType == MAPI_ATTACH)
					continue;

				if ((*iterObjects)->bDelete)
					continue;

				this->ulNextRecipUniqueId = (*iterObjects)->ulUniqueId > this->ulNextRecipUniqueId ? (*iterObjects)->ulUniqueId : this->ulNextRecipUniqueId;
				this->ulNextRecipUniqueId++;

				{
					ULONG ulProps = (*iterObjects)->lstProperties->size();
					LPSPropValue lpProps = NULL;
					SPropValue sKeyProp;
					ULONG i;
					LPSPropValue lpPropID = NULL;
					LPSPropValue lpPropObjType = NULL;

					// +1 for maybe missing PR_ROWID property
					// +1 for maybe missing PR_OBJECT_TYPE property
					ECAllocateBuffer(sizeof(SPropValue)*(ulProps+2), (void**)&lpProps);

					lpPropID = NULL;
					for (i = 0, iterPropVals = (*iterObjects)->lstProperties->begin(); iterPropVals != (*iterObjects)->lstProperties->end(); iterPropVals++, i++) {
						
						(*iterPropVals).CopyToByRef(&lpProps[i]);

						if (lpProps[i].ulPropTag == PR_ROWID) {
							lpPropID = &lpProps[i];
						} else if (lpProps[i].ulPropTag == PR_OBJECT_TYPE) {
							lpPropObjType = &lpProps[i];
						} else if (lpProps[i].ulPropTag == PR_EC_CONTACT_ENTRYID) {
							// rename to PR_ENTRYID
							lpProps[i].ulPropTag = PR_ENTRYID;
						}
					}

					if (lpPropID == NULL) {
						ulProps++;
						lpPropID = &lpProps[i++];
					}
					lpPropID->ulPropTag = PR_ROWID;
					lpPropID->Value.ul = (*iterObjects)->ulUniqueId;				// use uniqueid from "recount" code in WSMAPIPropStorage::ECSoapObjectToMapiObject()

					if (lpPropObjType == NULL) {
						ulProps++;
						lpPropObjType = &lpProps[i++];
					}
					lpPropObjType->ulPropTag = PR_OBJECT_TYPE;
					lpPropObjType->Value.ul = (*iterObjects)->ulObjType;

					sKeyProp.ulPropTag = PR_EC_HIERARCHYID;
					sKeyProp.Value.ul = (*iterObjects)->ulObjId;

					hr = lpRecips->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, &sKeyProp, lpProps, i);
					if (hr != hrSuccess)
						goto exit;

					ECFreeBuffer(lpProps);
					lpProps = NULL;
				}
			}

			// since we just loaded the table, all enties are clean
			hr = lpRecips->HrSetClean();
			if (hr != hrSuccess)
				goto exit;
		} // !fNew
	}


	hr = lpRecips->HrGetView(createLocaleFromName(""), ulFlags & MAPI_UNICODE, &lpView);

	if(hr != hrSuccess)
		goto exit;

	hr = lpView->QueryInterface(IID_IMAPITable, (void **)lppTable);

	lpView->Release();

exit:
	if (lpPropTagArray)
		MAPIFreeBuffer(lpPropTagArray);

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

/*
 * This is not as easy as it seems. This is how we handle modifyrecipients:
 *
 * If the user specified a PR_ROWID, we always use their ROW ID
 * If the user specifies no PR_ROWID, we generate one, starting at 1 and going upward
 * MODIFY and ADD are the same
 * 
 * This makes the following scenario possible:
 *
 * - Add row without id (row id 1 is generated)
 * - Add row without id (row id 2 is generated)
 * - Add row with id 5 (row 5 is added, we now have row 1,2,5)
 * - Add row with id 1 (row 1 is now modified, possibly without the caller wanting this)
 *
 * However, this seem to be what is required by outlook, as for example ExpandRecips from
 * the support object assumes the row id's to stay the same when it does ModifyRecipients(0, lpMods)
 * so we can't generate the ID's whenever ADD or 0 is specified.
 */

HRESULT ECMessage::ModifyRecipients(ULONG ulFlags, LPADRLIST lpMods)
{
	HRESULT hr = hrSuccess;
	IMAPITable *lpTable = NULL;
	LPSPropValue lpRecipProps = NULL;
	ULONG cValuesRecipProps = 0;
	SPropValue sPropAdd[2];
	SPropValue sKeyProp;
	unsigned int i = 0;

	if (lpMods == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!fModify) {
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	// Load the recipients table object
	if(lpRecips == NULL) {
		hr = GetRecipientTable(fMapiUnicode, &lpTable);

		if(hr != hrSuccess)
			goto exit;

		lpTable->Release();
	}

	if(lpRecips == NULL) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if(ulFlags == 0) {
		hr = lpRecips->HrDeleteAll();

		if(hr != hrSuccess)
			goto exit;
			
		ulNextRecipUniqueId = 0;
	}

	for (i=0; i<lpMods->cEntries; i++) {
		if(ulFlags & MODRECIP_ADD || ulFlags == 0) {
			// Add a new PR_ROWID
			sPropAdd[0].ulPropTag = PR_ROWID;
			sPropAdd[0].Value.ul = this->ulNextRecipUniqueId++;
			
			// Add a PR_INSTANCE_KEY which is equal to the row id
			sPropAdd[1].ulPropTag = PR_INSTANCE_KEY;
			sPropAdd[1].Value.bin.cb = sizeof(ULONG);
			sPropAdd[1].Value.bin.lpb = (unsigned char *)&sPropAdd[0].Value.ul;

			hr = Util::HrMergePropertyArrays(lpMods->aEntries[i].rgPropVals, lpMods->aEntries[i].cValues, sPropAdd, 2, &lpRecipProps, &cValuesRecipProps);
			if (hr != hrSuccess)
				continue;

			sKeyProp.ulPropTag = PR_EC_HIERARCHYID;
			sKeyProp.Value.ul = 0;

			// Add the new row
			hr = lpRecips->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, &sKeyProp, lpRecipProps, cValuesRecipProps);

			if (lpRecipProps) {
				ECFreeBuffer(lpRecipProps);
				lpRecipProps = NULL;
			} 
		} else if(ulFlags & MODRECIP_MODIFY) {
			// Simply update the existing row, leave the PR_EC_HIERARCHY key prop intact.
			hr = lpRecips->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, lpMods->aEntries[i].rgPropVals, lpMods->aEntries[i].cValues);
		} else if(ulFlags & MODRECIP_REMOVE) {
			hr = lpRecips->HrModifyRow(ECKeyTable::TABLE_ROW_DELETE, NULL, lpMods->aEntries[i].rgPropVals, lpMods->aEntries[i].cValues);
		}

		if(hr != hrSuccess)
			goto exit;
	}

	m_bRecipsDirty = TRUE;

exit:
	if(lpRecipProps)
		ECFreeBuffer(lpRecipProps);

	return hr;
}

HRESULT ECMessage::SubmitMessage(ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	SPropTagArray sPropTagArray;
	ULONG cValue = 0;
	ULONG ulRepCount = 0;
	ULONG ulPreprocessFlags = 0;
	ULONG ulSubmitFlag = 0;
	LPSPropValue lpsPropArray = NULL;
	LPMAPITABLE lpRecipientTable = NULL;
	LPSRowSet lpsRow = NULL;
	LPMAPITABLE lpTable = NULL;
	LPSPropValue lpRecip = NULL;
	ULONG cRecip = 0;
	SRowSet sRowSetRecip;
	SPropValue sPropResponsibility;
	FILETIME ft;
	ULONG ulPreFlags = 0;

	// Get message flag to check for resubmit. PR_MESSAGE_FLAGS 
	sPropTagArray.cValues = 1;
	sPropTagArray.aulPropTag[0] = PR_MESSAGE_FLAGS;

	hr = GetPropsInternal(&sPropTagArray, 0, &cValue, &lpsPropArray);
	if(HR_FAILED(hr))
		goto exit;

	if(cValue == 1 && lpsPropArray != NULL && PROP_TYPE(lpsPropArray->ulPropTag) != PT_ERROR && (lpsPropArray->Value.ul & MSGFLAG_RESEND))
	{
		hr = this->GetMsgStore()->lpSupport->SpoolerNotify(NOTIFY_READYTOSEND, NULL);
		if(hr != hrSuccess)
			goto exit;

		hr = this->GetMsgStore()->lpSupport->PrepareSubmit(&this->m_xMessage, &ulPreFlags);
		if(hr != hrSuccess)
			goto exit;
	}
	
	if(lpsPropArray->ulPropTag == PR_MESSAGE_FLAGS) {
		// Re-set 'unsent' as it is obviously not sent if we're submitting it ... This allows you to send a message
		// multiple times, but only if the client calls SubmitMessage multiple times.
		lpsPropArray->Value.ul |= MSGFLAG_UNSENT;
		
		hr = this->SetProps(1, lpsPropArray, NULL);
		if(hr != hrSuccess)
			goto exit;
	}

	// Get the recipientslist
	hr = this->GetRecipientTable(fMapiUnicode, &lpRecipientTable);
	if(hr != hrSuccess)
		goto exit;

	// Check if recipientslist is empty
	hr = lpRecipientTable->GetRowCount(0, &ulRepCount);
	if(hr != hrSuccess)
		goto exit;
	
	if(ulRepCount == 0) {
		hr = MAPI_E_NO_RECIPIENTS;
		goto exit;
	}

	// Step through recipient list, set PR_RESPONSIBILITY to FALSE for all recipients
	while(TRUE){
		hr = lpRecipientTable->QueryRows(1, 0L, &lpsRow);

		if (hr != hrSuccess)
			goto exit;

		if (lpsRow->cRows == 0)
			break;
		
		sPropResponsibility.ulPropTag = PR_RESPONSIBILITY;
		sPropResponsibility.Value.b = FALSE;

		// Set PR_RESPONSIBILITY
		hr = Util::HrAddToPropertyArray(lpsRow->aRow[0].lpProps, lpsRow->aRow[0].cValues, &sPropResponsibility, &lpRecip, &cRecip);

		if(hr != hrSuccess)
			goto exit;

		sRowSetRecip.cRows = 1;
		sRowSetRecip.aRow[0].lpProps = lpRecip;
		sRowSetRecip.aRow[0].cValues = cRecip;

		if(lpsRow->aRow[0].cValues > 1){
			hr = this->ModifyRecipients(MODRECIP_MODIFY, (LPADRLIST) &sRowSetRecip);
			if (hr != hrSuccess)
				goto exit;
		}

		ECFreeBuffer(lpRecip);
		lpRecip = NULL;

		FreeProws(lpsRow);
		lpsRow = NULL;
	}
	
	lpRecipientTable->Release();
	lpRecipientTable = NULL;

	// Get the time to add to the message as PR_CLIENT_SUBMIT_TIME 
    GetSystemTimeAsFileTime(&ft);
    
    if(lpsPropArray) {
        ECFreeBuffer(lpsPropArray);
        lpsPropArray = NULL;
    }

	hr = ECAllocateBuffer(sizeof(SPropValue)*2, (void**)&lpsPropArray);
	if (hr != hrSuccess)
		goto exit;

	lpsPropArray[0].ulPropTag = PR_CLIENT_SUBMIT_TIME;
	lpsPropArray[0].Value.ft = ft;
	
	lpsPropArray[1].ulPropTag = PR_MESSAGE_DELIVERY_TIME;
	lpsPropArray[1].Value.ft = ft;

	hr = SetProps(2, lpsPropArray, NULL);
  	if (hr != hrSuccess)
		goto exit;

	ECFreeBuffer(lpsPropArray);
	lpsPropArray = NULL;

	// Resolve recipients
	hr = this->GetMsgStore()->lpSupport->ExpandRecips(&this->m_xMessage, &ulPreprocessFlags);
	if (hr != hrSuccess)
		goto exit;

	if(this->GetMsgStore()->IsOfflineStore()){
		ulPreprocessFlags |= NEEDS_SPOOLER;
	}

	// Setup PR_SUBMIT_FLAGS
	if(ulPreprocessFlags & NEEDS_PREPROCESSING ) {
		ulSubmitFlag = SUBMITFLAG_PREPROCESS;
	}	
	if(ulPreprocessFlags & NEEDS_SPOOLER ){
		ulSubmitFlag = 0L;
	}

	hr = ECAllocateBuffer(sizeof(SPropValue)*1, (void**)&lpsPropArray);

	if (hr != hrSuccess)
		goto exit;

	lpsPropArray[0].ulPropTag = PR_SUBMIT_FLAGS;
	lpsPropArray[0].Value.l = ulSubmitFlag;
	
	hr = SetProps(1, lpsPropArray, NULL);
	if (hr != hrSuccess)
		goto exit;

	ECFreeBuffer(lpsPropArray);
	lpsPropArray = NULL;

	// All done, save changes
	hr = SaveChanges(KEEP_OPEN_READWRITE);
	if(hr != hrSuccess)
		goto exit;	

	// We look al ulPreprocessFlags to see whether to submit the message via the
	// spooler or not

	if(ulPreprocessFlags & NEEDS_SPOOLER) {
		TRACE_MAPI(TRACE_ENTRY, "Submitting through local queue, flags", "%d", ulPreprocessFlags);

		// Add this message into the local outgoing queue

		hr = this->GetMsgStore()->lpTransport->HrSubmitMessage(this->m_cbEntryId, this->m_lpEntryId, EC_SUBMIT_LOCAL);

		if(hr != hrSuccess)
			goto exit;

	} else {
		TRACE_MAPI(TRACE_ENTRY, "Submitting through master queue, flags", "%d", ulPreprocessFlags);

		// Add the message to the master outgoing queue, and request the spooler to DoSentMail()
		hr = this->GetMsgStore()->lpTransport->HrSubmitMessage(this->m_cbEntryId, this->m_lpEntryId, EC_SUBMIT_MASTER | EC_SUBMIT_DOSENTMAIL);

		if(hr != hrSuccess)
			goto exit;
	}

exit:
    if(lpRecip)
        ECFreeBuffer(lpRecip);
        
	if(lpsRow)
		FreeProws(lpsRow);
	
	if(lpsPropArray)
		ECFreeBuffer(lpsPropArray);

	if(lpRecipientTable)
		lpRecipientTable->Release();

	if(lpTable)
		lpTable->Release();

	return hr;
}

HRESULT ECMessage::SetReadFlag(ULONG ulFlags)
{
	HRESULT			hr = hrSuccess;
	LPSPropValue	lpReadReceiptRequest = NULL;
	LPSPropValue	lpPropFlags = NULL;
	LPSPropValue	lpsPropUserName = NULL;
	LPSPropTagArray	lpsPropTagArray = NULL;
	SPropValue		sProp;
	IMAPIFolder*	lpRootFolder = NULL;
	IMessage*		lpNewMessage = NULL;
	IMessage*		lpThisMessage = NULL;
	ULONG			ulObjType = 0;
	ULONG			cValues = 0;
	ULONG			cbStoreID = 0;
	LPENTRYID		lpStoreID = NULL;
	IMsgStore*		lpDefMsgStore = NULL;

	if((ulFlags &~ (CLEAR_READ_FLAG | CLEAR_NRN_PENDING | CLEAR_RN_PENDING | GENERATE_RECEIPT_ONLY | MAPI_DEFERRED_ERRORS | SUPPRESS_RECEIPT)) != 0 ||
		(ulFlags & (SUPPRESS_RECEIPT | CLEAR_READ_FLAG)) == (SUPPRESS_RECEIPT | CLEAR_READ_FLAG)||
		(ulFlags & (SUPPRESS_RECEIPT | CLEAR_READ_FLAG | GENERATE_RECEIPT_ONLY)) == (SUPPRESS_RECEIPT | CLEAR_READ_FLAG | GENERATE_RECEIPT_ONLY) ||
		(ulFlags & (CLEAR_READ_FLAG | GENERATE_RECEIPT_ONLY)) == (CLEAR_READ_FLAG | GENERATE_RECEIPT_ONLY) )
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if(m_lpParentID) {
		// Unsaved message, ignore (FIXME ?)
		hr = hrSuccess;
		goto exit;
	}

	// see if read receipts are requested
	hr = ECAllocateBuffer(CbNewSPropTagArray(2), (void**)&lpsPropTagArray);
	if(hr != hrSuccess)
		goto exit;

	///////////////////////////////////////////////////
	// Check for Read receipts
	//

	lpsPropTagArray->cValues = 2;
	lpsPropTagArray->aulPropTag[0] = PR_MESSAGE_FLAGS;
	lpsPropTagArray->aulPropTag[1] = PR_READ_RECEIPT_REQUESTED;

	hr = GetPropsInternal(lpsPropTagArray, 0, &cValues, &lpReadReceiptRequest);
	
	if(hr == hrSuccess && (!(ulFlags&(SUPPRESS_RECEIPT|CLEAR_READ_FLAG | CLEAR_NRN_PENDING | CLEAR_RN_PENDING)) || (ulFlags&GENERATE_RECEIPT_ONLY )) && 
		lpReadReceiptRequest[1].Value.b == TRUE && ((lpReadReceiptRequest[0].Value.ul & MSGFLAG_RN_PENDING) || (lpReadReceiptRequest[0].Value.ul & MSGFLAG_NRN_PENDING)))
	{		
		hr = QueryInterface(IID_IMessage, (void**)&lpThisMessage);
		if (hr != hrSuccess)
			goto exit;	

		if((ulFlags & (GENERATE_RECEIPT_ONLY | SUPPRESS_RECEIPT)) == (GENERATE_RECEIPT_ONLY | SUPPRESS_RECEIPT) )
		{
			sProp.ulPropTag = PR_READ_RECEIPT_REQUESTED;
			sProp.Value.b = FALSE;
			hr = HrSetOneProp(lpThisMessage, &sProp);
			if (hr != hrSuccess)
				goto exit;

			hr = lpThisMessage->SaveChanges(KEEP_OPEN_READWRITE);
			if (hr != hrSuccess)
				goto exit;

		}else {
			// Open the default store, by using the username property
			hr = HrGetOneProp(&GetMsgStore()->m_xMsgStore, PR_USER_NAME, &lpsPropUserName);
			if (hr != hrSuccess)
				goto exit;

			hr = GetMsgStore()->CreateStoreEntryID(NULL, lpsPropUserName->Value.LPSZ, fMapiUnicode, &cbStoreID, &lpStoreID);
			if (hr != hrSuccess)
				goto exit;

			hr = GetMsgStore()->lpSupport->OpenEntry(cbStoreID, lpStoreID, NULL, MAPI_MODIFY, &ulObjType, (LPUNKNOWN *) &lpDefMsgStore);
			if (hr != hrSuccess)
				goto exit;

			// Open the root folder of the default store to create a new message
			hr = lpDefMsgStore->OpenEntry(0, NULL, NULL, MAPI_MODIFY, &ulObjType, (LPUNKNOWN *) &lpRootFolder);
			if (hr != hrSuccess)
				goto exit;

			hr = lpRootFolder->CreateMessage(NULL, 0, &lpNewMessage);
			if (hr != hrSuccess)
				goto exit;

			hr = ClientUtil::ReadReceipt(0, lpThisMessage, &lpNewMessage);
			if(hr != hrSuccess)
				goto exit;

			hr = lpNewMessage->SubmitMessage(FORCE_SUBMIT);
			if(hr != hrSuccess)
				goto exit;

			// Oke, everything is fine, so now remove MSGFLAG_RN_PENDING and MSGFLAG_NRN_PENDING from PR_MESSAGE_FLAGS
			// Sent CLEAR_NRN_PENDING and CLEAR_RN_PENDING  for remove those properties
			ulFlags |= CLEAR_NRN_PENDING | CLEAR_RN_PENDING;
		}

	}

	hr = this->GetMsgStore()->lpTransport->HrSetReadFlag(this->m_cbEntryId, this->m_lpEntryId, ulFlags, 0);
	if(hr != hrSuccess)
	    goto exit;
	    
    // Server update OK, change local flags also
    MAPIAllocateBuffer(sizeof(SPropValue), (void **)&lpPropFlags);
    hr = HrGetRealProp(PR_MESSAGE_FLAGS, ulFlags, lpPropFlags, lpPropFlags);
    if(hr != hrSuccess)
        goto exit;
    
    if(ulFlags & CLEAR_READ_FLAG) {
        lpPropFlags->Value.ul &= ~MSGFLAG_READ;
    } else {
        lpPropFlags->Value.ul |= MSGFLAG_READ;
    }
    
    hr = HrSetRealProp(lpPropFlags);
    if(hr != hrSuccess)
        goto exit;

exit:
    if(lpPropFlags)
        ECFreeBuffer(lpPropFlags);
        
	if(lpsPropTagArray)
		ECFreeBuffer(lpsPropTagArray);

	if(lpReadReceiptRequest)
		ECFreeBuffer(lpReadReceiptRequest);

	if(lpsPropUserName)
		MAPIFreeBuffer(lpsPropUserName);

	if(lpStoreID)
		MAPIFreeBuffer(lpStoreID);

	if(lpRootFolder)
		lpRootFolder->Release();

	if(lpNewMessage)
		lpNewMessage->Release();

	if(lpThisMessage)
		lpThisMessage->Release();

	if(lpDefMsgStore)
		lpDefMsgStore->Release();

	return hr;
}

/**
 * Synchronizes this object's PR_DISPLAY_* properties from the
 * contents of the recipient table. They are pushed to the server
 * on save.
 */
HRESULT ECMessage::SyncRecips()
{
	HRESULT hr = hrSuccess;
	std::wstring wstrTo;
	std::wstring wstrCc;
	std::wstring wstrBcc;
	SPropValue sPropRecip;
	IMAPITable *lpTable = NULL;
	LPSRowSet lpRows = NULL;
	SizedSPropTagArray(2, sPropDisplay) = {2, { PR_RECIPIENT_TYPE, PR_DISPLAY_NAME_W} };

	if (this->lpRecips) {
		hr = GetRecipientTable(fMapiUnicode, &lpTable);
		if (hr != hrSuccess)
			goto exit;

		hr = lpTable->SetColumns((LPSPropTagArray)&sPropDisplay, 0);

		while (TRUE) {
			hr = lpTable->QueryRows(1, 0, &lpRows);
			if (hr != hrSuccess || lpRows->cRows != 1)
				break;

			if (lpRows->aRow[0].lpProps[0].ulPropTag == PR_RECIPIENT_TYPE && lpRows->aRow[0].lpProps[0].Value.ul == MAPI_TO) {
				if (lpRows->aRow[0].lpProps[1].ulPropTag == PR_DISPLAY_NAME_W) {
					if (wstrTo.length() > 0)
						wstrTo += L"; ";

					wstrTo += lpRows->aRow[0].lpProps[1].Value.lpszW;
				}
			}
			else if (lpRows->aRow[0].lpProps[0].ulPropTag == PR_RECIPIENT_TYPE && lpRows->aRow[0].lpProps[0].Value.ul == MAPI_CC) {
				if (lpRows->aRow[0].lpProps[1].ulPropTag == PR_DISPLAY_NAME_W) {
					if (wstrCc.length() > 0)
						wstrCc += L"; ";

					wstrCc += lpRows->aRow[0].lpProps[1].Value.lpszW;
				}
			}
			else if (lpRows->aRow[0].lpProps[0].ulPropTag == PR_RECIPIENT_TYPE && lpRows->aRow[0].lpProps[0].Value.ul == MAPI_BCC) {
				if (lpRows->aRow[0].lpProps[1].ulPropTag == PR_DISPLAY_NAME_W) {
					if (wstrBcc.length() > 0)
						wstrBcc += L"; ";

					wstrBcc += lpRows->aRow[0].lpProps[1].Value.lpszW;
				}
			}
			
			FreeProws(lpRows);
			lpRows = NULL;
		}

		sPropRecip.ulPropTag = PR_DISPLAY_TO_W;
		sPropRecip.Value.lpszW = (WCHAR *)wstrTo.c_str();

		HrSetRealProp(&sPropRecip);

		sPropRecip.ulPropTag = PR_DISPLAY_CC_W;
		sPropRecip.Value.lpszW = (WCHAR *)wstrCc.c_str();

		HrSetRealProp(&sPropRecip);

		sPropRecip.ulPropTag = PR_DISPLAY_BCC_W;
		sPropRecip.Value.lpszW = (WCHAR *)wstrBcc.c_str();

		HrSetRealProp(&sPropRecip);

		if(lpRows){
			FreeProws(lpRows);
			lpRows = NULL;
		}
	}

	m_bRecipsDirty = FALSE;

exit:
	if(lpRows)
		FreeProws(lpRows);

	if(lpTable)
		lpTable->Release();

	return hr;
}

HRESULT ECMessage::SaveRecips()
{
	HRESULT				hr = hrSuccess;
	LPSRowSet			lpRowSet = NULL;
	LPSPropValue		lpObjIDs = NULL;
	LPSPropValue		lpRowId = NULL;
	LPULONG				lpulStatus = NULL;
	LPSPropValue		lpEntryID = NULL;
	unsigned int		i = 0,
						j = 0;
	ULONG				ulRealObjType;
	LPSPropValue		lpObjType = NULL;
	ECMapiObjects::iterator iterSObj;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	// Get any changes and set it in the child list of this message
	hr = lpRecips->HrGetAllWithStatus(&lpRowSet, &lpObjIDs, &lpulStatus);
	if (hr != hrSuccess)
		goto exit;

	for (i=0; i<lpRowSet->cRows; i++) {
		MAPIOBJECT *mo = NULL;

		// Get the right object type for a DistList
		lpObjType = PpropFindProp(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_OBJECT_TYPE);
		if(lpObjType != NULL)
			ulRealObjType = lpObjType->Value.ul; // MAPI_MAILUSER or MAPI_DISTLIST
		else
			ulRealObjType = MAPI_MAILUSER; // add in list?

		lpRowId = PpropFindProp(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_ROWID); // unique value of recipient
		if (!lpRowId) {
			ASSERT(lpRowId);
			continue;
		}

		AllocNewMapiObject(lpRowId->Value.ul, lpObjIDs[i].Value.ul, ulRealObjType, &mo);

		// Move any PR_ENTRYID's to PR_EC_CONTACT_ENTRYID
		lpEntryID = PpropFindProp(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_ENTRYID);
		if(lpEntryID)
			lpEntryID->ulPropTag = PR_EC_CONTACT_ENTRYID;

		if (lpulStatus[i] == ECROW_MODIFIED || lpulStatus[i] == ECROW_ADDED) {
			mo->bChanged = true;
			for (j = 0; j < lpRowSet->aRow[i].cValues; j++) {
				if(PROP_TYPE(lpRowSet->aRow[i].lpProps[j].ulPropTag) != PT_NULL) {
					mo->lstModified->push_back(ECProperty(&lpRowSet->aRow[i].lpProps[j]));
					// as in ECGenericProp.cpp, we also save the properties to the known list,
					// since this is used when we reload the object from memory.
					mo->lstProperties->push_back(ECProperty(&lpRowSet->aRow[i].lpProps[j]));
				}
			}
		} else if (lpulStatus[i] == ECROW_DELETED) {
			mo->bDelete = true;
		} else {
			// ECROW_NORMAL, untouched recipient
			for (j = 0; j < lpRowSet->aRow[i].cValues; j++) {
				if(PROP_TYPE(lpRowSet->aRow[i].lpProps[j].ulPropTag) != PT_NULL)
					mo->lstProperties->push_back(ECProperty(&lpRowSet->aRow[i].lpProps[j]));
			}
		}

		// find old recipient in child list, and remove if present
		iterSObj = m_sMapiObject->lstChildren->find(mo);
		if (iterSObj != m_sMapiObject->lstChildren->end()) {
			FreeMapiObject(*iterSObj);
			m_sMapiObject->lstChildren->erase(iterSObj);
		}
		
		m_sMapiObject->lstChildren->insert(mo);
	}

	hr = lpRecips->HrSetClean();
	if(hr != hrSuccess)
		goto exit;

exit:
	if(lpObjIDs)
		ECFreeBuffer(lpObjIDs);

	if(lpRowSet)
		FreeProws(lpRowSet);

	if(lpulStatus)
		ECFreeBuffer(lpulStatus);

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

void ECMessage::RecursiveMarkDelete(MAPIOBJECT *lpObj) {
	ECMapiObjects::iterator iterSObj;

	lpObj->bDelete = true;
	lpObj->lstDeleted->clear();
	lpObj->lstAvailable->clear();
	lpObj->lstModified->clear();
	lpObj->lstProperties->clear();

	for (iterSObj = lpObj->lstChildren->begin(); iterSObj != lpObj->lstChildren->end(); iterSObj++) {
		RecursiveMarkDelete(*iterSObj);
	}
}

BOOL ECMessage::HasAttachment()
{
	HRESULT hr = hrSuccess;
	BOOL bRet = TRUE;
	ECMapiObjects::iterator iterObjects;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	if(lstProps == NULL) {
		hr = HrLoadProps();
		if (hr != hrSuccess)
			goto exit;
		if(lstProps == NULL) {
			hr = MAPI_E_CALL_FAILED;
			goto exit;
		}
	}			

	for (iterObjects = m_sMapiObject->lstChildren->begin(); iterObjects != m_sMapiObject->lstChildren->end(); iterObjects++) {
		if ((*iterObjects)->ulObjType == MAPI_ATTACH)
			break;
	}

	bRet = (iterObjects != m_sMapiObject->lstChildren->end());

exit:
	if(hr != hrSuccess)
		bRet = FALSE;

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return bRet;
}

// Syncs the Attachment table to the child list in the saved object
HRESULT ECMessage::SyncAttachments()
{
	HRESULT				hr = hrSuccess;
	LPSRowSet			lpRowSet = NULL;
	LPSPropValue		lpObjIDs = NULL;
	LPSPropValue		lpAttachNum = NULL;
	LPULONG				lpulStatus = NULL;
	unsigned int		i = 0;
	LPSPropValue		lpObjType = NULL;
	ECMapiObjects::iterator iterSObj;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	// Get any changes and set it in the child list of this message
	// Although we only need to know the deleted attachments, I also need to know the PR_ATTACH_NUM, which is in the rowset
	hr = lpAttachments->HrGetAllWithStatus(&lpRowSet, &lpObjIDs, &lpulStatus);
	if (hr != hrSuccess)
		goto exit;

	for (i=0; i<lpRowSet->cRows; i++) {

		if (lpulStatus[i] != ECROW_DELETED)
			continue;

		lpObjType = PpropFindProp(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_OBJECT_TYPE);
		if(lpObjType == NULL || lpObjType->Value.ul != MAPI_ATTACH)
			continue;

		lpAttachNum = PpropFindProp(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_ATTACH_NUM); // unique value of attachment
		if (!lpAttachNum) {
			ASSERT(lpAttachNum);
			continue;
		}

		// delete complete attachment
		MAPIOBJECT find(lpObjType->Value.ul, lpAttachNum->Value.ul);
		iterSObj = m_sMapiObject->lstChildren->find(&find);
		if (iterSObj != m_sMapiObject->lstChildren->end()) {
			RecursiveMarkDelete(*iterSObj);
		}
	}

	hr = lpAttachments->HrSetClean();
	if(hr != hrSuccess)
		goto exit;

exit:
	if(lpObjIDs)
		ECFreeBuffer(lpObjIDs);

	if(lpRowSet)
		FreeProws(lpRowSet);

	if(lpulStatus)
		ECFreeBuffer(lpulStatus);

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

HRESULT ECMessage::UpdateTable(ECMemTable *lpTable, ULONG ulObjType, ULONG ulObjKeyProp) {
	HRESULT hr = hrSuccess;
	ECMapiObjects::iterator iterObjects;
	SPropValue sKeyProp;
	SPropValue sUniqueProp;
	std::list<ECProperty>::iterator iterPropVals;
	LPSPropValue lpProps = NULL;
	LPSPropValue lpNewProps = NULL;
	LPSPropValue lpAllProps = NULL;
	ULONG cAllValues = 0;
	ULONG cValues = 0;
	ULONG ulProps = 0;
	ULONG i = 0;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	if (!m_sMapiObject) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// update hierarchy id in table
	for (iterObjects = m_sMapiObject->lstChildren->begin(); iterObjects != m_sMapiObject->lstChildren->end(); iterObjects++) {
		if ((*iterObjects)->ulObjType == ulObjType) {
			sUniqueProp.ulPropTag = ulObjKeyProp;
			sUniqueProp.Value.ul = (*iterObjects)->ulUniqueId;

			sKeyProp.ulPropTag = PR_EC_HIERARCHYID;
			sKeyProp.Value.ul = (*iterObjects)->ulObjId;

			hr = lpTable->HrUpdateRowID(&sKeyProp, &sUniqueProp, 1);
			if (hr != hrSuccess)
				goto exit;

			// put new server props in table too
			ulProps = (*iterObjects)->lstProperties->size();
			if (ulProps != 0) {
				// retrieve old row from table
				hr = lpTable->HrGetRowData(&sUniqueProp, &cValues, &lpProps);
				if (hr != hrSuccess)
					goto exit;

				// add new props
				MAPIAllocateBuffer(sizeof(SPropValue)*ulProps, (void**)&lpNewProps);

				for (i = 0, iterPropVals = (*iterObjects)->lstProperties->begin(); iterPropVals != (*iterObjects)->lstProperties->end(); iterPropVals++, i++) {
					(*iterPropVals).CopyToByRef(&lpNewProps[i]);
					if (PROP_ID(lpNewProps[i].ulPropTag) == PROP_ID(PR_ATTACH_DATA_OBJ)) {
						lpNewProps[i].ulPropTag = CHANGE_PROP_TYPE(lpNewProps[i].ulPropTag, PT_ERROR);
						lpNewProps[i].Value.err = MAPI_E_NOT_ENOUGH_MEMORY;
					} else if (PROP_TYPE(lpNewProps[i].ulPropTag) == PT_BINARY && lpNewProps[i].Value.bin.cb > MAX_TABLE_PROPSIZE) {
						lpNewProps[i].ulPropTag = CHANGE_PROP_TYPE(lpNewProps[i].ulPropTag, PT_ERROR);
						lpNewProps[i].Value.err = MAPI_E_NOT_ENOUGH_MEMORY;
					}
				}

				hr = Util::HrMergePropertyArrays(lpProps, cValues, lpNewProps, ulProps, &lpAllProps, &cAllValues);
				if (hr != hrSuccess)
					goto exit;

				hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_MODIFY, &sKeyProp, lpAllProps, cAllValues);
				if (hr != hrSuccess)
					goto exit;

				MAPIFreeBuffer(lpNewProps);
				lpNewProps = NULL;

				MAPIFreeBuffer(lpAllProps);
				lpAllProps = NULL;

				MAPIFreeBuffer(lpProps);
				lpProps = NULL;
			}
		}
	}

	hr = lpTable->HrSetClean();
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpAllProps)
		MAPIFreeBuffer(lpAllProps);

	if (lpNewProps)
		MAPIFreeBuffer(lpNewProps);
		
	if (lpProps)
		MAPIFreeBuffer(lpProps);

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

HRESULT ECMessage::SaveChanges(ULONG ulFlags)
{
	HRESULT				hr = hrSuccess;
	std::map<ULONG, SBinary>::iterator iterSubMessage;
	std::map<ULONG, SBinary>::iterator iterSubMessageNext;

	LPSPropTagArray		lpPropTagArray = NULL;
	LPSPropValue		lpsPropMessageFlags = NULL;
	ULONG				cValues = 0;
	LPMAPITABLE			lpTable = NULL;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	// could not have modified (easy way out of my bug)
	if (!fModify) {
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	// nothing changed -> no need to save 
 	if (this->lstProps == NULL) 
 		goto exit; 

	ASSERT(m_sMapiObject != NULL); // the actual bug .. keep open on submessage

	if (this->lpRecips) {
		hr = SaveRecips();
		if (hr != hrSuccess)
			goto exit;

		// Synchronize PR_DISPLAY_* ... FIXME should we do this after each ModifyRecipients ?
		SyncRecips();
	}

	// Synchronize any changes between RTF, HTML and plaintext before proceeding
	SyncRTF();

	if (this->lpAttachments) {
		// set deleted attachments in saved child list
		hr = SyncAttachments();
		if (hr != hrSuccess)
			goto exit;
	}


	// Property change of a new item
	if (fNew && this->GetMsgStore()->IsSpooler() == TRUE) {

		ECAllocateBuffer(CbNewSPropTagArray(1), (void**)&lpPropTagArray);
		lpPropTagArray->cValues = 1;
		lpPropTagArray->aulPropTag[0] = PR_MESSAGE_FLAGS;

		hr = GetPropsInternal(lpPropTagArray, 0, &cValues, &lpsPropMessageFlags);
		if(hr != hrSuccess)
			goto exit;
		
		lpsPropMessageFlags->ulPropTag = PR_MESSAGE_FLAGS;
		lpsPropMessageFlags->Value.l &= ~(MSGFLAG_READ|MSGFLAG_UNSENT);
		lpsPropMessageFlags->Value.l |= MSGFLAG_UNMODIFIED;
		
		hr = SetProps(1, lpsPropMessageFlags, NULL);
		if(hr != hrSuccess)
			goto exit;
	}

	// don't re-sync bodies that are returned from server
	m_bBusySyncRTF = TRUE;

	hr = ECMAPIProp::SaveChanges(ulFlags);

	m_ulLastChange = syncChangeNone;
	m_bBusySyncRTF = FALSE;
	m_bExplicitSubjectPrefix = FALSE;

	if(hr != hrSuccess)
		goto exit;

	// resync recip and attachment table, because of hierarchy id's, only on actual saved object
	if (m_sMapiObject && m_bEmbedded == false) {
		if (lpRecips) {
			hr = UpdateTable(lpRecips, MAPI_MAILUSER, PR_ROWID);
			if(hr != hrSuccess)
				goto exit;

			hr = UpdateTable(lpRecips, MAPI_DISTLIST, PR_ROWID);
			if(hr != hrSuccess)
				goto exit;
		}
		if (lpAttachments) {
			hr = UpdateTable(lpAttachments, MAPI_ATTACH, PR_ATTACH_NUM);
			if(hr != hrSuccess)
				goto exit;
		}
	}

exit:

	if (lpTable)
		lpTable->Release();

	if (lpPropTagArray)
		ECFreeBuffer(lpPropTagArray);

	if (lpsPropMessageFlags)
		ECFreeBuffer(lpsPropMessageFlags);

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

/**
 * Sync PR_SUBJECT, PR_SUBJECT_PREFIX
 */
HRESULT ECMessage::SyncSubject()
{
    HRESULT			hr = hrSuccess;
	HRESULT			hr1 = hrSuccess;
	HRESULT			hr2 = hrSuccess;
	BOOL			bDirtySubject = FALSE;
	BOOL			bDirtySubjectPrefix = FALSE;
	LPSPropValue	lpPropArray = NULL;
	ULONG			cValues = 0;
	WCHAR*			lpszColon = NULL;
	WCHAR*			lpszEnd = NULL;
	int				sizePrefix1 = 0;

	SizedSPropTagArray(2, sPropSubjects) = {2, { PR_SUBJECT_W, PR_SUBJECT_PREFIX_W} };
	
	hr1 = IsPropDirty(CHANGE_PROP_TYPE(PR_SUBJECT, PT_UNSPECIFIED), &bDirtySubject);
	hr2 = IsPropDirty(CHANGE_PROP_TYPE(PR_SUBJECT_PREFIX, PT_UNSPECIFIED), &bDirtySubjectPrefix);
	
	// if both not present or not dirty
	if( (hr1 != hrSuccess && hr2 != hrSuccess) || (hr1 == hr2 && bDirtySubject == FALSE && bDirtySubjectPrefix == FALSE) )
		goto exit;

	// If subject is deleted but the prefix is not, delete it
	if(hr1 != hrSuccess && hr2 == hrSuccess)
	{
		hr = HrDeleteRealProp(CHANGE_PROP_TYPE(PR_SUBJECT_PREFIX, PT_UNSPECIFIED), FALSE);
		goto exit;
	}

	//////////////////////////////////////////
	// Check if subject and prefix in sync

	hr = GetPropsInternal((LPSPropTagArray)&sPropSubjects, 0, &cValues, &lpPropArray);
	if(HR_FAILED(hr))
		goto exit;

	if(lpPropArray[0].ulPropTag == PR_SUBJECT_W)
		lpszColon = wcschr(lpPropArray[0].Value.lpszW, L':');

	if(lpszColon == NULL) {
		//Set emtpy PR_SUBJECT_PREFIX
		lpPropArray[1].ulPropTag = PR_SUBJECT_PREFIX_W;
		lpPropArray[1].Value.lpszW = L"";
		
		hr = HrSetRealProp(&lpPropArray[1]);
		if(hr != hrSuccess)
			goto exit;

	} else {
		sizePrefix1 = lpszColon - lpPropArray[0].Value.lpszW + 1;

		// synchronized PR_SUBJECT_PREFIX
		lpPropArray[1].ulPropTag = PR_SUBJECT_PREFIX_W;	// If PROP_TYPE(lpPropArray[1].ulPropTag) == PT_ERROR, we lose that info here.
			
		if (sizePrefix1 > 1 && sizePrefix1 <= 4)
		{
			if (lpPropArray[0].Value.lpszW[sizePrefix1] == L' ')
				lpPropArray[0].Value.lpszW[sizePrefix1+1] = 0; // with space "fwd: "
			else
				lpPropArray[0].Value.lpszW[sizePrefix1] = 0; // "fwd:"

			ASSERT(lpPropArray[0].Value.lpszW[sizePrefix1-1] == L':');
			lpPropArray[1].Value.lpszW = lpPropArray[0].Value.lpszW;

			wcstol(lpPropArray[1].Value.lpszW, &lpszEnd, 10);
			if (lpszEnd == lpszColon)
				lpPropArray[1].Value.lpszW = L"";	// skip a numeric prefix
		} else
			lpPropArray[1].Value.lpszW = L"";	// emtpy PR_SUBJECT_PREFIX

		hr = HrSetRealProp(&lpPropArray[1]);
		if (hr != hrSuccess)
			goto exit;

		// PR_SUBJECT_PREFIX and PR_SUBJECT are synchronized
	}
	
exit:
	if(lpPropArray)
		ECFreeBuffer(lpPropArray);

	return hr;
}

// Override IMAPIProp::SetProps
HRESULT ECMessage::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;
	LPSPropValue pvalSubject;
	LPSPropValue pvalSubjectPrefix;
	LPSPropValue pvalRtf;
	LPSPropValue pvalHtml;
	LPSPropValue pvalBody;

	// Send to IMAPIProp first
	hr = ECMAPIProp::SetProps(cValues, lpPropArray, lppProblems);
	if (hr != hrSuccess)
		goto exit;

	/* We only sync the subject (like a PST does) in the following conditions:
	 * 1) PR_SUBJECT is modified, and PR_SUBJECT_PREFIX was not set
	 * 2) PR_SUBJECT is modified, and PR_SUBJECT_PREFIX was modified by a previous SyncSubject() call
	 * If the caller ever does a SetProps on the PR_SUBJECT_PREFIX itself, we must never touch it ourselves again, until SaveChanges().
	 */
	pvalSubject = PpropFindProp(lpPropArray, cValues, CHANGE_PROP_TYPE(PR_SUBJECT, PT_UNSPECIFIED));
	pvalSubjectPrefix = PpropFindProp(lpPropArray, cValues, CHANGE_PROP_TYPE(PR_SUBJECT_PREFIX, PT_UNSPECIFIED));
	if (pvalSubjectPrefix)
		m_bExplicitSubjectPrefix = TRUE;
	if (pvalSubject && m_bExplicitSubjectPrefix == FALSE)
		SyncSubject();

	// Now, sync RTF
	pvalRtf = PpropFindProp(lpPropArray, cValues, PR_RTF_COMPRESSED);
	pvalHtml = PpropFindProp(lpPropArray, cValues, PROP_TAG(PT_UNSPECIFIED, PROP_ID(PR_BODY_HTML)) );
	pvalBody = PpropFindProp(lpPropArray, cValues, PROP_TAG(PT_UNSPECIFIED, PROP_ID(PR_BODY)) );

	// IF the user sets both the body and the RTF, assume RTF overrides
	if (pvalRtf) {
		m_ulLastChange = syncChangeRTF;
	} else if (pvalHtml) {
		m_ulLastChange = syncChangeHTML;
	} else if(pvalBody) {
		m_ulLastChange = syncChangeBody;
	}
	m_ulBodyType = bodyTypeUnknown;
	
exit:
	return hr;		
}

HRESULT ECMessage::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;
	SPropTagArray sSubjectPrefix = {1, { CHANGE_PROP_TYPE(PR_SUBJECT_PREFIX, PT_UNSPECIFIED) } };

	// Send to IMAPIProp first
	hr = ECMAPIProp::DeleteProps(lpPropTagArray, lppProblems);
	if (FAILED(hr))
		goto exit;

	// If the PR_SUBJECT is removed and we generated the prefix, we need to remove that property too.
	if (m_bExplicitSubjectPrefix == FALSE && Util::FindPropInArray(lpPropTagArray, CHANGE_PROP_TYPE(PR_SUBJECT, PT_UNSPECIFIED)) >= 0)
		ECMAPIProp::DeleteProps(&sSubjectPrefix, NULL);

	// If an explicit prefix was set and now removed, we must sync it again on the next SetProps of the subject
	if (m_bExplicitSubjectPrefix == TRUE && Util::FindPropInArray(lpPropTagArray, CHANGE_PROP_TYPE(PR_SUBJECT_PREFIX, PT_UNSPECIFIED)) >= 0)
		m_bExplicitSubjectPrefix = FALSE;

exit:
	return hr;
}

HRESULT ECMessage::TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType)
{
	HRESULT hr = hrSuccess;
	ECMsgStore *lpMsgStore = (ECMsgStore *)lpProvider;

	if(lpsPropValSrc->ulPropTag == PR_SOURCE_KEY) {
		if((lpMsgStore->m_ulProfileFlags & EC_PROFILE_FLAGS_TRUNCATE_SOURCEKEY) && lpsPropValSrc->Value.bin->__size > 22) {
			lpsPropValSrc->Value.bin->__size = 22;
			lpsPropValSrc->Value.bin->__ptr[lpsPropValSrc->Value.bin->__size-1] |= 0x80; // Set top bit
			hr = CopySOAPPropValToMAPIPropVal(lpsPropValDst, lpsPropValSrc, lpBase);
		} else {
			hr = MAPI_E_NOT_FOUND;
		}
	} else {
		hr = MAPI_E_NOT_FOUND;
	}

	return hr;
}

HRESULT	ECMessage::GetPropHandler(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase)
{
	HRESULT hr = hrSuccess;
	unsigned int ulSize = 0;
	LPBYTE	lpData = NULL;
	ECMessage *lpMessage = (ECMessage *)lpParam;
	
	switch(PROP_ID(ulPropTag)) {
	case PROP_ID(PR_RTF_IN_SYNC):
		lpsPropValue->ulPropTag = PR_RTF_IN_SYNC;
		lpsPropValue->Value.ul = TRUE; // Always in sync because we sync internally
		break;
	case PROP_ID(PR_HASATTACH):
		lpsPropValue->ulPropTag = PR_HASATTACH;
		lpsPropValue->Value.b = lpMessage->HasAttachment();
		break;
	case PROP_ID(PR_MESSAGE_FLAGS):
	{
		hr = lpMessage->HrGetRealProp(PR_MESSAGE_FLAGS, ulFlags, lpBase, lpsPropValue);
		if(hr != hrSuccess) {
			hr = hrSuccess;
			lpsPropValue->ulPropTag = PR_MESSAGE_FLAGS;
			lpsPropValue->Value.ul = MSGFLAG_READ;
		}
		// Force MSGFLAG_HASATTACH to the correct value
		lpsPropValue->Value.ul = (lpsPropValue->Value.ul & ~MSGFLAG_HASATTACH) | (lpMessage->HasAttachment() ? MSGFLAG_HASATTACH : 0);
		break;
	}
	case PROP_ID(PR_NORMALIZED_SUBJECT): 
		hr = lpMessage->HrGetRealProp(CHANGE_PROP_TYPE(PR_SUBJECT, PROP_TYPE(ulPropTag)), ulFlags, lpBase, lpsPropValue);
		if (hr != hrSuccess) {
			// change PR_SUBJECT in PR_NORMALIZED_SUBJECT
			lpsPropValue->ulPropTag = CHANGE_PROP_TYPE(PR_NORMALIZED_SUBJECT, PT_ERROR);
			break;
		}

		if (PROP_TYPE(ulPropTag) == PT_UNICODE) {
			lpsPropValue->ulPropTag = PR_NORMALIZED_SUBJECT_W;

			WCHAR *lpszColon = wcschr(lpsPropValue->Value.lpszW, ':');
			if (lpszColon && (lpszColon - lpsPropValue->Value.lpszW) > 1 && (lpszColon - lpsPropValue->Value.lpszW) < 4) {
				WCHAR *c = lpsPropValue->Value.lpszW;
				while (c < lpszColon && iswdigit(*c)) c++; // test for all digits prefix
				if (c != lpszColon) {
					lpszColon++;
					if (*lpszColon == ' ')
						lpszColon++;
					lpsPropValue->Value.lpszW = lpszColon; // set new subject string
				}
			}
		} else {
			lpsPropValue->ulPropTag = PR_NORMALIZED_SUBJECT_A;

			char *lpszColon = strchr(lpsPropValue->Value.lpszA, ':');
			if (lpszColon && (lpszColon - lpsPropValue->Value.lpszA) > 1 && (lpszColon - lpsPropValue->Value.lpszA) < 4) {
				char *c = lpsPropValue->Value.lpszA;
				while (c < lpszColon && isdigit(*c)) c++; // test for all digits prefix
				if (c != lpszColon) {
					lpszColon++;
					if (*lpszColon == ' ')
						lpszColon++;
					lpsPropValue->Value.lpszA = lpszColon; // set new subject string
				}
			}
		}
		break;
	case PROP_ID(PR_PARENT_ENTRYID):
		
		if(!lpMessage->m_lpParentID)
			hr = lpMessage->HrGetRealProp(PR_PARENT_ENTRYID, ulFlags, lpBase, lpsPropValue);
		else{
			lpsPropValue->ulPropTag = PR_PARENT_ENTRYID;
			lpsPropValue->Value.bin.cb = lpMessage->m_cbParentID;

			ECAllocateMore(lpsPropValue->Value.bin.cb, lpBase, (LPVOID *)&lpsPropValue->Value.bin.lpb);
			memcpy(lpsPropValue->Value.bin.lpb, lpMessage->m_lpParentID, lpsPropValue->Value.bin.cb);
		}
		break;
	case PROP_ID(PR_MESSAGE_SIZE):
		lpsPropValue->ulPropTag = PR_MESSAGE_SIZE;
		if(lpMessage->m_lpEntryId == NULL) //new message
			lpsPropValue->Value.l = 1024;
		else
			hr = lpMessage->HrGetRealProp(PR_MESSAGE_SIZE, ulFlags, lpBase, lpsPropValue);
		break;
	case PROP_ID(PR_DISPLAY_TO):
	case PROP_ID(PR_DISPLAY_CC):
	case PROP_ID(PR_DISPLAY_BCC):
		if((lpMessage->m_bRecipsDirty && lpMessage->SyncRecips() != erSuccess) || lpMessage->HrGetRealProp(ulPropTag, ulFlags, lpBase, lpsPropValue) != erSuccess) {
			lpsPropValue->ulPropTag = ulPropTag;
			if(PROP_TYPE(ulPropTag) == PT_UNICODE)
				lpsPropValue->Value.lpszW = L"";
			else
				lpsPropValue->Value.lpszA = "";
		}
		break;

	case PROP_ID(PR_ACCESS):
		if(lpMessage->HrGetRealProp(PR_ACCESS, ulFlags, lpBase, lpsPropValue) != hrSuccess)
		{
			lpsPropValue->ulPropTag = PR_ACCESS;
			lpsPropValue->Value.l = MAPI_ACCESS_READ | MAPI_ACCESS_MODIFY | MAPI_ACCESS_DELETE;
		}
		break;
	case PROP_ID(PR_MESSAGE_ATTACHMENTS):
		lpsPropValue->ulPropTag = PR_MESSAGE_ATTACHMENTS;
		lpsPropValue->Value.x = 1;
		break;
	case PROP_ID(PR_MESSAGE_RECIPIENTS):
		lpsPropValue->ulPropTag = PR_MESSAGE_RECIPIENTS;
		lpsPropValue->Value.x = 1;
		break;
	case PROP_ID(PR_BODY_HTML):
		if(ulPropTag != PR_BODY_HTML) {
			hr = MAPI_E_NOT_FOUND;
			break;
		}
		
		// Workaround for support html in outlook 2000/xp
		hr = lpMessage->HrGetRealProp(PR_HTML, ulFlags, lpBase, lpsPropValue);
		if(hr != hrSuccess || lpsPropValue->ulPropTag != PR_HTML){
			hr = MAPI_E_NOT_FOUND;
			break;
		}

		lpsPropValue->ulPropTag = PR_BODY_HTML;
		ulSize = lpsPropValue->Value.bin.cb;
		lpData = lpsPropValue->Value.bin.lpb;

		hr = ECAllocateMore(ulSize + 1, lpBase, (void**)&lpsPropValue->Value.lpszA);
		if(hr != hrSuccess)
			break;
		
		if(ulSize>0 && lpData){
			memcpy(lpsPropValue->Value.lpszA, lpData, ulSize);
		}else
			ulSize = 0;
		
		lpsPropValue->Value.lpszA[ulSize] = 0;			

		break;
	case PROP_ID(PR_SOURCE_KEY): {
		std::string strServerGUID;
		std::string strID;
		std::string strSourceKey;

		if(ECMAPIProp::DefaultMAPIGetProp(PR_SOURCE_KEY, lpProvider, ulFlags, lpsPropValue, lpParam, lpBase) == hrSuccess)
			goto exit;

		// The server did not supply a PR_SOURCE_KEY, generate one ourselves.

		hr = lpMsgStore->HrGetRealProp(PR_MAPPING_SIGNATURE, ulFlags, lpBase, lpsPropValue);
		if(hr != hrSuccess)
			goto exit;

		strServerGUID.assign((char*)lpsPropValue->Value.bin.lpb, lpsPropValue->Value.bin.cb);

		hr = lpMessage->HrGetRealProp(PR_RECORD_KEY, ulFlags, lpBase, lpsPropValue);
		if(hr != hrSuccess)
			goto exit;

		strID.assign((char *)lpsPropValue->Value.bin.lpb, lpsPropValue->Value.bin.cb);

		// Resize so it trails 6 null bytes
		strID.resize(6,0);

		strSourceKey = strServerGUID + strID;

		hr = MAPIAllocateMore(strSourceKey.size(), lpBase, (void **)&lpsPropValue->Value.bin.lpb);
		if(hr != hrSuccess)
			goto exit;

		lpsPropValue->ulPropTag = PR_SOURCE_KEY;
		lpsPropValue->Value.bin.cb = strSourceKey.size();
		memcpy(lpsPropValue->Value.bin.lpb, strSourceKey.c_str(), strSourceKey.size());

		break;
	}
	default:
		hr = MAPI_E_NOT_FOUND;
		break;
	}
exit:
	return hr;
}

HRESULT ECMessage::SetPropHandler(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam)
{
	ECMessage *lpMessage = (ECMessage *)lpParam;
	HRESULT hr = hrSuccess;
	char* lpData = NULL;

	switch(ulPropTag) {
	case PR_BODY_HTML:
		// Set PR_BODY_HTML to PR_HTML
		lpsPropValue->ulPropTag = PR_HTML;
		lpData = lpsPropValue->Value.lpszA;

		if(lpData) {
			lpsPropValue->Value.bin.cb = strlen(lpData);
			lpsPropValue->Value.bin.lpb = (LPBYTE)lpData;
		}
		else {
			lpsPropValue->Value.bin.cb = 0;
		}

		hr = lpMessage->HrSetRealProp(lpsPropValue);
		break;
	case PR_MESSAGE_FLAGS:
		if (lpMessage->m_sMapiObject == NULL || lpMessage->m_sMapiObject->ulObjId == 0) {
			// filter any invalid flags
			lpsPropValue->Value.l &= 0x03FF;

			if (lpMessage->HasAttachment())
				lpsPropValue->Value.l |= MSGFLAG_HASATTACH;

			hr = lpMessage->HrSetRealProp(lpsPropValue);
		}
		break;
	case PR_SOURCE_KEY:
		hr = ECMAPIProp::SetPropHandler(ulPropTag, lpProvider, lpsPropValue, lpParam);
		break;
	default:
		hr = MAPI_E_NOT_FOUND;
		break;
	}
	return hr;
}

// Use the support object to do the copying
HRESULT ECMessage::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;
	IECUnknown *lpECUnknown = NULL;
	LPSPropValue lpECObject = NULL;
	ECMAPIProp *lpECMAPIProp = NULL;
	ECMAPIProp *lpDestTop = NULL;
	ECMAPIProp *lpSourceTop = NULL;
	GUID sDestServerGuid = {0};
	GUID sSourceServerGuid = {0};

	if(lpDestObj == NULL) {
	    hr = MAPI_E_INVALID_PARAMETER;
	    goto exit;
    }

	// Wrap mapi object to zarafa object
	if (HrGetOneProp((LPMAPIPROP)lpDestObj, PR_EC_OBJECT, &lpECObject) == hrSuccess) {
		lpECUnknown = (IECUnknown*)lpECObject->Value.lpszA;
		lpECUnknown->AddRef();

		MAPIFreeBuffer(lpECObject);
	}

	// Deny copying within the same object. This is not allowed in exchange either and is required to deny
	// creating large recursive objects.
	if(lpECUnknown && lpECUnknown->QueryInterface(IID_ECMAPIProp, (void **)&lpECMAPIProp) == hrSuccess) {
		// Find the top-level objects for both source and destination objects
		lpDestTop = lpECMAPIProp->m_lpRoot;
		lpSourceTop = this->m_lpRoot;

		// destination may not be a child of the source, but source can be a child of destination
		if (!this->IsChildOf(lpDestTop)) {
			// ICS expects the entryids to be equal. So check if the objects reside on
			// the same server as well.
			hr = lpDestTop->GetMsgStore()->lpTransport->GetServerGUID(&sDestServerGuid);
			if (hr != hrSuccess)
				goto exit;

			hr = lpSourceTop->GetMsgStore()->lpTransport->GetServerGUID(&sSourceServerGuid);
			if (hr != hrSuccess)
				goto exit;

			if(lpDestTop->m_lpEntryId && lpSourceTop->m_lpEntryId && 
			   lpDestTop->m_cbEntryId == lpSourceTop->m_cbEntryId && 
			   memcmp(lpDestTop->m_lpEntryId, lpSourceTop->m_lpEntryId, lpDestTop->m_cbEntryId) == 0 &&
			   sDestServerGuid == sSourceServerGuid) {
				// Source and destination are the same on-disk objects (entryids are equal)

				hr = MAPI_E_NO_ACCESS;
				goto exit;
			}
		}

		lpECMAPIProp->Release();
		lpECMAPIProp = NULL;
	}
	
	hr = Util::DoCopyTo(&IID_IMessage, &this->m_xMessage, ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
	
	
exit:
	if(lpECMAPIProp)
		lpECMAPIProp->Release();

	if (lpECUnknown)
		lpECUnknown->Release();

	return hr;
}

// We override HrLoadProps to setup PR_BODY and PR_RTF_COMPRESSED in the initial message
// Normally, this should never be needed, as messages should store both the PR_BODY as the PR_RTF_COMPRESSED
// when saving.
HRESULT ECMessage::HrLoadProps()
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpsBodyProps = NULL;
	SizedSPropTagArray(3, sPropBodyTags) = { 3, { PR_BODY_W, PR_RTF_COMPRESSED, PR_HTML } };
	ULONG cValues = 0;
	BOOL fBodyOK = FALSE;
	BOOL fRTFOK = FALSE;
	BOOL fHTMLOK = FALSE;

	hr = ECMAPIProp::HrLoadProps();
	if(hr != hrSuccess)
		goto exit;

	// We just loaded, so no sync required except detection below
	this->m_ulLastChange = syncChangeNone;
	this->m_ulBodyType = bodyTypeUnknown;

	// Check for RTF and BODY properties
	hr = GetPropsInternal((LPSPropTagArray)&sPropBodyTags, 0, &cValues, &lpsBodyProps);

	if(HR_FAILED(hr)) {
		goto exit;
	}

	hr = hrSuccess;

	if(lpsBodyProps[0].ulPropTag == PR_BODY_W || (lpsBodyProps[0].ulPropTag == PROP_TAG(PT_ERROR, PROP_ID(PR_BODY)) && lpsBodyProps[0].Value.err == MAPI_E_NOT_ENOUGH_MEMORY))
		fBodyOK = TRUE;

	if(lpsBodyProps[1].ulPropTag == PR_RTF_COMPRESSED || (lpsBodyProps[1].ulPropTag == PROP_TAG(PT_ERROR, PROP_ID(PR_RTF_COMPRESSED)) && lpsBodyProps[1].Value.err == MAPI_E_NOT_ENOUGH_MEMORY))
		fRTFOK = TRUE;

	if(lpsBodyProps[2].ulPropTag == PR_HTML || (lpsBodyProps[2].ulPropTag == PROP_TAG(PT_ERROR, PROP_ID(PR_HTML)) && lpsBodyProps[2].Value.err == MAPI_E_NOT_ENOUGH_MEMORY))
		fHTMLOK = TRUE;

	// Neither RTF nor BODY nor HTML, do nothing
	if(!fBodyOK && !fRTFOK && !fHTMLOK) {
		goto exit;
	}
	// Both RTF and BODY AND HTML, do nothing
	else if(fBodyOK && fRTFOK && fHTMLOK) {
		goto exit;
	}
	// Only RTF
	else if(fRTFOK) {
		m_ulLastChange = syncChangeRTF;
	}
	// HTML
	else if(fHTMLOK) {
		m_ulLastChange = syncChangeHTML;
	}
	// Only BODY
	else if(fBodyOK) {
		m_ulLastChange = syncChangeBody;
	}

	SyncRTF();

exit:

	if(lpsBodyProps)
		ECFreeBuffer(lpsBodyProps);

	return hr;
}

HRESULT ECMessage::HrSetRealProp(SPropValue *lpsPropValue)
{
	HRESULT hr = hrSuccess;

	hr = ECMAPIProp::HrSetRealProp(lpsPropValue);
	if(hr != hrSuccess)
		goto exit;

	if (lpsPropValue->ulPropTag == PR_RTF_COMPRESSED)
		m_ulLastChange = syncChangeRTF;
	else if (lpsPropValue->ulPropTag == PR_HTML)
		m_ulLastChange = syncChangeHTML;
	else if (lpsPropValue->ulPropTag == PR_BODY_W || lpsPropValue->ulPropTag == PR_BODY_A)
		m_ulLastChange = syncChangeBody;

exit:
	return hr;
}

struct findobject_if {
    unsigned int m_ulUniqueId;
    unsigned int m_ulObjType;
    
    findobject_if(unsigned int ulObjType, unsigned int ulUniqueId) : m_ulUniqueId(ulUniqueId), m_ulObjType(ulObjType) {}
                
    bool operator()(MAPIOBJECT *entry)
    {
        return entry->ulUniqueId == m_ulUniqueId && entry->ulObjType == m_ulObjType;
    }
};
                                    
// Copies the server object IDs from lpSrc into lpDest by matching the correct object type
// and unique ID for each object.
HRESULT HrCopyObjIDs(MAPIOBJECT *lpDest, MAPIOBJECT *lpSrc)
{
    HRESULT hr = hrSuccess;
    ECMapiObjects::iterator iterSrc;
    ECMapiObjects::iterator iterDest;
    
    lpDest->ulObjId = lpSrc->ulObjId;

    for(iterSrc = lpSrc->lstChildren->begin(); iterSrc != lpSrc->lstChildren->end(); iterSrc++) {
    	iterDest = lpDest->lstChildren->find(*iterSrc);
        if(iterDest != lpDest->lstChildren->end()) {
            hr = HrCopyObjIDs(*iterDest, *iterSrc);
            if(hr != hrSuccess)
                goto exit;
        }
    }
    
exit:
    return hr;
}

HRESULT ECMessage::HrSaveChild(ULONG ulFlags, MAPIOBJECT *lpsMapiObject) {
	HRESULT hr = hrSuccess;
	IMAPITable *lpTable = NULL;
	ECMapiObjects::iterator iterSObj;
	SPropValue sKeyProp;
	LPSPropValue lpProps = NULL;
	ULONG ulProps = 0;
	LPSPropValue lpPropID = NULL;
	LPSPropValue lpPropObjType = NULL;
	std::list<ECProperty>::iterator iterPropVals;
	ULONG i;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	if (lpsMapiObject->ulObjType != MAPI_ATTACH) {
		// can only save attachments as child objects
		// (recipients are saved through SaveRecips() from SaveChanges() on this object)
		hr = MAPI_E_INVALID_OBJECT;
		goto exit;
	}

	if(this->lpAttachments == NULL) {
		hr = this->GetAttachmentTable(fMapiUnicode, &lpTable);

		if(hr != hrSuccess)
			goto exit;

		lpTable->Release();
	}

	if(this->lpAttachments == NULL) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}


	if (!m_sMapiObject) {
		// when does this happen? .. just a simple precaution for now
		ASSERT(m_sMapiObject != NULL);
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	// Replace the attachment in the object hierarchy with this one, but preserve server object id. This is needed
	// if the entire object has been saved to the server in the mean time.
	iterSObj = m_sMapiObject->lstChildren->find(lpsMapiObject);
	if (iterSObj != m_sMapiObject->lstChildren->end()) {
		// Preserve server IDs
		hr = HrCopyObjIDs(lpsMapiObject, (*iterSObj));
		if(hr != hrSuccess)
			goto exit;
			
		// Remove item
		FreeMapiObject(*iterSObj);
		m_sMapiObject->lstChildren->erase(iterSObj);
	}

	m_sMapiObject->lstChildren->insert(new MAPIOBJECT(lpsMapiObject));

	// Update the attachment table. The attachment table contains all properties of the attachments
	ulProps = lpsMapiObject->lstProperties->size();
	
	// +2 for maybe missing PR_ATTACH_NUM and PR_OBJECT_TYPE properties
	ECAllocateBuffer(sizeof(SPropValue)*(ulProps+2), (void**)&lpProps);

	lpPropID = NULL;
	for (i = 0, iterPropVals = lpsMapiObject->lstProperties->begin(); iterPropVals != lpsMapiObject->lstProperties->end(); iterPropVals++, i++) {
		(*iterPropVals).CopyToByRef(&lpProps[i]);

		if (lpProps[i].ulPropTag == PR_ATTACH_NUM) {
			lpPropID = &lpProps[i];
		} else if(lpProps[i].ulPropTag == PR_OBJECT_TYPE) {
			lpPropObjType = &lpProps[i];
		} else if (PROP_ID(lpProps[i].ulPropTag) == PROP_ID(PR_ATTACH_DATA_OBJ)) {
			lpProps[i].ulPropTag = CHANGE_PROP_TYPE(lpProps[i].ulPropTag, PT_ERROR);
			lpProps[i].Value.err = MAPI_E_NOT_ENOUGH_MEMORY;
		} else if (PROP_TYPE(lpProps[i].ulPropTag) == PT_BINARY && lpProps[i].Value.bin.cb > MAX_TABLE_PROPSIZE) {
			lpProps[i].ulPropTag = CHANGE_PROP_TYPE(lpProps[i].ulPropTag, PT_ERROR);
			lpProps[i].Value.err = MAPI_E_NOT_ENOUGH_MEMORY;
		}
	}

	if (lpPropID == NULL) {
		ulProps++;
		lpPropID = &lpProps[i++];
	}

	if (lpPropObjType == NULL) {
		ulProps++;
		lpPropObjType = &lpProps[i++];
	}

	lpPropObjType->ulPropTag = PR_OBJECT_TYPE;
	lpPropObjType->Value.ul = MAPI_ATTACH;

	lpPropID->ulPropTag = PR_ATTACH_NUM;
	lpPropID->Value.ul = lpsMapiObject->ulUniqueId;

	sKeyProp.ulPropTag = PR_EC_HIERARCHYID;
	sKeyProp.Value.ul = lpsMapiObject->ulObjId;

	hr = lpAttachments->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, &sKeyProp, lpProps, ulProps);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpProps)
		ECFreeBuffer(lpProps);

	pthread_mutex_unlock(&m_hMutexMAPIObject);
	return hr;
}

/**
 * Synchronises all 3 body types from the one that was modified the last time.
 *
 * @note Don't use RTFSync as the RTFSync function can only sync when there is an existing
 * PR_RTF_COMPRESSED. Which is quite often not the case. 
 */
HRESULT ECMessage::SyncRTF()
{
	IStream *lpBodyStream = NULL;
	IStream *lpRTFCompressedStream = NULL;
	IStream *lpRTFUncompressedStream = NULL;
	IStream *lpHTMLStream = NULL;
	ECMemStream *lpEmptyMemStream = NULL;
	LPSPropValue lpsPropValue = NULL;
	HRESULT hr = hrSuccess;
	ULONG ulRead = 0;
	ULONG ulWritten = 0;
	unsigned int ulCodepage = 0;
	wstring strOut;
	string strHTMLOut;
	string strBody;

	BOOL fModifySaved = FALSE;
	BOOL bUpdated = FALSE;
	eSyncChange ulLastChange;

	std::string strRTF;
	char lpBuf[4096];
	LARGE_INTEGER moveBegin = {{0,0}};
	ULARGE_INTEGER emptySize = {{0,0}};

	enum eRTFType { RTFTypeOther, RTFTypeFromText, RTFTypeFromHTML};
	eRTFType	rtfType = RTFTypeOther;

	// HACK ALERT: we force fModify to TRUE, because even on read-only messages,
	// we want to be able to create RTF_COMPRESSED or BODY from the other, which
	// is basically a WRITE to the object
	
	fModifySaved = this->fModify;
	this->fModify = TRUE;

	if(m_ulLastChange == syncChangeNone || m_bBusySyncRTF == TRUE)
		goto exit; // Nothing to do

	// Mark as busy now, so we don't recurse on ourselves
	m_bBusySyncRTF = TRUE;
	ulLastChange = m_ulLastChange;
	m_ulLastChange = syncChangeNone;

	ECAllocateBuffer(sizeof(SPropValue), (void**)&lpsPropValue);
	if(HrGetRealProp(PR_INTERNET_CPID, 0, lpsPropValue, lpsPropValue) == hrSuccess &&
		lpsPropValue->ulPropTag == PR_INTERNET_CPID )
	{
		ulCodepage = lpsPropValue->Value.ul;
	}
	if(lpsPropValue){ECFreeBuffer(lpsPropValue); lpsPropValue = NULL;}

	if(ulLastChange == syncChangeBody) {
		//  PR_BODY has changed, update PR_RTF_COMPRESSED
		if (OpenProperty(PR_BODY_W, &IID_IStream, 0, 0, (LPUNKNOWN *)&lpBodyStream) == hrSuccess) {

			hr = OpenProperty(PR_RTF_COMPRESSED, &IID_IStream, STGM_TRANSACTED, MAPI_CREATE|MAPI_MODIFY, (LPUNKNOWN *)&lpRTFCompressedStream);
			if(hr != hrSuccess)
				goto exit;

			//Truncate to zero
			hr = lpRTFCompressedStream->SetSize(emptySize);
			if(hr != hrSuccess)
				goto exit;
			
			hr = WrapCompressedRTFStream(lpRTFCompressedStream, MAPI_MODIFY, &lpRTFUncompressedStream);
			if(hr != hrSuccess)
				goto exit;

			// Convert it now
			hr = Util::HrTextToRtf(lpBodyStream, lpRTFUncompressedStream);
			if(hr != hrSuccess)
				goto exit;

			// Commit uncompress data
			hr = lpRTFUncompressedStream->Commit(0);
			if(hr != hrSuccess)
				goto exit;

			// Commit compresed data
			hr = lpRTFCompressedStream->Commit(0);
			if(hr != hrSuccess)
				goto exit;

			////////////////////////////////////////////////////
			// Update HTML, always use the binary propval
			//
			hr = OpenProperty(PR_HTML, &IID_IStream, STGM_TRANSACTED, MAPI_CREATE|MAPI_MODIFY, (LPUNKNOWN *)&lpHTMLStream);
			if(hr != hrSuccess)
				goto exit;

			hr = lpBodyStream->Seek(moveBegin, STREAM_SEEK_SET, NULL);
			if(hr != hrSuccess)
				goto exit;

			hr = lpHTMLStream->SetSize(emptySize);
			if(hr != hrSuccess)
				goto exit;

			hr = Util::HrTextToHtml(lpBodyStream, lpHTMLStream, ulCodepage);
			if(hr != hrSuccess)
				goto exit;

			hr = lpHTMLStream->Commit(0);
			if(hr != hrSuccess)
				goto exit;
		}
		
		// We generated these properties but don't really want to save it to the server
		HrSetCleanProperty(PR_HTML);
		HrSetCleanProperty(PR_RTF_COMPRESSED);
		// and mark them as deleted, since we want the server to remove the old version if this was in the database
		m_setDeletedProps.insert(PR_HTML);
		m_setDeletedProps.insert(PR_RTF_COMPRESSED);
	} else if (ulLastChange == syncChangeRTF) {
		// PR_RTF_COMPRESSED changed, update PR_BODY_W

		if(OpenProperty(PR_RTF_COMPRESSED, &IID_IStream, 0, 0, (LPUNKNOWN *)&lpRTFCompressedStream) == hrSuccess) {
			// Read the RTF stream
			
			hr = WrapCompressedRTFStream(lpRTFCompressedStream, 0, &lpRTFUncompressedStream);
			if(hr != hrSuccess)
			{
				// Broken RTF, fallback on empty stream
				hr = ECMemStream::Create(NULL, 0, 0, NULL, NULL, NULL, &lpEmptyMemStream);
				if(hr != hrSuccess)
					goto exit;

				hr = lpEmptyMemStream->QueryInterface(IID_IStream, (void**)&lpRTFUncompressedStream);
				if(hr != hrSuccess)
					goto exit;

			}
					
			// Read the entire uncompressed RTF stream into strRTF
			while(1) {
				hr = lpRTFUncompressedStream->Read(lpBuf, 4096, &ulRead);

				if(hr != hrSuccess)
					goto exit;

				if(ulRead == 0) {
					break;
				}
				strRTF.append(lpBuf, ulRead);
			}

			if(isrtfhtml(strRTF.c_str(), strRTF.size())){
				rtfType = RTFTypeFromHTML;
			}else if (isrtftext(strRTF.c_str(), strRTF.size()) ){
				rtfType = RTFTypeFromText;
			}else { 
                rtfType = RTFTypeOther;
			}

			if(rtfType == RTFTypeOther)
			{
				// PR_RTF_COMPRESSED changed, update PR_BODY 
				if(RTFSync(&this->m_xMessage, RTF_SYNC_RTF_CHANGED, &bUpdated) == hrSuccess) 
				{
					bUpdated = TRUE;

					hr = OpenProperty(PR_BODY_W, &IID_IStream, 0, 0, (LPUNKNOWN *)&lpBodyStream);
					if(hr != hrSuccess)
						goto exit;

					////////////////////////////////////////////////////
					// Update HTML, always use the binary propval
					//
					hr = OpenProperty(PR_HTML, &IID_IStream, STGM_TRANSACTED, MAPI_CREATE|MAPI_MODIFY, (LPUNKNOWN *)&lpHTMLStream);
					if(hr != hrSuccess)
						goto exit;

					hr = lpBodyStream->Seek(moveBegin, STREAM_SEEK_SET, NULL);
					if(hr != hrSuccess)
						goto exit;

					hr = lpHTMLStream->SetSize(emptySize);
					if(hr != hrSuccess)
						goto exit;

					hr = Util::HrTextToHtml(lpBodyStream, lpHTMLStream, ulCodepage);
					if(hr != hrSuccess)
						goto exit;

					hr = lpHTMLStream->Commit(0);
					if(hr != hrSuccess)
						goto exit;
				}
			}

			if(bUpdated == FALSE) // NOTE: RTFSync does't work under linux so bUpdated = false
			{

				// Decode the RTF into HTML with decodertfhtml (conversion is done in-place)
				switch(rtfType)
				{
					case RTFTypeFromHTML:
						// Decode RTF into a WCHAR HTML source stream
						hr = HrExtractHTMLFromRTF(strRTF, strHTMLOut, ulCodepage);
						break;
					case RTFTypeFromText:
						// Decode RTFtext into a WCHAR HTML source stream
						hr = HrExtractHTMLFromTextRTF(strRTF, strHTMLOut, ulCodepage);
						break;
					default:
						hr = HrExtractHTMLFromRealRTF(strRTF, strHTMLOut, ulCodepage);
						break;
				}

				////////////////////////////////////////////////////
				// Update HTML, always use the binary propval
				//
				hr = OpenProperty(PR_HTML, &IID_IStream, STGM_TRANSACTED, MAPI_CREATE|MAPI_MODIFY, (LPUNKNOWN *)&lpHTMLStream);
				if(hr != hrSuccess)
					goto exit;

				hr = lpHTMLStream->SetSize(emptySize);
				if(hr != hrSuccess)
					goto exit;

				//FIXME: write in blocks of 4096
				hr = lpHTMLStream->Write(strHTMLOut.c_str(), strHTMLOut.size(), &ulWritten);
				if(hr != hrSuccess)
					goto exit;

				hr = lpHTMLStream->Commit(0);
				if(hr != hrSuccess)
					goto exit;

				// Write the data as PR_BODY
				hr = OpenProperty(PR_BODY_W, &IID_IStream, STGM_WRITE | STGM_TRANSACTED, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN *)&lpBodyStream);
				if(hr != hrSuccess)
					goto exit;

				hr = lpBodyStream->SetSize(emptySize);
					if(hr != hrSuccess)
						goto exit;

				if(rtfType == RTFTypeFromText)
				{
					// @todo, make sure it writes PT_UNICODE
					hr = HrExtractBODYFromTextRTF(strRTF, strOut);
					if(hr != hrSuccess)
						goto exit;

					hr = lpBodyStream->Write(strOut.c_str(), strOut.size() * sizeof(WCHAR), &ulWritten);
					if(hr != hrSuccess)
						goto exit;
				} else {
					//Seek to begin
					hr = lpHTMLStream->Seek(moveBegin, STREAM_SEEK_SET, NULL);
					if(hr != hrSuccess)
						goto exit;

					hr = Util::HrHtmlToText(lpHTMLStream, lpBodyStream, ulCodepage);
					if(hr != hrSuccess)
						goto exit;
				}
				
				hr = lpBodyStream->Commit(0);
				if(hr != hrSuccess)
					goto exit;
			}
		}
		
		// Dont want to save generated PR_HTML
		HrSetCleanProperty(PR_HTML);
		// and mark it as deleted, since we want the server to remove the old version if this was in the database
		m_setDeletedProps.insert(PR_HTML);
	} else if (ulLastChange == syncChangeHTML) {
		if(OpenProperty(PR_HTML, &IID_IStream, 0, 0, (LPUNKNOWN *)&lpHTMLStream) == hrSuccess) {
			//Update RTF
			hr = OpenProperty(PR_RTF_COMPRESSED, &IID_IStream, STGM_TRANSACTED, MAPI_CREATE|MAPI_MODIFY, (LPUNKNOWN *)&lpRTFCompressedStream);
			if(hr != hrSuccess)
				goto exit;

			hr = lpRTFCompressedStream->SetSize(emptySize);
			if(hr != hrSuccess)
				goto exit;

			hr = WrapCompressedRTFStream(lpRTFCompressedStream, MAPI_MODIFY, &lpRTFUncompressedStream);

			if(hr != hrSuccess)
				goto exit;

			
			// Convert it now
			hr = Util::HrHtmlToRtf(lpHTMLStream, lpRTFUncompressedStream, ulCodepage);

			if(hr != hrSuccess)
				goto exit;

			// Commit uncompress data
			hr = lpRTFUncompressedStream->Commit(0);
			if(hr != hrSuccess)
				goto exit;

			// Commit compresed data
			hr = lpRTFCompressedStream->Commit(0);
			if(hr != hrSuccess)
				goto exit;

			//////////////////////////////////
			// Write the data as PR_BODY

			//Seek to begin
			hr = lpHTMLStream->Seek(moveBegin, STREAM_SEEK_SET, NULL);
			if(hr != hrSuccess)
				goto exit;

			hr = OpenProperty(PR_BODY_W, &IID_IStream, STGM_WRITE | STGM_TRANSACTED, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN *)&lpBodyStream);

			if(hr != hrSuccess)
				goto exit;

			hr = lpBodyStream->SetSize(emptySize);
			if(hr != hrSuccess)
				goto exit;
			
			hr = Util::HrHtmlToText(lpHTMLStream, lpBodyStream, ulCodepage);
			if(hr != hrSuccess)
				goto exit;

			hr = lpBodyStream->Commit(0);
			if(hr != hrSuccess)
				goto exit;
		}
		
		// Dont want to save generated RTF
		HrSetCleanProperty(PR_RTF_COMPRESSED);
		// and mark it as deleted, since we want the server to remove the old version if this was in the database
		m_setDeletedProps.insert(PR_RTF_COMPRESSED);
	}

	m_bBusySyncRTF = FALSE;
	m_ulLastChange = syncChangeNone;

exit:
	if (hr != hrSuccess) {
		m_bBusySyncRTF = FALSE;
		m_ulLastChange = ulLastChange; // Set last change type back.
	}

	this->fModify = fModifySaved;

	if(lpRTFUncompressedStream)
		lpRTFUncompressedStream->Release();

	if(lpRTFCompressedStream)
		lpRTFCompressedStream->Release();

	if(lpBodyStream)
		lpBodyStream->Release();

	if(lpHTMLStream)
		lpHTMLStream->Release();

	if (lpEmptyMemStream)
		lpEmptyMemStream->Release();

	if(lpsPropValue)
		ECFreeBuffer(lpsPropValue);

	return hr;

}

HRESULT ECMessage::GetBodyType(eBodyType *lpulBodyType)
{
	HRESULT		hr = hrSuccess;
	LPSTREAM	lpRTFCompressedStream = NULL;
	LPSTREAM	lpRTFUncompressedStream = NULL;
	char		szRtfBuf[64] = {0};
	ULONG		cbRtfBuf = 0;

	if (m_ulBodyType == bodyTypeUnknown) {
		hr = OpenProperty(PR_RTF_COMPRESSED, &IID_IStream, 0, 0, (LPUNKNOWN *)&lpRTFCompressedStream);
		if (hr != hrSuccess)
			goto exit;

		hr = WrapCompressedRTFStream(lpRTFCompressedStream, 0, &lpRTFUncompressedStream);
		if (hr != hrSuccess)
			goto exit;

		hr = lpRTFUncompressedStream->Read(szRtfBuf, sizeof(szRtfBuf), &cbRtfBuf);
		if (hr != hrSuccess)
			goto exit;

		if(isrtftext(szRtfBuf, cbRtfBuf)) {
			m_ulBodyType = bodyTypePlain;
		} else if(isrtfhtml(szRtfBuf, cbRtfBuf)) {
			m_ulBodyType = bodyTypeHTML;
		} else
			m_ulBodyType = bodyTypeRTF;
	}

	*lpulBodyType = m_ulBodyType;

exit:
	if (lpRTFUncompressedStream)
		lpRTFUncompressedStream->Release();

	if (lpRTFCompressedStream)
		lpRTFCompressedStream->Release();

	return hr;
}

// Use the support object to do the copying
HRESULT ECMessage::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = Util::DoCopyProps(&IID_IMessage, &this->m_xMessage, lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);

	return hr;
}

HRESULT ECMessage::xMessage::QueryInterface(REFIID refiid , void** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IMessage::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECMessage::xMessage::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::AddRef", "");
	METHOD_PROLOGUE_(ECMessage, Message);
	return pThis->AddRef();
}

ULONG ECMessage::xMessage::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::Release", "");
	METHOD_PROLOGUE_(ECMessage, Message);
	return pThis->Release();
}


HRESULT ECMessage::xMessage::GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::GetLastError", "");
	METHOD_PROLOGUE_(ECMessage, Message);
	return pThis->GetLastError(hError, ulFlags, lppMapiError);
}

HRESULT ECMessage::xMessage::SaveChanges(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::SaveChanges", "flags=%d", ulFlags);
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->SaveChanges(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMessage::SaveChanges", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::GetProps", "PropTagArray[%d]=%s\nfFlags=0x%08X", lpPropTagArray ? lpPropTagArray->cValues : 0, PropNameFromPropTagArray(lpPropTagArray).c_str(), ulFlags);
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
	TRACE_MAPI(TRACE_RETURN, "IMessage::GetProps", "%s\n %s", GetMAPIErrorDescription(hr).c_str(), PropNameFromPropArray(*lpcValues, *lppPropArray).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::GetPropList", "flags=0x%08X", ulFlags);
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->GetPropList(ulFlags, lppPropTagArray);
	TRACE_MAPI(TRACE_RETURN, "IMessage::GetPropList", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::OpenProperty", "proptag=%s, flags=0x%08X, lpiid=%s, InterfaceOptions=0x%08X", PropNameFromPropTag(ulPropTag).c_str(), ulFlags, (lpiid)?DBGGUIDToString(*lpiid).c_str():"NULL", ulInterfaceOptions);
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->OpenProperty(ulPropTag,lpiid, ulInterfaceOptions, ulFlags, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IMessage::OpenProperty", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::SetProps", "%s", PropNameFromPropArray(cValues, lpPropArray).c_str());
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->SetProps(cValues, lpPropArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMessage::SetProps", "%s: problems %s", GetMAPIErrorDescription(hr).c_str(), lppProblems && *lppProblems ? ProblemArrayToString(*lppProblems).c_str() : "<none>");
	return hr;
}

HRESULT ECMessage::xMessage::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::DeleteProps", "%s", PropNameFromPropTagArray(lpPropTagArray).c_str());
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->DeleteProps(lpPropTagArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMessage::DeleteProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::CopyTo", "ciidExclude=%d, lpExcludeProps=%s, lpDestInterface=%s, ulFlags=0x%X", ciidExclude, PropNameFromPropTagArray(lpExcludeProps).c_str(), (lpInterface)?DBGGUIDToString(*lpInterface).c_str():"NULL", ulFlags);
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMessage::CopyTo", "%s %s", GetMAPIErrorDescription(hr).c_str(), lppProblems && *lppProblems ? ProblemArrayToString(*lppProblems).c_str() : "<null>");
	return hr;
}

HRESULT ECMessage::xMessage::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::CopyProps", "");
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMessage::CopyProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::GetNamesFromIDs", "");
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->GetNamesFromIDs(pptaga, lpguid, ulFlags, pcNames, pppNames);
	TRACE_MAPI(TRACE_RETURN, "IMessage::GetNamesFromIDs", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::GetIDsFromNames", "");
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->GetIDsFromNames(cNames, ppNames, ulFlags, pptaga);
	TRACE_MAPI(TRACE_RETURN, "IMessage::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}


HRESULT ECMessage::xMessage::GetAttachmentTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::GetAttachmentTable", "");
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->GetAttachmentTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IMessage::GetAttachmentTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::OpenAttach(ULONG ulAttachmentNum, LPCIID lpInterface, ULONG ulFlags, LPATTACH *lppAttach)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::OpenAttach", "attachNum=%d, interface=%s, flags=0x%0X", ulAttachmentNum, (lpInterface)?DBGGUIDToString(*lpInterface).c_str():"NULL", ulFlags);
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->OpenAttach(ulAttachmentNum, lpInterface, ulFlags, lppAttach);
	TRACE_MAPI(TRACE_RETURN, "IMessage::OpenAttach", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::CreateAttach(LPCIID lpInterface, ULONG ulFlags, ULONG *lpulAttachmentNum, LPATTACH *lppAttach)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::CreateAttach", "flags=%d", ulFlags);
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->CreateAttach(lpInterface, ulFlags, lpulAttachmentNum, lppAttach);
	TRACE_MAPI(TRACE_RETURN, "IMessage::CreateAttach", "%s: attachNum=%d", GetMAPIErrorDescription(hr).c_str(), *lpulAttachmentNum);
	return hr;
}

HRESULT ECMessage::xMessage::DeleteAttach(ULONG ulAttachmentNum, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::DeleteAttach", "");
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->DeleteAttach(ulAttachmentNum, ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMessage::DeleteAttach", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::GetRecipientTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::GetRecipientTable", "");
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->GetRecipientTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IMessage::GetRecipientTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::ModifyRecipients(ULONG ulFlags, LPADRLIST lpMods)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::ModifyRecipients", "flags %d, %s", ulFlags, RowSetToString((LPSRowSet)lpMods).c_str());
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->ModifyRecipients(ulFlags, lpMods);
	TRACE_MAPI(TRACE_RETURN, "IMessage::ModifyRecipients", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::SubmitMessage(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::SubmitMessage", "flags=0x%08X", ulFlags);
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->SubmitMessage(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMessage::SubmitMessage", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMessage::xMessage::SetReadFlag(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMessage::SetReadFlag", "flags=0x%08X", ulFlags);
	METHOD_PROLOGUE_(ECMessage, Message);
	HRESULT hr = pThis->SetReadFlag(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMessage::SetReadFlag", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
