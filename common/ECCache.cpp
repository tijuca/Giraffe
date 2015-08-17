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

#include <platform.h>
#include <ZarafaCode.h>

#include "ECCache.h"
#include <ECLogger.h>
#include <stringutil.h>

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


ECCacheBase::ECCacheBase(const std::string &strCachename, size_type ulMaxSize, long lMaxAge)
	: m_strCachename(strCachename)
	, m_ulMaxSize(ulMaxSize)
	, m_lMaxAge(lMaxAge)
	, m_ulCacheHit(0)
	, m_ulCacheValid(0)
{ }

ECCacheBase::~ECCacheBase() {}

void ECCacheBase::RequestStats(void(callback)(const std::string &, const std::string &, const std::string &, void*), void *obj)
{
	callback((std::string)"cache_" + m_strCachename + "_items", (std::string)"Cache " + m_strCachename + " items", stringify_int64(ItemCount()), obj);
	callback((std::string)"cache_" + m_strCachename + "_size", (std::string)"Cache " + m_strCachename + " size", stringify_int64(Size()), obj);
	callback((std::string)"cache_" + m_strCachename + "_maxsz", (std::string)"Cache " + m_strCachename + " maximum size", stringify_int64(m_ulMaxSize), obj);
	callback((std::string)"cache_" + m_strCachename + "_req", (std::string)"Cache " + m_strCachename + " requests", stringify_int64(HitCount()), obj);
	callback((std::string)"cache_" + m_strCachename + "_hit", (std::string)"Cache " + m_strCachename + " hits", stringify_int64(ValidCount()), obj);
}

void ECCacheBase::DumpStats(ECLogger *lpLogger)
{
	unsigned long long z;
	std::string strName;
	
	strName = m_strCachename + " cache size:";
	z = Size();
	lpLogger->Log(EC_LOGLEVEL_FATAL,
		"  %-30s  %8lu (%8llu bytes) (usage %.02f%%)",
		strName.c_str(), ItemCount(), z, z / (double)MaxSize() * 100.0);
	strName = m_strCachename + " cache hits:";
	z = ValidCount();
	lpLogger->Log(EC_LOGLEVEL_FATAL, "  %-30s  %8llu / %llu (%.02f%%)",
		strName.c_str(), z, static_cast<unsigned long long>(HitCount()),
		z / (double)HitCount() * 100.0);
}
