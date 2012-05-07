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

#include <stdio.h>
#include <algorithm>
#include <assert.h>
#include <limits.h>

#include "stringutil.h"
#include "ECConfigImpl.h"

#include "charset/convert.h"

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

const directive_t ECConfigImpl::s_sDirectives[] = {
	{ "include",	&ECConfigImpl::HandleInclude },
	{ "propmap",	&ECConfigImpl::HandlePropMap },
	{ NULL }
};

class PathCompare
{
public:
	PathCompare(const fs::path &ref): m_ref(ref) {}
	bool operator()(const fs::path &other) { return fs::equivalent(m_ref, other); }
private:
	const fs::path &m_ref;
};

// Configuration file parser

ECConfigImpl::ECConfigImpl(const configsetting_t *lpDefaults, const char **lpszDirectives)
{
	pthread_rwlock_init(&m_settingsRWLock, NULL);
	
	m_lpDefaults = lpDefaults;

	// allowed directives in this config object
	for (int i = 0; lpszDirectives != NULL && lpszDirectives[i] != NULL; i++)
		m_lDirectives.push_back(lpszDirectives[i]);

	InitDefaults(LOADSETTING_INITIALIZING | LOADSETTING_UNKNOWN | LOADSETTING_OVERWRITE);
}

bool ECConfigImpl::LoadSettings(const char *szFilename)
{
	m_szConfigFile = szFilename;	
	return InitConfigFile(LOADSETTING_OVERWRITE);
}

const char *ECConfigImpl::GetSettingsPath()
{
	return m_szConfigFile;
}

bool ECConfigImpl::ReloadSettings()
{
	// unsetting this value isn't possible
	if (!m_szConfigFile)
		return false;

	// Check if we can still open the main config file. Do not reset to Defaults
	FILE *fp = NULL;
	if(!(fp = fopen(m_szConfigFile, "rt"))) {
		return false;
	} else {
		fclose(fp);
	}

	// reset to defaults because unset items in config file should return to default values.
	InitDefaults(LOADSETTING_OVERWRITE_RELOAD);

	return InitConfigFile(LOADSETTING_OVERWRITE_RELOAD);
}

bool ECConfigImpl::AddSetting(const char *szName, const char *szValue, const unsigned int ulGroup)
{
	configsetting_t sSetting;

	sSetting.szName = szName;
	sSetting.szValue = szValue;
	sSetting.ulFlags = 0;
	sSetting.ulGroup = ulGroup;

	return AddSetting(&sSetting, ulGroup ? LOADSETTING_OVERWRITE_GROUP : LOADSETTING_OVERWRITE);
}

void freeSettings(settingmap_t::value_type entry)
{
	// see InsertOrReplace
	delete [] entry.second;
}

void ECConfigImpl::CleanupMap(settingmap_t *lpMap)
{
	if (!lpMap->empty())
		for_each(lpMap->begin(), lpMap->end(), freeSettings);
}

ECConfigImpl::~ECConfigImpl()
{
	pthread_rwlock_wrlock(&m_settingsRWLock);

	CleanupMap(&m_mapSettings);
	CleanupMap(&m_mapAliases);

	pthread_rwlock_unlock(&m_settingsRWLock);

	pthread_rwlock_destroy(&m_settingsRWLock);
}

/** 
 * Adds a new setting to the map, or replaces the current data.
 * Only the first 1024 bytes of the value are saved, longer values are truncated.
 * The map must be locked by the m_settingsRWLock.
 * 
 * @param lpMap settings map to set value in
 * @param s key to access map point
 * @param szValue new value to set in map
 */
void ECConfigImpl::InsertOrReplace(settingmap_t *lpMap, const settingkey_t &s, const char* szValue)
{
	pair<settingmap_t::iterator, bool> res;
	char* data = NULL;
	size_t len = std::min((size_t)1023, strlen(szValue));

	res = lpMap->insert(make_pair(s, (char*)NULL));
	if (res.second)
		data = res.first->second = new char[1024];
	else
		data = res.first->second;
	strncpy(data, szValue, len);
	data[len] = '\0';
}

char *ECConfigImpl::GetMapEntry(settingmap_t *lpMap, const char *szName)
{
	settingmap_t::iterator iterSettings;
	settingkey_t s;
	char *retval = NULL;

	memset(&s, 0, sizeof(s));
	strcpy(s.s, szName);

	pthread_rwlock_rdlock(&m_settingsRWLock);

	iterSettings = lpMap->find(s);
	if(iterSettings != lpMap->end())
		retval = iterSettings->second;
		
	pthread_rwlock_unlock(&m_settingsRWLock);

	return retval;
}

char *ECConfigImpl::GetSetting(const char *szName)
{
	return GetMapEntry(&m_mapSettings, szName);
}

char *ECConfigImpl::GetAlias(const char *szName)
{
	return GetMapEntry(&m_mapAliases, szName);
}

char* ECConfigImpl::GetSetting(const char *szName, char *equal, char *other)
{
	char *value = this->GetSetting(szName);
	if ((value == equal) || (value && equal && !strcmp(value, equal)))
		return other;
	else
		return value;
}

wchar_t* ECConfigImpl::GetSettingW(const char *szName)
{
	const char *value = GetSetting(szName);
	pair<ConvertCache::iterator, bool> result = m_convertCache.insert(ConvertCache::value_type(value, L""));
	ConvertCache::iterator iter = result.first;

	if (result.second)
		iter->second = convert_to<wstring>(value);

	return const_cast<wchar_t*>(iter->second.c_str());
}

wchar_t* ECConfigImpl::GetSettingW(const char *szName, wchar_t *equal, wchar_t *other)
{
	wchar_t *value = this->GetSettingW(szName);
	if ((value == equal) || (value && equal && !wcscmp(value, equal)))
		return other;
	else
		return value;
}

list<configsetting_t> ECConfigImpl::GetSettingGroup(unsigned int ulGroup)
{
	list<configsetting_t> lGroup;
	configsetting_t sSetting;

	for (settingmap_t::iterator iter = m_mapSettings.begin(); iter != m_mapSettings.end(); iter++) {
		if ((iter->first.ulGroup & ulGroup) == ulGroup) {
			if (CopyConfigSetting(&iter->first, iter->second, &sSetting))
				lGroup.push_back(sSetting);
		}
	}

	return lGroup;
}

bool ECConfigImpl::InitDefaults(unsigned int ulFlags)
{
	unsigned int i = 0;

	/* throw error? this is unacceptable! useless object, since it won't set new settings */
	if (!m_lpDefaults)
		return false;

	while (m_lpDefaults[i].szName != NULL) {
		if (m_lpDefaults[i].ulFlags & CONFIGSETTING_ALIAS) {
			/* Aliases are only initialized once */
			if (ulFlags & LOADSETTING_INITIALIZING)
				AddAlias(&m_lpDefaults[i]);
		} else
			AddSetting(&m_lpDefaults[i], ulFlags);
		i++;
	}

	return true;
}

bool ECConfigImpl::InitConfigFile(unsigned int ulFlags)
{
	bool bResult = false;

	assert(m_readFiles.empty());

	if (!m_szConfigFile)
		return false;

	bResult = ReadConfigFile(m_szConfigFile, ulFlags);

	m_readFiles.clear();

	return bResult;
}

bool ECConfigImpl::ReadConfigFile(const path_type &file, unsigned int ulFlags, unsigned int ulGroup)
{
	FILE *fp;
	bool bReturn = false;
	char cBuffer[MAXLINELEN] = {0};
	string strFilename;
	string strLine;
	string strName;
	string strValue;
	size_t pos;

	// Store the path of the previous file in case we're recursively processing files.
	fs::path prevFile = m_currentFile;
	
	// We need to keep track of the current path so we can handle relative includes in HandleInclude
	m_currentFile = file;

	if (!exists(file)) {
		errors.push_back("Config file '" + file.file_string() + "' does not exist.");
		goto exit;
	}
	if (is_directory(file)) {
		errors.push_back("Config file '" + file.file_string() + "' is a directory.");
		goto exit;
	}

	/* Check if we read this file before. */
	if (find_if(m_readFiles.begin(), m_readFiles.end(), PathCompare(file)) != m_readFiles.end()) {
		bReturn = true;
		goto exit;
	}

    m_readFiles.insert(file);

	if(!(fp = fopen(file.file_string().c_str(), "rt"))) {
		errors.push_back("Unable to open config file '" + file.file_string() + "'");
		goto exit;
	}

	while (!feof(fp)) {
		memset(&cBuffer, 0, sizeof(cBuffer));

		if (!fgets(cBuffer, sizeof(cBuffer), fp))
			continue;

		strLine = string(cBuffer);

		/* Skip empty lines any lines which start with # */
		if (strLine.empty() || strLine[0] == '#')
 			continue;

		/* Handle special directives which start with '!' */
		if (strLine[0] == '!') {
			if (!HandleDirective(strLine, ulFlags))
				goto exit;
			continue;
		}

		/* Get setting name */
		pos = strLine.find('=');
		if (pos != string::npos) {
			strName = strLine.substr(0, pos);
			strValue = strLine.substr(pos + 1);
		} else
			continue;

		/*
		 * The line is build up like this:
		 * config_name = bla bla
		 *
		 * Whe should clean it in such a way that it resolves to:
		 * config_name=bla bla
		 *
		 * Be carefull _not_ to remove any whitespace characters
		 * within the configuration value itself
		 */
		strName = trim(strName, " \t\r\n");
		strValue = trim(strValue, " \t\r\n");

		if(!strName.empty()) {
			// Save it
			configsetting_t setting = { strName.c_str(), strValue.c_str(), 0, ulGroup };
			AddSetting(&setting, ulFlags);
		}
	}

	fclose(fp);

	bReturn = true;

exit:
	// Restore the path of the previous file.
	m_currentFile.swap(prevFile);

	return bReturn;
}

bool ECConfigImpl::HandleDirective(string &strLine, unsigned int ulFlags)
{
	size_t pos = strLine.find_first_of(" \t", 1);
	string strName = strLine.substr(1, pos - 1);

	/* Check if this directive is known */
	for (int i = 0; s_sDirectives[i].lpszDirective != NULL; ++i) {
		if (strName.compare(s_sDirectives[i].lpszDirective) == 0) {
			/* Check if this directive is supported */
			list<string>::iterator f = find(m_lDirectives.begin(), m_lDirectives.end(), strName);
			if (f != m_lDirectives.end())
				return (this->*s_sDirectives[i].fExecute)(strLine.substr(pos).c_str(), ulFlags);

			warnings.push_back("Unsupported directive '" + strName + "' found!");
			return true;
		}
	}

	warnings.push_back("Unknown directive '" + strName + "' found!");
	return true;
}


#if (((BOOST_VERSION / 100) % 1000) < 36)
	#define remove_filename remove_leaf
#endif
bool ECConfigImpl::HandleInclude(const char *lpszArgs, unsigned int ulFlags)
{
	string strValue;
	path_type file;
	
	file = (strValue = trim(lpszArgs, " \t\r\n"));
	if (!file.is_complete()) {
		// Rebuild the path
		file = m_currentFile.remove_filename();
		file /= strValue;
	}
	
	return ReadConfigFile(file, ulFlags);
}

bool ECConfigImpl::HandlePropMap(const char *lpszArgs, unsigned int ulFlags)
{
	string	strValue;
	bool	bResult;

	strValue = trim(lpszArgs, " \t\r\n");
	bResult = ReadConfigFile(strValue.c_str(), LOADSETTING_UNKNOWN | LOADSETTING_OVERWRITE_GROUP, CONFIGGROUP_PROPMAP);

	return bResult;
}

bool ECConfigImpl::CopyConfigSetting(const configsetting_t *lpsSetting, settingkey_t *lpsKey)
{
	if (lpsSetting->szName == NULL || lpsSetting->szValue == NULL)
		return false;

	memset(lpsKey, 0, sizeof(*lpsKey));
	strncpy(lpsKey->s, lpsSetting->szName, sizeof(lpsKey->s));
	lpsKey->ulFlags = lpsSetting->ulFlags;
	lpsKey->ulGroup = lpsSetting->ulGroup;

	return true;
}

bool ECConfigImpl::CopyConfigSetting(const settingkey_t *lpsKey, const char *szValue, configsetting_t *lpsSetting)
{
	if (strlen(lpsKey->s) == 0 || szValue == NULL)
		return false;

	lpsSetting->szName = lpsKey->s;
	lpsSetting->szValue = szValue;
	lpsSetting->ulFlags = lpsKey->ulFlags;
	lpsSetting->ulGroup = lpsKey->ulGroup;

	return true;
}

bool ECConfigImpl::AddSetting(const configsetting_t *lpsConfig, unsigned int ulFlags)
{
	settingmap_t::iterator iterSettings;
	settingkey_t s;
	char *szAlias = NULL;
	bool bReturnValue = true;

	if (!CopyConfigSetting(lpsConfig, &s))
		return false;

	// Lookup name as alias
	szAlias = GetAlias(lpsConfig->szName);
	if (szAlias) {
		if (!(ulFlags & LOADSETTING_INITIALIZING))
			warnings.push_back("Option '" + string(lpsConfig->szName) + "' is deprecated! New name for option is '" + szAlias + "'.");
		strncpy(s.s, szAlias, sizeof(s.s));
	}

	pthread_rwlock_wrlock(&m_settingsRWLock);

	iterSettings = m_mapSettings.find(s);

	if (iterSettings == m_mapSettings.end()) {
		// new items from file are illegal, add error
		if (!(ulFlags & LOADSETTING_UNKNOWN)) {
			errors.push_back("Unknown option '" + string(lpsConfig->szName) + "' found!");
			goto exit;
		}
	} else {
		// Check for permissions before overwriting
		if (ulFlags & LOADSETTING_OVERWRITE_GROUP) {
			if (iterSettings->first.ulGroup != lpsConfig->ulGroup) {
				errors.push_back("option '" + string(lpsConfig->szName) + "' cannot be overridden (different group)!");
				bReturnValue = false;
				goto exit;
			}
		} else if (ulFlags & LOADSETTING_OVERWRITE_RELOAD) {
			if (!(iterSettings->first.ulFlags & CONFIGSETTING_RELOADABLE)) {
				bReturnValue = false;
				goto exit;
			}
		} else if (!(ulFlags & LOADSETTING_OVERWRITE)) {
			errors.push_back("option '" + string(lpsConfig->szName) + "' cannot be overridden!");
			bReturnValue = false;
			goto exit;
		}

		if (!(ulFlags & LOADSETTING_INITIALIZING) &&
			(iterSettings->first.ulFlags & CONFIGSETTING_UNUSED))
				warnings.push_back("Option '" + string(lpsConfig->szName) + "' is not used anymore.");
	}


	if (lpsConfig->szValue[0] == '$' && (lpsConfig->ulFlags & CONFIGSETTING_EXACT) == 0) {
		const char *szValue = getenv(lpsConfig->szValue + 1);
		if (szValue == NULL) {
			warnings.push_back("'" + string(lpsConfig->szValue + 1) + "' not found in environment, using '" + lpsConfig->szValue + "' for options '" + lpsConfig->szName + "'.");
			szValue = lpsConfig->szValue;
		}

		InsertOrReplace(&m_mapSettings, s, szValue);
	} else
		InsertOrReplace(&m_mapSettings, s, lpsConfig->szValue);

exit:
	pthread_rwlock_unlock(&m_settingsRWLock);
	return bReturnValue;
}

void ECConfigImpl::AddAlias(const configsetting_t *lpsAlias)
{
	settingkey_t s;

	if (!CopyConfigSetting(lpsAlias, &s))
		return;

	pthread_rwlock_wrlock(&m_settingsRWLock);
	InsertOrReplace(&m_mapAliases, s, lpsAlias->szValue);
	pthread_rwlock_unlock(&m_settingsRWLock);
}

bool ECConfigImpl::HasWarnings() {
	return !warnings.empty();
}

list<string>* ECConfigImpl::GetWarnings() {
	return &warnings;
}

bool ECConfigImpl::HasErrors() {
	settingmap_t::iterator iterSettings;

	/* First validate the configuration settings */
	pthread_rwlock_rdlock(&m_settingsRWLock);

	for (iterSettings = m_mapSettings.begin(); iterSettings != m_mapSettings.end(); iterSettings++) {
		if (iterSettings->first.ulFlags & CONFIGSETTING_NONEMPTY) {
			if (!iterSettings->second || strlen(iterSettings->second) == 0)
				errors.push_back("option '" + string(iterSettings->first.s) + "' cannot be empty!");
		}
	}
	
	pthread_rwlock_unlock(&m_settingsRWLock);

	return !errors.empty();
}

list<string>* ECConfigImpl::GetErrors() {
	return &errors;
}


bool ECConfigImpl::WriteSettingToFile(const char *szName, const char *szValue, const char* szFileName)
{
	string strOutFileName;
	string strLine;
	string strName;

	strOutFileName = "outfile.cfg";

	ifstream in(szFileName);

	if( !in.is_open())
	{
		cout << "Input confif file failed to open creating it\n";
		std::ofstream createFile(szFileName);
		createFile.close();

		in.open(szFileName);
		if ( !in.is_open())
		{
			cout << "Input file failed to open after trying to create it" << endl;
			return false;
		}
	}

	// open temp output file
	ofstream out(strOutFileName.c_str());

	WriteLinesToFile(szName, szValue, in, out, true);

	in.close();
	out.close();

	// delete the original file
	remove(szFileName);

	// rename out file to new
	rename(strOutFileName.c_str(),szFileName);

	return true;
}

bool ECConfigImpl::WriteSettingsToFile(const char* szFileName)
{
	string strName;
	fs::path pathOutFile;
	fs::path pathBakFile;

	pathOutFile = pathBakFile = szFileName;
	pathOutFile.remove_filename() /= "config_out.cfg";
	pathBakFile.remove_filename() /= "config_bak.cfg";

	ifstream in(szFileName);

	if( !in.is_open())
	{
		cout << "Input config file failed to open creating it\n";
		std::ofstream createFile(szFileName);
		createFile.close();

		in.open(szFileName);
		if ( !in.is_open())
		{
			cout << "Input file failed to open after trying to create it" << endl;
			return false;
		}
	}

	// open temp output file
	ofstream out(pathOutFile.file_string().c_str());

	settingmap_t::iterator iterSettings;
	const char* szName = NULL;
	const char* szValue = NULL;

	for(iterSettings = m_mapSettings.begin(); 
		iterSettings != m_mapSettings.end();
		iterSettings++)
	{

		szName = iterSettings->first.s;
		szValue = iterSettings->second;

		this->WriteLinesToFile(szName, szValue, in, out, false);
	}
	in.close();
	out.close();

// the stdio functions does not work in win release mode in some cases
	remove(szFileName);
	rename(pathOutFile.file_string().c_str(),szFileName);

	return true;
}


void ECConfigImpl::WriteLinesToFile(const char* szName, const char* szValue, ifstream& in, ofstream& out, bool bWriteAll)
{
	string strLine;
	string strTmp;
	bool bFound = false;

	if (bWriteAll == true)
	{
		// This is done in order to keep comments and config order if there already exist a config
		while( getline(in,strLine) )
		{
			/* Skip empty lines any lines which start with # */
			if (strLine.empty() || strLine[0] == '#'){
				out << strLine << "\n";
				continue;
			}

			/* Handle special directives which start with '!' */
			if (strLine[0] == '!') {
				out << strLine << "\n";
				continue;
			}

			/* Get setting name */
			size_t pos = strLine.find('=');
			if (pos != string::npos) {
				string strName = strLine.substr(0, pos);
				strName = trim(strName, " \t\r\n");

				// Found it, set new value for setting
				if (strName == szName)
				{
					//This is a empty config add comment in front
					if (string(szValue) == "")
						strTmp = "#" + strName + " = " + szValue + "\n";	
					else
						strTmp = strName + " = " + szValue + "\n";	

					out << strTmp;
					bFound = true;
				}
				else
				{
					out << strLine << "\n";
				}
			} else
				continue;
		}
	}

	if (!bFound || bWriteAll == false) // Not found in original config, this is still valid, add it to output
	{				
		if (string(szValue) == "")
			strTmp = "#" + string(szName) + " = " + szValue + "\n";	
		else
			strTmp = string(szName) + " = " + szValue + "\n";	

		out << strTmp;
	}
}
