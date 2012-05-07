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
#include "ECArchiveAwareAttach.h"
#include "ECArchiveAwareMessage.h"


HRESULT ECArchiveAwareAttachFactory::Create(ECMsgStore *lpMsgStore, ULONG ulObjType, BOOL fModify, ULONG ulAttachNum, ECMAPIProp *lpRoot, ECAttach **lppAttach) const
{
	return ECArchiveAwareAttach::Create(lpMsgStore, ulObjType, fModify, ulAttachNum, lpRoot, lppAttach);
}



ECArchiveAwareAttach::ECArchiveAwareAttach(ECMsgStore *lpMsgStore, ULONG ulObjType, BOOL fModify, ULONG ulAttachNum, ECMAPIProp *lpRoot) 
: ECAttach(lpMsgStore, ulObjType, fModify, ulAttachNum, lpRoot)
, m_lpRoot(dynamic_cast<ECArchiveAwareMessage*>(lpRoot))
{
	ASSERT(m_lpRoot != NULL);	// We don't expect an ECArchiveAwareAttach to be ever created by any other object than a ECArchiveAwareMessage.

	// Override the handler defined in ECAttach
	this->HrAddPropHandlers(PR_ATTACH_SIZE, ECAttach::GetPropHandler, SetPropHandler, (void*)this, FALSE, FALSE);
}

HRESULT	ECArchiveAwareAttach::Create(ECMsgStore *lpMsgStore, ULONG ulObjType, BOOL fModify, ULONG ulAttachNum, ECMAPIProp *lpRoot, ECAttach **lppAttach)
{
	HRESULT hr = hrSuccess;
	ECArchiveAwareAttach *lpAttach = NULL;

	lpAttach = new ECArchiveAwareAttach(lpMsgStore, ulObjType, fModify, ulAttachNum, lpRoot);

	hr = lpAttach->QueryInterface(IID_ECAttach, (void **)lppAttach);

	return hr;
}

HRESULT	ECArchiveAwareAttach::SetPropHandler(ULONG ulPropTag, void* /*lpProvider*/, LPSPropValue lpsPropValue, void *lpParam)
{
	ECArchiveAwareAttach *lpAttach = (ECArchiveAwareAttach *)lpParam;
	HRESULT hr = hrSuccess;

	switch(ulPropTag) {
	case PR_ATTACH_SIZE:
		if (lpAttach->m_lpRoot && lpAttach->m_lpRoot->IsLoading())
			hr = lpAttach->HrSetRealProp(lpsPropValue);
		else
			hr = MAPI_E_COMPUTED;
	default:
		hr = MAPI_E_NOT_FOUND;
		break;
	}
	return hr;
}
