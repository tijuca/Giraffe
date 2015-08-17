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

#ifndef ECCONFIG_H
#define ECCONFIG_H

#include <list>
#include <string>

struct configsetting_t {
	const char *szName;
	const char *szValue;
	unsigned short ulFlags;
#define CONFIGSETTING_ALIAS			0x0001
#define CONFIGSETTING_RELOADABLE	0x0002
#define CONFIGSETTING_UNUSED		0x0004
#define CONFIGSETTING_NONEMPTY		0x0008
#define CONFIGSETTING_EXACT			0x0010
#define CONFIGSETTING_SIZE			0x0020
	unsigned short ulGroup;
#define CONFIGGROUP_PROPMAP			0x0001
};

#ifdef UNICODE
#define GetConfigSetting(_config, _name) ((_config)->GetSettingW(_name))
#else
#define GetConfigSetting(_config, _name) ((_config)->GetSetting(_name))
#endif

static const char *lpszDEFAULTDIRECTIVES[] = { "include", NULL };

class ECConfig {
public:
	static ECConfig* Create(const configsetting_t *lpDefaults, const char **lpszDirectives = lpszDEFAULTDIRECTIVES);
	static const char* GetDefaultPath(const char* lpszBasename);

	virtual ~ECConfig(void) {};

	virtual bool	LoadSettings(const char *szFilename) = 0;
	virtual bool	LoadSettings(const wchar_t *szFilename);
	virtual bool    ParseParams(int argc, char *argv[], int *lpargidx) = 0;
	virtual const char*	GetSettingsPath() = 0;
	virtual bool	ReloadSettings() = 0;

	virtual bool	AddSetting(const char *szName, const char *szValue, const unsigned int ulGroup = 0) = 0;

	virtual const char *GetSetting(const char *szName) = 0;
	virtual const char *GetSetting(const char *szName, const char *equal, const char *other) = 0;
	virtual const wchar_t *GetSettingW(const char *szName) = 0;
	virtual const wchar_t *GetSettingW(const char *szName, const wchar_t *equal, const wchar_t *other) = 0;

	virtual std::list<configsetting_t> GetSettingGroup(unsigned int ulGroup) = 0;
	virtual std::list<configsetting_t> GetAllSettings() = 0;

	virtual bool	HasWarnings() = 0;
	virtual const std::list<std::string> *GetWarnings(void) = 0;
	virtual bool	HasErrors() = 0;
	virtual const std::list<std::string> *GetErrors(void) = 0;

	virtual bool WriteSettingToFile(const char *szName, const char *szValue, const char* szFileName) = 0;
	virtual bool WriteSettingsToFile(const char* szFileName) = 0;
};

#endif // ECCONFIG_H
