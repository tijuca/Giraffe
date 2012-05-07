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

// Standard Conversion Library (Currently for Python only)
#ifndef SCL_H
#define SCL_H

// Get Py_ssize_t for older versions of python
#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
# define PY_SSIZE_T_MAX INT_MAX
# define PY_SSIZE_T_MIN INT_MIN
#endif

namespace priv {
	/**
	 * Default version of conv_out, which is intended to convert one script value
	 * to a native value.
	 * This version will always generate a compile error as the actual conversions
	 * should be performed by specializations for the specific native types.
	 *
	 * @tparam		_Type	The type of the resulting value.
	 * @param[in]	Value	The scripted value to convert.
	 * @param[in]	flags	Allowed values:
	 *						@remark @c MAPI_UNICODE If the data is a string, it's a wide character string
	 * @param[out]	result	The native value.
	 */
	template <typename _Type>
	void conv_out(PyObject* value, LPVOID /*lpBase*/, ULONG /*ulFlags*/, _Type* result) {
		// Just generate an error here
		value = result;
	}

	/**
	 * Specialization for extracting a string (TCHAR*) from a script value.
	 *
	 * @tparam		_Type	The type of the resulting value.
	 * @param[in]	Value	The scripted value to convert.
	 * @param[out]	result	The native value.
	 */
	template <>
	void conv_out<LPTSTR>(PyObject* value, LPVOID lpBase, ULONG ulFlags, LPTSTR *lppResult) {
		if(value == Py_None) {
			*lppResult = NULL;
		} else {
			if ((ulFlags & MAPI_UNICODE) == 0)
				*(LPSTR*)lppResult = PyString_AsString(value);
			else {
				int len = PyUnicode_GetSize(value);
				MAPIAllocateMore((len + 1) * sizeof(WCHAR), lpBase, (LPVOID*)lppResult);
				len = PyUnicode_AsWideChar((PyUnicodeObject*)value, *(LPWSTR*)lppResult, len);
				(*(LPWSTR*)lppResult)[len] = L'\0';
			}
		}
	}

	/**
	 * Specialization for extracting an unsigned int from a script value.
	 *
	 * @tparam		_Type	The type of the resulting value.
	 * @param[in]	Value	The scripted value to convert.
	 * @param[out]	result	The native value.
	 */
	template <>
	void conv_out<unsigned int>(PyObject* value, LPVOID /*lpBase*/, ULONG /*ulFlags*/, unsigned int *lpResult) {
		*lpResult = (unsigned int)PyLong_AsUnsignedLong(value);
	}

	/**
	 * Specialization for extracting a boolean from a script value.
	 *
	 * @tparam		_Type	The type of the resulting value.
	 * @param[in]	Value	The scripted value to convert.
	 * @param[out]	result	The native value.
	 */
	template <>
	void conv_out<bool>(PyObject* value, LPVOID /*lpBase*/, ULONG /*ulFlags*/, bool *lpResult) {
		*lpResult = (bool)PyLong_AsUnsignedLong(value);
	}

	/**
	 * Specialization for extracting a long long from a script value.
	 *
	 * @tparam		_Type	The type of the resulting value.
	 * @param[in]	Value	The scripted value to convert.
	 * @param[out]	result	The native value.
	 */
	template <>
	void conv_out<long long>(PyObject* value, LPVOID /*lpBase*/, ULONG /*ulFlags*/, long  long *lpResult) {
		*lpResult = (long long)PyLong_AsUnsignedLong(value);
	}

	/**
	 * Specialization for extracting an ECENTRYID from a script value.
	 *
	 * @tparam		_Type	The type of the resulting value.
	 * @param[in]	Value	The scripted value to convert.
	 * @param[out]	result	The native value.
	 */
	template <>
	void conv_out<ECENTRYID>(PyObject* value, LPVOID lpBase, ULONG /*ulFlags*/, ECENTRYID *lpResult) {
		char *data;
		Py_ssize_t size;
		if(value == Py_None) {
			lpResult->cb = 0;
			lpResult->lpb = NULL;
		} else {
			PyString_AsStringAndSize(value, &data, &size);
			lpResult->cb = size;
			MAPIAllocateMore(size, lpBase, (LPVOID*)&lpResult->lpb);
			memcpy(lpResult->lpb, data, size);
		}
	}

	/**
	 * Specialization for extracting an objectclass_t from a script value.
	 * @note In the script language an objectclass_t will be an unsigned int value.
	 *
	 * @tparam		_Type	The type of the resulting value.
	 * @param[in]	Value	The scripted value to convert.
	 * @param[out]	result	The native value.
	 */
	template <>
	void conv_out<objectclass_t>(PyObject* value, LPVOID /*lpBase*/, ULONG /*ulFlags*/, objectclass_t *lpResult) {
		*lpResult = (objectclass_t)PyLong_AsUnsignedLong(value);
	}	
} // namspace priv

/**
 * This is the default convert function for converting a script value to
 * a native value that's part of a struct (on both sides). The actual conversion
 * is delegated to a specialization of the private::conv_out template.
 *
 * @tparam			_ObjType	The type of the structure containing the values that
 * 								are to be converted.
 * @tparam			_MemType	The type of the member for which this particular instantiation
 * 								is intended.
 * @tparam			_Member		The data member pointer that points to the actual field
 * 								for which this particula instantiation is intended.
 * @param[in,out]	lpObj		The native object whos members will be populated with
 * 								values converted from the scripted object.
 * @param[in]		elem		The scipted object, whose values will be converted to
 * 								a native representation.
 * @param[in]		lpszMember	The name of the member in the scripted object.
 * @param[in]		flags		Allowed values:
 *								@remark @c MAPI_UNICODE If the data is a string, it's a wide character string
 */
template <typename _ObjType, typename _MemType, _MemType(_ObjType::*_Member)>
void conv_out_default(_ObjType *lpObj, PyObject *elem, const char *lpszMember, LPVOID lpBase, ULONG ulFlags) {
	PyObject *value = PyObject_GetAttrString(elem, const_cast<char*>(lpszMember));	// Older versions of python might expect a non-const char pointer.
	if (PyErr_Occurred())
		return;

	priv::conv_out(value, lpBase, ulFlags, &(lpObj->*_Member));
	Py_DECREF(value);
}

/**
 * This structure is used to create a list of items that need to be converted from
 * their scripted representation to their native representation.
 */
template <typename _ObjType>
struct conv_out_info {
	typedef void(*conv_out_func_t)(_ObjType*, PyObject*, const char*, LPVOID lpBase, ULONG ulFlags);
	
	conv_out_func_t		conv_out_func;
	const char*			membername;
};

/**
 * This function processes an array of conv_out_info structures, effectively
 * performing the complete conversion.
 *
 * @tparam		_	ObjType		The type of the structure containing the values that
 * 								are to be converted. This is determined automatically.
 * @tparam			N			The size of the array. This is determined automatically.
 * @param[in]		array		The array containing the conv_out_info structures that
 * 								define the conversion operations.
 * @param[in]		flags		Allowed values:
 *								@remark @c MAPI_UNICODE If the data is a string, it's a wide character string
 */
template <typename _ObjType, size_t N>
void process_conv_out_array(_ObjType *lpObj, PyObject *elem, const conv_out_info<_ObjType> (&array)[N], LPVOID lpBase, ULONG ulFlags) {
	for (size_t n = 0; !PyErr_Occurred() && n < N; ++n)
		array[n].conv_out_func(lpObj, elem, array[n].membername, lpBase, ulFlags);
}

#endif // ndef SCL_H
