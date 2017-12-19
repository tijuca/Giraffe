/*
 * Copyright 2017 - Kopano and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <kopano/zcdefs.h>
#include <memory>
#include <new>
#include <cstdlib>
#include <libical/vcc.h>
#include <libical/vobject.h>
#include <mapicode.h>
#include <mapiutil.h>
#include <mapix.h>
#include <kopano/memory.hpp>
#include <kopano/platform.h>
#include <kopano/mapiguidext.h>
#include "mapitovcf.hpp"

using namespace KCHL;

namespace KC {

class mapitovcf_impl _kc_final : public mapitovcf {
	public:
	HRESULT add_message(IMessage *) _kc_override;
	HRESULT finalize(std::string *) _kc_override;

	private:
	VObject *to_prop(VObject *node, const char *prop, const SPropValue &value);
	VObject *to_prop(VObject *node, const char *prop, const wchar_t *value);

	std::string m_result;
	/*
	 * Since we do not want downstream projects to add include paths for
	 * libical, this is only in the implementation version.
	 */
};

HRESULT create_mapitovcf(mapitovcf **ret)
{
	*ret = new(std::nothrow) mapitovcf_impl();
	return *ret != nullptr ? hrSuccess : MAPI_E_NOT_ENOUGH_MEMORY;
}

VObject *mapitovcf_impl::to_prop(VObject *node, const char *prop,
    const SPropValue &s)
{
	auto newnode = addProp(node, prop);
	if (newnode == nullptr)
		return nullptr;
	if (PROP_TYPE(s.ulPropTag) == PT_UNICODE)
		setVObjectUStringZValue_(newnode, wcsdup(s.Value.lpszW));
	else if (PROP_TYPE(s.ulPropTag) == PT_STRING8)
		setVObjectStringZValue(newnode, s.Value.lpszA);
	return newnode;
}

VObject *mapitovcf_impl::to_prop(VObject *node, const char *prop,
    const wchar_t *value)
{
	auto newnode = addProp(node, prop);
	if (newnode == nullptr)
		return nullptr;
	setVObjectUStringZValue_(newnode, wcsdup(value));
	return newnode;
}

HRESULT mapitovcf_impl::add_message(IMessage *lpMessage)
{
	memory_ptr<SPropValue> lpMessageClass;

	if (lpMessage == nullptr)
		return MAPI_E_INVALID_PARAMETER;
	auto hr = HrGetOneProp(lpMessage, PR_MESSAGE_CLASS_A, &~lpMessageClass);
	if (hr != hrSuccess)
		return hr;
	if (strcasecmp(lpMessageClass->Value.lpszA, "IPM.Contact") != 0)
		return MAPI_E_INVALID_PARAMETER;

	auto root = newVObject(VCCardProp);
	memory_ptr<SPropValue> msgprop, msgprop2;
	hr = HrGetOneProp(lpMessage, PR_GIVEN_NAME, &~msgprop);
	HRESULT hr2 = HrGetOneProp(lpMessage, PR_SURNAME, &~msgprop2);
	if (hr == hrSuccess || hr2 == hrSuccess) {
		auto node = addGroup(root, VCNameProp);
		if (msgprop != nullptr)
			to_prop(node, VCGivenNameProp, *msgprop);
		if (msgprop2 != nullptr)
			to_prop(node, VCFamilyNameProp, *msgprop2);
	} else if (hr != MAPI_E_NOT_FOUND) {
		return hr;
	} else if (hr2 != MAPI_E_NOT_FOUND) {
		return hr2;
	}

	hr = HrGetOneProp(lpMessage, PR_DISPLAY_NAME, &~msgprop);
	if (hr == hrSuccess)
		to_prop(root, VCFullNameProp, *msgprop);
	else if (hr != MAPI_E_NOT_FOUND)
		return hr;

	hr = HrGetOneProp(lpMessage, PR_TITLE, &~msgprop);
	if (hr == hrSuccess)
		to_prop(root, VCTitleProp, *msgprop);
	else if (hr != MAPI_E_NOT_FOUND)
		return hr;

	hr = HrGetOneProp(lpMessage, PR_COMPANY_NAME, &~msgprop);
	if (hr == hrSuccess)
		to_prop(root, VCOrgProp, *msgprop);
	else if (hr != MAPI_E_NOT_FOUND)
		return hr;

	hr = HrGetOneProp(lpMessage, PR_HOME_TELEPHONE_NUMBER, &~msgprop);
	if (hr == hrSuccess) {
		auto node = to_prop(root, VCTelephoneProp, *msgprop);
		to_prop(node, "TYPE", L"HOME");
	} else if (hr != MAPI_E_NOT_FOUND) {
		return hr;
	}

	hr = HrGetOneProp(lpMessage, PR_MOBILE_TELEPHONE_NUMBER, &~msgprop);
	if (hr == hrSuccess) {
		auto node = to_prop(root, VCTelephoneProp, *msgprop);
		to_prop(node, "TYPE", L"MOBILE");
	} else if (hr != MAPI_E_NOT_FOUND) {
		return hr;
	}

	/* Email */
	for (int lid = 0x8083; lid <= 0x80a3; lid += 0x10) {
		MAPINAMEID name;
		MAPINAMEID *namep = &name;
		name.lpguid = const_cast<GUID *>(&PSETID_Address);
		name.ulKind = MNID_ID;
		name.Kind.lID = lid;

		memory_ptr<SPropTagArray> proptag;
		hr = lpMessage->GetIDsFromNames(1, &namep, MAPI_BEST_ACCESS, &~proptag);
		if (hr != hrSuccess)
			continue;

		ULONG proptype = CHANGE_PROP_TYPE(proptag->aulPropTag[0], PT_UNICODE);
		hr = HrGetOneProp(lpMessage, proptype, &~msgprop);
		if (hr == hrSuccess)
			to_prop(root, VCEmailAddressProp, *msgprop);
		else if (hr != MAPI_E_NOT_FOUND)
			continue;
	}

	static constexpr const SizedSPropTagArray(5, home_props) =
		{5, {PR_HOME_ADDRESS_STREET, PR_HOME_ADDRESS_CITY,
			 PR_HOME_ADDRESS_STATE_OR_PROVINCE, PR_HOME_ADDRESS_POSTAL_CODE,
			 PR_HOME_ADDRESS_COUNTRY}};
	memory_ptr<SPropValue> msgprop_array;
	unsigned int count;

	hr = lpMessage->GetProps(home_props, 0, &count, &~msgprop_array);
	if (hr == hrSuccess) {
		auto adrnode = addProp(root, VCAdrProp);
		auto node = addProp(adrnode, "TYPE");
		setVObjectStringZValue(node, "HOME");
		to_prop(adrnode, "STREET", msgprop_array[0].Value.lpszW);
		to_prop(adrnode, "L", msgprop_array[1].Value.lpszW);
		to_prop(adrnode, "R", msgprop_array[2].Value.lpszW);
		to_prop(adrnode, "PC", msgprop_array[3].Value.lpszW);
		to_prop(adrnode, "C", msgprop_array[4].Value.lpszW);
	}

	static constexpr const SizedSPropTagArray(5, other_props) =
		{5, {PR_OTHER_ADDRESS_STREET, PR_OTHER_ADDRESS_CITY,
			 PR_OTHER_ADDRESS_STATE_OR_PROVINCE, PR_OTHER_ADDRESS_POSTAL_CODE,
			 PR_OTHER_ADDRESS_COUNTRY}};

	hr = lpMessage->GetProps(other_props, 0, &count, &~msgprop_array);
	if (hr == hrSuccess) {
		auto adrnode = addProp(root, VCAdrProp);
		to_prop(adrnode, "STREET", msgprop_array[0].Value.lpszW);
		to_prop(adrnode, "L", msgprop_array[1].Value.lpszW);
		to_prop(adrnode, "R", msgprop_array[2].Value.lpszW);
		to_prop(adrnode, "PC", msgprop_array[3].Value.lpszW);
		to_prop(adrnode, "C", msgprop_array[4].Value.lpszW);
	}

	memory_ptr<MAPINAMEID> nameids;
	hr = MAPIAllocateBuffer(5 * sizeof(MAPINAMEID), &~nameids);
	if (hr != hrSuccess)
		return hr;

	memory_ptr<MAPINAMEID *> nameids_ptrs;
	hr = MAPIAllocateBuffer(5 * sizeof(MAPINAMEID *), &~nameids_ptrs);
	if (hr != hrSuccess)
		return hr;

	for (size_t i = 0; i < 5; ++i) {
		nameids[i].lpguid = const_cast<GUID *>(&PSETID_Address);
		nameids[i].ulKind = MNID_ID;
		nameids[i].Kind.lID = 0x8045 + i;
		nameids_ptrs[i] = &nameids[i];
	}

	memory_ptr<SPropTagArray> proptag;
	hr = lpMessage->GetIDsFromNames(5, nameids_ptrs, MAPI_BEST_ACCESS, &~proptag);
	if (hr == hrSuccess) {
		for (size_t i = 0; i < 5; ++i)
			proptag->aulPropTag[i] = CHANGE_PROP_TYPE(proptag->aulPropTag[i], PT_UNICODE);

		hr = lpMessage->GetProps(proptag, 0, &count, &~msgprop_array);
		if (hr == hrSuccess) {
			auto adrnode = addProp(root, VCAdrProp);
			auto node = addProp(adrnode, "TYPE");
			setVObjectStringZValue(node, "WORK");
			to_prop(adrnode, "STREET", msgprop_array[0].Value.lpszW);
			to_prop(adrnode, "L", msgprop_array[1].Value.lpszW);
			to_prop(adrnode, "R", msgprop_array[2].Value.lpszW);
			to_prop(adrnode, "PC", msgprop_array[3].Value.lpszW);
			to_prop(adrnode, "C", msgprop_array[4].Value.lpszW);
		}
	}

	/* Write memobject */
	int len = 0;
	auto cresult = writeMemVObject(nullptr, &len, root);
	if (cresult == nullptr)
		return MAPI_E_NOT_ENOUGH_MEMORY;
	m_result = cresult;
	free(cresult);
	cleanVObject(root);
	return hrSuccess;
}

HRESULT mapitovcf_impl::finalize(std::string *s)
{
	if (s == nullptr)
		return MAPI_E_INVALID_PARAMETER;
	*s = m_result;
	return hrSuccess;
}

} /* namespace */
