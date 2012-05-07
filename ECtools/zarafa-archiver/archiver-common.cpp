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

#include <mapix.h>

#include "archiver-common.h"

#include <boost/algorithm/string/predicate.hpp>
namespace ba = boost::algorithm;

bool entryid_t::operator==(const entryid_t &other) const
{
	return getUnwrapped().m_vEntryId == other.getUnwrapped().m_vEntryId;
}

bool entryid_t::operator<(const entryid_t &other) const
{
	return getUnwrapped().m_vEntryId < other.getUnwrapped().m_vEntryId;
}

bool entryid_t::operator>(const entryid_t &other) const
{
	return getUnwrapped().m_vEntryId > other.getUnwrapped().m_vEntryId;
}

bool entryid_t::wrap(const std::string &strPath)
{
	if (!ba::istarts_with(strPath, "file://") &&
		!ba::istarts_with(strPath, "http://") &&
		!ba::istarts_with(strPath, "https://"))
		return false;
	
	m_vEntryId.insert(m_vEntryId.begin(), (LPBYTE)strPath.c_str(), (LPBYTE)strPath.c_str() + strPath.size() + 1);	// Include NULL terminator
	return true;
}

bool entryid_t::unwrap(std::string *lpstrPath)
{
	if (!isWrapped())
		return false;
	
	std::vector<BYTE>::iterator iter = std::find(m_vEntryId.begin(), m_vEntryId.end(), 0);
	if (iter == m_vEntryId.end())
		return false;
		
	if (lpstrPath)
		lpstrPath->assign((char*)&m_vEntryId.front(), iter - m_vEntryId.begin());
	
	m_vEntryId.erase(m_vEntryId.begin(), ++iter);
	return true;
}

bool entryid_t::isWrapped() const
{
	// ba::istarts_with doesn't work well on unsigned char. So we use a temporary instead.
	const std::string strEntryId((char*)&m_vEntryId.front(), m_vEntryId.size());

	return (ba::istarts_with(strEntryId, "file://") ||
			ba::istarts_with(strEntryId, "http://") ||
			ba::istarts_with(strEntryId, "https://"));
}

entryid_t entryid_t::getUnwrapped() const
{
	if (!isWrapped())
		return *this;

	entryid_t tmp(*this);
	tmp.unwrap(NULL);
	return tmp;
}

int abentryid_t::compare(const abentryid_t &other) const
{
	if (size() != other.size())
		return int(size()) - int(other.size());

	if (size() <= 32) {
		// Too small, just compare the whole thing
		return memcmp(LPBYTE(*this), LPBYTE(other), size());
	}

	// compare the part before the legacy user id.
	int res = memcmp(LPBYTE(*this), LPBYTE(other), 28);
	if (res != 0)
		return res;

	// compare the part after the legacy user id.
	return memcmp(LPBYTE(*this) + 32, LPBYTE(other) + 32, size() - 32);
}

eResult MAPIErrorToArchiveError(HRESULT hr)
{
	switch (hr) {
		case hrSuccess:					return Success;
		case MAPI_E_NOT_ENOUGH_MEMORY:	return OutOfMemory;
		case MAPI_E_INVALID_PARAMETER:	return InvalidParameter;
		case MAPI_W_PARTIAL_COMPLETION:	return PartialCompletion;
		default: 						return Failure;
	}
}
