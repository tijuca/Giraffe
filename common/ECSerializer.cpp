/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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
static const char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


ECStreamSerializer::ECStreamSerializer(IStream *lpBuffer)
{
	SetBuffer(lpBuffer);
	m_ulRead = m_ulWritten = 0;
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
	union {
		char c[8];
		short s;
		int i;
		long long ll;
	} tmp;

	if (ptr == NULL)
		return ZARAFA_E_INVALID_PARAMETER;

	switch (size) {
	case 1:
		er = m_lpBuffer->Write(ptr, nmemb, &cbWritten);
		break;
	case 2:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			tmp.s = htons(((const short *)ptr)[x]);
			er = m_lpBuffer->Write(&tmp, size, &cbWritten);
		}
		break;
	case 4:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			tmp.i = htonl(((const int *)ptr)[x]);
			er = m_lpBuffer->Write(&tmp, size, &cbWritten);
		}
		break;
	case 8:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			tmp.ll = htonll(((const long long *)ptr)[x]);
			er = m_lpBuffer->Write(&tmp, size, &cbWritten);
		}
		break;
	default:
		er = ZARAFA_E_INVALID_PARAMETER;
		break;
	}
	
	m_ulWritten += size * nmemb;

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

	m_ulRead += cbRead;

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
	char buffer[4096];
	ULONG read = 0;
	size_t total = 0;

	while(total < (nmemb*size)) {
		er = m_lpBuffer->Read(buffer, std::min(sizeof(buffer), (size * nmemb) - total), &read);
		if(er != erSuccess)
			goto exit;
		
		total += read;
	}
	
exit:
	return er;
}

ECRESULT ECStreamSerializer::Flush()
{
	ECRESULT er = erSuccess;
	ULONG cbRead = 0;
	char buf[16384];
	
	while(true) {
		er = m_lpBuffer->Read(buf, sizeof(buf), &cbRead);
		if (er != erSuccess)
			goto exit;

		m_ulRead += cbRead;

		if(cbRead < sizeof(buf))
			break;
	}

exit:
	return er;
}

ECRESULT ECStreamSerializer::Stat(ULONG *lpcbRead, ULONG *lpcbWrite)
{
	if(lpcbRead)
		*lpcbRead = m_ulRead;
		
	if(lpcbWrite)
		*lpcbWrite = m_ulWritten;
		
	return erSuccess;
}

ECFifoSerializer::ECFifoSerializer(ECFifoBuffer *lpBuffer, eMode mode)
{
	SetBuffer(lpBuffer);
	m_mode = mode;
	m_ulRead = m_ulWritten = 0;
}

ECFifoSerializer::~ECFifoSerializer()
{
	if (m_lpBuffer) {
		ECFifoBuffer::close_flags flags = (m_mode == serialize ? ECFifoBuffer::cfWrite : ECFifoBuffer::cfRead);
		m_lpBuffer->Close(flags);
	}
}

ECRESULT ECFifoSerializer::SetBuffer(void *lpBuffer)
{
	m_lpBuffer = (ECFifoBuffer *)lpBuffer;
	return erSuccess;
}

ECRESULT ECFifoSerializer::Write(const void *ptr, size_t size, size_t nmemb)
{
	ECRESULT er = erSuccess;
	union {
		char c[8];
		short s;
		int i;
		long long ll;
	} tmp;

	if (m_mode != serialize)
		return ZARAFA_E_NO_SUPPORT;

	if (ptr == NULL)
		return ZARAFA_E_INVALID_PARAMETER;

	switch (size) {
	case 1:
		er = m_lpBuffer->Write(ptr, nmemb, STR_DEF_TIMEOUT, NULL);
		break;
	case 2:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			tmp.s = htons(((const short *)ptr)[x]);
			er = m_lpBuffer->Write(&tmp, size, STR_DEF_TIMEOUT, NULL);
		}
		break;
	case 4:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			tmp.i = htonl(((const int *)ptr)[x]);
			er = m_lpBuffer->Write(&tmp, size, STR_DEF_TIMEOUT, NULL);
		}
		break;
	case 8:
		for (size_t x = 0; x < nmemb && er == erSuccess; ++x) {
			tmp.ll = htonll(((const long long *)ptr)[x]);
			er = m_lpBuffer->Write(&tmp, size, STR_DEF_TIMEOUT, NULL);
		}
		break;
	default:
		er = ZARAFA_E_INVALID_PARAMETER;
		break;
	}
	
	m_ulWritten += size * nmemb;

	return er;
}

ECRESULT ECFifoSerializer::Read(void *ptr, size_t size, size_t nmemb)
{
	ECRESULT er = erSuccess;
	ECFifoBuffer::size_type cbRead = 0;

	if (m_mode != deserialize) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	if (ptr == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = m_lpBuffer->Read(ptr, size * nmemb, STR_DEF_TIMEOUT, &cbRead);
	if (er != erSuccess)
		goto exit;

	m_ulRead += cbRead;

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
	char buf[16384];
	
	while(true) {
		er = m_lpBuffer->Read(buf, sizeof(buf), STR_DEF_TIMEOUT, &cbRead);
		if (er != erSuccess)
			goto exit;

		m_ulRead += cbRead;

		if(cbRead < sizeof(buf))
			break;
	}

exit:
	return er;
}

ECRESULT ECFifoSerializer::Stat(ULONG *lpcbRead, ULONG *lpcbWrite)
{
	if(lpcbRead)
		*lpcbRead = m_ulRead;
		
	if(lpcbWrite)
		*lpcbWrite = m_ulWritten;
		
	return erSuccess;
}
