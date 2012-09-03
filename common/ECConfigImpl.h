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

#ifndef ECCONFIGIMPL_H
#define ECCONFIGIMPL_H

using namespace std;

#include "ECConfig.h"

#include <map>
#include <set>
#include <list>
#include <string>
#include <string.h>
#include <pthread.h>

#include <boost/filesystem/path.hpp>

#include <iostream>
#include <fstream>

struct settingkey_t {
	char s[256];
	unsigned short ulFlags;
	unsigned short ulGroup;
};

struct settingcompare
{
	bool operator()(const settingkey_t &a, const settingkey_t &b) const
	{
		return strcmp(a.s, b.s) < 0;
	}
};

#define MAXLINELEN 4096

class ECConfigImpl;

/* Note: char* in map is allocated ONCE to 1024, and GetSetting will always return the same pointer to this buffer */
typedef std::map<settingkey_t, char*, settingcompare> settingmap_t;
typedef bool (ECConfigImpl::*directive_func_t)(const char *, unsigned int);
typedef struct {
	const char			*lpszDirective;
	directive_func_t	fExecute;
} directive_t;

/*
 * Flags for the InitDefaults & InitConfigFile functions
 */
#define LOADSETTING_INITIALIZING		0x0001	/* ECConfig is initializing, turns on extra debug information */
#define LOADSETTING_UNKNOWN				0x0002	/* Allow adding new configuration options */
#define LOADSETTING_OVERWRITE			0x0004	/* Allow overwriting predefined configuration options */
#define LOADSETTING_OVERWRITE_GROUP		0x0008	/* Same as CONFIG_LOAD_OVERWRITE but only if options are in the same group */
#define LOADSETTING_OVERWRITE_RELOAD	0x0010	/* Same as CONFIG_LOAD_OVERWRITE but only if option is marked reloadable */
#define LOADSETTING_CMDLINE_PARAM		0x0020	/* This setting is being set from commandline parameters. Sets the option non-reloadable */

class ECConfigImpl : public ECConfig {
public:
	ECConfigImpl(const configsetting_t *lpDefaults, const char **lpszDirectives);
	~ECConfigImpl();

	bool	LoadSettings(const char *szFilename);
	virtual bool    ParseParams(int argc, char *argv[], int *lpargidx);
	const char *	GetSettingsPath();
	bool	ReloadSettings();

	bool	AddSetting(const char *szName, const char *szValue, const unsigned int ulGroup = 0);

	void	AddWriteSetting(const char *szName, const char *szValue, const unsigned int ulGroup = 0);

	char*	GetSetting(const char *szName);
	char*	GetSetting(const char *szName, char *equal, char *other);
	wchar_t* GetSettingW(const char *szName);
	wchar_t* GetSettingW(const char *szName, wchar_t *equal, wchar_t *other);

	std::list<configsetting_t> GetSettingGroup(unsigned int ulGroup);

	bool	HasWarnings();
	std::list<std::string>* GetWarnings();
	bool	HasErrors();
	std::list<std::string>* GetErrors();

	bool    WriteSettingToFile(const char *szName, const char *szValue, const char* szFileName);
	bool WriteSettingsToFile(const char* szFileName);

private:
	typedef boost::filesystem::path path_type;

	bool	InitDefaults(unsigned int ulFlags);
	bool	InitConfigFile(unsigned int ulFlags);
	bool	ReadConfigFile(const path_type &file, unsigned int ulFlags, unsigned int ulGroup = 0);

	bool	HandleDirective(std::string &strLine, unsigned int ulFlags);
	bool	HandleInclude(const char *lpszArgs, unsigned int ulFlags);
	bool	HandlePropMap(const char *lpszArgs, unsigned int ulFlags);

	size_t  GetSize(const char *szValue);
	void	InsertOrReplace(settingmap_t *lpMap, const settingkey_t &s, const char* szValue, bool bIsSize);

	char*	GetMapEntry(settingmap_t *lpMap, const char *szName);
	char*	GetAlias(const char *szAlias);

	bool	AddSetting(const configsetting_t *lpsConfig, unsigned int ulFlags);
	void	AddAlias(const configsetting_t *lpsAlias);

	void	CleanupMap(settingmap_t *lpMap);
	bool	CopyConfigSetting(const configsetting_t *lpsSetting, settingkey_t *lpsKey);
	bool	CopyConfigSetting(const settingkey_t *lpsKey, const char *szValue, configsetting_t *lpsSetting);

	void    WriteLinesToFile(const char* szName, const char* szValue, ifstream& in, ofstream& out, bool bWriteAll);

private:
	const configsetting_t	*m_lpDefaults;
	const char*				m_szConfigFile;
	std::list<std::string>	m_lDirectives;

	/* m_mapSettings & m_mapAliases are protected by m_settingsLock */
	pthread_rwlock_t m_settingsRWLock;

	settingmap_t			m_mapSettings;
	settingmap_t			m_mapAliases;
	std::list<std::string>	warnings;
	std::list<std::string>	errors;
	
	path_type			m_currentFile;
	std::set<path_type>	m_readFiles;

	typedef std::map<const char*, std::wstring>	ConvertCache;
	ConvertCache		m_convertCache;

	static const directive_t	s_sDirectives[];
};

#endif // ECCONFIGIMPL_H
