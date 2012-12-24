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

#ifndef ECIterators_INCLUDED
#define ECIterators_INCLUDED

#include <boost/iterator/iterator_facade.hpp>
#include <mapi_ptr.h>

class ECHierarchyIteratorBase
{
public:
	ECHierarchyIteratorBase(): m_ulFlags(0), m_ulDepth(0), m_ulRowIndex(0) {}  // creates the "end" iterator
	ECHierarchyIteratorBase(LPMAPICONTAINER lpContainer, ULONG ulFlags = 0, ULONG ulDepth = 0);

	MAPIContainerPtr& dereference() const
	{
		ASSERT(m_ptrCurrent && "attempt to dereference end iterator");
		return const_cast<MAPIContainerPtr&>(m_ptrCurrent);
	}

	void increment();

	bool equal(const ECHierarchyIteratorBase &rhs) const
	{
		return m_ptrCurrent == rhs.m_ptrCurrent; 
	}

private:
	friend class boost::iterator_core_access;

	MAPIContainerPtr	m_ptrContainer;
	ULONG				m_ulFlags;
	ULONG				m_ulDepth;
	MAPITablePtr		m_ptrTable;
	SRowSetPtr			m_ptrRows;
	ULONG				m_ulRowIndex;
	MAPIContainerPtr	m_ptrCurrent;
};

template <typename ContainerPtrType>
class ECHierarchyIterator
	: public boost::iterator_facade<
		ECHierarchyIterator<ContainerPtrType>,
		ContainerPtrType,
		boost::single_pass_traversal_tag>
	, public ECHierarchyIteratorBase
{
public:
	ECHierarchyIterator() {}
	ECHierarchyIterator(LPMAPICONTAINER lpContainer, ULONG ulFlags = 0, ULONG ulDepth = 0)
		: ECHierarchyIteratorBase(lpContainer, ulFlags, ulDepth) {}

	ContainerPtrType& dereference() const
	{
		ECHierarchyIteratorBase::dereference().QueryInterface(m_ptr);
		return m_ptr;
	}

private:
	mutable ContainerPtrType	m_ptr;
};

typedef ECHierarchyIterator<MAPIFolderPtr> ECFolderIterator;
typedef ECHierarchyIterator<ABContainerPtr> ECABContainerIterator;



class ECContentsIteratorBase
{
protected:
	ECContentsIteratorBase(): m_ulFlags(0), m_ulRowIndex(0) {}  // creates the "end" iterator
	ECContentsIteratorBase(LPMAPICONTAINER lpContainer, LPSRestriction lpRestriction, ULONG ulFlags, bool bOwnRestriction);

	UnknownPtr& dereference() const
	{
		ASSERT(m_ptrCurrent && "attempt to dereference end iterator");
		return const_cast<UnknownPtr&>(m_ptrCurrent);
	}

	void increment();

	bool equal(const ECContentsIteratorBase &rhs) const
	{
		return m_ptrCurrent == rhs.m_ptrCurrent; 
	}

private:
	friend class boost::iterator_core_access;

	MAPIContainerPtr	m_ptrContainer;
	ULONG				m_ulFlags;
	SRestrictionPtr		m_ptrRestriction;
	MAPITablePtr		m_ptrTable;
	SRowSetPtr			m_ptrRows;
	ULONG				m_ulRowIndex;
	UnknownPtr			m_ptrCurrent;
};

template <typename ContainerPtrType>
class ECContentsIterator
	: public boost::iterator_facade<
		ECContentsIterator<ContainerPtrType>,
		ContainerPtrType,
		boost::single_pass_traversal_tag>
	, public ECContentsIteratorBase
{
public:
	ECContentsIterator() {}
	ECContentsIterator(LPMAPICONTAINER lpContainer, ULONG ulFlags = 0)
		: ECContentsIteratorBase(lpContainer, NULL, ulFlags, false) {}
	ECContentsIterator(LPMAPICONTAINER lpContainer, LPSRestriction lpRestriction, ULONG ulFlags = 0)
		: ECContentsIteratorBase(lpContainer, lpRestriction, ulFlags, false) {}

	ContainerPtrType& dereference() const
	{
		ECContentsIteratorBase::dereference().QueryInterface(m_ptr);
		return m_ptr;
	}

private:
	mutable ContainerPtrType	m_ptr;
};

typedef ECContentsIterator<MessagePtr> ECMessageIterator;
typedef ECContentsIterator<MailUserPtr> ECMailUserIterator;


/*
 * Declare ECContentsIterator<MailUserPtr> contructor specializations.
 * Needed to ensure no groups are returned.
 */
template <> ECContentsIterator<MailUserPtr>::ECContentsIterator(LPMAPICONTAINER lpContainer, ULONG ulFlags);
template <> ECContentsIterator<MailUserPtr>::ECContentsIterator(LPMAPICONTAINER lpContainer, LPSRestriction lpRestriction, ULONG ulFlags);


#endif // ndef ECIterators_INCLUDED
