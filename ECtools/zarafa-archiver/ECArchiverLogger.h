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

#ifndef LOGGER_H
#define LOGGER_H

#include "ECLogger.h"
#include "tstring.h"

class ECArchiverLogger : public ECLogger
{
public:
	ECArchiverLogger(ECLogger *lpLogger);
	~ECArchiverLogger();

	tstring SetUser(tstring strUser = tstring());
	tstring SetFolder(tstring strFolder = tstring());

	const tstring& GetUser() const { return m_strUser; }
	const tstring& GetFolder() const { return m_strFolder; }

	void Reset();
	void Log(unsigned int loglevel, const std::string &message);
	void Log(unsigned int loglevel, const char *format, ...) __LIKE_PRINTF(3, 4);
	void LogVA(unsigned int loglevel, const char *format, va_list& va);

private:
	std::string CreateFormat(const char *format);
	std::string EscapeFormatString(const std::string &strFormat);

private:
	ECArchiverLogger(const ECArchiverLogger&);
	ECArchiverLogger& operator=(const ECArchiverLogger&);

private:
	ECLogger	*m_lpLogger;
	tstring		m_strUser;
	tstring		m_strFolder;
};


class ScopedUserLogging
{
public:
	ScopedUserLogging(ECArchiverLogger *lpLogger, const tstring &strUser);
	~ScopedUserLogging();

private:
	ScopedUserLogging(const ScopedUserLogging&);
	ScopedUserLogging& operator=(const ScopedUserLogging&);

private:
	ECArchiverLogger *m_lpLogger;
	const tstring m_strPrevUser;
};


class ScopedFolderLogging
{
public:
	ScopedFolderLogging(ECArchiverLogger *lpLogger, const tstring &strFolder);
	~ScopedFolderLogging();

private:
	ScopedFolderLogging(const ScopedFolderLogging&);
	ScopedFolderLogging& operator=(const ScopedFolderLogging&);

private:
	ECArchiverLogger *m_lpLogger;
	const tstring m_strPrevFolder;
};

#endif // ndef LOGGER_H
