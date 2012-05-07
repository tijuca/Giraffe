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

#ifndef ECCONFIGCHECK_H
#define ECCONFIGCHECK_H

#include <list>
#include <map>
#include <string>

enum CHECK_STATUS {
	CHECK_OK,
	CHECK_WARNING,
	CHECK_ERROR
};

enum CHECK_FLAGS {
	CONFIG_MANDATORY		= (1 << 0),	/* Configuration option is mandatory */
	CONFIG_HOSTED_USED		= (1 << 1),	/* Configuration option is only usable with hosted enabled */
	CONFIG_HOSTED_UNUSED	= (1 << 2),	/* Configuration option is only usable with hosted disabled */
	CONFIG_MULTI_USED		= (1 << 3),	/* Configuration option is only usable with multi-server enabled */
	CONFIG_MULTI_UNUSED		= (1 << 4)	/* Configuration option is only usable with multi-server disabled */
};

/* Each subclass of ECConfigCheck can register check functions
 * for the configuration options. */
struct config_check_t {
	bool hosted;
	bool multi;
	std::string option1;
	std::string option2;

	std::string value1;
	std::string value2;

	int (*check)(config_check_t *check);
};

class ECConfigCheck {
public:
	ECConfigCheck(const char *lpszName, const char *lpszConfigFile);
	virtual ~ECConfigCheck();

	/* Must be overwritten by subclass */
	virtual void loadChecks() = 0;

	bool isDirty();
	void setHosted(bool hosted);
	void setMulti(bool multi);
	void validate();

	std::string getSetting(std::string option);

protected:
	static void printError(std::string option, std::string message);
	static void printWarning(std::string option, std::string message);

	void addCheck(std::string option, unsigned int flags,
				  int (*check)(config_check_t *check) = NULL);
	void addCheck(std::string option1, std::string option2, unsigned int flags,
				  int (*check)(config_check_t *check) = NULL);

private:
	void readConfigFile(const char *lpszConfigFile);

	/* Generic check functions */
	static int testMandatory(config_check_t *check);
	static int testUsedWithHosted(config_check_t *check);
	static int testUsedWithoutHosted(config_check_t *check);
	static int testUsedWithMultiServer(config_check_t *check);
	static int testUsedWithoutMultiServer(config_check_t *check);

protected:
	static int testDirectory(config_check_t *check);
	static int testFile(config_check_t *check);
	static int testCharset(config_check_t *check);
	static int testBoolean(config_check_t *check);
	static int testNonZero(config_check_t *check);

private:
	/* Generic addCheckFunction */
	void addCheck(config_check_t config_check, unsigned int flags);

	/* private variables */
	const char *m_lpszName;
	const char *m_lpszConfigFile;

	std::map<std::string, std::string> m_mSettings;
	std::list<config_check_t> m_lChecks;

	bool m_bDirty;
	bool m_bHosted;
	bool m_bMulti;
};

#endif

