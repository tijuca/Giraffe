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

#ifndef WSMessageStreamImporter_INCLUDED
#define WSMessageStreamImporter_INCLUDED

#include "zcdefs.h"
#include "ECUnknown.h"
#include "mapi_ptr.h"
#include "soapStub.h"
#include "ECFifoBuffer.h"
#include "ECThreadPool.h"
#include "WSTransport.h"

class ECMAPIFolder;
typedef mapi_object_ptr<WSTransport> WSTransportPtr;

class WSMessageStreamImporter;

/**
 * This class represents the data sink into which the stream data can be written.
 * It is returned from WSMessageStreamImporter::StartTransfer.
 */
class WSMessageStreamSink _final : public ECUnknown
{
public:
	static HRESULT Create(ECFifoBuffer *lpFifoBuffer, ULONG ulTimeout, WSMessageStreamImporter *lpImporter, WSMessageStreamSink **lppSink);
	HRESULT Write(LPVOID lpData, ULONG cbData);

private:
	WSMessageStreamSink(ECFifoBuffer *lpFifoBuffer, ULONG ulTimeout, WSMessageStreamImporter *lpImporter);
	~WSMessageStreamSink();

private:
	ECFifoBuffer	*m_lpFifoBuffer;
	WSMessageStreamImporter *m_lpImporter;
	ULONG			m_ulTimeout;
};

typedef mapi_object_ptr<WSMessageStreamSink> WSMessageStreamSinkPtr;


/**
 * This class is used to perform an message stream import to the server.
 * The actual import call to the server is deferred until StartTransfer is called. When that
 * happens, the actual transfer is done on a worker thread so the calling thread can start writing
 * data in the returned WSMessageStreamSink. Once the returned stream is deleted, GetAsyncResult can
 * be used to wait for the worker and obtain its return values.
 */
class WSMessageStreamImporter _final : public ECUnknown, private ECWaitableTask
{
public:
	static HRESULT Create(ULONG ulFlags, ULONG ulSyncId, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG cbFolderEntryID, LPENTRYID lpFolderEntryID, bool bNewMessage, LPSPropValue lpConflictItems, WSTransport *lpTransport, WSMessageStreamImporter **lppStreamImporter);

	HRESULT StartTransfer(WSMessageStreamSink **lppSink);
	HRESULT GetAsyncResult(HRESULT *lphrResult);

private:
	WSMessageStreamImporter(ULONG ulFlags, ULONG ulSyncId, const entryId &sEntryId, const entryId &sFolderEntryId, bool bNewMessage, const propVal &sConflictItems, WSTransport *lpTransport, ULONG ulBufferSize, ULONG ulTimeout);
	~WSMessageStreamImporter();

	void run();

	static void  *StaticMTOMReadOpen(struct soap *soap, void *handle, const char *id, const char *type, const char *description);
	static size_t StaticMTOMRead(struct soap *soap, void *handle, char *buf, size_t len);
	static void   StaticMTOMReadClose(struct soap *soap, void *handle);

	void  *MTOMReadOpen(struct soap *soap, void *handle, const char *id, const char *type, const char *description);
	size_t MTOMRead(struct soap *soap, void *handle, char *buf, size_t len);
	void   MTOMReadClose(struct soap *soap, void *handle);


private:
	ULONG m_ulFlags;
	ULONG m_ulSyncId;
	entryId m_sEntryId;
	entryId m_sFolderEntryId;
	bool m_bNewMessage;
	propVal m_sConflictItems;
	WSTransportPtr m_ptrTransport;

	HRESULT m_hr;
	ECFifoBuffer m_fifoBuffer;
	ECThreadPool m_threadPool;
	ULONG m_ulTimeout;
};

typedef mapi_object_ptr<WSMessageStreamImporter> WSMessageStreamImporterPtr;

#endif // ndef WSMessageStreamImporter_INCLUDED
