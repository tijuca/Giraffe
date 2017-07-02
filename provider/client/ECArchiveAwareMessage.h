/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include <kopano/zcdefs.h>
#include <kopano/memory.hpp>
#include "ECMessage.h"
#include <kopano/CommonUtil.h>
#include <string>

class ECArchiveAwareMsgStore;

class _kc_export_dycast ECArchiveAwareMessage _kc_final : public ECMessage {
protected:
	/**
	 * \param lpMsgStore	The store owning this message.
	 * \param fNew			Specifies whether the message is a new message.
	 * \param fModify		Specifies whether the message is writable.
	 * \param ulFlags		Flags.
	 */
	_kc_hidden ECArchiveAwareMessage(ECArchiveAwareMsgStore *, BOOL fNew, BOOL modify, ULONG flags);
	_kc_hidden virtual ~ECArchiveAwareMessage(void) _kc_impdtor;

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
	_kc_hidden static HRESULT Create(ECArchiveAwareMsgStore *store, BOOL fNew, BOOL modify, ULONG flags, ECMessage **);
	_kc_hidden virtual HRESULT HrLoadProps(void);
	_kc_hidden virtual HRESULT HrSetRealProp(const SPropValue *);
	_kc_hidden virtual HRESULT OpenProperty(ULONG proptag, LPCIID lpiid, ULONG iface_opts, ULONG flags, LPUNKNOWN *);
	_kc_hidden virtual HRESULT OpenAttach(ULONG atnum, LPCIID iface, ULONG flags, LPATTACH *ret);
	_kc_hidden virtual HRESULT CreateAttach(LPCIID iface, ULONG flags, ULONG *atnum, LPATTACH *ret);
	_kc_hidden virtual HRESULT DeleteAttach(ULONG atnum, ULONG ui_param, LPMAPIPROGRESS, ULONG flags);
	_kc_hidden virtual HRESULT ModifyRecipients(ULONG flags, const ADRLIST *mods);
	_kc_hidden virtual HRESULT SaveChanges(ULONG flags);
	_kc_hidden static HRESULT SetPropHandler(ULONG proptag, void *prov, const SPropValue *, void *param);
	_kc_hidden bool IsLoading(void) const { return m_bLoading; }

protected:
	_kc_hidden virtual HRESULT HrDeleteRealProp(ULONG proptag, BOOL overwrite_ro);

private:
	_kc_hidden HRESULT MapNamedProps(void);
	_kc_hidden HRESULT CreateInfoMessage(const SPropTagArray *deleteprop, const std::string &bodyhtml);
	_kc_hidden std::string CreateErrorBodyUtf8(HRESULT);
	_kc_hidden std::string CreateOfflineWarnBodyUtf8(void);

	bool	m_bLoading;

	bool	m_bNamedPropsMapped;
	PROPMAP_DECL()
	PROPMAP_DEF_NAMED_ID(ARCHIVE_STORE_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(ARCHIVE_ITEM_ENTRYIDS)
	PROPMAP_DEF_NAMED_ID(STUBBED)
	PROPMAP_DEF_NAMED_ID(DIRTY)
	PROPMAP_DEF_NAMED_ID(ORIGINAL_SOURCE_KEY)

	typedef KCHL::memory_ptr<SPropValue> SPropValuePtr;
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

	typedef KCHL::object_ptr<ECMessage, IID_ECMessage> ECMessagePtr;
	ECMessagePtr	m_ptrArchiveMsg;
};

class ECArchiveAwareMessageFactory _kc_final : public IMessageFactory {
public:
	HRESULT Create(ECMsgStore *lpMsgStore, BOOL fNew, BOOL fModify, ULONG ulFlags, BOOL bEmbedded, ECMAPIProp *lpRoot, ECMessage **lppMessage) const;
};

#endif // ndef ECARCHIVEAWAREMESSAGE_H
