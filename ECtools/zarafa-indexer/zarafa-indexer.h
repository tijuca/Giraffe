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

#ifndef ZARAFA_INDEXER_H
#define ZARAFA_INDEXER_H

#include <set>
#include <string>

#include "ECLogger.h"
#include "ECConfig.h"
#include "stringutil.h"

class ECFileIndex;
class ECLucene;

/**
 * @page zarafa_indexer1 zarafa-indexer
 *
 * @par Searching
 *	Currently three different commands are supported by the zarafa-indexer:
 *	- PROPS \n
 *		This will return a list of all property id's which are being indexed
 *		for each message. This command does not accept any additional arguments.
 *		The format of this returned list is: \n
 *			- OK: <name> <propid>; <name> <propid>;...
 *			.
 *	- SCOPE \n
 *		This is used to limit the scope where to search for the messages. It
 *		is mandatory to call SCOPE before QUERY with at least a restriction
 *		on the store. Optionally the scope can be restricted to folders as well.
 *		The format for providing the scope is: \n
 *			- SCOPE: <storeentryid>; <folderentryid>; <folderentryid>;...
 *			.
 *		The reply will always indicate success or failure in which case the
 *		error message will be provided as part of the returned string.
 *	- QUERY \n
 *		This is used to send the CLucene query to be executed. This command
 *		only takes a single argument which is the CLucene query string.
 *		The format for the command is: \n
 *			- QUERY: <CLucene query>
 *			.
 *		If the query was executed correctly the result will be: \n
 *			- OK: <messageentryid> <score>; <messageentryid> <score>;...
 *			.
 *		if the query provided no results, the return code will be 'OK:',
 *		only when the query itself failed will an error code be returned.
 */

/**
 * Data shared between all active threads
 */
class ECThreadData
{
public:
	/**
	 * Constructor
	 */
	ECThreadData();

	/**
	 * Destructor
	 */
	~ECThreadData();

	/**
	 * ECLogger for logging message to file
	 */
	ECLogger *lpLogger;

	/**
	 * ECConfig for reading configuration from file
	 */
	ECConfig *lpConfig;

	/**
	 * ECFileIndex wrapper for physical harddisk access
	 */
	ECFileIndex *lpFileIndex;

	/**
	 * ECLucene wrapper for CLucene classess access
	 */
	ECLucene *lpLucene;

	/**
	 * Global parameter which indicates if server is shutting down
	 */
	BOOL bShutdown;

	/* re-caches parsed config settings */
	void ReloadConfigOptions();

	/* parsed config settings */
	std::string m_strCommand;
	ULONG m_ulAttachMaxSize;
	LONGLONG m_ulParserMaxMemory;
	LONGLONG m_ulParserMaxCpuTime;
	std::set<std::string, stricmp_comparison> m_setMimeFilter;
	std::set<std::string, stricmp_comparison> m_setExtFilter;	
};

#endif /* ZARAFA_INDEXER_H */
