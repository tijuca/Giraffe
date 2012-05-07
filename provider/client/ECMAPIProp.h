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

#ifndef ECMAPIPROP_H
#define ECMAPIPROP_H

#include "Zarafa.h"
#include "IECSecurity.h"
#include "ECGenericProp.h"

// For HrSetFlags
#define SET         1
#define UNSET       2

class ECMsgStore;


class ECMAPIProp : public ECGenericProp
{
protected:
	ECMAPIProp(void *lpProvider, ULONG ulObjType, BOOL fModify, ECMAPIProp *lpRoot, char *szClassName = NULL);
	virtual ~ECMAPIProp();

public:
	/**
	 * \brief Obtain a different interface to this object.
	 *
	 * See ECUnkown::QueryInterface.
	 */
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

	static HRESULT TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType);

	// Callback for Commit() on streams
	static HRESULT	HrStreamCommit(IStream *lpStream, void *lpData);
	// Callback for ECMemStream delete local data
	static HRESULT	HrStreamCleanup(void *lpData);

	static HRESULT	DefaultMAPIGetProp(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);
	static HRESULT	SetPropHandler(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam);

	// IMAPIProp override
	virtual HRESULT OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk);
	virtual HRESULT SaveChanges(ULONG ulFlags);
	virtual HRESULT CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
	virtual HRESULT CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
	virtual HRESULT GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames);
	virtual HRESULT GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga);

	virtual HRESULT HrSetSyncId(ULONG ulSyncId);

	virtual HRESULT SetParentID(ULONG cbParentID, LPENTRYID lpParentID);

	virtual HRESULT SetICSObject(BOOL bICSObject);
	virtual BOOL IsICSObject();

protected:
	HRESULT HrLoadProps();
	virtual HRESULT HrSaveChild(ULONG ulFlags, MAPIOBJECT *lpsMapiObject);

	HRESULT GetSerializedACLData(LPVOID lpBase, LPSPropValue lpsPropValue);
	HRESULT SetSerializedACLData(LPSPropValue lpsPropValue);
	HRESULT	UpdateACLs(ULONG cNewPerms, LPECPERMISSION lpNewPerms);

protected:
	// IECServiceAdmin and IECSecurity
	virtual HRESULT GetUserList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER* lppsUsers);
	virtual HRESULT GetGroupList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcGroups, LPECGROUP *lppsGroups);
	virtual HRESULT GetCompanyList(ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY* lppsCompanies);
	// IECSecurity
	virtual HRESULT GetOwner(ULONG *lpcbOwner, LPENTRYID *lppOwner);
	virtual HRESULT GetPermissionRules(int ulType, ULONG* lpcPermissions, LPECPERMISSION* lppECPermissions);
	virtual HRESULT SetPermissionRules(ULONG cPermissions, LPECPERMISSION lpECPermissions);

public:
	ECMsgStore*				GetMsgStore();


public:	
	class xMAPIProp : public IMAPIProp
	{
	public:
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();	
		
		// From IMAPIProp
		virtual HRESULT __stdcall GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError);
		virtual HRESULT __stdcall SaveChanges(ULONG ulFlags);
		virtual HRESULT __stdcall GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray);
		virtual HRESULT __stdcall GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray);
		virtual HRESULT __stdcall OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk);
		virtual HRESULT __stdcall SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames);
		virtual HRESULT __stdcall GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga);
	} m_xMAPIProp;

	class xECSecurity : public IECSecurity
	{
	public:
		virtual ULONG AddRef();
		virtual ULONG Release();
		virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);
		
		virtual HRESULT GetOwner(ULONG *lpcbOwner, LPENTRYID *lppOwner);
		virtual HRESULT GetPermissionRules(int ulType, ULONG* lpcPermissions, LPECPERMISSION* lppECPermissions);
		virtual HRESULT SetPermissionRules(ULONG cPermissions, LPECPERMISSION lpECPermissions);
		virtual HRESULT GetUserList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
		virtual HRESULT GetGroupList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcGroups, LPECGROUP *lppsGroups);
		virtual HRESULT GetCompanyList(ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY *lppCompanies);
	} m_xECSecurity;

private:
	BOOL		m_bICSObject; // coming from the ICS system
	ULONG		m_ulSyncId;
	ULONG		m_cbParentID;
	LPENTRYID	m_lpParentID; // Overrides the parentid from the server

public:
	ECMAPIProp *m_lpRoot;		// Points to the 'root' object that was opened by OpenEntry; normally points to 'this' except for Attachments and Submessages
};

typedef struct STREAMDATA {
	ULONG ulPropTag;
	ECMAPIProp *lpProp;
} STREAMDATA;

#endif // ECMAPIPROP_H
