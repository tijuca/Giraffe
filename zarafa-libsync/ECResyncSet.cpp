/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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
#include "ECResyncSet.h"

void ECResyncSet::Append(const SBinary &sbinSourceKey, const SBinary &sbinEntryID, const FILETIME &lastModTime)
{
	m_map.insert(map_type::value_type(
		array_type(sbinSourceKey.lpb, sbinSourceKey.lpb + sbinSourceKey.cb),
		storage_type(array_type(sbinEntryID.lpb, sbinEntryID.lpb + sbinEntryID.cb), lastModTime)));
}

bool ECResyncSet::Remove(const SBinary &sbinSourceKey)
{
	return m_map.erase(array_type(sbinSourceKey.lpb, sbinSourceKey.lpb + sbinSourceKey.cb)) == 1;
}


ECResyncSetIterator::ECResyncSetIterator(ECResyncSet &resyncSet)
: m_lpResyncSet(&resyncSet)
, m_iterator(m_lpResyncSet->m_map.begin())
{ }

ECResyncSetIterator::ECResyncSetIterator(ECResyncSet &resyncSet, const SBinary &sbinSourceKey)
: m_lpResyncSet(&resyncSet)
, m_iterator(m_lpResyncSet->m_map.find(ECResyncSet::array_type(sbinSourceKey.lpb, sbinSourceKey.lpb + sbinSourceKey.cb)))
{ }

bool ECResyncSetIterator::IsValid() const
{
	return m_lpResyncSet && m_iterator != m_lpResyncSet->m_map.end();
}

LPENTRYID ECResyncSetIterator::GetEntryID() const 
{
	return IsValid() ? (LPENTRYID)&m_iterator->second.entryId.front() : NULL;
}

ULONG ECResyncSetIterator::GetEntryIDSize() const
{
	return IsValid() ? m_iterator->second.entryId.size() : 0;
}

const FILETIME& ECResyncSetIterator::GetLastModTime() const
{
	return IsValid() ? m_iterator->second.lastModTime : s_nullTime;
}

ULONG ECResyncSetIterator::GetFlags() const
{
	return IsValid() ? m_iterator->second.flags : 0;
}

void ECResyncSetIterator::SetFlags(ULONG flags)
{
	if (IsValid())
		m_iterator->second.flags = flags;
}

void ECResyncSetIterator::Next()
{
	if (IsValid())
		m_iterator++;
}

const FILETIME ECResyncSetIterator::s_nullTime = {0, 0};