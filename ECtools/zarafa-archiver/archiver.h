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

#ifndef archiver_INCLUDED
#define archiver_INCLUDED

#include <memory>
#include <list>
#include <string>

#include "platform.h"

#ifdef ARCHIVER_EXTRA
#include <mapix.h>
#endif
class ECLogger;
class ECConfig;

enum eResult {
	Success = 0,
	Failure,
	Uninitialized,
	OutOfMemory,
	InvalidParameter,
	FileNotFound,
	InvalidConfig,
	PartialCompletion
};


class ArchiveControl {
public:
	typedef std::auto_ptr<ArchiveControl>	auto_ptr_type;

	virtual ~ArchiveControl() {};

	virtual eResult ArchiveAll(bool bLocalOnly, bool bAutoAttach, unsigned int ulFlags) = 0;
	virtual eResult Archive(const TCHAR *lpszUser, bool bAutoAttach, unsigned int ulFlags) = 0;
	virtual eResult CleanupAll(bool bLocalOnly) = 0;
	virtual eResult Cleanup(const TCHAR *lpszUser) = 0;

protected:
	ArchiveControl() {};
};

typedef ArchiveControl::auto_ptr_type	ArchiveControlPtr;




struct ArchiveEntry {
	std::string StoreName;
	std::string FolderName;
	std::string StoreOwner;
	unsigned Rights;
};
typedef std::list<ArchiveEntry> ArchiveList;

#define ARCHIVE_RIGHTS_ERROR	(unsigned)-1
#define ARCHIVE_RIGHTS_ABSENT	(unsigned)-2

struct UserEntry {
	std::string UserName;
};
typedef std::list<UserEntry> UserList;


class ArchiveManage {
public:
	enum {
		UseIpmSubtree = 1,
		Writable = 2,
		ReadOnly = 4
	};

	typedef std::auto_ptr<ArchiveManage>	auto_ptr_type;

	virtual ~ArchiveManage() {};

	virtual eResult AttachTo(const char *lpszArchiveServer, const TCHAR *lpszArchive, const TCHAR *lpszFolder, unsigned int ulFlags) = 0;
	virtual eResult DetachFrom(const char *lpszArchiveServer, const TCHAR *lpszArchive, const TCHAR *lpszFolder) = 0;
	virtual eResult DetachFrom(unsigned int ulArchive) = 0;
	virtual eResult ListArchives(std::ostream &ostr) = 0;
	virtual eResult ListArchives(ArchiveList *lplstArchives, const char *lpszIpmSubtreeSubstitude = NULL) = 0;
	virtual eResult ListAttachedUsers(std::ostream &ostr) = 0;
	virtual eResult ListAttachedUsers(UserList *lplstUsers) = 0;
	virtual eResult AutoAttach(unsigned int ulFlags) = 0;

protected:
	ArchiveManage() {};
};

typedef ArchiveManage::auto_ptr_type	ArchiveManagePtr;



#define ARCHIVER_API

struct configsetting_t;

class Archiver {
public:
	typedef std::auto_ptr<Archiver>		auto_ptr_type;

	enum {
		RequireConfig		= 0x00000001,
		AttachStdErr		= 0x00000002,
		InhibitErrorLogging	= 0x40000000	// To silence Init errors in the unit test.
	};

	static const char* ARCHIVER_API GetConfigPath();
	static const configsetting_t* ARCHIVER_API GetConfigDefaults();
	static eResult ARCHIVER_API Create(auto_ptr_type *lpptrArchiver);
#ifdef ARCHIVER_EXTRA
	static HRESULT ARCHIVER_API CreateManage(LPMAPISESSION lpSession, ECLogger *lpLogger, const TCHAR *lpszUser, ArchiveManagePtr *lpptrManage);
#endif

	virtual ~Archiver() {};

	virtual eResult Init(const char *lpszAppName, const char *lpszConfig, const configsetting_t *lpExtraSettings = NULL, unsigned int ulFlags = 0) = 0;

	virtual eResult GetControl(ArchiveControlPtr *lpptrControl, bool bForceCleanup = false) = 0;
	virtual eResult GetManage(const TCHAR *lpszUser, ArchiveManagePtr *lpptrManage) = 0;
	virtual eResult AutoAttach(unsigned int ulFlags) = 0;

	virtual ECConfig* GetConfig() const = 0;
	virtual ECLogger* GetLogger() const = 0;

protected:
	Archiver() {};
};

typedef Archiver::auto_ptr_type		ArchiverPtr;




#endif // ndef archiver_INCLUDED
