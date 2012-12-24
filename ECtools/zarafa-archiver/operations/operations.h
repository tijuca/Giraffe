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

#ifndef operations_INCLUDED
#define operations_INCLUDED

#include "operations_fwd.h"

#include <mapix.h>
#include "mapi_ptr.h"

class ECArchiverLogger;

namespace za { namespace operations {

/**
 * IArchiveOperation specifies the interface to all archiver operations.
 */
class IArchiveOperation
{
public:
	/**
	 * virtual destructor.
	 */
	virtual ~IArchiveOperation() {}
	
	/**
	 * Entrypoint for all archive operations.
	 * 
	 * @param[in]	lpFolder
	 *					A MAPIFolder pointer that points to the folder that's to be processed. This folder will usualy be
	 *					a searchfolder containing the messages that are ready to be processed.
	 * @param[in]	cProps
	 *					The number op properties pointed to by lpProps.
	 * @param[in]	lpProps
	 *					Pointer to an array of properties that are used by the Operation object.
	 *
	 * @return HRESULT
	 */
	virtual HRESULT ProcessEntry(LPMAPIFOLDER lpFolder, ULONG cProps, const LPSPropValue lpProps) = 0;

	/**
	 * Obtain the restriction the messages need to match in order to be processed by this operation.
	 *
	 * @param[in]	LPMAPIPROP			A MAPIProp object that's used to resolve named properties on.
	 * @param[out]	lppRestriction		The restriction to be matched.
	 */
	virtual HRESULT GetRestriction(LPMAPIPROP LPMAPIPROP, LPSRestriction *lppRestriction) = 0;

	/**
	 * Verify the message to make sure a message isn't processed while it shouldn't caused
	 * by searchfolder lags.
	 *
	 * @param[in]	lpMessage		The message to verify
	 *
	 * @return	hrSuccess			The message is verified to be valid for the operation.
	 * @return	MAPI_E_NOT_FOUND	The message should not be processed.
	 */
	virtual HRESULT VerifyRestriction(LPMESSAGE lpMessage) = 0;
};


/**
 * This class contains some basic functionality for all operations.
 *
 * It generates the restriction.
 * It contains the logger.
 */
class ArchiveOperationBase : public IArchiveOperation
{
public:
	ArchiveOperationBase(ECArchiverLogger *lpLogger, int ulAge, bool bProcessUnread, ULONG ulInhibitMask);
	HRESULT GetRestriction(LPMAPIPROP LPMAPIPROP, LPSRestriction *lppRestriction);
	HRESULT VerifyRestriction(LPMESSAGE lpMessage);

protected:
	/**
	 * Returns a pointer to the logger.
	 * @return An ECArchiverLogger pointer.
	 */
	ECArchiverLogger* Logger() { return m_lpLogger; }
	
private:
	ECArchiverLogger *m_lpLogger;
	const int m_ulAge;
	const bool m_bProcessUnread;
	const ULONG m_ulInhibitMask;
	FILETIME m_ftCurrent;
};


/**
 * ArchiveOperationBaseEx is the base implementation of the IArchiveOperation interface. It's main functionality
 * is iterating through all the items in the folder passed to ProcessEntry.
 */
class ArchiveOperationBaseEx : public ArchiveOperationBase
{
public:
	ArchiveOperationBaseEx(ECArchiverLogger *lpLogger, int ulAge, bool bProcessUnread, ULONG ulInhibitMask);
	HRESULT ProcessEntry(LPMAPIFOLDER lpFolder, ULONG cProps, const LPSPropValue lpProps);
	
protected:
	/**
	 * Returns a pointer to a MAPIFolderPtr, which references the current working folder.
	 * @return A reference to a MAPIFolderPtr.
	 */
	MAPIFolderPtr& CurrentFolder() { return m_ptrCurFolder; }
	
private:
	/**
	 * Called by ProcessEntry after switching source folders. Derived classes will need to
	 * implement this method. It can be used to perform operations that only need to be done when
	 * the source folder is switched.
	 *
	 * @param[in]	lpFolder	The just opened folder.
	 * @return HRESULT
	 */
	virtual HRESULT EnterFolder(LPMAPIFOLDER lpFolder) = 0;
	
	/**
	 * Called by ProcessEntry before switching source folders. Derived classes will need to
	 * implement this method. It can be used to perform operations that only need to be done when
	 * the source folder is switched.
	 *
	 * @return HRESULT
	 */
	virtual HRESULT LeaveFolder() = 0;
	
	/**
	 * Called by ProcessEntry for each message found in the search folder that also matches the age restriction.
	 *
	 * @param[in]	cProps
	 *					The number op properties pointed to by lpProps.
	 * @param[in]	lpProps
	 *					Pointer to an array of properties that are used by the Operation object.
	 *
	 * @return HRESULT
	 */
	virtual HRESULT DoProcessEntry(ULONG cProps, const LPSPropValue &lpProps) = 0;
	
private:
	SPropValuePtr m_ptrCurFolderEntryId;
	MAPIFolderPtr m_ptrCurFolder;
};

}} // namespaces

#endif // ndef operations_INCLUDED
