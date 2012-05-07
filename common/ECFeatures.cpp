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

#include "ECFeatures.h"
#include "ECFeatureList.h"
#include "CommonUtil.h"
#include "mapi_ptr.h"

using namespace std;

/** 
 * Checks if given feature name is an actual zarafa feature.
 * 
 * @param[in] feature name of the feature to check
 * 
 * @return 
 */
bool isFeature(const char* feature)
{
	for (size_t i = 0; i < arraySize(zarafa_features); i++)
		if (stricmp(feature, zarafa_features[i]) == 0)
			return true;
	return false;
}

/** 
 * Checks if the given feature name is present in the MV list in the given prop
 * 
 * @param[in] feature check for this feature in the MV_STRING8 list in lpProps
 * @param[in] lpProps should be either PR_EC_(DIS/EN)ABLED_FEATURES_A
 * 
 * @return MAPI Error code
 */
HRESULT hasFeature(const char* feature, LPSPropValue lpProps)
{
	if (!feature || !lpProps || PROP_TYPE(lpProps->ulPropTag) != PT_MV_STRING8)
		return MAPI_E_INVALID_PARAMETER;

	for (ULONG i = 0; i < lpProps->Value.MVszA.cValues; i++) {
		if (stricmp(lpProps->Value.MVszA.lppszA[i], feature) == 0)
			return hrSuccess;
	}

	return MAPI_E_NOT_FOUND;
}

/** 
 * Checks if the given feature name is present in the MV list in the given prop
 * 
 * @param[in] feature check for this feature in the MV_STRING8 list in lpProps
 * @param[in] lpProps should be either PR_EC_(DIS/EN)ABLED_FEATURES_A
 * 
 * @return MAPI Error code
 */
HRESULT hasFeature(const WCHAR* feature, LPSPropValue lpProps)
{
	if (!feature || !lpProps || PROP_TYPE(lpProps->ulPropTag) != PT_MV_UNICODE)
		return MAPI_E_INVALID_PARAMETER;

	for (ULONG i = 0; i < lpProps->Value.MVszW.cValues; i++) {
		if (wcscasecmp(lpProps->Value.MVszW.lppszW[i], feature) == 0)
			return hrSuccess;
	}

	return MAPI_E_NOT_FOUND;
}

/** 
 * Retrieve a property from the owner of the given store
 * 
 * @param[in] lpStore Store of a user to get a property of
 * @param[in] ulPropTag Get this property from the owner of the store
 * @param[out] lpProps Prop value
 * 
 * @return MAPI Error code
 */
HRESULT HrGetUserProp(IAddrBook *lpAddrBook, IMsgStore *lpStore, ULONG ulPropTag, LPSPropValue *lpProps)
{
	HRESULT hr = hrSuccess;
	SPropValuePtr ptrProps;
	MailUserPtr ptrUser;
	ULONG ulObjType;

	if (!lpStore || PROP_TYPE(ulPropTag) != PT_MV_STRING8 || !lpProps) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = HrGetOneProp(lpStore, PR_MAILBOX_OWNER_ENTRYID, &ptrProps);
	if (hr != hrSuccess)
		goto exit;

	hr = lpAddrBook->OpenEntry(ptrProps->Value.bin.cb, (LPENTRYID)ptrProps->Value.bin.lpb, &IID_IMailUser, 0, &ulObjType, &ptrUser);
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(ptrUser, ulPropTag, &ptrProps);
	if (hr != hrSuccess)
		goto exit;

	*lpProps = ptrProps.release();

exit:
	return hr;
}

/** 
 * Check if a feature is present in a given enabled/disabled list
 * 
 * @param[in] feature check this feature name
 * @param[in] lpStore Get information from this user store
 * @param[in] ulPropTag either enabled or disabled feature list property tag
 * 
 * @return Only return true if explicitly set in property
 */
bool checkFeature(const char* feature, IAddrBook *lpAddrBook, IMsgStore *lpStore, ULONG ulPropTag)
{
	HRESULT hr = hrSuccess;
	SPropValuePtr ptrProps;

	if (!feature || !lpStore || PROP_TYPE(ulPropTag) != PT_MV_STRING8) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = HrGetUserProp(lpAddrBook, lpStore, ulPropTag, &ptrProps);
	if (hr != hrSuccess)
		goto exit;

	hr = hasFeature(feature, ptrProps);

exit:
	return hr == hrSuccess;
}

bool isFeatureEnabled(const char* feature, IAddrBook *lpAddrBook, IMsgStore *lpStore)
{
	return checkFeature(feature, lpAddrBook, lpStore, PR_EC_ENABLED_FEATURES_A);
}

bool isFeatureDisabled(const char* feature, IAddrBook *lpAddrBook, IMsgStore *lpStore)
{
	return checkFeature(feature, lpAddrBook, lpStore, PR_EC_DISABLED_FEATURES_A);
}
