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

#ifndef ECGENERICPROP_H
#define ECGENERICPROP_H

#include "ECUnknown.h"
#include "IECPropStorage.h"
#include "ECPropertyEntry.h"
#include "IECSingleInstance.h"

#include <list>
#include <map>
#include <set>

// These are the callback functions called when a software-handled property is requested
typedef HRESULT (* SetPropCallBack)(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam);
typedef HRESULT (* GetPropCallBack)(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);

typedef struct PROPCALLBACK {
	ULONG ulPropTag;
	SetPropCallBack lpfnSetProp;
	GetPropCallBack lpfnGetProp;
	void *			lpParam;
	BOOL			fRemovable;
	BOOL			fHidden; // hidden from GetPropList

	bool operator==(const PROPCALLBACK &callback) {
		return callback.ulPropTag == this->ulPropTag;
	}
} PROPCALLBACK;

typedef std::map<short, PROPCALLBACK>			ECPropCallBackMap;
typedef ECPropCallBackMap::iterator				ECPropCallBackIterator;
typedef std::map<short, ECPropertyEntry>		ECPropertyEntryMap;
typedef ECPropertyEntryMap::iterator			ECPropertyEntryIterator;

class ECGenericProp : public ECUnknown
{
protected:
	ECGenericProp(void *lpProvider, ULONG ulObjType, BOOL fModify, char *szClassName = NULL);
	virtual ~ECGenericProp();

public:
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

	HRESULT SetProvider(void* lpProvider);
	HRESULT SetEntryId(ULONG cbEntryId, LPENTRYID lpEntryId);

	static HRESULT		DefaultGetPropGetReal(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);	
	static HRESULT		DefaultSetPropComputed(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam);	
	static HRESULT		DefaultSetPropIgnore(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam);	
	static HRESULT		DefaultSetPropSetReal(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam);
	static HRESULT		DefaultGetProp(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);

	// Our table-row getprop handler (handles client-side generation of table columns)
	static HRESULT		TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType);

	virtual HRESULT HrSetPropStorage(IECPropStorage *lpStorage, BOOL fLoadProps);
	
	virtual HRESULT		HrSetRealProp(SPropValue *lpsPropValue);
	virtual HRESULT		HrGetRealProp(ULONG ulPropTag, ULONG ulFlags, void *lpBase, LPSPropValue lpsPropValue, ULONG ulMaxSize = 0);
	virtual HRESULT		HrAddPropHandlers(ULONG ulPropTag, GetPropCallBack lpfnGetProp, SetPropCallBack lpfnSetProp, void *lpParam, BOOL fRemovable = FALSE, BOOL fHidden = FALSE);
	virtual HRESULT 	HrLoadEmptyProps();

	bool IsReadOnly() const;

protected: ///?
	virtual HRESULT		HrLoadProps();
	HRESULT				HrLoadProp(ULONG ulPropTag);
	virtual HRESULT		HrDeleteRealProp(ULONG ulPropTag, BOOL fOverwriteRO);
	HRESULT				HrGetHandler(ULONG ulPropTag, SetPropCallBack *lpfnSetProp, GetPropCallBack *lpfnGetProp, void **lpParam);
	HRESULT				IsPropDirty(ULONG ulPropTag, BOOL *lpbDirty);
	HRESULT				HrSetClean();
	HRESULT				HrSetCleanProperty(ULONG ulPropTag);

	/* used by child to save here in it's parent */
	friend class ECParentStorage;
	virtual HRESULT HrSaveChild(ULONG ulFlags, MAPIOBJECT *lpsMapiObject);
	virtual HRESULT HrRemoveModifications(MAPIOBJECT *lpsMapiObject, ULONG ulPropTag);

	// For IECSingleInstance
	virtual HRESULT GetSingleInstanceId(ULONG *lpcbInstanceID, LPSIEID *lppInstanceID);
	virtual HRESULT SetSingleInstanceId(ULONG cbInstanceID, LPSIEID lpInstanceID);

public:
	// From IMAPIProp
	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR FAR * lppMAPIError);
	virtual HRESULT SaveChanges(ULONG ulFlags);
	virtual HRESULT GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray);
	virtual HRESULT GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray);
	
	/**
	 * \brief Opens a property.
	 *
	 * \param ulPropTag				The proptag of the property to open.
	 * \param lpiid					Pointer to the requested interface for the object to be opened.
	 * \param ulInterfaceOptions	Interface options.
	 * \param ulFlags				Flags.
	 * \param lppUnk				Pointer to an IUnknown pointer where the opened object will be stored.
	 *
	 * \return hrSuccess on success.
	 */
	virtual HRESULT OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk);
	virtual HRESULT SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems);
	virtual HRESULT DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems);
	virtual HRESULT CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
	virtual HRESULT CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
	virtual HRESULT GetNamesFromIDs(LPSPropTagArray FAR * lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG FAR * lpcPropNames, LPMAPINAMEID FAR * FAR * lpppPropNames);
	virtual HRESULT GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID FAR * lppPropNames, ULONG ulFlags, LPSPropTagArray FAR * lppPropTags);

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

	class xECSingleInstance : public IECSingleInstance {
	public:
		// IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();

		// IECSingleInstance
		virtual HRESULT __stdcall GetSingleInstanceId(ULONG *lpcbInstanceID, LPENTRYID *lppInstanceID);
		virtual HRESULT __stdcall SetSingleInstanceId(ULONG cbInstanceID, LPENTRYID lpInstanceID);
	} m_xECSingleInstance;

protected:
	ECPropertyEntryMap*		lstProps;
	std::set<ULONG>			m_setDeletedProps;
	ECPropCallBackMap		lstCallBack;
	DWORD					dwLastError;
	BOOL					fSaved;			// only 0 if just created
	ULONG					ulObjType;
	ULONG					ulObjFlags;		// message: MAPI_ASSOCIATED, folder: FOLDER_SEARCH (last?)

	BOOL					fModify;
	void*					lpProvider;
	BOOL					isTransactedObject;

public:
	// Current entryid of object
	ULONG					m_cbEntryId;
	LPENTRYID				m_lpEntryId;

	MAPIOBJECT				*m_sMapiObject;
	pthread_mutex_t			m_hMutexMAPIObject;	///< Mutex for locking the MAPIObject
	BOOL					m_bReload;
	BOOL					m_bLoading;

	IECPropStorage*			lpStorage;
};


// Inlines
inline bool ECGenericProp::IsReadOnly() const {
	return !fModify;
}

#endif
