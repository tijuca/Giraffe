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
#include <algorithm>
#include <memory>
#include <new>
#include <kopano/ECRestriction.h>
#include <kopano/charset/convert.h>
#include <kopano/mapiext.h>
#include <kopano/mapiguidext.h>
#include <kopano/memory.hpp>
#include <kopano/namedprops.h>
#include <kopano/platform.h>
#include <kopano/stringutil.h>
#include <libical/vcc.h>
#include <libical/vobject.h>
#include <mapi.h>
#include <mapiutil.h>
#include <mapix.h>
#include <vector>
#include "vcftomapi.hpp"

using namespace KCHL;

namespace KC {

class vcftomapi_impl _kc_final : public vcftomapi {
	public:
	/*
	 * - lpPropObj to lookup named properties
	 * - Addressbook (Global AddressBook for looking up users)
	 */
	vcftomapi_impl(IMAPIProp *o) : vcftomapi(o) {}
	HRESULT parse_vcf(const std::string &) _kc_override;
	HRESULT get_item(IMessage *) _kc_override;

	private:
	HRESULT save_props(const std::list<SPropValue> &, IMAPIProp *);
	HRESULT handle_N(VObject *);
	HRESULT handle_TEL(VObject *);
	HRESULT handle_EMAIL(VObject *);
	HRESULT handle_ADR(VObject *);
	HRESULT vobject_to_prop(VObject *, SPropValue &, ULONG proptype);
	HRESULT vobject_to_named_prop(VObject *, SPropValue &, ULONG named_proptype);
	HRESULT unicode_to_named_prop(const wchar_t *, SPropValue &, ULONG named_proptype);

	size_t email_count = 0;
};

/**
 * Create a class implementing the ICalToMapi "interface".
 */
HRESULT create_vcftomapi(IMAPIProp *prop, vcftomapi **ret)
{
	if (prop == nullptr || ret == nullptr)
		return MAPI_E_INVALID_PARAMETER;
	*ret = new(std::nothrow) vcftomapi_impl(prop);
	return *ret != nullptr ? hrSuccess : MAPI_E_NOT_ENOUGH_MEMORY;
}

HRESULT vcftomapi_impl::handle_N(VObject *v)
{
	VObjectIterator tt;

	for (initPropIterator(&tt, v); moreIteration(&tt); ) {
		auto vv = nextVObject(&tt);
		auto name = vObjectName(vv);
		SPropValue s;

		if (strcmp(name, VCFamilyNameProp) == 0) {
			auto ret = vobject_to_prop(vv, s, PR_SURNAME);
			if (ret != hrSuccess)
				return ret;
			props.push_back(s);
		} else if (strcmp(name, VCGivenNameProp) == 0) {
			auto ret = vobject_to_prop(vv, s, PR_GIVEN_NAME);
			if (ret != hrSuccess)
				return ret;
			props.push_back(s);
		}
	}
	return hrSuccess;
}

HRESULT vcftomapi_impl::handle_TEL(VObject *v)
{
	VObjectIterator t;

	for (initPropIterator(&t, v); moreIteration(&t); ) {
		auto vv = nextVObject(&t);
		auto name = vObjectName(vv);
		SPropValue s;
		const char *namep = strcmp(name, "TYPE") == 0 ? vObjectStringZValue(vv) : name;
		auto tokens = tokenize(namep, ',');

		for (auto const &token : tokens) {
			if (strcasecmp(token.c_str(), "HOME") == 0) {
				auto ret = vobject_to_prop(v, s, PR_HOME_TELEPHONE_NUMBER);
				if (ret != hrSuccess)
					return ret;
				props.push_back(s);
			}
			if (strcasecmp(token.c_str(), "MOBILE") == 0) {
				auto ret = vobject_to_prop(v, s, PR_MOBILE_TELEPHONE_NUMBER);
				if (ret != hrSuccess)
					return ret;
				props.push_back(s);
			}
		}
	}
	return hrSuccess;
}

HRESULT vcftomapi_impl::handle_EMAIL(VObject *v)
{
	// we can only accept 3 email addresses
	if (email_count > 2)
		return hrSuccess;

	unsigned int prop_id = 0x8083 + (email_count * 0x10);
	SPropValue s;
	vobject_to_named_prop(v, s, prop_id);
	props.push_back(s);

	prop_id = 0x8082 + (email_count * 0x10);
	auto ret = unicode_to_named_prop(L"SMTP", s, prop_id);
	if (ret != hrSuccess)
		return ret;
	props.push_back(s);

	email_count++;
	return hrSuccess;
}

HRESULT vcftomapi_impl::handle_ADR(VObject *v)
{
	enum { OTHER, WORK, HOME } adr_type = OTHER;
	VObjectIterator t;

	initPropIterator(&t, v);
	if (moreIteration(&t)) {
		auto vv = nextVObject(&t);
		auto name = vObjectName(vv);

		const char *namep = strcmp(name, "TYPE") == 0 ? vObjectStringZValue(vv) : name;
		if (strcmp(namep, "HOME") == 0)
			adr_type = HOME;
		else if (strcmp(namep, "WORK") == 0)
			adr_type = WORK;
	}

	while (moreIteration(&t)) {
		auto vv = nextVObject(&t);
		auto name = vObjectName(vv);
		SPropValue s;

		s.ulPropTag = 0;

		if (adr_type == HOME && !strcmp(name, "STREET"))
			vobject_to_prop(vv, s, PR_HOME_ADDRESS_STREET);
		else if (adr_type == HOME && !strcmp(name, "L"))
			vobject_to_prop(vv, s, PR_HOME_ADDRESS_CITY);
		else if (adr_type == HOME && !strcmp(name, "R"))
			vobject_to_prop(vv, s, PR_HOME_ADDRESS_STATE_OR_PROVINCE);
		else if (adr_type == HOME && !strcmp(name, "PC"))
			vobject_to_prop(vv, s, PR_HOME_ADDRESS_POSTAL_CODE);
		else if (adr_type == HOME && !strcmp(name, "C"))
			vobject_to_prop(vv, s, PR_HOME_ADDRESS_COUNTRY);
		else if (adr_type == WORK && !strcmp(name, "STREET"))
			vobject_to_named_prop(vv, s, 0x8045);
		else if (adr_type == WORK && !strcmp(name, "L"))
			vobject_to_named_prop(vv, s, 0x8046);
		else if (adr_type == WORK && !strcmp(name, "R"))
			vobject_to_named_prop(vv, s, 0x8047);
		else if (adr_type == WORK && !strcmp(name, "PC"))
			vobject_to_named_prop(vv, s, 0x8048);
		else if (adr_type == WORK && !strcmp(name, "C"))
			vobject_to_named_prop(vv, s, 0x8049);
		else if (adr_type == OTHER && !strcmp(name, "STREET"))
			vobject_to_prop(vv, s, PR_OTHER_ADDRESS_STREET);
		else if (adr_type == OTHER && !strcmp(name, "L"))
			vobject_to_prop(vv, s, PR_OTHER_ADDRESS_CITY);
		else if (adr_type == OTHER && !strcmp(name, "R"))
			vobject_to_prop(vv, s, PR_OTHER_ADDRESS_STATE_OR_PROVINCE);
		else if (adr_type == OTHER && !strcmp(name, "PC"))
			vobject_to_prop(vv, s, PR_OTHER_ADDRESS_POSTAL_CODE);
		else if (adr_type == OTHER && !strcmp(name, "C"))
			vobject_to_prop(vv, s, PR_OTHER_ADDRESS_COUNTRY);

		if (s.ulPropTag > 0)
			props.push_back(s);
	}

	return hrSuccess;
}

/**
 * Parses an ICal string (with a certain charset) and converts the
 * data in memory. The real MAPI object can be retrieved using
 * GetItem().
 */
HRESULT vcftomapi_impl::parse_vcf(const std::string &ical)
{
	HRESULT hr = hrSuccess;
	auto v = Parse_MIME(ical.c_str(), ical.length());
	if (v == nullptr)
		return MAPI_E_CORRUPT_DATA;

	auto v_orig = v;
	VObjectIterator t;
	for (initPropIterator(&t, v); moreIteration(&t); ) {
		SPropValue s;
		v = nextVObject(&t);
		auto name = vObjectName(v);

		if (vObjectValueType(v) == VCVT_NOVALUE)
			continue;

		if (strcmp(name, VCNameProp) == 0) {
			hr = handle_N(v);
			if (hr != hrSuccess)
				return hr;
		} else if (strcmp(name, VCFullNameProp) == 0) {
			hr = vobject_to_prop(v, s, PR_DISPLAY_NAME);
			if (hr != hrSuccess)
				return hr;
			props.push_back(s);
		} else if (strcmp(name, VCTitleProp) == 0) {
			hr = vobject_to_prop(v, s, PR_TITLE);
			if (hr != hrSuccess)
				return hr;
			props.push_back(s);
		} else if (strcmp(name, VCOrgProp) == 0) {
			hr = vobject_to_prop(v, s, PR_COMPANY_NAME);
			if (hr != hrSuccess)
				return hr;
			props.push_back(s);
		} else if (strcmp(name, VCTelephoneProp) == 0) {
			hr = handle_TEL(v);
			if (hr != hrSuccess)
				return hr;
		} else if (strcmp(name, VCEmailAddressProp) == 0) {
			hr = handle_EMAIL(v);
			if (hr != hrSuccess)
				return hr;
		} else if (strcmp(name, VCAdrProp) == 0) {
			hr = handle_ADR(v);
			if (hr != hrSuccess)
				return hr;
		}
	}
	cleanVObject(v_orig);
	return hr;
}

HRESULT vcftomapi_impl::vobject_to_prop(VObject *v, SPropValue &s, ULONG proptype)
{
	auto value_type = vObjectValueType(v);
	if (value_type == VCVT_STRINGZ) {
		s.ulPropTag = CHANGE_PROP_TYPE(proptype, PT_STRING8);
		auto val = vObjectStringZValue(v);
		auto ret = MAPIAllocateBuffer(strlen(val) + 1, reinterpret_cast<void **>(&s.Value.lpszA));
		if (ret != hrSuccess)
			return ret;
		strcpy(s.Value.lpszA, val);
	}
	else if (value_type == VCVT_USTRINGZ) {
		s.ulPropTag = CHANGE_PROP_TYPE(proptype, PT_UNICODE);
		auto uval = vObjectUStringZValue(v);
		auto ret = MAPIAllocateBuffer(sizeof(wchar_t) * (wcslen(uval) + 1), reinterpret_cast<void **>(&s.Value.lpszW));
		if (ret != hrSuccess)
			return ret;
		wcscpy(s.Value.lpszW, uval);
	}
	else
		return MAPI_E_CALL_FAILED;

	return hrSuccess;
}

HRESULT vcftomapi_impl::vobject_to_named_prop(VObject *v, SPropValue &s,
    ULONG named_proptype)
{
	HRESULT hr;
        MAPINAMEID name;
	MAPINAMEID *namep = &name;
	memory_ptr<SPropTagArray> proptag;

	name.lpguid = const_cast<GUID *>(&PSETID_Address);
	name.ulKind = MNID_ID;
	name.Kind.lID = named_proptype;
	hr = m_propobj->GetIDsFromNames(1, &namep, MAPI_CREATE, &~proptag);
	if (hr != hrSuccess)
		return hr;
	return vobject_to_prop(v, s, proptag->aulPropTag[0]);
}

HRESULT vcftomapi_impl::unicode_to_named_prop(const wchar_t *v, SPropValue &s,
    ULONG named_proptype)
{
        MAPINAMEID name;
	MAPINAMEID *namep = &name;
	memory_ptr<SPropTagArray> proptag;

	name.lpguid = const_cast<GUID *>(&PSETID_Address);
	name.ulKind = MNID_ID;
	name.Kind.lID = named_proptype;

	HRESULT hr = m_propobj->GetIDsFromNames(1, &namep,
	             MAPI_CREATE, &~proptag);
	if (hr != hrSuccess)
		return hr;
	s.ulPropTag = CHANGE_PROP_TYPE(proptag->aulPropTag[0], PT_UNICODE);
	auto ret = MAPIAllocateBuffer(sizeof(wchar_t) * (wcslen(v) + 1),
               reinterpret_cast<void **>(&s.Value.lpszW));
	if (ret != hrSuccess)
		return hr;
	wcscpy(s.Value.lpszW, v);
	return hrSuccess;
}

/**
 * Sets mapi properties in Imessage object from the icalitem.
 */
HRESULT vcftomapi_impl::get_item(IMessage *msg)
{
	if (msg == nullptr)
		return MAPI_E_INVALID_PARAMETER;

	SPropValue s;
	s.ulPropTag = CHANGE_PROP_TYPE(PR_MESSAGE_CLASS, PT_STRING8);
	s.Value.lpszA = const_cast<char *>("IPM.Contact");
	HRESULT hr = HrSetOneProp(msg, &s);
	if (hr != hrSuccess)
		return hr;
	return save_props(props, msg);
}

/**
 * Helper function for GetItem. Saves all properties converted from
 * ICal to MAPI in the MAPI object. Does not call SaveChanges.
 *
 * @param[in] lpPropList list of properties to save in lpMapiProp
 * @param[in] lpMapiProp The MAPI object to save properties in
 *
 * @return MAPI error code
 */
HRESULT vcftomapi_impl::save_props(const std::list<SPropValue> &proplist,
    IMAPIProp *mapiprop)
{
	memory_ptr<SPropValue> propvals;
	HRESULT hr = MAPIAllocateBuffer(proplist.size() * sizeof(SPropValue),
	             &~propvals);
	if (hr != hrSuccess)
		return hr;

	size_t i = 0;
	for (const auto &prop : proplist)
		propvals[i++] = prop;
	auto ret = mapiprop->SetProps(i, propvals, nullptr);
	for (const auto &prop : proplist) {
		if (PROP_TYPE(prop.ulPropTag) == PT_UNICODE)
			MAPIFreeBuffer(prop.Value.lpszW);
		else if (PROP_TYPE(prop.ulPropTag) == PT_STRING8)
			MAPIFreeBuffer(prop.Value.lpszA);
	}
	return ret;
}

} /* namespace */
