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
#include "utfutil.h"

#include "utf8iter.h"
#include "utf32iter.h"

#include <unicode/ustring.h>

#if (U_ICU_VERSION_MAJOR_NUM < 4) || ((U_ICU_VERSION_MAJOR_NUM == 4) && (U_ICU_VERSION_MINOR_NUM < 2))
#if (U_ICU_VERSION_MAJOR_NUM > 3) || ((U_ICU_VERSION_MAJOR_NUM == 3) && (U_ICU_VERSION_MINOR_NUM >= 6))

UnicodeString UTF8ToUnicode(const char *utf8)
{
	UnicodeString result;
	const int32_t length = strlen(utf8);
	int32_t capacity = length > 512 ? length : 512;
	do {
		UChar *utf16 = result.getBuffer(capacity);
		int32_t length16;
		UErrorCode errorCode = U_ZERO_ERROR;
		u_strFromUTF8WithSub(utf16, result.getCapacity(), &length16,
			utf8, length,
			0xfffd,  // Substitution character.
			NULL,    // Don't care about number of substitutions.
			&errorCode);
		result.releaseBuffer(length16);
		if(errorCode == U_BUFFER_OVERFLOW_ERROR) {
			capacity = length16 + 1;  // +1 for the terminating NUL.
			continue;
		} else if(U_FAILURE(errorCode)) {
			result.setToBogus();
		}
		break;
	} while(TRUE);
	return result;
}

UnicodeString UTF32ToUnicode(const UChar32 *utf32)
{
	UnicodeString result;
	const int32_t length = wcslen((wchar_t*)utf32);
	int32_t capacity = length > 512 ? length + (length >> 4) + 4 : 512;
	do {
		UChar *utf16 = result.getBuffer(capacity);
		int32_t length16;
		UErrorCode errorCode = U_ZERO_ERROR;
		u_strFromUTF32(utf16, result.getCapacity(), &length16,
			utf32, length,
			&errorCode);
		result.releaseBuffer(length16);
		if(errorCode == U_BUFFER_OVERFLOW_ERROR) {
			capacity = length16 + 1;  // +1 for the terminating NUL.
			continue;
		} else if(U_FAILURE(errorCode)) {
			result.setToBogus();
		}
		break;
	} while(TRUE);
	return result;
}

#else   	// ICU < 3.6

#include <memory>
#include <unicode/chariter.h>
UnicodeString UTF8ToUnicode(const char *utf8)
{
	UnicodeString result;
	const int32_t length = strlen(utf8);
	int32_t capacity = length > 512 ? length : 512;
	int32_t index = 0;
	
	while (index < length) {
		UChar32 cp;
		U8_NEXT(utf8, index, length, cp);
		if (cp == U_SENTINEL) {
			result.append('?');
			break;
		}
		result.append(cp);
	}

	return result;
}

UnicodeString UTF32ToUnicode(const UChar32 *utf32)
{
	UnicodeString result;
	const int32_t length = wcslen((wchar_t*)utf32);
	int32_t capacity = length > 512 ? length + (length >> 4) + 4 : 512;

	for (int32_t index = 0; index < length; ++index)
		result.append(utf32[index]);

	return result;
}

#endif	// ICU >= 3.6
#endif	// ICU >= 4.2
