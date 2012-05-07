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
#include <charset/utf16string.h>
#include <charset/convert.h>

#include "RecurrenceState.h"
#include "zarafa-fsck.h"

HRESULT ZarafaFsckCalendar::ValidateMinimalNamedFields(LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropertyArray = NULL;
	LPSPropTagArray lpPropertyTagArray = NULL;

	enum {
		E_REMINDER,
		E_ALLDAYEVENT,
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

	lppTagArray[E_ALLDAYEVENT]->lpguid = (LPGUID)&PSETID_Appointment;
	lppTagArray[E_ALLDAYEVENT]->ulKind = MNID_ID;
	lppTagArray[E_ALLDAYEVENT]->Kind.lID = dispidAllDayEvent;

	strTagName[E_REMINDER] = "dispidReminderSet";
	strTagName[E_ALLDAYEVENT] = "dispidAllDayEvent";

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
	if (lppTagArray)
		freeNamedIdList(lppTagArray);

	if (lpPropertyArray)
		MAPIFreeBuffer(lpPropertyArray);

	if (lpPropertyTagArray)
		MAPIFreeBuffer(lpPropertyTagArray);

	return hr;
}

HRESULT ZarafaFsckCalendar::ValidateTimestamps(LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropertyArray = NULL;
	LPSPropTagArray lpPropertyTagArray = NULL;
	LPFILETIME lpStart;
	LPFILETIME lpEnd;
	LPFILETIME lpCommonStart;
	LPFILETIME lpCommonEnd;
	LONG ulDuration;

	enum {
		E_START,
		E_END,
		E_CSTART,
		E_CEND,
		E_DURATION,
		TAG_COUNT
	};

	LPMAPINAMEID *lppTagArray = NULL;

	/*
	 * Allocate the NamedID list and initialize it to all
	 * properties which could give us some information about the name.
	 */
	hr = allocNamedIdList(TAG_COUNT, &lppTagArray);
	if (hr != hrSuccess)
		goto exit;

	lppTagArray[E_START]->lpguid = (LPGUID)&PSETID_Appointment;
	lppTagArray[E_START]->ulKind = MNID_ID;
	lppTagArray[E_START]->Kind.lID = dispidApptStartWhole;

	lppTagArray[E_END]->lpguid = (LPGUID)&PSETID_Appointment;
	lppTagArray[E_END]->ulKind = MNID_ID;
	lppTagArray[E_END]->Kind.lID = dispidApptEndWhole;

	lppTagArray[E_CSTART]->lpguid = (LPGUID)&PSETID_Common;
	lppTagArray[E_CSTART]->ulKind = MNID_ID;
	lppTagArray[E_CSTART]->Kind.lID = dispidCommonStart;

	lppTagArray[E_CEND]->lpguid = (LPGUID)&PSETID_Common;
	lppTagArray[E_CEND]->ulKind = MNID_ID;
	lppTagArray[E_CEND]->Kind.lID = dispidCommonEnd;

	lppTagArray[E_DURATION]->lpguid = (LPGUID)&PSETID_Appointment;
	lppTagArray[E_DURATION]->ulKind = MNID_ID;
	lppTagArray[E_DURATION]->Kind.lID = dispidApptDuration;

	hr = ReadNamedProperties(lpMessage, TAG_COUNT, lppTagArray, &lpPropertyTagArray, &lpPropertyArray);
	if (FAILED(hr))
		goto exit;

	/*
	 * Validate parameters:
	 * If E_START is missing it can be substituted with E_CSTART and vice versa
	 * If E_END is missing it can be substituted with E_CEND and vice versa
	 * E_{C}START < E_{C}END
	 * E_{C}END - E_{C}START / 60 = Duration
	 * If duration is missing, calculate and set it.
	 */
	if (PROP_TYPE(lpPropertyArray[E_START].ulPropTag) == PT_ERROR) {
		if (PROP_TYPE(lpPropertyArray[E_CSTART].ulPropTag) == PT_ERROR) {
			std::cout << "No valid starting address could be detected." << std::endl;
			hr = E_INVALIDARG;
			goto exit;
		}
		hr = AddMissingProperty(lpMessage, "dispidApptStartWhole",
					CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_START], PT_SYSTIME),
					lpPropertyArray[E_CSTART].Value);
		if (hr != hrSuccess)
			goto exit;

		lpStart = &lpPropertyArray[E_CSTART].Value.ft;
	} else
		lpStart = &lpPropertyArray[E_START].Value.ft;

	if (PROP_TYPE(lpPropertyArray[E_CSTART].ulPropTag) == PT_ERROR) {
		if (PROP_TYPE(lpPropertyArray[E_START].ulPropTag) == PT_ERROR) {
			std::cout << "No valid starting address could be detected." << std::endl;
			hr = E_INVALIDARG;
			goto exit;
		}
		hr = AddMissingProperty(lpMessage, "dispidCommonStart",
					CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_CSTART], PT_SYSTIME),
					lpPropertyArray[E_START].Value);
		if (hr != hrSuccess)
			goto exit;

		lpCommonStart = &lpPropertyArray[E_START].Value.ft;
	} else
		lpCommonStart = &lpPropertyArray[E_CSTART].Value.ft;

	if (PROP_TYPE(lpPropertyArray[E_END].ulPropTag) == PT_ERROR) {
		if (PROP_TYPE(lpPropertyArray[E_CEND].ulPropTag) == PT_ERROR) {
			std::cout << "No valid end address could be detected." << std::endl;
			hr = E_INVALIDARG;
			goto exit;
		}
		hr = AddMissingProperty(lpMessage, "dispidApptEndWhole",
					CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_END], PT_SYSTIME),
					lpPropertyArray[E_CEND].Value);
		if (hr != hrSuccess)
			goto exit;

		lpEnd = &lpPropertyArray[E_CEND].Value.ft;
	} else
		lpEnd = &lpPropertyArray[E_END].Value.ft;

	if (PROP_TYPE(lpPropertyArray[E_CEND].ulPropTag) == PT_ERROR) {
		if (PROP_TYPE(lpPropertyArray[E_END].ulPropTag) == PT_ERROR) {
			std::cout << "No valid starting address could be detected." << std::endl;
			hr = E_INVALIDARG;
			goto exit;
		}
		hr = AddMissingProperty(lpMessage, "dispidCommonEnd",
					CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_CEND], PT_SYSTIME),
					lpPropertyArray[E_END].Value);
		if (hr != hrSuccess)
			goto exit;

		lpCommonEnd = &lpPropertyArray[E_END].Value.ft;
	} else
		lpCommonEnd = &lpPropertyArray[E_CEND].Value.ft;

	if (*lpStart > *lpEnd && *lpCommonStart < *lpCommonEnd) {
		hr = ReplaceProperty(lpMessage, "dispidApptStartWhole",
				     CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_START], PT_SYSTIME),
				     "Whole start after whole end date.",
				     lpPropertyArray[E_CSTART].Value);
		if (hr != hrSuccess)
			goto exit;

		hr = ReplaceProperty(lpMessage, "dispidApptEndWhole",
				     CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_END], PT_SYSTIME),
				     "Whole start after whole end date.",
				     lpPropertyArray[E_CEND].Value);
		if (hr != hrSuccess)
			goto exit;
	}

	if (*lpCommonStart > *lpCommonEnd && *lpStart < *lpEnd) {
		hr = ReplaceProperty(lpMessage, "dispidCommonStart",
				     CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_CSTART], PT_SYSTIME),
				     "Common start after common end date.",
				     lpPropertyArray[E_START].Value);
		if (hr != hrSuccess)
			goto exit;

		hr = ReplaceProperty(lpMessage, "dispidCommonEnd",
				     CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_CEND], PT_SYSTIME),
				     "Common start after common end date.",
				     lpPropertyArray[E_END].Value);
		if (hr != hrSuccess)
			goto exit;
	}

	if ((*lpEnd - *lpStart) != (*lpCommonEnd - *lpCommonStart)) {
		std::cout << "Difference in duration: " << endl;
		std::cout << "Common duration (" << (*lpCommonEnd - *lpCommonStart) << ") ";
		std::cout << "- Whole duration (" << (*lpEnd - *lpStart) << ")" <<std::endl;
		hr = E_INVALIDARG;
		goto exit;
	}

	/*
	 * Common duration matches whole duration,
	 * now we need to compare the duration itself.
	 */
	__UPV Value;
	Value.l = (*lpEnd - *lpStart) / 60;

	if (PROP_TYPE(lpPropertyArray[E_DURATION].ulPropTag) == PT_ERROR) {
		hr = AddMissingProperty(lpMessage, "dispidApptDuration",
					CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_DURATION], PT_LONG),
					Value);
		if (hr != hrSuccess)
			goto exit;
	} else {
		ulDuration = lpPropertyArray[E_DURATION].Value.l;
		/*
		 * We already compared duration between common and start,
		 * now we have to check if that duration also equals what was set.
		 */
		if (ulDuration != Value.l) {
			hr = ReplaceProperty(lpMessage, "dispidApptDuration",
					     CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_DURATION], PT_LONG),
					     "Duration does not match (End - Start / 60)",
					     Value);
			if (hr != hrSuccess)
				goto exit;
		}
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

HRESULT ZarafaFsckCalendar::ValidateRecurrence(LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropertyArray = NULL;
	LPSPropTagArray lpPropertyTagArray = NULL;
	BOOL bRecurring = FALSE;
	LONG ulType = 0;
	LPSTR lpPattern = "";
	char *lpData = NULL;
	unsigned int ulLen = 0;

	enum {
		E_RECURRENCE,
		E_RECURRENCE_TYPE,
		E_RECURRENCE_PATTERN,
		E_RECURRENCE_STATE,
		TAG_COUNT
	};

	LPMAPINAMEID *lppTagArray = NULL;

	/*
	 * Allocate the NamedID list and initialize it to all
	 * properties which could give us some information about the name.
	 */
	hr = allocNamedIdList(TAG_COUNT, &lppTagArray);
	if (hr != hrSuccess)
		goto exit;

	lppTagArray[E_RECURRENCE]->lpguid = (LPGUID)&PSETID_Appointment;
	lppTagArray[E_RECURRENCE]->ulKind = MNID_ID;
	lppTagArray[E_RECURRENCE]->Kind.lID = dispidRecurring;

	lppTagArray[E_RECURRENCE_TYPE]->lpguid = (LPGUID)&PSETID_Appointment;
	lppTagArray[E_RECURRENCE_TYPE]->ulKind = MNID_ID;
	lppTagArray[E_RECURRENCE_TYPE]->Kind.lID = dispidRecurrenceType;

	lppTagArray[E_RECURRENCE_PATTERN]->lpguid = (LPGUID)&PSETID_Appointment;
	lppTagArray[E_RECURRENCE_PATTERN]->ulKind = MNID_ID;
	lppTagArray[E_RECURRENCE_PATTERN]->Kind.lID = dispidRecurrencePattern;

	lppTagArray[E_RECURRENCE_STATE]->lpguid = (LPGUID)&PSETID_Appointment;
	lppTagArray[E_RECURRENCE_STATE]->ulKind = MNID_ID;
	lppTagArray[E_RECURRENCE_STATE]->Kind.lID = dispidRecurrenceState;

	hr = ReadNamedProperties(lpMessage, TAG_COUNT, lppTagArray, &lpPropertyTagArray, &lpPropertyArray);
	if (FAILED(hr))
		goto exit;

	if (PROP_TYPE(lpPropertyArray[E_RECURRENCE].ulPropTag) == PT_ERROR) {
		__UPV Value;

		/*
		 * Check if the recurrence type is set, and if this is the case,
		 * if the type indicates recurrence.
		 */
		if (PROP_TYPE(lpPropertyArray[E_RECURRENCE_TYPE].ulPropTag) == PT_ERROR ||
		    lpPropertyArray[E_RECURRENCE_TYPE].Value.l == 0)
			Value.b = false;
		else
			Value.b = true;

		hr = AddMissingProperty(lpMessage, "dispidRecurring",
					CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_RECURRENCE], PT_BOOLEAN),
					Value);
		if (hr != hrSuccess)
			goto exit;

		bRecurring = Value.b;
	} else
		bRecurring = lpPropertyArray[E_RECURRENCE].Value.b;

	/*
	 * The type has 4 possible values:
	 * 	0 - No recurrence
	 *	1 - Daily
	 *	2 - Weekly
	 *	3 - Monthly
	 *	4 - Yearly
	 */
	if (PROP_TYPE(lpPropertyArray[E_RECURRENCE_TYPE].ulPropTag) == PT_ERROR) {
		if (bRecurring) {
			std::cout << "Item is recurring but is missing recurrence type" << std::endl;
			hr= E_INVALIDARG;
			goto exit;
		} else
			ulType = 0;
	} else
		ulType = lpPropertyArray[E_RECURRENCE_TYPE].Value.l;

	if (!bRecurring && ulType > 0) {
		__UPV Value;
		Value.l = 0;

		hr = ReplaceProperty(lpMessage, "dispidRecurrenceType",
				     CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_RECURRENCE_TYPE], PT_LONG),
				     "No recurrence, but recurrence type is > 0.",
				     Value);
		if (hr != hrSuccess)
			goto exit;
	} else if (bRecurring && ulType == 0) {
		__UPV Value;
		Value.b = false;

		hr = ReplaceProperty(lpMessage, "dispidRecurring",
				     CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_RECURRENCE], PT_BOOLEAN),
				     "Recurrence has been set, but type indicates no recurrence.",
				     Value);
		if (hr != hrSuccess)
			goto exit;
	} else if (ulType > 4) {
		__UPV Value;

		if (bRecurring) {
			Value.b = false;
			bRecurring = false;

			hr = ReplaceProperty(lpMessage, "dispidRecurring",
					     CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_RECURRENCE], PT_BOOLEAN),
					     "Invalid recurrence type, disabling recurrence.",
					     Value);
			if (hr != hrSuccess)
				goto exit;
		}

		Value.l = 0;
		ulType = 0;

		hr = ReplaceProperty(lpMessage, "dispidRecurrenceType",
				     CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_RECURRENCE], PT_LONG),
				     "Invalid recurrence type, disabling recurrence.",
				     Value);
		if (hr != hrSuccess)
			goto exit;
	}

	if (PROP_TYPE(lpPropertyArray[E_RECURRENCE_PATTERN].ulPropTag) == PT_ERROR ||
	    strcmp(lpPropertyArray[E_RECURRENCE_PATTERN].Value.lpszA, "") == 0) {
		if (bRecurring) {
			__UPV Value;
	
			switch (ulType) {
			case 1:
				Value.lpszA = "Daily";
				break;
			case 2:
				Value.lpszA = "Weekly";
				break;
			case 3:
				Value.lpszA = "Monthly";
				break;
			case 4:
				Value.lpszA = "Yearly";
				break;
			default:
				Value.lpszA = "Invalid";
				break;
			}

			hr = AddMissingProperty(lpMessage, "dispidRecurrencePattern",
						CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_RECURRENCE_PATTERN], PT_STRING8),
						Value);
			if (hr != hrSuccess)
				goto exit;
		}
	}
	
	if (bRecurring && PROP_TYPE(lpPropertyArray[E_RECURRENCE_STATE].ulPropTag) != PT_ERROR) {
	    // Check the actual recurrence state
	    RecurrenceState r;
	    __UPV Value;
        std::vector<RecurrenceState::Exception>::iterator iEx;
        std::vector<RecurrenceState::ExtendedException>::iterator iEEx;
		convert_context convertContext;

	    switch(r.ParseBlob((char *)lpPropertyArray[E_RECURRENCE_STATE].Value.bin.lpb, lpPropertyArray[E_RECURRENCE_STATE].Value.bin.cb, RECURRENCE_STATE_CALENDAR)) {
	        case hrSuccess:
	        case MAPI_W_ERRORS_RETURNED:
	            // Recurrence state is readable, but may have errors.
	            
	            // First, make sure the number of extended exceptions is correct
	            while(r.lstExtendedExceptions.size() > r.lstExceptions.size()) {
	                r.lstExtendedExceptions.erase(--r.lstExtendedExceptions.end());
	            }
	            
	            // Add new extendedexceptions if missing
                iEx = r.lstExceptions.begin();
	            
	            for(size_t i = 0; i < r.lstExtendedExceptions.size(); i++) {
	                iEx++;
	            }
	            
	            while(r.lstExtendedExceptions.size() < r.lstExceptions.size()) {
	                wstring wstr;
	                RecurrenceState::ExtendedException ex;
	                
	                ex.ulStartDateTime = iEx->ulStartDateTime;
	                ex.ulEndDateTime = iEx->ulEndDateTime;
	                ex.ulOriginalStartDate = iEx->ulOriginalStartDate;
	                
					TryConvert(convertContext, iEx->strSubject, rawsize(iEx->strSubject), "windows-1252", wstr);
	                ex.strWideCharSubject.assign(wstr.c_str(), wstr.size());
	                
					TryConvert(convertContext, iEx->strLocation, rawsize(iEx->strLocation), "windows-1252", wstr);
	                ex.strWideCharLocation.assign(wstr.c_str(), wstr.size());
	                
	                r.lstExtendedExceptions.push_back(ex);
	                iEx++;
                }
                
                // Set some defaults right for exceptions
                for(iEx = r.lstExceptions.begin(); iEx != r.lstExceptions.end(); iEx++) {
                    iEx->ulOriginalStartDate = (iEx->ulOriginalStartDate / 1440) * 1440;
                }
                
                // Set some defaults for extended exceptions
                iEx = r.lstExceptions.begin();
                for(iEEx = r.lstExtendedExceptions.begin(); iEEx != r.lstExtendedExceptions.end(); iEEx++) {
                    wstring wstr;
                    iEEx->strReservedBlock1 = "";
                    iEEx->strReservedBlock2 = "";
                    iEEx->ulChangeHighlightValue = 0;
                    iEEx->ulOriginalStartDate = (iEx->ulOriginalStartDate / 1440) * 1440;

					TryConvert(convertContext, iEx->strSubject, rawsize(iEx->strSubject), "windows-1252", wstr);
	                iEEx->strWideCharSubject.assign(wstr.c_str(), wstr.size());

					TryConvert(convertContext, iEx->strLocation, rawsize(iEx->strLocation), "windows-1252", wstr);
	                iEEx->strWideCharLocation.assign(wstr.c_str(), wstr.size());
                    iEx++;
                }
                
                // Reset reserved data to 0
                r.strReservedBlock1 = "";
                r.strReservedBlock2 = "";
                
                // These are constant
                r.ulReaderVersion = 0x3004;
                r.ulWriterVersion = 0x3004;
                
                r.GetBlob(&lpData, &ulLen);
                Value.bin.lpb = (BYTE *)lpData;
                Value.bin.cb = ulLen;
    
                // Update the recurrence if there is a change            
                if(ulLen != lpPropertyArray[E_RECURRENCE_STATE].Value.bin.cb || memcmp(lpPropertyArray[E_RECURRENCE_STATE].Value.bin.lpb, lpData, ulLen) != 0)
                    hr = ReplaceProperty(lpMessage, "dispidRecurrenceState", CHANGE_PROP_TYPE(lpPropertyArray[E_RECURRENCE_STATE].ulPropTag, PT_BINARY), "Recoverable recurrence state.", Value);
	            break;
	        default:
	            // Recurrence state is useless
                Value.l = 0;

                hr = ReplaceProperty(lpMessage, "dispidRecurrenceState",
                             CHANGE_PROP_TYPE(lpPropertyTagArray->aulPropTag[E_RECURRENCE], PT_LONG),
                             "Invalid recurrence state, disabling recurrence.",
                             Value);
	            break;
	    }

        if (hr != hrSuccess)
            goto exit;
	}
	
	lpPattern = lpPropertyArray[E_RECURRENCE_PATTERN].Value.lpszA;

    /* If we are here, we were succcessfull */
    hr = hrSuccess;

exit:
    if (lpData)
        MAPIFreeBuffer(lpData);
        
	if (lppTagArray)
		freeNamedIdList(lppTagArray);

	if (lpPropertyArray)
		MAPIFreeBuffer(lpPropertyArray);

	if (lpPropertyTagArray)
		MAPIFreeBuffer(lpPropertyTagArray);

	return hr;
}

HRESULT ZarafaFsckCalendar::ValidateItem(LPMESSAGE lpMessage, string strClass)
{
	HRESULT hr = hrSuccess;

	if (strClass != "IPM.Appointment") {
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

	hr = ValidateRecurrence(lpMessage);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}
