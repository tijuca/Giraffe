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
#include "ECConfigImpl.h"
#include "charset/convert.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECConfig* ECConfig::Create(const configsetting_t *lpDefaults, const char **lpszDirectives)
{
	return new ECConfigImpl(lpDefaults, lpszDirectives);
}

ECConfig::~ECConfig()
{ }

bool ECConfig::LoadSettings(const wchar_t *szFilename)
{
	convert_context converter;
	return LoadSettings(converter.convert_to<char*>(szFilename));
}


/**
 * Get the default path for the configuration file specified with lpszBasename.
 * Usually this will return '/etc/zarafa/<lpszBasename>'. However, the path to
 * the configuration files can be altered by setting the 'ZARAFA_CONFIG_PATH'
 * environment variable.
 *
 * @param[in]	lpszBasename
 * 						The basename of the requested configuration file. Passing
 * 						NULL or an empty string will result in the default path
 * 						to be returned.
 * 
 * @returns		The full path to the requested configuration file. Memory for
 * 				the returned data is allocated in this function and will be freed
 * 				at program termination.
 *
 * @warning This function is not thread safe!
 */
const char* ECConfig::GetDefaultPath(const char* lpszBasename)
{
	typedef map<string, string> stringmap_t;
	typedef stringmap_t::iterator iterator_t;
	typedef pair<iterator_t, bool> insertresult_t;

	// @todo: Check how this behaves with dlopen,dlclose,dlopen,etc...
	// We use a static map here to store the strings we're going to return.
	// This could have been a global, but this way everything is kept together.
	static stringmap_t s_mapPaths;

	if (!lpszBasename)
		lpszBasename = "";

	insertresult_t result = s_mapPaths.insert(stringmap_t::value_type(lpszBasename, string()));
	if (result.second == true) {		// New item added, so create the actual path
		const char *lpszDirname = getenv("ZARAFA_CONFIG_PATH");
		if (!lpszDirname || lpszDirname[0] == '\0')
			lpszDirname = "/etc/zarafa";
		result.first->second = string(lpszDirname) + "/" + lpszBasename;
	}
	return result.first->second.c_str();
}
