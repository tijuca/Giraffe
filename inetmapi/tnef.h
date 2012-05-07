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

#ifndef TNEF_H
#define TNEF_H

#include <list>

#include "mapidefs.h"

// We loosely follow the MS class ITNEF with the following main exceptions:
//
// - No special RTF handling
// - No recipient handling
// - No problem reporting
//

#pragma pack(push,1)
struct AttachRendData {
    unsigned short usType;
    unsigned int ulPosition;
    unsigned short usWidth;
    unsigned short usHeight;
    unsigned int ulFlags;
};
#pragma pack(pop)

#define AttachTypeFile 0x0001
#define AttachTypeOle 0x0002

class ECTNEF {
public:
	ECTNEF(ULONG ulFlags, IMessage *lpMessage, IStream *lpStream);
	virtual ~ECTNEF();
    
	// Add properties to the TNEF stream from the message
	virtual HRESULT AddProps(ULONG ulFlags, LPSPropTagArray lpPropList);
    
	// Extract properties from the TNEF stream to the message
	virtual HRESULT ExtractProps(ULONG ulFlags, LPSPropTagArray lpPropList);
    
	// Set some extra properties (warning!, make sure that this pointer stays in memory until Finish() is called!)
	virtual HRESULT SetProps(ULONG cValues, LPSPropValue lpProps);

	// Add other components (currently only attachments supported)
	virtual HRESULT FinishComponent(ULONG ulFlags, ULONG ulComponentID, LPSPropTagArray lpPropList);
    
	// Finish up and write the data (write stream with TNEF_ENCODE, write to message with TNEF_DECODE)
	virtual HRESULT Finish();
    
private:
	HRESULT HrReadDWord(IStream *lpStream, ULONG *ulData);
	HRESULT HrReadWord(IStream *lpStream, unsigned short *ulData);
	HRESULT HrReadByte(IStream *lpStream, unsigned char *ulData);
	HRESULT HrReadData(IStream *lpStream, char *lpData, ULONG ulLen);
    
	HRESULT HrWriteDWord(IStream *lpStream, ULONG ulData);
	HRESULT HrWriteWord(IStream *lpStream, unsigned short ulData);
	HRESULT HrWriteByte(IStream *lpStream, unsigned char ulData);
	HRESULT HrWriteData(IStream *lpStream, char *lpDAta, ULONG ulLen);
    
	HRESULT HrWritePropStream(IStream *lpStream, std::list<SPropValue *> &proplist);
	HRESULT HrWriteSingleProp(IStream *lpStream, LPSPropValue lpProp);
	HRESULT HrReadPropStream(char *lpBuffer, ULONG ulSize, std::list<SPropValue *> &proplist);
	HRESULT HrReadSingleProp(char *lpBuffer, ULONG ulSize, ULONG *lpulRead, LPSPropValue *lppProp);

	HRESULT HrGetChecksum(IStream *lpStream, ULONG *lpulChecksum);
	ULONG GetChecksum(char *lpData, unsigned int ulLen);
	
	HRESULT HrWriteBlock(IStream *lpDest, IStream *lpSrc, ULONG ulBlockID, ULONG ulLevel);
	HRESULT HrWriteBlock(IStream *lpDest, char *lpData, unsigned int ulSize, ULONG ulBlockID, ULONG ulLevel);

    HRESULT HrReadStream(IStream *lpStream, void *lpBase, BYTE **lppData, ULONG *lpulSize);
	
	IStream *m_lpStream;
	IMessage *m_lpMessage;
	ULONG ulFlags;
    
	// Accumulator for properties from AddProps and SetProps
	std::list<SPropValue *> lstProps;

	struct tnefattachment {
		std::list<SPropValue *> lstProps;
		ULONG size;
		BYTE *data;
		AttachRendData rdata;
	};
	std::list<tnefattachment*> lstAttachments;

	void FreeAttachmentData(tnefattachment* lpTnefAtt);

};

// Flags for constructor
#define TNEF_ENCODE			0x00000001
#define TNEF_DECODE			0x00000002

// Flags for FinishComponent
#define TNEF_COMPONENT_MESSAGE 		0x00001000
#define TNEF_COMPONENT_ATTACHMENT 	0x00002000

// Flags for ExtractProps
#define TNEF_PROP_EXCLUDE	0x00000003
#define TNEF_PROP_INCLUDE	0x00000004

// Flags for AddProps
// #define TNEF_PROP_EXCLUDE 		0x00000001
// #define TNEF_PROP_INCLUDE 		0x00000002

#define TNEF_SIGNATURE		0x223e9f78


#endif
