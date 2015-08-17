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

#ifndef ECFIFOBUFFER_H
#define ECFIFOBUFFER_H

#include "zcdefs.h"
#include <deque>
#include <pthread.h>

#include "ZarafaCode.h"

// Thread safe buffer for FIFO operations
class ECFifoBuffer _final {
public:
	typedef std::deque<unsigned char>	storage_type;
	typedef storage_type::size_type		size_type;
	enum close_flags { cfRead = 1, cfWrite = 2 };

public:
	ECFifoBuffer(size_type ulMaxSize = 131072);
	~ECFifoBuffer();
	
	ECRESULT Write(const void *lpBuf, size_type cbBuf, unsigned int ulTimeoutMs, size_type *lpcbWritten);
	ECRESULT Read(void *lpBuf, size_type cbBuf, unsigned int ulTimeoutMs, size_type *lpcbRead);
	ECRESULT Close(close_flags flags);
	ECRESULT Flush();

	bool IsClosed(ULONG flags) const;
	bool IsEmpty() const;
	bool IsFull() const;
	unsigned long Size();
	
private:
	// prohibit copy
	ECFifoBuffer(const ECFifoBuffer &);
	ECFifoBuffer& operator=(const ECFifoBuffer &);
	
private:
	storage_type	m_storage;
	size_type		m_ulMaxSize;
	bool			m_bReaderClosed;
	bool            m_bWriterClosed;

	pthread_mutex_t	m_hMutex;
	pthread_cond_t	m_hCondNotEmpty;
	pthread_cond_t	m_hCondNotFull;
	pthread_cond_t	m_hCondFlushed;
};


// inlines
inline bool ECFifoBuffer::IsClosed(ULONG flags) const {
	switch (flags) {
	case cfRead:
		return m_bReaderClosed;
	case cfWrite:
		return m_bWriterClosed;
	case cfRead|cfWrite:
		return m_bReaderClosed && m_bWriterClosed;
	default:
		ASSERT(FALSE);
		return false;
	}
}

inline bool ECFifoBuffer::IsEmpty() const {
	return m_storage.empty();
}

inline bool ECFifoBuffer::IsFull() const {
	return m_storage.size() == m_ulMaxSize;
}

inline unsigned long ECFifoBuffer::Size() {
	return m_storage.size();
}

#endif // ndef ECFIFOBUFFER_H
