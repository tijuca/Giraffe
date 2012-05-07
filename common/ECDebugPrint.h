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

#ifndef ECDebugPrint_INCLUDED
#define ECDebugPrint_INCLUDED

// @todo: Cleanup 'n' Document

#include <mapidefs.h>
#include "stringutil.h"
#include "ECDebug.h"

#include <boost/type_traits/is_convertible.hpp>
#include <boost/utility/enable_if.hpp>

class ECDebugPrintBase {
public:
	enum DerefMode {
		Deref,
		NoDeref
	};
};


template<typename string_type, ECDebugPrintBase::DerefMode deref_mode>
class ECDebugPrint;

namespace details {

	template<typename string_type>
	class conversion_helpers {
	};

	template<>
	class conversion_helpers<std::string> {
	public:
		static std::string& convert_from(std::string &s)	{return s;}
		static std::string  convert_from(const std::wstring &s);

		static std::string	stringify(unsigned int x) {
			return ::stringify(x, true) + "(" + ::stringify(x, false) + ")";
		}

		static std::string	stringify(LPCVOID lpVoid);

		static std::string  bin2hex(ULONG cbData, LPBYTE lpData) {
			return ::bin2hex(cbData, lpData);
		}

		static const std::string strNULL;
		static const std::string strCOMMA;
	};

	template<>
	class conversion_helpers<std::wstring> {
	public:
		static std::wstring  convert_from(const std::string &s);
		static std::wstring& convert_from(std::wstring &s)	{return s;}

		static std::wstring	stringify(unsigned int x) {
			return ::wstringify(x, true) + L"(" + ::wstringify(x, false) + L")";
		}

		static std::wstring	stringify(LPCVOID lpVoid);

		static std::wstring  bin2hex(ULONG cbData, LPBYTE lpData) {
			return ::bin2hexw(cbData, lpData);
		}

		static const std::wstring strNULL;
		static const std::wstring strCOMMA;
	};


	template<typename string_type, typename T, ECDebugPrintBase::DerefMode deref_mode>
	class defaultPrinter {
	};

	template<typename string_type, typename T, ECDebugPrintBase::DerefMode deref_mode>
	class defaultPrinter<string_type, T*, deref_mode> {
		T* m_lpa;
	public:
		defaultPrinter(T* lpa): m_lpa(lpa) {}
		string_type toString() const {
			if (deref_mode == ECDebugPrintBase::NoDeref || m_lpa == NULL)
				return ECDebugPrint<string_type, deref_mode>::toString(reinterpret_cast<LPVOID>(m_lpa));
			else
				return ECDebugPrint<string_type, ECDebugPrintBase::NoDeref>::toString(*m_lpa);	// Never dereference twice
		}
	};

	template<typename string_type, typename T, ECDebugPrintBase::DerefMode deref_mode>
	class defaultPrinter<string_type, std::basic_string<T>, deref_mode > {
		const std::basic_string<T> &m_str;
	public:
		defaultPrinter(const std::basic_string<T> &str): m_str(str) {}
		string_type toString() const {
			return conversion_helpers<string_type>::convert_from(m_str);
		}
	};

	template<ECDebugPrintBase::DerefMode deref_mode, typename string_type, typename T>
	defaultPrinter<string_type, T, deref_mode> makeDefaultPrinter(T& a) {
		return defaultPrinter<string_type, T, deref_mode>(a);
	}



	template<typename string_type, typename T1, typename T2, ECDebugPrintBase::DerefMode deref_mode>
	class defaultPrinter2 {
	};

	template<typename string_type, typename T1, typename T2, ECDebugPrintBase::DerefMode deref_mode>
	class defaultPrinter2<string_type, T1*, T2**, deref_mode> {
		T1* m_lpa1;
		T2** m_lppa2;
	public:
		defaultPrinter2(T1* lpa1, T2** lppa2): m_lpa1(lpa1), m_lppa2(lppa2) {}
		string_type toString() const { 
			if (deref_mode == ECDebugPrintBase::NoDeref || m_lpa1 == NULL || m_lppa2 == NULL)
				return "(" + ECDebugPrint<string_type, deref_mode>::toString(reinterpret_cast<LPVOID>(m_lpa1)) +
					   "," + ECDebugPrint<string_type, deref_mode>::toString(reinterpret_cast<LPVOID>(m_lppa2)) + ")";
			else
				return ECDebugPrint<string_type, ECDebugPrintBase::NoDeref>::toString(*m_lpa1, *m_lppa2);
		}
	};

	template<ECDebugPrintBase::DerefMode deref_mode, typename string_type, typename T1, typename T2>
	defaultPrinter2<string_type, T1, T2, deref_mode> makeDefaultPrinter2(T1& a1, T2& a2) {
		return defaultPrinter2<string_type, T1, T2, deref_mode>(a1, a2);
	}

} // namespace details


template<typename string_type, ECDebugPrintBase::DerefMode deref_mode>
class ECDebugPrint : public ECDebugPrintBase {
public:
	typedef details::conversion_helpers<string_type>	helpers;

	static string_type& toString(string_type &str) {
		return str;
	}

	template <typename T>
	static string_type toString(T& a, typename boost::disable_if<boost::is_convertible<T, LPUNKNOWN> >::type* = 0) {
		return details::makeDefaultPrinter<deref_mode, string_type>(a).toString();
	}

	template <typename T1, typename T2>
	static string_type toString(T1& a1, T2& a2) {
		return details::makeDefaultPrinter2<deref_mode, string_type>(a1, a2).toString();
	}


	static string_type toString(unsigned int ulValue) {
		return helpers::stringify(ulValue);
	}

	static string_type toString(WORD ulValue) {
		return helpers::stringify(ulValue);
	}

	static string_type toString(HRESULT hr) {
		return helpers::stringify(hr);
	}

	static string_type toString(const IID &refiid) {
		return helpers::convert_from(DBGGUIDToString(refiid));
	}

	static string_type toString(LPCIID lpciid) {
		return lpciid == NULL ? helpers::strNULL : toString(*lpciid);
	}

	static string_type toString(LPGUID lpguid) {
		return lpguid == NULL ? helpers::strNULL : helpers::convert_from(DBGGUIDToString(*lpguid));
	}

	static string_type toString(LPVOID lpVoid) {
		return helpers::stringify(lpVoid);
	}

	static string_type toString(LPCVOID lpVoid) {
		return helpers::stringify(lpVoid);
	}

	static string_type toString(LPUNKNOWN lpUnk) {
		return helpers::stringify(reinterpret_cast<LPVOID>(lpUnk));
	}

	static string_type toString(LPSPropValue lpsPropValue) {
		return helpers::convert_from(PropNameFromPropArray(1, lpsPropValue));
	}

	static string_type toString(ULONG *lpcValues, LPSPropValue *lppPropArray) {
		return toString(reinterpret_cast<LPVOID>(lpcValues)) + helpers::strCOMMA + toString(reinterpret_cast<LPVOID>(lppPropArray));
	}
	static string_type toString(ULONG *lpcbData, LPBYTE *lppData) {
		return toString(reinterpret_cast<LPVOID>(lpcbData)) + helpers::strCOMMA + toString(reinterpret_cast<LPVOID>(lppData));
	}

	static string_type toString(ULONG cValues, LPSPropValue lpPropArray) {
		return helpers::convert_from(PropNameFromPropArray(cValues, lpPropArray));
	}

	static string_type toString(LPSPropTagArray lpPropTagArray) {
		if (lpPropTagArray == NULL)
			return helpers::strNULL;
		return helpers::convert_from(PropNameFromPropTagArray(lpPropTagArray));
	}

	static string_type toString(ULONG ciidExclude, LPCIID rgiidExclude) {
		return string_type();
	}

	static string_type toString(ULONG cNames, LPMAPINAMEID *ppNames) {
		return helpers::convert_from(MapiNameIdListToString(cNames, ppNames));
	}

	static string_type toString(ULONG cbData, LPBYTE lpData) {
		return helpers::bin2hex(cbData, lpData);
	}

	static string_type toString(LPSRestriction lpRestrict) {
		return helpers::convert_from(RestrictionToString(lpRestrict));
	}

	static string_type toString(LPSSortOrderSet lpSortOrderSet) {
		return helpers::convert_from(SortOrderSetToString(lpSortOrderSet));
	}

	static string_type toString(LPMAPIERROR lpError) {
		return toString(reinterpret_cast<LPVOID>(lpError));
	}

	static string_type toString(LPSRowSet lpRowSet) {
		return helpers::convert_from(RowSetToString(lpRowSet));
	}

	static string_type toString(LPSPropProblemArray lpPropblemArray) {
		return helpers::convert_from(ProblemArrayToString(lpPropblemArray));
	}

	static string_type toString(ULONG cbEntryID, LPENTRYID lpEntryID) {
		return helpers::bin2hex(cbEntryID, (LPBYTE)lpEntryID);
	}

	static string_type toString(MAPIUID uid) {
		return helpers::bin2hex(sizeof(MAPIUID), (LPBYTE)&uid);
	}

	static string_type toString(LPTSTR data) {
		return (char *)data;
	}

	static string_type toString(LARGE_INTEGER li) {
		return helpers::convert_from(stringify_int64(li.QuadPart));
	}

	static string_type toString(ULARGE_INTEGER li) {
		return helpers::convert_from(stringify_int64(li.QuadPart));
	}

	static string_type toString(STATSTG) {
		return helpers::convert_from(std::string("STATSTG (not implemented)"));
	}

};

#endif // ndef ECDebugPrint_INCLUDED
