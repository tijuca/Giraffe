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
#include "ECFifoBuffer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECFifoBuffer::ECFifoBuffer(size_type ulMaxSize)
	: m_ulMaxSize(ulMaxSize)
	, m_bClosed(false)
{ 
	pthread_mutex_init(&m_hMutex, NULL);
	pthread_cond_init(&m_hCondNotFull, NULL);
	pthread_cond_init(&m_hCondNotEmpty, NULL);
}

ECFifoBuffer::~ECFifoBuffer()
{
	pthread_mutex_destroy(&m_hMutex);
	pthread_cond_destroy(&m_hCondNotFull);
	pthread_cond_destroy(&m_hCondNotEmpty);
}

/**
 * Write data into the FIFO.
 *
 * @param[in]	lpBuf			Pointer to the data being written.
 * @param[in]	cbBuf			The amount of data to write (in bytes).
 * @param[out]	lpcbWritten		The amount of data actually written.
 * @param[in]	ulTimeoutMs		The maximum amount that this function may block.
 *
 * @retval	erSuccess					The data was successfully written.
 * @retval	ZARAFA_E_INVALID_PARAMETER	lpBuf is NULL.
 * @retval	ZARAFA_E_NOT_ENOUGH_MEMORY	There was not enough memory available to store the data.
 * @retval	ZARAFA_E_TIMEOUT			Not all data was writting within the specified time limit.
 *										The amount of data that was written is returned in lpcbWritten.
 * @retval	ZARAFA_E_CALL_FAILED		The buffer was closed prior to this call.
 */
ECRESULT ECFifoBuffer::Write(const void *lpBuf, size_type cbBuf, unsigned int ulTimeoutMs, size_type *lpcbWritten)
{
	ECRESULT			er = erSuccess;
	size_type			cbWritten = 0;
	struct timespec		deadline = {0};
	const unsigned char	*lpData = reinterpret_cast<const unsigned char*>(lpBuf);

	if (lpBuf == NULL)
		return ZARAFA_E_INVALID_PARAMETER;

	if (cbBuf == 0) {
		if (lpcbWritten)
			*lpcbWritten = 0;
		return erSuccess;
	}

	if (ulTimeoutMs > 0)
		deadline = GetDeadline(ulTimeoutMs);

	pthread_mutex_lock(&m_hMutex);

	while (cbWritten < cbBuf) {
		while (IsFull()) {
			if (m_bClosed) {
				er = ZARAFA_E_CALL_FAILED;
				goto exit;
			}

			if (ulTimeoutMs > 0) {
				if (pthread_cond_timedwait(&m_hCondNotFull, &m_hMutex, &deadline) == ETIMEDOUT) {
					er = ZARAFA_E_TIMEOUT;
					goto exit;
				}
			} else
				pthread_cond_wait(&m_hCondNotFull, &m_hMutex);
		}

		const size_type cbNow = std::min(cbBuf - cbWritten, m_ulMaxSize - m_storage.size());
		try {
			m_storage.insert(m_storage.end(), lpData + cbWritten, lpData + cbWritten + cbNow);
		} catch (const std::bad_alloc &) {
			er = ZARAFA_E_NOT_ENOUGH_MEMORY;
			goto exit;
		}
		pthread_cond_signal(&m_hCondNotEmpty);
		cbWritten += cbNow;
	}

exit:
	pthread_mutex_unlock(&m_hMutex);

	if (lpcbWritten && (er == erSuccess || er == ZARAFA_E_TIMEOUT))
		*lpcbWritten = cbWritten;

	return er;
}

/**
 * Read data from the FIFO.
 *
 * @param[in,out]	lpBuf			Pointer to where the data should be stored.
 * @param[in]		cbBuf			The amount of data to read (in bytes).
 * @param[out]		lpcbWritten		The amount of data actually read.
 * @param[in]		ulTimeoutMs		The maximum amount that this function may block.
 *
 * @retval	erSuccess					The data was successfully written.
 * @retval	ZARAFA_E_INVALID_PARAMETER	lpBuf is NULL.
 * @retval	ZARAFA_E_TIMEOUT			Not all data was writting within the specified time limit.
 *										The amount of data that was written is returned in lpcbWritten.
 */
ECRESULT ECFifoBuffer::Read(void *lpBuf, size_type cbBuf, unsigned int ulTimeoutMs, size_type *lpcbRead)
{
	ECRESULT		er = erSuccess;
	size_type		cbRead = 0;
	struct timespec	deadline = {0};
	unsigned char	*lpData = reinterpret_cast<unsigned char*>(lpBuf);

	if (lpBuf == NULL)
		return ZARAFA_E_INVALID_PARAMETER;

	if (cbBuf == 0) {
		if (lpcbRead)
			*lpcbRead = 0;
		return erSuccess;
	}

	if (ulTimeoutMs > 0)
		deadline = GetDeadline(ulTimeoutMs);
	
	pthread_mutex_lock(&m_hMutex);

	while (cbRead < cbBuf) {
		while (IsEmpty()) {
			if (IsClosed())
				goto exit;

			if (ulTimeoutMs > 0) {
				if (pthread_cond_timedwait(&m_hCondNotEmpty, &m_hMutex, &deadline) == ETIMEDOUT) {
					er = ZARAFA_E_TIMEOUT;
					goto exit;
				}
			} else
				pthread_cond_wait(&m_hCondNotEmpty, &m_hMutex);
		}

		const size_type cbNow = std::min(cbBuf - cbRead, m_storage.size());
		storage_type::iterator iEndNow = m_storage.begin() + cbNow;
		std::copy(m_storage.begin(), iEndNow, lpData + cbRead);
		m_storage.erase(m_storage.begin(), iEndNow);
		pthread_cond_signal(&m_hCondNotFull);
		cbRead += cbNow;
	}

exit:
	pthread_mutex_unlock(&m_hMutex);

	if (lpcbRead && (er == erSuccess || er == ZARAFA_E_TIMEOUT))
		*lpcbRead = cbRead;

	return er;
}

/**
 * Close a buffer.
 * This causes new writes to the buffer to fail with ZARAFA_E_CALL_FAILED and all
 * (pending) reads on the buffer to return immediately.
 *
 * @retval	erSucces (never fails)
 */
ECRESULT ECFifoBuffer::Close()
{
	pthread_mutex_lock(&m_hMutex);
	m_bClosed = true;
	pthread_cond_signal(&m_hCondNotEmpty);
	pthread_cond_signal(&m_hCondNotFull);
	pthread_mutex_unlock(&m_hMutex);
	return erSuccess;
}
