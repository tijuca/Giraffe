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
#include "logger.h"

ECArchiverLogger::ECArchiverLogger(ECLogger *lpLogger)
: ECLogger(0)
, m_lpLogger(lpLogger)
{
	if (m_lpLogger)
		m_lpLogger->AddRef();
}

ECArchiverLogger::~ECArchiverLogger()
{
	if (m_lpLogger)
		m_lpLogger->Release();
}

tstring ECArchiverLogger::SetUser(tstring strUser)
{
	std::swap(strUser, m_strUser);
	return strUser;
}

tstring ECArchiverLogger::SetFolder(tstring strFolder)
{
	std::swap(strFolder, m_strFolder);
	return strFolder;
}

void ECArchiverLogger::Reset()
{
	if (m_lpLogger)
		m_lpLogger->Reset();
}

void ECArchiverLogger::Log(int loglevel, const std::string &message)
{
	if (m_lpLogger)
		m_lpLogger->Log(loglevel, message);
}

void ECArchiverLogger::Log(int loglevel, const char *format, ...)
{
	if (m_lpLogger && m_lpLogger->Log(loglevel)) {
		std::string strFormat = CreateFormat(format);
		va_list va;
		va_start(va, format);
		m_lpLogger->LogVA(loglevel, strFormat.c_str(), va);
		va_end(va);
	}
}

void ECArchiverLogger::LogVA(int loglevel, const char *format, va_list& va)
{
	if (m_lpLogger && m_lpLogger->Log(loglevel)) {
		std::string strFormat = CreateFormat(format);
		m_lpLogger->LogVA(loglevel, strFormat.c_str(), va);
	}
}

std::string ECArchiverLogger::CreateFormat(const char *format)
{
	char buffer[4096];
	int len;
	std::string strPrefix;

	if (!m_strUser.empty()) {
		if (m_strFolder.empty()) {
			len = m_lpLogger->snprintf(buffer, sizeof(buffer), "For '"TSTRING_PRINTF"': ", m_strUser.c_str());
			strPrefix = EscapeFormatString(std::string(buffer, len));
		} else {
			len = m_lpLogger->snprintf(buffer, sizeof(buffer), "For '"TSTRING_PRINTF"' in folder '"TSTRING_PRINTF"': ", m_strUser.c_str(), m_strFolder.c_str());
			strPrefix = EscapeFormatString(std::string(buffer, len));
		}
	}

	return strPrefix + format;
}

std::string ECArchiverLogger::EscapeFormatString(const std::string &strFormat)
{
	std::string strEscaped;
	strEscaped.reserve(strFormat.length() * 2);

	for (std::string::const_iterator c = strFormat.begin(); c != strFormat.end(); ++c) {
		if (*c == '\\')
			strEscaped.append("\\\\");
		else if (*c == '%')
			strEscaped.append("%%");
		else
			strEscaped.append(1, *c);
	}
	
	return strEscaped;
}


ScopedUserLogging::ScopedUserLogging(ECArchiverLogger *lpLogger, const tstring &strUser)
: m_lpLogger(lpLogger)
, m_strPrevUser(lpLogger->SetUser(strUser))
{ }

ScopedUserLogging::~ScopedUserLogging()
{
	m_lpLogger->SetUser(m_strPrevUser);
}


ScopedFolderLogging::ScopedFolderLogging(ECArchiverLogger *lpLogger, const tstring &strFolder)
: m_lpLogger(lpLogger)
, m_strPrevFolder(lpLogger->SetFolder(strFolder))
{ }

ScopedFolderLogging::~ScopedFolderLogging()
{
	m_lpLogger->SetFolder(m_strPrevFolder);
}
