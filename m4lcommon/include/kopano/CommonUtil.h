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

#ifndef COMMONUTIL_H
#define COMMONUTIL_H

#include <kopano/zcdefs.h>
#include <vector>
#include <mapidefs.h>
#include <mapix.h>
#include <string>
#include <kopano/ECTags.h>
#include <kopano/memory.hpp>
#include <kopano/ustringutil.h>

// Version of GetClientVersion
#define CLIENT_VERSION_OLK2000			9
#define CLIENT_VERSION_OLK2002			10
#define CLIENT_VERSION_OLK2003			11
#define CLIENT_VERSION_OLK2007			12
#define CLIENT_VERSION_OLK2010			14
#define CLIENT_VERSION_LATEST			CLIENT_VERSION_OLK2010 /* UPDATE ME */

/* darn, no sane place because of depend include on mapidefs.h */
extern _kc_export bool operator==(const SBinary &, const SBinary &) noexcept;
extern _kc_export bool operator<(const SBinary &, const SBinary &) noexcept;

namespace KC {

extern _kc_export const char *GetServerUnixSocket(const char *pref = nullptr);
extern _kc_export HRESULT HrOpenECAdminSession(IMAPISession **, const char *const app_ver, const char *app_misc, const char *path = nullptr, ULONG profflags = 0, const char *sslkey_file = nullptr, const char *sslkey_password = nullptr);
extern _kc_export HRESULT HrOpenECSession(IMAPISession **ses, const char *app_ver, const char *app_misc, const wchar_t *user, const wchar_t *pass, const char *path = nullptr, ULONG profile_flags = 0, const char *sslkey_file = nullptr, const char *sslkey_password = nullptr, const char *profname = nullptr);
extern _kc_export HRESULT HrOpenECPublicStoreOnline(IMAPISession *, IMsgStore **ret);
extern _kc_export HRESULT ECCreateOneOff(const TCHAR * name, const TCHAR * addrtype, const TCHAR * addr, ULONG flags, ULONG *eid_size, LPENTRYID *eid);
extern _kc_export HRESULT ECParseOneOff(const ENTRYID *eid, ULONG eid_size, std::wstring &name, std::wstring &type, std::wstring &addr);
extern _kc_export HRESULT HrNewMailNotification(IMsgStore *, IMessage *);
extern _kc_export HRESULT HrCreateEmailSearchKey(const char *type, const char *addr, ULONG *size, LPBYTE *out);
extern _kc_export HRESULT DoSentMail(IMAPISession *, IMsgStore *, ULONG flags, KCHL::object_ptr<IMessage>);
extern _kc_export HRESULT GetClientVersion(unsigned int *);
extern _kc_export HRESULT OpenSubFolder(LPMDB, const wchar_t *folder, wchar_t psep, bool is_public, bool create_folder, LPMAPIFOLDER *subfolder);
extern _kc_export HRESULT HrOpenDefaultCalendar(LPMDB, LPMAPIFOLDER *default_folder);
extern _kc_export HRESULT HrGetAllProps(IMAPIProp *prop, ULONG flags, ULONG *nvals, LPSPropValue *props);
extern _kc_export HRESULT UnWrapStoreEntryID(ULONG eid_size, const ENTRYID *eid, ULONG *ret_size, ENTRYID **ret);

// Auto-accept settings
extern _kc_export HRESULT HrGetRemoteAdminStore(IMAPISession *, IMsgStore *, LPCTSTR server, ULONG flags, IMsgStore **ret);
extern _kc_export HRESULT GetConfigMessage(LPMDB, const char *msgname, IMessage **msgout);

extern _kc_export HRESULT HrOpenDefaultStore(IMAPISession *, IMsgStore **ret);
extern _kc_export HRESULT HrOpenDefaultStore(IMAPISession *, ULONG flags, IMsgStore **ret);
extern _kc_export HRESULT HrOpenECPublicStore(IMAPISession *, IMsgStore **ret);
extern _kc_export HRESULT HrGetAddress(LPADRBOOK, IMessage *, ULONG tag_eid, ULONG tag_name, ULONG tag_type, ULONG tag_addr, std::wstring &name, std::wstring &type, std::wstring &addr);
extern _kc_export HRESULT HrGetAddress(IAddrBook *, const SPropValue *props, ULONG nvals, ULONG tag_eid, ULONG tag_name, ULONG tag_type, ULONG tag_addr, std::wstring &name, std::wstring &type, std::wstring &addr);
extern _kc_export HRESULT HrGetAddress(IAddrBook *, const ENTRYID *eid, ULONG eid_size, std::wstring &name, std::wstring &type, std::wstring &addr);
extern _kc_export std::string ToQuotedBase64Header(const std::wstring &);
extern HRESULT TestRestriction(const SRestriction *cond, ULONG nvals, const SPropValue *props, const ECLocale &, ULONG level = 0);
extern _kc_export HRESULT TestRestriction(const SRestriction *cond, IMAPIProp *msg, const ECLocale &, ULONG level = 0);
extern _kc_export HRESULT HrOpenUserMsgStore(LPMAPISESSION, const wchar_t *user, LPMDB *store);
// Auto-accept settings
extern _kc_export HRESULT SetAutoAcceptSettings(IMsgStore *, bool auto_accept, bool decline_conflict, bool decline_recurring);
extern _kc_export HRESULT GetAutoAcceptSettings(IMsgStore *, bool *auto_accept, bool *decline_conflict, bool *decline_recurring);

/**
 * NAMED PROPERTY utilities
 *
 * HOW TO USE
 * Make sure you have an IMAPIProp interface to pass to PROPMAP_INIT
 * All properties are allocated an ULONG with name PROP_XXXXX (XXXXX passed in first param of PROPMAP_NAMED_ID
 *
 * EXAMPLE
 *
 * PROPMAP_START(2)
 *  PROPMAP_NAMED_ID(RECURRING, PT_BOOLEAN, PSETID_Appointment, dispidRecurring)
 *  PROPMAP_NAMED_ID(START, 	PT_SYSTIME, PSETID_Appointment, dispidStart)
 * PROPMAP_INIT(lpMessage)
 *
 * printf("%X %X\n", PROP_RECURRING, PROP_START);
 *
 */
class _kc_export ECPropMapEntry _kc_final {
public:
    ECPropMapEntry(GUID guid, ULONG ulId);
    ECPropMapEntry(GUID guid, const char *strName);
	_kc_hidden ECPropMapEntry(const ECPropMapEntry &);
	_kc_hidden ECPropMapEntry(ECPropMapEntry &&);
    ~ECPropMapEntry();
	_kc_hidden MAPINAMEID *GetMAPINameId(void);
private:
    MAPINAMEID m_sMAPINameId;
    GUID m_sGuid;
};

class _kc_export ECPropMap _kc_final {
public:
    ECPropMap(size_t = 0);
    void AddProp(ULONG *lpId, ULONG ulType, const ECPropMapEntry &entry);
    HRESULT Resolve(IMAPIProp *lpMAPIProp);
private:
    std::vector<ECPropMapEntry> lstNames;
    std::vector<ULONG *> lstVars;
    std::vector<ULONG> lstTypes;
};

#define PROPMAP_DECL() ECPropMap m_propmap;
#define PROPMAP_START(hint) ECPropMap m_propmap(hint);
#define PROPMAP_NAMED_ID(name, type, guid, id) ULONG PROP_##name; m_propmap.AddProp(&PROP_##name, type, ECPropMapEntry(guid, id));
#define PROPMAP_INIT(lpObject) do { hr = m_propmap.Resolve(lpObject); if (hr != hrSuccess) goto exitpm; } while (false);
#define PROPMAP_DEF_NAMED_ID(name) ULONG PROP_##name = 0;
#define PROPMAP_INIT_NAMED_ID(name, type, guid, id) m_propmap.AddProp(&PROP_##name, type, ECPropMapEntry(guid, id));

} /* namespace */

#endif // COMMONUTIL_H
