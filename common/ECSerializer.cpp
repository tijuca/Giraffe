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

#include <arpa/inet.h>

#include "ECFifoBuffer.h"
#include "ECSerializer.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


ECStreamSerializer::ECStreamSerializer(IStream *lpBuffer)
{
	SetBuffer(lpBuffer);
}

ECStreamSerializer::~ECStreamSerializer()
{
}

ECRESULT ECStreamSerializer::SetBuffer(void *lpBuffer)
{
	m_lpBuffer = (IStream *)lpBuffer;
	return erSuccess;
}

ECRESULT ECStreamSerializer::Write(const void *ptr, size_t size, size_t nmemb)
{
	ECRESULT er = erSuccess;
	ULONG cbWritten = 0;
	char tmp[8];

	if (ptr == NULL)
		return ZARAFA_E_INVALID_PARAMETER;

	switch (size) {
	case 1:
		er = m_lpBuffer->Write(ptr, nmemb, &cbWritten);
		break;
	case 2:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			((short *)tmp)[0] = htons(((const short *)ptr)[x]);
			er = m_lpBuffer->Write(tmp, size, &cbWritten);
		}
		break;
	case 4:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			((int *)tmp)[0] = htonl(((const int *)ptr)[x]);
			er = m_lpBuffer->Write(tmp, size, &cbWritten);
		}
		break;
	case 8:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			((long long *)tmp)[0] = htonll(((const long long *)ptr)[x]);
			er = m_lpBuffer->Write(tmp, size, &cbWritten);
		}
		break;
	default:
		er = ZARAFA_E_INVALID_PARAMETER;
		break;
	}

	return er;
}

ECRESULT ECStreamSerializer::Read(void *ptr, size_t size, size_t nmemb)
{
	ECRESULT er = erSuccess;
	ULONG cbRead = 0;

	if (ptr == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = m_lpBuffer->Read(ptr, size * nmemb, &cbRead);
	if (er != erSuccess)
		goto exit;
	if (cbRead != size * nmemb) {
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

	switch (size) {
	case 1: break;
	case 2:
		for (size_t x = 0; x < nmemb; ++x)
			((short *)ptr)[x] = ntohs(((short *)ptr)[x]);
		break;
	case 4:
		for (size_t x = 0; x < nmemb; ++x)
			((int *)ptr)[x] = ntohl(((int *)ptr)[x]);
		break;
	case 8:
		for (size_t x = 0; x < nmemb; ++x)
			((long long *)ptr)[x] = ntohll(((long long *)ptr)[x]);
		break;
	default:
		er = ZARAFA_E_INVALID_PARAMETER;
		break;
	}

exit:
	return er;
}

ECRESULT ECStreamSerializer::Skip(size_t size, size_t nmemb)
{
	ECRESULT er = erSuccess;
	LARGE_INTEGER ff;

	ff.QuadPart = size*nmemb;
	er = m_lpBuffer->Seek(ff, SEEK_CUR, NULL);

	return er;
}

ECRESULT ECStreamSerializer::Flush()
{
	LARGE_INTEGER zero = {{0,0}};
	return m_lpBuffer->Seek(zero, SEEK_END, NULL);
}


ECFifoSerializer::ECFifoSerializer(ECFifoBuffer *lpBuffer)
{
	SetBuffer(lpBuffer);
}

ECFifoSerializer::~ECFifoSerializer()
{
	if (m_lpBuffer)
		m_lpBuffer->Close();
}

ECRESULT ECFifoSerializer::SetBuffer(void *lpBuffer)
{
	m_lpBuffer = (ECFifoBuffer *)lpBuffer;
	return erSuccess;
}

ECRESULT ECFifoSerializer::Write(const void *ptr, size_t size, size_t nmemb)
{
	ECRESULT er = erSuccess;
	char tmp[8];

	if (ptr == NULL)
		return ZARAFA_E_INVALID_PARAMETER;

	switch (size) {
	case 1:
		er = m_lpBuffer->Write(ptr, nmemb, STR_DEF_TIMEOUT, NULL);
		break;
	case 2:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			((short *)tmp)[0] = htons(((const short *)ptr)[x]);
			er = m_lpBuffer->Write(tmp, size, STR_DEF_TIMEOUT, NULL);
		}
		break;
	case 4:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			((int *)tmp)[0] = htonl(((const int *)ptr)[x]);
			er = m_lpBuffer->Write(tmp, size, STR_DEF_TIMEOUT, NULL);
		}
		break;
	case 8:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			((long long *)tmp)[0] = htonll(((const long long *)ptr)[x]);
			er = m_lpBuffer->Write(tmp, size, STR_DEF_TIMEOUT, NULL);
		}
		break;
	default:
		er = ZARAFA_E_INVALID_PARAMETER;
		break;
	}

	return er;
}

ECRESULT ECFifoSerializer::Read(void *ptr, size_t size, size_t nmemb)
{
	ECRESULT er = erSuccess;
	ECFifoBuffer::size_type cbRead = 0;

	if (ptr == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = m_lpBuffer->Read(ptr, size * nmemb, STR_DEF_TIMEOUT, &cbRead);
	if (er != erSuccess)
		goto exit;
	if (cbRead !=  size * nmemb) {
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

	switch (size) {
	case 1: break;
	case 2:
		for (size_t x = 0; x < nmemb; ++x)
			((short *)ptr)[x] = ntohs(((short *)ptr)[x]);
		break;
	case 4:
		for (size_t x = 0; x < nmemb; ++x) 
			((int *)ptr)[x] = ntohl(((int *)ptr)[x]);
		break;
	case 8:
		for (size_t x = 0; x < nmemb; ++x)
			((long long *)ptr)[x] = ntohll(((long long *)ptr)[x]);
		break;
	default:
		er = ZARAFA_E_INVALID_PARAMETER;
		break;
	}

exit:
	return er;
}

ECRESULT ECFifoSerializer::Skip(size_t size, size_t nmemb)
{
	ECRESULT er = erSuccess;
	char *buf = NULL;

	try {
		buf = new char[size*nmemb];
	} catch(...) {
		er = ZARAFA_E_NOT_ENOUGH_MEMORY;
	}
	if (er != erSuccess)
		goto exit;

	er = Read(buf, size, nmemb);

exit:
	if (buf)
		delete [] buf;

	return er;
}

ECRESULT ECFifoSerializer::Flush()
{
	ECRESULT er = erSuccess;
	size_t cbRead = 0;
	char buf[1024];
	
	while(true) {
		er = m_lpBuffer->Read(buf, 1024, STR_DEF_TIMEOUT, &cbRead);
		if (er != erSuccess)
			goto exit;

		if(cbRead < 1024)
			break;
	}

exit:
	return er;
}
