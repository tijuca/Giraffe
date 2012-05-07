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

#ifndef ustringutil_INCLUDED
#define ustringutil_INCLUDED

#include "ZarafaCode.h"
#include <string>

#if HAVE_ICU
#include <unicode/coll.h>
#include <unicode/sortkey.h>
typedef Locale ECLocale;
typedef CollationKey ECSortKey;
#else

//typedef locale_t ECLocale;
class ECLocale {
public:
	ECLocale();
	ECLocale(int category, const char *locale);
	ECLocale(const ECLocale &other);
	~ECLocale();

	ECLocale &operator=(const ECLocale &other);
	void swap(ECLocale &other);

	operator const locale_t&() const { return m_locale; }

private:
	locale_t	m_locale;

	int			m_category;
	std::string	m_localeid;
};


class ECSortKey {
public:
	ECSortKey(const unsigned char *lpSortData, unsigned int cbSortData);
	ECSortKey(const ECSortKey &other);
	~ECSortKey();

	ECSortKey& operator=(const ECSortKey &other);
	int compareTo(const ECSortKey &other) const;

private:
	const unsigned char *m_lpSortData;
	unsigned int m_cbSortData;
};

#endif

// us-ascii strings
const char* str_ifind(const char *haystack, const char *needle);

// Current locale strings
bool str_equals(const char *s1, const char *s2, const ECLocale &locale);
bool str_iequals(const char *s1, const char *s2, const ECLocale &locale);
bool str_startswith(const char *s1, const char *s2, const ECLocale &locale);
bool str_istartswith(const char *s1, const char *s2, const ECLocale &locale);
int str_compare(const char *s1, const char *s2, const ECLocale &locale);
int str_icompare(const char *s1, const char *s2, const ECLocale &locale);
bool str_contains(const char *haystack, const char *needle, const ECLocale &locale);
bool str_icontains(const char *haystack, const char *needle, const ECLocale &locale);

// Wide character strings
bool wcs_equals(const wchar_t *s1, const wchar_t *s2, const ECLocale &locale);
bool wcs_iequals(const wchar_t *s1, const wchar_t *s2, const ECLocale &locale);
bool wcs_startswith(const wchar_t *s1, const wchar_t *s2, const ECLocale &locale);
bool wcs_istartswith(const wchar_t *s1, const wchar_t *s2, const ECLocale &locale);
int wcs_compare(const wchar_t *s1, const wchar_t *s2, const ECLocale &locale);
int wcs_icompare(const wchar_t *s1, const wchar_t *s2, const ECLocale &locale);
bool wcs_contains(const wchar_t *haystack, const wchar_t *needle, const ECLocale &locale);
bool wcs_icontains(const wchar_t *haystack, const wchar_t *needle, const ECLocale &locale);

// UTF-8 strings
bool u8_equals(const char *s1, const char *s2, const ECLocale &locale);
bool u8_iequals(const char *s1, const char *s2, const ECLocale &locale);
bool u8_startswith(const char *s1, const char *s2, const ECLocale &locale);
bool u8_istartswith(const char *s1, const char *s2, const ECLocale &locale);
int u8_compare(const char *s1, const char *s2, const ECLocale &locale);
int u8_icompare(const char *s1, const char *s2, const ECLocale &locale);
bool u8_contains(const char *haystack, const char *needle, const ECLocale &locale);
bool u8_icontains(const char *haystack, const char *needle, const ECLocale &locale);

unsigned u8_ncpy(const char *src, unsigned n, std::string *lpstrDest);
unsigned u8_cappedbytes(const char *s, unsigned max);
unsigned u8_len(const char *s);

ECLocale createLocaleFromName(const char *lpszLocale);
ECRESULT LocaleIdToLCID(const char *lpszLocaleID, ULONG *lpulLcid);
ECRESULT LCIDToLocaleId(ULONG ulLcid, const char **lppszLocaleID);
ECRESULT LocaleIdToLocaleName(const char *lpszLocaleID, const char **lppszLocaleName);

void createSortKeyData(const char *s, int nCap, const ECLocale &locale, unsigned int *lpcbKey, unsigned char **lppKey);
void createSortKeyData(const wchar_t *s, int nCap, const ECLocale &locale,unsigned int *lpcbKey, unsigned char **lppKey);
void createSortKeyDataFromUTF8(const char *s, int nCap, const ECLocale &locale, unsigned int *lpcbKey, unsigned char **lppKey);
ECSortKey createSortKeyFromUTF8(const char *s, int nCap, const ECLocale &locale);

int compareSortKeys(unsigned int cbKey1, const unsigned char *lpKey1, unsigned int cbKey2, const unsigned char *lpKey2);


#endif // ndef ustringutil_INCLUDED
