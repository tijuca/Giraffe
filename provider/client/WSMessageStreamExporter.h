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

#ifndef WSMessageStreamExporter_INCLUDED
#define WSMessageStreamExporter_INCLUDED

#include "ECUnknown.h"
#include "soapStub.h"
#include "mapi_ptr.h"

#include <string>
#include <map>

class WSTransport;
class WSSerializedMessage;

/**
 * This object encapsulates an set of exported streams. It allows the user to request each individual stream. The
 * streams must be requested in the correct sequence.
 */
class WSMessageStreamExporter : public ECUnknown
{
public:
	static HRESULT Create(ULONG ulOffset, ULONG ulCount, const messageStreamArray &streams, WSTransport *lpTransport, WSMessageStreamExporter **lppStreamExporter);

	bool IsDone() const;
	HRESULT GetSerializedMessage(ULONG ulIndex, WSSerializedMessage **lppSerializedMessage);

private:
	WSMessageStreamExporter();
	~WSMessageStreamExporter();

	// Inhibit copying
	WSMessageStreamExporter(const WSMessageStreamExporter&);
	WSMessageStreamExporter& operator=(const WSMessageStreamExporter&);

private:
	typedef mapi_object_ptr<WSTransport> WSTransportPtr;

	struct StreamInfo {
		std::string	id;
		unsigned long	cbPropVals;
		SPropArrayPtr	ptrPropVals;
	};
	typedef std::map<ULONG, StreamInfo*>	StreamInfoMap;


	ULONG			m_ulExpectedIndex;
	ULONG			m_ulMaxIndex;
	WSTransportPtr	m_ptrTransport;
	StreamInfoMap	m_mapStreamInfo;
};

typedef mapi_object_ptr<WSMessageStreamExporter> WSMessageStreamExporterPtr;

#endif // ndef ECMessageStreamExporter_INCLUDED
