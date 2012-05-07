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

HRESULT ZarafaFsckContact::ValidateContactNames(LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropertyArray = NULL;

	enum {
		E_SUBJECT,
		E_FULLNAME,
		E_PREFIX,
		E_FIRSTNAME,
		E_MIDDLENAME,
		E_LASTNAME,
		E_SUFFIX,
		TAG_COUNT
	};

	ULONG ulTags[] = {
		PR_SUBJECT_A,
		PR_DISPLAY_NAME_A,
		PR_DISPLAY_NAME_PREFIX_A,
		PR_GIVEN_NAME_A,
		PR_MIDDLE_NAME_A,
		PR_SURNAME_A,
		PR_GENERATION_A,
	};

	std::string result[TAG_COUNT];

	hr = ReadProperties(lpMessage, TAG_COUNT, ulTags, &lpPropertyArray);
	/* Ignore error, we are going to fix this :) */
	if (!lpPropertyArray)
		goto exit;

	for (ULONG i = 0; i < TAG_COUNT; i++) {
		if (PROP_TYPE(lpPropertyArray[i].ulPropTag) != PT_ERROR &&
			lpPropertyArray[i].Value.lpszA &&
			strlen(lpPropertyArray[i].Value.lpszA))
				result[i] = std::string(lpPropertyArray[i].Value.lpszA);
	}

	/* Generate fullname based on remaining fields */
	if (result[E_FULLNAME].empty()) {
		__UPV Value;

		/* Just loop through the list and attach all provided information,
		 * note that the order of the definitions must for this reason be:
		 * PREFIX FIRSTNAME MIDDLENAME LASTNAME SUFFIX */
		for (ULONG j = E_PREFIX; j < ((ULONG)E_SUFFIX)+1; j++) {
			if (!result[E_FULLNAME].empty() && !result[j].empty())
				result[E_FULLNAME] += " ";

			result[E_FULLNAME] += result[j];
		}

		/* Still empty? Things are terribly broken */
		if (result[E_FULLNAME].empty()) {
			hr = E_INVALIDARG;
			goto exit;
		}

		Value.lpszA = (char *)result[E_FULLNAME].c_str();

		hr = ReplaceProperty(lpMessage, "PR_DISPLAY_NAME", PR_DISPLAY_NAME_A, "No display name was provided", Value);
		if (hr != hrSuccess)
			goto exit;
	}

	if(result[E_SUBJECT].empty()) {
		__UPV Value;
		
		Value.lpszA = (char *)result[E_FULLNAME].c_str();

        hr = ReplaceProperty(lpMessage, "PR_SUBJECT", PR_SUBJECT_A, "No subject was provided", Value);
		if (hr != hrSuccess)
            goto exit;
	}

	/* Given name and surname are not allowed to be empty for BlackBerry, Webaccess did generate
	 * contacts without a given- and surname in the past so we must recover this now based on the
	 * displayname information */
	if (result[E_FIRSTNAME].empty() && result[E_LASTNAME].empty()) {
		__UPV Value;

		if (result[E_FULLNAME].empty()) {
			hr = E_INVALIDARG;
			goto exit;
		}

		/* If a prefix and suffix were provided, strip them from the fullname */
		if (!result[E_PREFIX].empty()) {
			if (result[E_FULLNAME].substr(0, result[E_PREFIX].size()) == result[E_PREFIX])
				result[E_FULLNAME].erase(0, result[E_PREFIX].size());
		}

		if (!result[E_SUFFIX].empty()) {
			if (result[E_FULLNAME].substr(result[E_FULLNAME].size() - result[E_SUFFIX].size(), std::string::npos) == result[E_SUFFIX])
				result[E_FULLNAME].erase(result[E_FULLNAME].size() - result[E_SUFFIX].size(), std::string::npos);
		}

		/* Well technically this could happen... But somebody seriously wrecked his item in that case :S */
		if (result[E_FULLNAME].empty()) {
			hr = E_INVALIDARG;
			goto exit;
		}

		/* Now we have a fullname containing a first and last name, but which is which. We are going
		 * to use the same algorithm as Outlook: The first word is always the first name any words
		 * after it are the last name. Ok perhaps not litteraly the same, but it should be somewhat
		 * sufficient */
		size_t pos;

		/* Strip leading spaces*/
		pos = result[E_FULLNAME].find_first_not_of(" ");
		result[E_FULLNAME].erase(0, pos);

		/* Determine first namee */
		pos = result[E_FULLNAME].find_first_of(" ", pos);
		result[E_FIRSTNAME] = result[E_FULLNAME].substr(0, pos);
		if (pos != std::string::npos) {
			/* Determine last name */
			pos = result[E_FULLNAME].find_first_of(" ", pos);
			result[E_LASTNAME] = result[E_FULLNAME].substr(pos, std::string::npos);
		}

		Value.lpszA = (char *)result[E_FIRSTNAME].c_str();

		hr = ReplaceProperty(lpMessage, "PR_GIVEN_NAME", PR_GIVEN_NAME_A, "No given name was provided", Value);
		if (hr != hrSuccess)
			goto exit;

		Value.lpszA = (char *)result[E_LASTNAME].c_str();

		hr = ReplaceProperty(lpMessage, "PR_SURNAME", PR_SURNAME_A, "No surname was provided", Value);
		if (hr != hrSuccess)
			goto exit;
	}

	/* If we are here, we were succcessfull */
	hr = hrSuccess;

exit:
	if (lpPropertyArray)
		MAPIFreeBuffer(lpPropertyArray);

	return hr;
}

HRESULT ZarafaFsckContact::ValidateItem(LPMESSAGE lpMessage, string strClass)
{
	HRESULT hr = hrSuccess;

	if (strClass != "IPM.Contact" && strClass != "IPM.DistList") {
		std::cout << "Illegal class: \"" << strClass << "\"" << std::endl;
		hr = E_INVALIDARG;
		goto exit;
	}

	if (strClass == "IPM.Contact")
		hr = ValidateContactNames(lpMessage);
	// else: @todo distlist validation
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}
