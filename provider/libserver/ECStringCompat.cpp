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
#include "ECStringCompat.h"

#include "soapH.h"

#include "charset/convert.h"
#include "charset/utf8string.h"
#include "utf8.h"

using namespace std;

char *ECStringCompat::WTF1252_to_WINDOWS1252(soap *lpsoap, const char *szWTF1252, convert_context *lpConverter)
{
	if (!szWTF1252)
		return NULL;

	string str1252;
	str1252.reserve(strlen(szWTF1252));

	while (*szWTF1252) {
		utf8::uint32_t cp = utf8::unchecked::next(szWTF1252);

		// Since the string was originally windows-1252, all code points
		// should be in the range 0 <= cp < 256.
		str1252.append(1, cp < 256 ? cp : '?');
	}

	return s_strcpy(lpsoap, str1252.c_str());
}

char *ECStringCompat::WTF1252_to_UTF8(soap *lpsoap, const char *szWTF1252, convert_context *lpConverter)
{
	if (!szWTF1252)
		return NULL;

	string str1252;
	str1252.reserve(strlen(szWTF1252));

	while (*szWTF1252) {
		utf8::uint32_t cp = utf8::unchecked::next(szWTF1252);

		// Since the string was originally windows-1252, all code points
		// should be in the range 0 <= cp < 256.
		str1252.append(1, cp < 256 ? cp : '?');
	}

	// Now convert the windows-1252 string to proper UTF8.
	utf8string strUTF8;
	if (lpConverter)
		strUTF8 = lpConverter->convert_to<utf8string>(str1252, rawsize(str1252), "WINDOWS-1252");
	else
		strUTF8 = convert_to<utf8string>(str1252, rawsize(str1252), "WINDOWS-1252");

	return s_strcpy(lpsoap, strUTF8.c_str());
}

char *ECStringCompat::UTF8_to_WTF1252(soap *lpsoap, const char *szUTF8, convert_context *lpConverter)
{
	if (!szUTF8)
		return NULL;

	string str1252;
	if (lpConverter)
		str1252 = lpConverter->convert_to<string>("WINDOWS-1252//TRANSLIT", szUTF8, rawsize(szUTF8), "UTF-8");
	else
		str1252 = convert_to<string>("WINDOWS-1252//TRANSLIT", szUTF8, rawsize(szUTF8), "UTF-8");
	
	string strWTF1252;
	back_insert_iterator<string> iWTF1252 = back_inserter(strWTF1252);

	strWTF1252.reserve(string::size_type(str1252.size() * 1.3));	// It will probably grow a bit, 1.3 is just a guess.
	for (string::const_iterator i1252 = str1252.begin(); i1252 != str1252.end(); ++i1252)
		utf8::unchecked::append((unsigned char)*i1252, iWTF1252);

	return s_strcpy(lpsoap, strWTF1252.c_str());
}

ECStringCompat::ECStringCompat(unsigned int ulClientCaps)
: m_ulClientCaps(ulClientCaps)
, m_lpConverter(ulClientCaps & ZARAFA_CAP_UNICODE ? NULL : new convert_context)
{ }

ECStringCompat::~ECStringCompat()
{
	// deleting a NULL ptr is allowed.
	delete m_lpConverter;
}


ECRESULT FixPropEncoding(struct soap *soap, const ECStringCompat &stringCompat, enum EncodingFixDirection type, struct propVal *lpProp, bool bNoTagUpdate)
{
	if (PROP_TYPE(lpProp->ulPropTag) == PT_STRING8 || PROP_TYPE(lpProp->ulPropTag) == PT_UNICODE) {
		if (type == In) {
			lpProp->Value.lpszA = stringCompat.to_UTF8(soap, lpProp->Value.lpszA);
			if (!bNoTagUpdate)
				lpProp->ulPropTag = CHANGE_PROP_TYPE(lpProp->ulPropTag, PT_UNICODE);
		} else {
			lpProp->Value.lpszA = stringCompat.from_UTF8(soap, lpProp->Value.lpszA);
			if (!bNoTagUpdate)
				lpProp->ulPropTag = CHANGE_PROP_TYPE(lpProp->ulPropTag, stringCompat.string_prop_type());
		}
	} else if (PROP_TYPE(lpProp->ulPropTag) == PT_MV_STRING8 || PROP_TYPE(lpProp->ulPropTag) == PT_MV_UNICODE) {
		if (type == In) {
			for (int i = 0; i < lpProp->Value.mvszA.__size; ++i)
				lpProp->Value.mvszA.__ptr[i] = stringCompat.to_UTF8(soap, lpProp->Value.mvszA.__ptr[i]);
			if (!bNoTagUpdate)
				lpProp->ulPropTag = CHANGE_PROP_TYPE(lpProp->ulPropTag, PT_MV_UNICODE);
		} else {
			for (int i = 0; i < lpProp->Value.mvszA.__size; ++i)
				lpProp->Value.mvszA.__ptr[i] = stringCompat.from_UTF8(soap, lpProp->Value.mvszA.__ptr[i]);
			if (!bNoTagUpdate)
				lpProp->ulPropTag = CHANGE_PROP_TYPE(lpProp->ulPropTag, stringCompat.string_prop_type() | MV_FLAG);
		}
	}

	return erSuccess;
}

ECRESULT FixRestrictionEncoding(struct soap *soap, const ECStringCompat &stringCompat, enum EncodingFixDirection type, struct restrictTable *lpRestrict)
{
	ECRESULT er = erSuccess;

	ASSERT(soap != NULL);
	ASSERT(lpRestrict != NULL);

	switch (lpRestrict->ulType) {
	case RES_AND:
		for (ULONG i = 0; er == erSuccess && i < lpRestrict->lpAnd->__size; ++i)
			er = FixRestrictionEncoding(soap, stringCompat, type, lpRestrict->lpAnd->__ptr[i]);
		break;
	case RES_BITMASK:
		break;
	case RES_COMMENT:
		for (int i = 0; er == erSuccess && i < lpRestrict->lpComment->sProps.__size; ++i)
			er = FixPropEncoding(soap, stringCompat, type, &lpRestrict->lpComment->sProps.__ptr[i]);
		if (er == erSuccess)
			er = FixRestrictionEncoding(soap, stringCompat, type, lpRestrict->lpComment->lpResTable);
		break;
	case RES_COMPAREPROPS:
		break;
	case RES_CONTENT:
		er = FixPropEncoding(soap, stringCompat, type, lpRestrict->lpContent->lpProp);
		break;
	case RES_EXIST:
		break;
	case RES_NOT:
		er = FixRestrictionEncoding(soap, stringCompat, type, lpRestrict->lpNot->lpNot);
		break;
	case RES_OR:
		for (ULONG i = 0; er == erSuccess && i < lpRestrict->lpOr->__size; ++i)
			er = FixRestrictionEncoding(soap, stringCompat, type, lpRestrict->lpOr->__ptr[i]);
		break;
	case RES_PROPERTY:
		er = FixPropEncoding(soap, stringCompat, type, lpRestrict->lpProp->lpProp);
		break;
	case RES_SIZE:
		break;
	case RES_SUBRESTRICTION:
		er = FixRestrictionEncoding(soap, stringCompat, type, lpRestrict->lpSub->lpSubObject);
		break;
	default:
		return ZARAFA_E_INVALID_TYPE;
	}

	return er;
}

ECRESULT FixRowSetEncoding(struct soap *soap, const ECStringCompat &stringCompat, enum EncodingFixDirection type, struct rowSet *lpRowSet)
{
	ECRESULT er = erSuccess;

	for (int i = 0; er == erSuccess && i < lpRowSet->__size; ++i) {
		for (int j = 0; er == erSuccess && j < lpRowSet->__ptr[i].__size; ++j) {
			er = FixPropEncoding(soap, stringCompat, type, &lpRowSet->__ptr[i].__ptr[j], true);
		}
	}

	return er;
}

ECRESULT FixUserEncoding(struct soap *soap, const ECStringCompat &stringCompat, enum EncodingFixDirection type, struct user *lpUser)
{
	ECRESULT er = erSuccess;

	if (type == In) {
		lpUser->lpszFullName = stringCompat.to_UTF8(soap, lpUser->lpszFullName);
		lpUser->lpszMailAddress = stringCompat.to_UTF8(soap, lpUser->lpszMailAddress);
		lpUser->lpszPassword = stringCompat.to_UTF8(soap, lpUser->lpszPassword);
		lpUser->lpszServername = stringCompat.to_UTF8(soap, lpUser->lpszServername);
		lpUser->lpszUsername = stringCompat.to_UTF8(soap, lpUser->lpszUsername);

		if (lpUser->lpsPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpUser->lpsPropmap->__size; ++i)
				lpUser->lpsPropmap->__ptr[i].lpszValue = stringCompat.to_UTF8(soap, lpUser->lpsPropmap->__ptr[i].lpszValue);

		if (lpUser->lpsMVPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpUser->lpsMVPropmap->__size; ++i)
				for (int j = 0; er == erSuccess && j < lpUser->lpsMVPropmap->__ptr[i].sValues.__size; ++j)
					lpUser->lpsMVPropmap->__ptr[i].sValues.__ptr[j] = stringCompat.to_UTF8(soap, lpUser->lpsMVPropmap->__ptr[i].sValues.__ptr[j]);
	} else {
		lpUser->lpszFullName = stringCompat.from_UTF8_cpy(soap, lpUser->lpszFullName);
		lpUser->lpszMailAddress = stringCompat.from_UTF8_cpy(soap, lpUser->lpszMailAddress);
		lpUser->lpszPassword = stringCompat.from_UTF8_cpy(soap, lpUser->lpszPassword);
		lpUser->lpszServername = stringCompat.from_UTF8_cpy(soap, lpUser->lpszServername);
		lpUser->lpszUsername = stringCompat.from_UTF8_cpy(soap, lpUser->lpszUsername);

		if (lpUser->lpsPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpUser->lpsPropmap->__size; ++i)
				lpUser->lpsPropmap->__ptr[i].lpszValue = stringCompat.from_UTF8_cpy(soap, lpUser->lpsPropmap->__ptr[i].lpszValue);

		if (lpUser->lpsMVPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpUser->lpsMVPropmap->__size; ++i)
				for (int j = 0; er == erSuccess && j < lpUser->lpsMVPropmap->__ptr[i].sValues.__size; ++j)
					lpUser->lpsMVPropmap->__ptr[i].sValues.__ptr[j] = stringCompat.from_UTF8_cpy(soap, lpUser->lpsMVPropmap->__ptr[i].sValues.__ptr[j]);
	}

	return er;
}

ECRESULT FixGroupEncoding(struct soap *soap, const ECStringCompat &stringCompat, enum EncodingFixDirection type, struct group *lpGroup)
{
	ECRESULT er = erSuccess;

	if (type == In) {
		lpGroup->lpszFullname = stringCompat.to_UTF8(soap, lpGroup->lpszFullname);
		lpGroup->lpszFullEmail = stringCompat.to_UTF8(soap, lpGroup->lpszFullEmail);
		lpGroup->lpszGroupname = stringCompat.to_UTF8(soap, lpGroup->lpszGroupname);

		if (lpGroup->lpsPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpGroup->lpsPropmap->__size; ++i)
				lpGroup->lpsPropmap->__ptr[i].lpszValue = stringCompat.to_UTF8(soap, lpGroup->lpsPropmap->__ptr[i].lpszValue);

		if (lpGroup->lpsMVPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpGroup->lpsMVPropmap->__size; ++i)
				for (int j = 0; er == erSuccess && j < lpGroup->lpsMVPropmap->__ptr[i].sValues.__size; ++j)
					lpGroup->lpsMVPropmap->__ptr[i].sValues.__ptr[j] = stringCompat.to_UTF8(soap, lpGroup->lpsMVPropmap->__ptr[i].sValues.__ptr[j]);
	} else {
		lpGroup->lpszFullname = stringCompat.from_UTF8_cpy(soap, lpGroup->lpszFullname);
		lpGroup->lpszFullEmail = stringCompat.from_UTF8_cpy(soap, lpGroup->lpszFullEmail);
		lpGroup->lpszGroupname = stringCompat.from_UTF8_cpy(soap, lpGroup->lpszGroupname);

		if (lpGroup->lpsPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpGroup->lpsPropmap->__size; ++i)
				lpGroup->lpsPropmap->__ptr[i].lpszValue = stringCompat.from_UTF8_cpy(soap, lpGroup->lpsPropmap->__ptr[i].lpszValue);

		if (lpGroup->lpsMVPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpGroup->lpsMVPropmap->__size; ++i)
				for (int j = 0; er == erSuccess && j < lpGroup->lpsMVPropmap->__ptr[i].sValues.__size; ++j)
					lpGroup->lpsMVPropmap->__ptr[i].sValues.__ptr[j] = stringCompat.from_UTF8_cpy(soap, lpGroup->lpsMVPropmap->__ptr[i].sValues.__ptr[j]);
	}

	return er;
}

ECRESULT FixCompanyEncoding(struct soap *soap, const ECStringCompat &stringCompat, enum EncodingFixDirection type, struct company *lpCompany)
{
	ECRESULT er = erSuccess;

	if (type == In) {
		lpCompany->lpszCompanyname = stringCompat.to_UTF8(soap, lpCompany->lpszCompanyname);
		lpCompany->lpszServername = stringCompat.to_UTF8(soap, lpCompany->lpszServername);

		if (lpCompany->lpsPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpCompany->lpsPropmap->__size; ++i)
				lpCompany->lpsPropmap->__ptr[i].lpszValue = stringCompat.to_UTF8(soap, lpCompany->lpsPropmap->__ptr[i].lpszValue);

		if (lpCompany->lpsMVPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpCompany->lpsMVPropmap->__size; ++i)
				for (int j = 0; er == erSuccess && j < lpCompany->lpsMVPropmap->__ptr[i].sValues.__size; ++j)
					lpCompany->lpsMVPropmap->__ptr[i].sValues.__ptr[j] = stringCompat.to_UTF8(soap, lpCompany->lpsMVPropmap->__ptr[i].sValues.__ptr[j]);
	} else {
		lpCompany->lpszCompanyname = stringCompat.from_UTF8_cpy(soap, lpCompany->lpszCompanyname);
		lpCompany->lpszServername = stringCompat.from_UTF8_cpy(soap, lpCompany->lpszServername);

		if (lpCompany->lpsPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpCompany->lpsPropmap->__size; ++i)
				lpCompany->lpsPropmap->__ptr[i].lpszValue = stringCompat.from_UTF8_cpy(soap, lpCompany->lpsPropmap->__ptr[i].lpszValue);

		if (lpCompany->lpsMVPropmap)
			for (unsigned i = 0; er == erSuccess && i < lpCompany->lpsMVPropmap->__size; ++i)
				for (int j = 0; er == erSuccess && j < lpCompany->lpsMVPropmap->__ptr[i].sValues.__size; ++j)
					lpCompany->lpsMVPropmap->__ptr[i].sValues.__ptr[j] = stringCompat.from_UTF8_cpy(soap, lpCompany->lpsMVPropmap->__ptr[i].sValues.__ptr[j]);
	}

	return er;
}

ECRESULT FixNotificationsEncoding(struct soap *soap, const ECStringCompat &stringCompat, struct notificationArray *notifications)
{
	ECRESULT er = erSuccess;

	for (unsigned i = 0; i < notifications->__size; ++i) {
		switch (notifications->__ptr[i].ulEventType) {
			case fnevNewMail:
				notifications->__ptr[i].newmail->lpszMessageClass = stringCompat.from_UTF8(soap, notifications->__ptr[i].newmail->lpszMessageClass);
				break;

			case fnevTableModified:
				if (notifications->__ptr[i].tab->pRow)
					for (int j = 0; er == erSuccess && j < notifications->__ptr[i].tab->pRow->__size; ++j)
						er = FixPropEncoding(soap, stringCompat, Out, notifications->__ptr[i].tab->pRow->__ptr + j, true);
				break;

			default:
				break;			
		}
	}

	return er;
}
