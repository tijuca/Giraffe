/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include <kopano/platform.h>
#include <map>
#include <new>
#include <string>
#include "ECConfigImpl.h"
#include <kopano/charset/convert.h>

namespace KC {

ECConfig *ECConfig::Create(const configsetting_t *lpDefaults,
    const char *const *lpszDirectives)
{
	return new ECConfigImpl(lpDefaults, lpszDirectives);
}

ECConfig *ECConfig::Create(const std::nothrow_t &,
    const configsetting_t *dfl, const char *const *direc)
{
	return new(std::nothrow) ECConfigImpl(dfl, direc);
}

/**
 * Get the default path for the configuration file specified with lpszBasename.
 * Usually this will return '/etc/kopano/<lpszBasename>'. However, the path to
 * the configuration files can be altered by setting the 'KOPANO_CONFIG_PATH'
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
	// @todo: Check how this behaves with dlopen,dlclose,dlopen,etc...
	// We use a static map here to store the strings we're going to return.
	// This could have been a global, but this way everything is kept together.
	static std::map<std::string, std::string> s_mapPaths;

	if (!lpszBasename)
		lpszBasename = "";
	auto result = s_mapPaths.emplace(lpszBasename, "");
	if (result.second == true) {		// New item added, so create the actual path
		const char *lpszDirname = getenv("KOPANO_CONFIG_PATH");
		if (!lpszDirname || lpszDirname[0] == '\0')
			lpszDirname = "/etc/kopano";
		result.first->second = std::string(lpszDirname) + "/" + lpszBasename;
	}
	return result.first->second.c_str();
}

} /* namespace */
