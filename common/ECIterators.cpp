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

#include "platform.h"
#include "ECIterators.h"
#include "ECRestriction.h"
#include "HrException.h"

/**
 * ECHierarchyIteratorBase implementation
 */
ECHierarchyIteratorBase::ECHierarchyIteratorBase(LPMAPICONTAINER lpContainer, ULONG ulFlags, ULONG ulDepth)
: m_ptrContainer(lpContainer, true)
, m_ulFlags(ulFlags)
, m_ulDepth(ulDepth)
, m_ulRowIndex(0)
{
	increment();
}

void ECHierarchyIteratorBase::increment()
{
	HRESULT hr = hrSuccess;
	ULONG ulType;

	enum {IDX_ENTRYID};

	if (!m_ptrTable) {
		SPropValuePtr ptrFolderType;

		SizedSPropTagArray(1, sptaColumnProps) = {1, {PR_ENTRYID}};

		hr = HrGetOneProp(m_ptrContainer, PR_FOLDER_TYPE, &ptrFolderType);
		if (hr == hrSuccess && ptrFolderType->Value.ul == FOLDER_SEARCH) {
			// No point in processing search folders
			m_ptrCurrent.reset();
			goto exit;
		}			

		hr = m_ptrContainer->GetHierarchyTable(m_ulDepth == 1 ? 0 : CONVENIENT_DEPTH, &m_ptrTable);
		if (hr != hrSuccess)
			goto exit;

		if (m_ulDepth > 1) {
			SPropValue sPropDepth;
			ECPropertyRestriction res(RELOP_LE, PR_DEPTH, &sPropDepth, ECRestriction::Cheap);
			SRestrictionPtr ptrRes;

			sPropDepth.ulPropTag = PR_DEPTH;
			sPropDepth.Value.ul = m_ulDepth;

			hr = res.CreateMAPIRestriction(&ptrRes, ECRestriction::Cheap);
			if (hr != hrSuccess)
				goto exit;

			hr = m_ptrTable->Restrict(ptrRes, TBL_BATCH);
			if (hr != hrSuccess)
				goto exit;
		}

		hr = m_ptrTable->SetColumns((LPSPropTagArray)&sptaColumnProps, TBL_BATCH);
		if (hr != hrSuccess)
			goto exit;
	}

	if (!m_ptrRows.get()) {
		hr = m_ptrTable->QueryRows(32, 0, &m_ptrRows);
		if (hr != hrSuccess)
			goto exit;

		if (m_ptrRows.empty()) {
			m_ptrCurrent.reset();
			goto exit;
		}

		m_ulRowIndex = 0;
	}

	ASSERT(m_ulRowIndex < m_ptrRows.size());
	hr = m_ptrContainer->OpenEntry(m_ptrRows[m_ulRowIndex].lpProps[IDX_ENTRYID].Value.bin.cb, (LPENTRYID)m_ptrRows[m_ulRowIndex].lpProps[IDX_ENTRYID].Value.bin.lpb, &m_ptrCurrent.iid, m_ulFlags, &ulType, &m_ptrCurrent);
	if (hr != hrSuccess)
		goto exit;

	if (++m_ulRowIndex == m_ptrRows.size())
		m_ptrRows.reset();

exit:
	if (hr != hrSuccess)
		throw HrException(hr);	// @todo: Fix this
}


/**
 * ECContentsIteratorBase implementation
 */
ECContentsIteratorBase::ECContentsIteratorBase(LPMAPICONTAINER lpContainer, LPSRestriction lpRestriction, ULONG ulFlags, bool bOwnRestriction)
: m_ptrContainer(lpContainer, true)
, m_ulFlags(ulFlags)
, m_ulRowIndex(0)
{
	if (lpRestriction) {
		if (!bOwnRestriction) {
			HRESULT hr = Util::HrCopySRestriction(&m_ptrRestriction, lpRestriction);
			if (hr != hrSuccess)
				throw HrException(hr);
		} else
			m_ptrRestriction.reset(lpRestriction);
	}
	
	increment();
}

void ECContentsIteratorBase::increment()
{
	HRESULT hr = hrSuccess;
	ULONG ulType = 0;

	enum {IDX_ENTRYID};

	if (!m_ptrTable) {
		SizedSPropTagArray(1, sptaColumnProps) = {1, {PR_ENTRYID}};

		hr = m_ptrContainer->GetContentsTable(0, &m_ptrTable);
		if (hr != hrSuccess)
			goto exit;

		hr = m_ptrTable->SetColumns((LPSPropTagArray)&sptaColumnProps, TBL_BATCH);
		if (hr != hrSuccess)
			goto exit;

		if (m_ptrRestriction.get()) {
			hr = m_ptrTable->Restrict(m_ptrRestriction, TBL_BATCH);
			if (hr != hrSuccess)
				goto exit;
		}
	}

	if (!m_ptrRows.get()) {
		hr = m_ptrTable->QueryRows(32, 0, &m_ptrRows);
		if (hr != hrSuccess)
			goto exit;

		if (m_ptrRows.empty()) {
			m_ptrCurrent.reset();
			goto exit;
		}

		m_ulRowIndex = 0;
	}

	ASSERT(m_ulRowIndex < m_ptrRows.size());
	hr = m_ptrContainer->OpenEntry(m_ptrRows[m_ulRowIndex].lpProps[IDX_ENTRYID].Value.bin.cb, (LPENTRYID)m_ptrRows[m_ulRowIndex].lpProps[IDX_ENTRYID].Value.bin.lpb, &m_ptrCurrent.iid, m_ulFlags, &ulType, &m_ptrCurrent);
	if (hr != hrSuccess)
		goto exit;

	if (++m_ulRowIndex == m_ptrRows.size())
		m_ptrRows.reset();

exit:
	if (hr != hrSuccess)
		throw HrException(hr);	// @todo: Fix this
}

static inline LPSRestriction CreateMailUserRestriction(LPSRestriction lpRestriction) {
	HRESULT hr = hrSuccess;
	SPropValue sPropObjType;
	ECPropertyRestriction resMailUser(RELOP_EQ, PR_OBJECT_TYPE, &sPropObjType, ECRestriction::Cheap);
	LPSRestriction lpResultRestriction = NULL;
	sPropObjType.ulPropTag = PR_OBJECT_TYPE;
	sPropObjType.Value.ul = MAPI_MAILUSER;

	if (lpRestriction) {
		ECAndRestriction resAnd(
			ECRawRestriction(lpRestriction, ECRestriction::Cheap) +
			resMailUser
		);

		hr = resAnd.CreateMAPIRestriction(&lpResultRestriction);
	} else
		hr = resMailUser.CreateMAPIRestriction(&lpResultRestriction);

	if (hr != hrSuccess)
		throw HrException(hr);

	return lpResultRestriction;
}

template <>
ECContentsIterator<MailUserPtr>::ECContentsIterator(LPMAPICONTAINER lpContainer, ULONG ulFlags)
: ECContentsIteratorBase(lpContainer, CreateMailUserRestriction(NULL), ulFlags, true)
{}

template <>
ECContentsIterator<MailUserPtr>::ECContentsIterator(LPMAPICONTAINER lpContainer, LPSRestriction lpRestriction, ULONG ulFlags)
: ECContentsIteratorBase(lpContainer, CreateMailUserRestriction(lpRestriction), ulFlags, true)
{}
