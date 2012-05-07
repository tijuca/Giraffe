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
#include "base64.h"

#include <string>
#include <charset/convert.h>
#include <assert.h>

/**
 * Check if the provided password is crypted.
 * 
 * Crypted passwords have the format "{N}:<crypted password>, with N being the encryption algorithm. 
 * Currently only algorithm number 1 and 2 are supported:
 * 1: base64-of-XOR-A5 of windows-1252 encoded data
 * 2: base64-of-XOR-A5 of UTF-8 encoded data
 * 
 * @param[in]	strCrypted
 * 					The string to test.
 * 
 * @return	boolean
 * @retval	true	The provided string was encrypted.
 * @retval	false 	The provided string was not encrypted.
 */
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

bool SymmetricIsCrypted(const std::string &strCrypted)
{
	std::string strPrefix = strCrypted.substr(0,4);

	if(strPrefix == "{1}:" || strPrefix == "{2}:")
		return true;
	else
		return false;
}

/**
 * Check if the provided password is crypted.
 * 
 * Crypted passwords have the format "{N}:<crypted password>, with N being the encryption algorithm. 
 * Currently only algorithm number 1 and 2 are supported:
 * 1: base64-of-XOR-A5 of windows-1252 encoded data
 * 2: base64-of-XOR-A5 of UTF-8 encoded data
 * 
 * @param[in]	strCrypted
 * 					The wide character string to test.
 * 
 * @return	boolean
 * @retval	true	The provided string was encrypted.
 * @retval	false 	The provided string was not encrypted.
 */
bool SymmetricIsCrypted(const std::wstring &wstrCrypted)
{
	std::wstring wstrPrefix = wstrCrypted.substr(0,4);

	if(wstrPrefix == L"{1}:" || wstrPrefix == L"{2}:")
		return true;
	else
		return false;
}

/**
 * Crypt a pasword using algorithm 2.
 * 
 * @param[in]	strPlain
 * 					The string to encrypt.
 * 
 * @return	The encrypted string encoded in UTF-8.
 */
std::string SymmetricCrypt(const std::wstring &strPlain)
{
	std::string strUTF8ed = convert_to<std::string>("UTF-8", strPlain, rawsize(strPlain), CHARSET_WCHAR);
	std::string strXORed;

	// Do the XOR 0xa5
	for(unsigned int i = 0; i < strUTF8ed.size(); i++) {
		strXORed.append(1, (unsigned char)(((unsigned char)strUTF8ed.at(i)) ^ 0xa5));
	}

	// Do the base64 encode
	std::string strBase64 = base64_encode((const unsigned char *)strXORed.c_str(), strXORed.size());

	// Prefix with {1}:
	std::string strCrypted = (std::string)"{2}:" + strBase64;

	return strCrypted;
}

/**
 * Crypt a pasword using algorithm 2.
 * 
 * @param[in]	strPlain
 * 					The string to encrypt.
 * 
 * @return	The encrypted string encoded in UTF-16/32 (depending on type of wchar_t).
 */
std::wstring SymmetricCryptW(const std::wstring &strPlain)
{
	return convert_to<std::wstring>(SymmetricCrypt(strPlain));
}

/**
 * Decrypt the crypt data.
 * 
 * Depending on the N value, the password is decrypted using algorithm 1 or 2.
 * 
 * @param[in]	ulAlg
 * 					The number selecting the algorithm. (1 or 2)
 * @param[in]	strXORed
 * 					The binary data to decrypt.
 * 
 * @return	The decrypted password encoded in UTF-8.
 */
static std::string SymmetricDecryptBlob(unsigned int ulAlg, const std::string &strXORed)
{
	std::string strRaw;
	
	assert(ulAlg == 1 || ulAlg == 2);

	for(unsigned int i = 0; i < strXORed.size(); i++) {
		strRaw.append(1, (unsigned char)(((unsigned char)strXORed.at(i)) ^ 0xa5));
	}
	
	// Check the encoding algorithm. If it equals 1, the raw data is windows-1252.
	// Otherwise it must be 2, which means it's allready UTF-8.
	if (ulAlg == 1)
		strRaw = convert_to<std::string>("UTF-8", strRaw, rawsize(strRaw), "WINDOWS-1252");
	
	return strRaw;
}

/**
 * Decrypt an encrypted password.
 * 
 * Depending on the N value, the password is decrypted using algorithm 1 or 2.
 * 
 * @param[in]	strCrypted
 * 					The UTF-8 encoded encrypted password to decrypt.
 * 
 * @return	THe decrypted password encoded in UTF-8.
 */
std::string SymmetricDecrypt(const std::string &strCrypted)
{
	if(!SymmetricIsCrypted(strCrypted))
		return "";

	// Remove prefix
	std::string strBase64 = convert_to<std::string>(strCrypted.substr(4));
	std::string strXORed = base64_decode(strBase64);
	
	return SymmetricDecryptBlob(strCrypted.at(1) - '0', strXORed);
}

/**
 * Decrypt an encrypted password.
 * 
 * Depending on the N value, the password is decrypted using algorithm 1 or 2.
 * 
 * @param[in]	strCrypted
 * 					The wide character encrypted password to decrypt.
 * 
 * @return	THe decrypted password encoded in UTF-8.
 */
std::string SymmetricDecrypt(const std::wstring &wstrCrypted)
{
	if(!SymmetricIsCrypted(wstrCrypted))
		return "";

	// Remove prefix
	std::string strBase64 = convert_to<std::string>(wstrCrypted.substr(4));
	std::string strXORed = base64_decode(strBase64);

	return SymmetricDecryptBlob(wstrCrypted.at(1) - '0', strXORed);
}

/**
 * Decrypt an encrypted password.
 * 
 * Depending on the N value, the password is decrypted using algorithm 1 or 2.
 * 
 * @param[in]	strCrypted
 * 					The encrypted password to decrypt.
 * 
 * @return	THe decrypted password encoded in in UTF-16/32 (depending on type of wchar_t).
 */
std::wstring SymmetricDecryptW(const std::wstring &wstrCrypted)
{
	const std::string strDecrypted = SymmetricDecrypt(wstrCrypted);
	return convert_to<std::wstring>(strDecrypted, rawsize(strDecrypted), "UTF-8");
}
