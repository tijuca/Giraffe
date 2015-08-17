/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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
#include "mapi_ptr.h"
#include "ECMAPIDebug.h"
#include "ECDebug.h"

using namespace std;

HRESULT Dump(std::ostream &os, LPMAPIPROP lpProp, const std::string &strPrefix)
{
	HRESULT hr = hrSuccess;
	ULONG cValues;
	SPropArrayPtr ptrProps;
	std::string strObjType = "MAPIProp";
	LPSPropValue lpObjType = NULL;
	
	if (lpProp == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpProp->GetProps(NULL, 0, &cValues, &ptrProps);
	if (FAILED(hr))
		goto exit;

	lpObjType = PpropFindProp(ptrProps.get(), cValues, PR_OBJECT_TYPE);
	if (lpObjType) {
		switch (lpObjType->Value.l) {
			case MAPI_MESSAGE:
				strObjType = "Message";
				break;
			case MAPI_ATTACH:
				strObjType = "Attach";
				break;
			default:
				break;
		}
	}

	os << strPrefix << "Object type: " << strObjType << endl;
	os << strPrefix << "Properties (count=" << cValues << "):" << endl;
	for (ULONG i = 0; i < cValues; ++i)
		os << strPrefix << "  " << PropNameFromPropTag(ptrProps[i].ulPropTag) << " - " << PropValueToString(&ptrProps[i]) << endl;

	// TODO: Handle more object types
	if (lpObjType && lpObjType->Value.l == MAPI_MESSAGE) {
		MessagePtr ptrMessage;
		MAPITablePtr ptrTable;
		ULONG ulCount = 0;

		hr = lpProp->QueryInterface(ptrMessage.iid, &ptrMessage);
		if (hr != hrSuccess)
			goto exit;

		// List recipients
		hr = ptrMessage->GetRecipientTable(0, &ptrTable);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrTable->GetRowCount(0, &ulCount);
		if (hr != hrSuccess)
			goto exit;

		os << strPrefix << "Recipients (count=" << ulCount << "):" << endl;
		if (ulCount > 0) {
			SRowSetPtr ptrRows;

			while (true) {
				hr = ptrTable->QueryRows(64, 0, &ptrRows);
				if (hr != hrSuccess)
					goto exit;

				if (ptrRows.empty())
					break;

				for (SRowSetPtr::size_type i = 0; i < ptrRows.size(); ++i) {
					LPSPropValue lpRowId = PpropFindProp(ptrRows[i].lpProps, ptrRows[i].cValues, PR_ROWID);

					os << strPrefix << "  Recipient: ";
					if (lpRowId)
						os << lpRowId->Value.l;
					else
						os << "???";
					os << endl;

					for (ULONG j = 0; j < ptrRows[i].cValues; ++j)
						os << strPrefix << "    " << PropNameFromPropTag(ptrRows[i].lpProps[j].ulPropTag) << " - " << PropValueToString(&ptrRows[i].lpProps[j]) << endl;
				}
			}
		}

		// List attachments
		hr = ptrMessage->GetAttachmentTable(0, &ptrTable);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrTable->GetRowCount(0, &ulCount);
		if (hr != hrSuccess)
			goto exit;

		os << strPrefix << "Attachments (count=" << ulCount << "):" << endl;
		if (ulCount > 0) {
			SizedSPropTagArray(1, sptaAttachProps) = {1, {PR_ATTACH_NUM}};

			hr = ptrTable->SetColumns((LPSPropTagArray)&sptaAttachProps, TBL_BATCH);
			if (hr != hrSuccess)
				goto exit;

			while (true) {
				SRowSetPtr ptrRows;

				hr = ptrTable->QueryRows(64, 0, &ptrRows);
				if (hr != hrSuccess)
					goto exit;

				if (ptrRows.empty())
					break;

				for (SRowSetPtr::size_type i = 0; i < ptrRows.size(); ++i) {
					AttachPtr ptrAttach;

					if (ptrRows[i].lpProps[0].ulPropTag != PR_ATTACH_NUM)
						goto exit;

					hr = ptrMessage->OpenAttach(ptrRows[i].lpProps[0].Value.l, &ptrAttach.iid, 0, &ptrAttach);
					if (hr != hrSuccess)
						goto exit;

					os << strPrefix << "  Attachment: " << ptrRows[i].lpProps[0].Value.l << endl;
					hr = Dump(os, ptrAttach, strPrefix + "  ");
					if (hr != hrSuccess)
						goto exit;
				}
			}
		}
	}

	else if (lpObjType && lpObjType->Value.l == MAPI_ATTACH) {
		AttachPtr ptrAttach;
		SPropValuePtr ptrAttachMethod;

		hr = lpProp->QueryInterface(ptrAttach.iid, &ptrAttach);
		if (hr != hrSuccess)
			goto exit;

		hr = HrGetOneProp(ptrAttach, PR_ATTACH_METHOD, &ptrAttachMethod);
		if (hr != hrSuccess)
			goto exit;

		// TODO: Handle more attachment types.
		if (ptrAttachMethod->Value.l == ATTACH_EMBEDDED_MSG) {
			MessagePtr ptrMessage;

			hr = ptrAttach->OpenProperty(PR_ATTACH_DATA_OBJ, &ptrMessage.iid, 0, 0, &ptrMessage);
			if (hr != hrSuccess)
				goto exit;

			os << strPrefix << "Embedded message:" << endl;
			hr = Dump(os, ptrMessage, strPrefix + "  ");
			if (hr != hrSuccess)
				goto exit;
		}
	}

exit:
	return hr;
}
