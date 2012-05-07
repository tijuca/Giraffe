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

#include <iostream>
#include <algorithm>

#include <sys/stat.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;

#include "ECConfigCheck.h"

ECConfigCheck::ECConfigCheck(const char *lpszName, const char *lpszConfigFile)
{
	m_lpszName = lpszName;
	m_lpszConfigFile = lpszConfigFile;
	m_bDirty = false;
	m_bHosted = false;
	m_bMulti = false;

	readConfigFile(lpszConfigFile);
}

ECConfigCheck::~ECConfigCheck()
{
}

string clearCharacters(string s, string whitespaces)
{
	size_t pos = 0;

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
	pos = s.find_first_not_of(whitespaces);
	s.erase(0, pos);

	pos = s.find_last_not_of(whitespaces);
	if (pos != string::npos)
		s.erase(pos + 1, string::npos);

	return s;
}

void ECConfigCheck::readConfigFile(const char *lpszConfigFile)
{
	FILE *fp = NULL;
	char cBuffer[1024];
	string strLine;
	string strName;
	string strValue;
	size_t pos;

	if (!lpszConfigFile) {
		m_bDirty = true;
		return;
	}

	if(!(fp = fopen(lpszConfigFile, "rt"))) {
		m_bDirty = true;
		return;
	}

	while (!feof(fp)) {
		memset(&cBuffer, 0, sizeof(cBuffer));

		if (!fgets(cBuffer, sizeof(cBuffer), fp))
			continue;

		strLine = string(cBuffer);

		/* Skip empty lines any lines which start with # */
		if (strLine.empty() || strLine[0] == '#')
			continue;

		/* Get setting name */
		pos = strLine.find('=');
		if (pos != string::npos) {
			strName = strLine.substr(0, pos);
			strValue = strLine.substr(pos + 1);
		} else
			continue;

		strName = clearCharacters(strName, " \t\r\n");
		strValue = clearCharacters(strValue, " \t\r\n");

		if(!strName.empty())
			m_mSettings[strName] = strValue;
	}

	fclose(fp);
}

bool ECConfigCheck::isDirty()
{
	if (m_bDirty)
		cerr << "Validation of " << m_lpszName << " failed: file could not be read (" << m_lpszConfigFile << ")" << endl;
	return m_bDirty;
}

void ECConfigCheck::setHosted(bool hosted)
{
	m_bHosted = hosted;
}

void ECConfigCheck::setMulti(bool multi)
{
	m_bMulti = multi;
}

void ECConfigCheck::validate()
{
	int warnings = 0;
	int errors = 0;

	cout << "Starting configuration validation of " << m_lpszName << endl;

	for (list<config_check_t>::iterator it = m_lChecks.begin(); it != m_lChecks.end(); it++) {
		it->hosted = m_bHosted;
		it->multi = m_bMulti;
		it->value1 = getSetting(it->option1);
		it->value2 = getSetting(it->option2);
		int retval = 0;

		if (it->check)
			retval = it->check(&(*it));

		warnings += (retval == CHECK_WARNING);
		errors += (retval == CHECK_ERROR);
	}

	cout << "Validation of " << m_lpszName << " ended with " << warnings << " warnings and " << errors << " errors" << endl;
}

int ECConfigCheck::testMandatory(config_check_t *check)
{
	if (!check->value1.empty())
		return CHECK_OK;

	printError(check->option1, "required option is empty");
	return CHECK_ERROR;
}

int ECConfigCheck::testDirectory(config_check_t *check)
{
	struct stat statfile;

	if (check->value1.empty())
		return CHECK_OK;

	// check if path exists, and is a directory (not a symlink)
	if (stat(check->value1.c_str(), &statfile) == 0 && S_ISDIR(statfile.st_mode))
		return CHECK_OK;

	printError(check->option1, "does not point to existing direcory: \"" + check->value1 + "\"");
	return CHECK_ERROR;
}

int ECConfigCheck::testFile(config_check_t *check)
{
	struct stat statfile;

	if (check->value1.empty())
		return CHECK_OK;

	// check if file exists, and is a normal file (not a symlink, or a directory
	if (stat(check->value1.c_str(), &statfile) == 0 && S_ISREG(statfile.st_mode))
		return CHECK_OK;

	printError(check->option1, "does not point to existing file: \"" + check->value1 + "\"");
	return CHECK_ERROR;
}

int ECConfigCheck::testUsedWithHosted(config_check_t *check)
{
	if (check->hosted)
		return CHECK_OK;

	if (check->value1.empty())
		return CHECK_OK;

	printWarning(check->option1, "Multi-company disabled: this option will be ignored");
	return CHECK_WARNING;
}

int ECConfigCheck::testUsedWithoutHosted(config_check_t *check)
{
	if (!check->hosted)
		return CHECK_OK;

	if (check->value1.empty())
		return CHECK_OK;

	printWarning(check->option1, "Multi-company enabled: this option will be ignored");
	return CHECK_WARNING;
}

int ECConfigCheck::testUsedWithMultiServer(config_check_t *check)
{
	if (check->multi)
		return CHECK_OK;

	if (check->value1.empty())
		return CHECK_OK;

	printWarning(check->option1, "Multi-server disabled: this option will be ignored");
	return CHECK_WARNING;
}

int ECConfigCheck::testUsedWithoutMultiServer(config_check_t *check)
{
	if (!check->multi)
		return CHECK_OK;

	if (check->value1.empty())
		return CHECK_OK;

	printWarning(check->option1, "Multi-server enabled: this option will be ignored");
	return CHECK_WARNING;
}

int ECConfigCheck::testCharset(config_check_t *check)
{
	FILE *fp = NULL;

	/* When grepping iconv output, all lines have '//' appended,
	 * additionally all charsets are uppercase */
	transform(check->value1.begin(), check->value1.end(), check->value1.begin(), ::toupper);
	fp = popen(("iconv -l | grep -x \"" + check->value1 + "//\"").c_str(), "r");

	if (fp) {
		char buffer[50];
		string output;

		memset(buffer, 0, sizeof(buffer));

		fread(buffer, sizeof(buffer), 1, fp);
		output = buffer;

		fclose(fp);

		if (output.find(check->value1) == string::npos) {
			printError(check->option1, "contains unknown chartype \"" + check->value1 + "\"");
			return CHECK_ERROR;
		}
	} else {
		printWarning(check->option1, "Failed to validate charset");
		return CHECK_WARNING;
	}

	return CHECK_OK;
}

int ECConfigCheck::testBoolean(config_check_t *check)
{
	transform(check->value1.begin(), check->value1.end(), check->value1.begin(), ::tolower);

	if (check->value1.empty() ||
		check->value1 == "true" ||
		check->value1 == "false" ||
		check->value1 == "yes" ||
		check->value1 == "no")
			return CHECK_OK;

	printError(check->option1, "does not contain boolean value: \"" + check->value1 + "\"");
	return CHECK_ERROR;
}

int ECConfigCheck::testNonZero(config_check_t *check)
{
	if (check->value1.empty() || atoi(check->value1.c_str()))
		return CHECK_OK;
	printError(check->option1, "must contain a positive (non-zero) value: " + check->value1);
	return CHECK_ERROR;
}

void ECConfigCheck::addCheck(config_check_t check, unsigned int flags)
{
	if (flags & CONFIG_MANDATORY) {
		if (!check.option1.empty())
			addCheck(check.option1, 0, &testMandatory);
		if (!check.option2.empty())
			addCheck(check.option2, 0, &testMandatory);
	}

	if (flags & CONFIG_HOSTED_UNUSED) {
		if (!check.option1.empty())
			addCheck(check.option1, 0, &testUsedWithoutHosted);
		if (!check.option2.empty())
			addCheck(check.option2, 0, &testUsedWithoutHosted);
	}
	if (flags & CONFIG_HOSTED_USED) {
		if (!check.option1.empty())
			addCheck(check.option1, 0, &testUsedWithHosted);
		if (!check.option2.empty())
			addCheck(check.option2, 0, &testUsedWithHosted);
	}

	if (flags & CONFIG_MULTI_UNUSED) {
		if (!check.option1.empty())
			addCheck(check.option1, 0, &testUsedWithoutMultiServer);
		if (!check.option2.empty())
			addCheck(check.option2, 0, &testUsedWithoutMultiServer);
	}
	if (flags & CONFIG_MULTI_USED) {
		if (!check.option1.empty())
			addCheck(check.option1, 0, &testUsedWithMultiServer);
		if (!check.option2.empty())
			addCheck(check.option2, 0, &testUsedWithMultiServer);
	}

	m_lChecks.push_back(check);
}

void ECConfigCheck::addCheck(string option, unsigned int flags,
							 int (*check)(config_check_t *check))
{
	config_check_t config_check;

	config_check.option1 = option;
	config_check.option2 = "";
	config_check.check = check;

	addCheck(config_check, flags);
}

void ECConfigCheck::addCheck(string option1, string option2, unsigned int flags,
							 int (*check)(config_check_t *check))
{
	config_check_t config_check;

	config_check.option1 = option1;
	config_check.option2 = option2;
	config_check.check = check;

	addCheck(config_check, flags);
}

string ECConfigCheck::getSetting(string option)
{
	return m_mSettings[option];
}

void ECConfigCheck::printError(string option, string message)
{
	cerr << "[ERROR] " << option << ": " << message << endl;
}

void ECConfigCheck::printWarning(string option, string message)
{
	cerr << "[WARNING] " << option << ": " << message << endl;
}
