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

#include <platform.h>

#include <iostream>

#include <CommonUtil.h>
#include <mapiext.h>
#include <mapiguidext.h>
#include <mapiutil.h>
#include <mapix.h>
#include <namedprops.h>

#include "zarafa-fsck.h"

HRESULT ZarafaFsckTask::ValidateMinimalNamedFields(LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropertyArray = NULL;
	LPSPropTagArray lpPropertyTagArray = NULL;

	enum {
		E_REMINDER,
		TAG_COUNT
	};

	LPMAPINAMEID *lppTagArray = NULL;
	std::string strTagName[TAG_COUNT];

	/*
	 * Allocate the NamedID list and initialize it to all
	 * properties which could give us some information about the name.
	 */
	hr = allocNamedIdList(TAG_COUNT, &lppTagArray);
	if (hr != hrSuccess)
		goto exit;

	lppTagArray[E_REMINDER]->lpguid = (LPGUID)&PSETID_Common;
	lppTagArray[E_REMINDER]->ulKind = MNID_ID;
	lppTagArray[E_REMINDER]->Kind.lID = dispidReminderSet;

	strTagName[E_REMINDER] = "dispidReminderSet";

	hr = ReadNamedProperties(lpMessage, TAG_COUNT, lppTagArray, &lpPropertyTagArray, &lpPropertyArray);
	if (FAILED(hr))
		goto exit;

	for (ULONG i = 0; i < TAG_COUNT; i++) {
		if (PROP_TYPE(lpPropertyArray[i].ulPropTag) == PT_ERROR) {
			__UPV Value;
			Value.b = false;

			hr = AddMissingProperty(lpMessage, strTagName[i],
						CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[i], PT_BOOLEAN),
						Value);
			if (hr != hrSuccess)
				goto exit;
		}
	}

	/* If we are here, we were succcessfull */
	hr = hrSuccess;

exit:
	return hr;
}

HRESULT ZarafaFsckTask::ValidateTimestamps(LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropertyArray = NULL;
	LPSPropTagArray lpPropertyTagArray = NULL;

	enum {
		E_START_DATE,
		E_DUE_DATE,
		TAG_COUNT
	};

	LPMAPINAMEID *lppTagArray = NULL;

	/*
	 * Allocate the NameID list and initialize it to all
	 * properties which could give us some information about the timestamps.
	 */
	hr = allocNamedIdList(TAG_COUNT, &lppTagArray);
	if (hr != hrSuccess)
		goto exit;

	lppTagArray[E_START_DATE]->lpguid = (LPGUID)&PSETID_Task;
	lppTagArray[E_START_DATE]->ulKind = MNID_ID;
	lppTagArray[E_START_DATE]->Kind.lID = dispidTaskStartDate;

	lppTagArray[E_DUE_DATE]->lpguid = (LPGUID)&PSETID_Task;
	lppTagArray[E_DUE_DATE]->ulKind = MNID_ID;
	lppTagArray[E_DUE_DATE]->Kind.lID = dispidTaskDueDate;

	hr = ReadNamedProperties(lpMessage, TAG_COUNT, lppTagArray, &lpPropertyTagArray, &lpPropertyArray);
	if (FAILED(hr))
		goto exit;

	/*
	 * Validate parameters
	 * When Completion is set, the completion date should have been set.
	 * No further restrictions apply, but we will fill in missing tags
	 * based on the results of the other tags.
	 */
	if (PROP_TYPE(lpPropertyArray[E_START_DATE].ulPropTag) != PT_ERROR &&
	    PROP_TYPE(lpPropertyArray[E_DUE_DATE].ulPropTag) != PT_ERROR) {
		LPFILETIME lpStart = &lpPropertyArray[E_START_DATE].Value.ft;
		LPFILETIME lpDue = &lpPropertyArray[E_DUE_DATE].Value.ft;

		/*
		 * We cannot start a task _after_ it is due.
		 */
		if (*lpStart > *lpDue) {
			__UPV Value;
			Value.ft = *lpDue;

			hr = ReplaceProperty(lpMessage, "dispidTaskStartDate",
					     CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_START_DATE], PT_SYSTIME),
					     "Start date cannot be after due date",
					     Value);
			if (hr != hrSuccess)
				goto exit;
		}
	} else
		hr = hrSuccess;

exit:
	return hr;
}

HRESULT ZarafaFsckTask::ValidateCompletion(LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropertyArray = NULL;
	LPSPropTagArray lpPropertyTagArray = NULL;
	bool bCompleted;
	double dblCompleted;

	enum {
		E_COMPLETE,
		E_PERCENT_COMPLETE,
		E_COMPLETION_DATE,
		TAG_COUNT
	};

	LPMAPINAMEID *lppTagArray = NULL;

	/*
	 * Allocate the NamedID list and initialize it to all
	 * properties which could give us some information about the completion.
	 */
	hr = allocNamedIdList(TAG_COUNT, &lppTagArray);
	if (hr != hrSuccess)
		goto exit;

	lppTagArray[E_COMPLETE]->lpguid = (LPGUID)&PSETID_Task;
	lppTagArray[E_COMPLETE]->ulKind = MNID_ID;
	lppTagArray[E_COMPLETE]->Kind.lID = dispidTaskComplete;

	lppTagArray[E_PERCENT_COMPLETE]->lpguid = (LPGUID)&PSETID_Task;
	lppTagArray[E_PERCENT_COMPLETE]->ulKind = MNID_ID;
	lppTagArray[E_PERCENT_COMPLETE]->Kind.lID = dispidTaskPercentComplete;

	lppTagArray[E_COMPLETION_DATE]->lpguid = (LPGUID)&PSETID_Task;
	lppTagArray[E_COMPLETION_DATE]->ulKind = MNID_ID;
	lppTagArray[E_COMPLETION_DATE]->Kind.lID = dispidTaskDateCompleted;

	hr = ReadNamedProperties(lpMessage, TAG_COUNT, lppTagArray, &lpPropertyTagArray, &lpPropertyArray);
	if (FAILED(hr))
		goto exit;

	/*
	 * Validate parameters
	 * When Completion is set, the completion date should have been set.
	 * No further restrictions apply, but we will fill in missing tags
	 * based on the results of the other tags.
	 */
	if (PROP_TYPE(lpPropertyArray[E_COMPLETE].ulPropTag) == PT_ERROR) {
		__UPV Value;

		if (((PROP_TYPE(lpPropertyArray[E_PERCENT_COMPLETE].ulPropTag) != PT_ERROR) &&
		     (lpPropertyArray[E_PERCENT_COMPLETE].Value.dbl == 1)) ||
		    (PROP_TYPE((lpPropertyArray[E_COMPLETION_DATE].ulPropTag) != PT_ERROR)))
			Value.b = true;
		else
			Value.b = false;

		hr = AddMissingProperty(lpMessage, "dispidTaskComplete",
					CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_COMPLETE], PT_BOOLEAN),
					Value);
		if (hr != hrSuccess)
			goto exit;

		bCompleted = Value.b;
	} else
		bCompleted = lpPropertyArray[E_COMPLETE].Value.b;

	if (PROP_TYPE(lpPropertyArray[E_PERCENT_COMPLETE].ulPropTag) == PT_ERROR) {
		__UPV Value;
		Value.dbl = !!bCompleted; /* Value.dbl = 1 => 100% */

		 hr = AddMissingProperty(lpMessage, "dispidTaskPercentComplete",
		 			 CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_PERCENT_COMPLETE], PT_DOUBLE),
					 Value);
		if (hr != hrSuccess)
			goto exit;

		dblCompleted = Value.dbl;
	}
		dblCompleted = lpPropertyArray[E_PERCENT_COMPLETE].Value.dbl;

	if (PROP_TYPE(lpPropertyArray[E_COMPLETION_DATE].ulPropTag) == PT_ERROR && bCompleted) {
		__UPV Value;

		GetSystemTimeAsFileTime(&Value.ft);

		hr = AddMissingProperty(lpMessage, "dispidTaskDateCompleted",
					CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_COMPLETION_DATE], PT_SYSTIME),
					Value);
		if (hr != hrSuccess)
			goto exit;
	}

	/* If we are here, we were succcessfull */
	hr = hrSuccess;

exit:
	if (lppTagArray)
		freeNamedIdList(lppTagArray);

	if (lpPropertyArray)
		MAPIFreeBuffer(lpPropertyArray);

	if (lpPropertyTagArray)
		MAPIFreeBuffer(lpPropertyTagArray);
	
	return hr;
}

HRESULT ZarafaFsckTask::ValidateItem(LPMESSAGE lpMessage, string strClass)
{
	HRESULT hr = hrSuccess;

	if (strClass != "IPM.Task") {
		std::cout << "Illegal class: \"" << strClass << "\"" << std::endl;
		hr = E_INVALIDARG;
		goto exit;
	}

	hr = ValidateMinimalNamedFields(lpMessage);
	if (hr != hrSuccess)
		goto exit;

	hr = ValidateTimestamps(lpMessage);
	if (hr != hrSuccess)
		goto exit;

	hr = ValidateCompletion(lpMessage);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}
