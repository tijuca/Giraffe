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

#ifndef ECARCHIVEAWAREMESSAGE_H
#define ECARCHIVEAWAREMESSAGE_H

#include "ECMessage.h"
#include "CommonUtil.h"

#include "mapi_ptr/mapi_memory_ptr.h"
#include "mapi_ptr/mapi_object_ptr.h"

#include <string>

class ECArchiveAwareMsgStore;

class ECArchiveAwareMessage : public ECMessage {
protected:
	/**
	 * \brief Constructor
	 *
	 * \param lpMsgStore	The store owning this message.
	 * \param fNew			Specifies whether the message is a new message.
	 * \param fModify		Specifies whether the message is writable.
	 * \param ulFlags		Flags.
	 */
	ECArchiveAwareMessage(ECArchiveAwareMsgStore *lpMsgStore, BOOL fNew, BOOL fModify, ULONG ulFlags);
	virtual ~ECArchiveAwareMessage();

public:
	/**
	 * \brief Creates a new ECMessage object.
	 *
	 * Use this static method to create a new ECMessage object.
	 *
	 * \param lpMsgStore	The store owning this message.
	 * \param fNew			Specifies whether the message is a new message.
	 * \param fModify		Specifies whether the message is writable.
	 * \param ulFlags		Flags.
	 * \param bEmbedded		Specifies whether the message is embedded.
	 *
	 * \return hrSuccess on success.
	 */
	static HRESULT	Create(ECArchiveAwareMsgStore *lpMsgStore, BOOL fNew, BOOL fModify, ULONG ulFlags, ECMessage **lppMessage);

	virtual HRESULT HrLoadProps();
	virtual HRESULT	HrSetRealProp(SPropValue *lpsPropValue);

	virtual HRESULT OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk);

	virtual HRESULT OpenAttach(ULONG ulAttachmentNum, LPCIID lpInterface, ULONG ulFlags, LPATTACH *lppAttach);
	virtual HRESULT CreateAttach(LPCIID lpInterface, ULONG ulFlags, ULONG *lpulAttachmentNum, LPATTACH *lppAttach);
	virtual HRESULT DeleteAttach(ULONG ulAttachmentNum, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);

	virtual HRESULT ModifyRecipients(ULONG ulFlags, LPADRLIST lpMods);

	virtual HRESULT SaveChanges(ULONG ulFlags);

	static HRESULT	SetPropHandler(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam);

	bool IsLoading() const { return m_bLoading; }

protected:
	virtual HRESULT	HrDeleteRealProp(ULONG ulPropTag, BOOL fOverwriteRO);

private:
	HRESULT MapNamedProps();
	std::string CreateErrorBodyUtf8(HRESULT hResult);

private:
	bool	m_bLoading;

	bool	m_bNamedPropsMapped;
	PROPMAP_START
	PROPMAP_DEF_NAMED_ID(ARCHIVE_STORE_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(ARCHIVE_ITEM_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(STUBBED)
	PROPMAP_DEF_NAMED_ID(DIRTY)
	PROPMAP_DEF_NAMED_ID(ORIGINAL_SOURCE_KEY)

	typedef mapi_memory_ptr<SPropValue> SPropValuePtr;
	SPropValuePtr	m_ptrStoreEntryIDs;
	SPropValuePtr	m_ptrItemEntryIDs;

	enum eMode {
		MODE_UNARCHIVED,	// Not archived
		MODE_ARCHIVED,		// Archived and not stubbed
		MODE_STUBBED,		// Archived and stubbed
		MODE_DIRTY			// Archived and modified saved message
	};
	eMode	m_mode;
	bool	m_bChanged;

	typedef mapi_object_ptr<ECMessage, IID_ECMessage>	ECMessagePtr;
	ECMessagePtr	m_ptrArchiveMsg;
};

class ECArchiveAwareMessageFactory : public IMessageFactory {
public:
	HRESULT Create(ECMsgStore *lpMsgStore, BOOL fNew, BOOL fModify, ULONG ulFlags, BOOL bEmbedded, ECMAPIProp *lpRoot, ECMessage **lppMessage) const;
};

#endif // ndef ECARCHIVEAWAREMESSAGE_H
