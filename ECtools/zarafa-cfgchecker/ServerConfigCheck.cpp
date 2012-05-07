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
#include "ServerConfigCheck.h"
#include "stringutil.h"

ServerConfigCheck::ServerConfigCheck(const char *lpszConfigFile) : ECConfigCheck("Server Configuration file", lpszConfigFile)
{
	std::string setting;

	setting = getSetting("enable_hosted_zarafa");
	if (!setting.empty())
		setHosted(parseBool(setting));

	setting = getSetting("enable_distributed_zarafa");
	if (!setting.empty())
		setMulti(parseBool(setting));
}

ServerConfigCheck::~ServerConfigCheck()
{
}

void ServerConfigCheck::loadChecks()
{
	addCheck("attachment_storage", 0, &testAttachment);
	addCheck("attachment_storage", "attachment_path", 0, &testAttachmentPath);

	addCheck("user_plugin", 0, &testPlugin);
	addCheck("user_plugin", "user_plugin_config", 0, &testPluginConfig);
	addCheck("user_plugin", "plugin_path", 0, &testPluginPath);

	addCheck("createuser_script", 0, &testFile);
	addCheck("deleteuser_script", 0, &testFile);
	addCheck("creategroup_script", 0, &testFile);
	addCheck("deletegroup_script", 0, &testFile);
	addCheck("createcompany_script", CONFIG_HOSTED_USED, &testFile);
	addCheck("deletecompany_script", CONFIG_HOSTED_USED, &testFile);

	addCheck("enable_hosted_zarafa", 0, &testBoolean);
	addCheck("storename_format", 0, &testStorename);
	addCheck("user_plugin", "loginname_format", 0, &testLoginname);

	addCheck("enable_distributed_zarafa", 0, &testBoolean);
	addCheck("server_name", CONFIG_MULTI_USED);
// 	addCheck("thread_stacksize", 0, &testMinInt, 25);


	addCheck("enable_gab", 0, &testBoolean);
	addCheck("enable_sso_ntlmauth", 0, &testBoolean);
	addCheck("sync_log_all_changes", 0, &testBoolean);
	addCheck("client_update_enabled", 0, &testBoolean);
	addCheck("hide_everyone", 0, &testBoolean);
	addCheck("index_services_enabled", 0, &testBoolean);
	addCheck("enable_enhanced_ics", 0, &testBoolean);

	addCheck("softdelete_lifetime", 0, &testNonZero);

	addCheck("auth_method", 0, &testAuthMethod);
}

int ServerConfigCheck::testAttachment(config_check_t *check)
{
	if (check->value1.empty())
		return CHECK_OK;

	if (check->value1 == "database" || check->value1 == "files")
		return CHECK_OK;

	printError(check->option1, "contains unknown storage type: \"" + check->value1 + "\"");
	return CHECK_ERROR;
}

int ServerConfigCheck::testAttachmentPath(config_check_t *check)
{
	if (check->value1 != "files")
		return CHECK_OK;

	config_check_t check2;
	check2.hosted = check->hosted;
	check2.multi = check->multi;
	check2.option1 = check->option2;
	check2.value1 = check->value2;

	return testDirectory(&check2);
}

int ServerConfigCheck::testPlugin(config_check_t *check)
{
	if (check->value1.empty()) {
		printWarning(check->option1, "Plugin not set, defaulting to 'db' plugin");
		return CHECK_OK;
	}

	if (check->hosted && check->value1 == "unix") {
		printError(check->option1, "Unix plugin does not support multi-tenancy");
		return CHECK_ERROR;
	}

	if (check->multi && check->value1 != "ldapms") {
		printError(check->option1, check->value1 + " plugin does not support multiserver");
		return CHECK_ERROR;
	}

	if (check->multi && check->value1 == "ldap") {
		printError(check->option1, "Unix plugin does not support multiserver");
		return CHECK_ERROR;
	}

	if (check->value1 == "ldap" || check->value1 == "ldapms" || check->value1 == "unix" || check->value1 == "db")
		return CHECK_OK;

	printError(check->option1, "contains unknown plugin: \"" + check->value1 + "\"");
	return CHECK_ERROR;
}

int ServerConfigCheck::testPluginConfig(config_check_t *check)
{
	if (check->value1 != "ldap" && check->value1 != "unix")
		return CHECK_OK;

	config_check_t check2;
	check2.hosted = check->hosted;
	check2.option1 = check->option2;
	check2.value1 = check->value2;

	return testFile(&check2);
}

int ServerConfigCheck::testPluginPath(config_check_t *check)
{
	if (check->value1 != "ldap" && check->value1 != "unix")
		return CHECK_OK;

	config_check_t check2;
	check2.hosted = check->hosted;
	check2.option1 = check->option2;
	check2.value1 = check->value2;

	return testDirectory(&check2);
}

int ServerConfigCheck::testStorename(config_check_t *check)
{
	if (!check->hosted) {
		if (check->value1.find("%c") != std::string::npos) {
			printError(check->option1, "multi-tenancy disabled, but value contains %c: " + check->value1);
			return CHECK_ERROR;
		}
	}

	return CHECK_OK;
}

int ServerConfigCheck::testLoginname(config_check_t *check)
{
	/* LDAP has no rules for loginname */
	if (check->value1 == "ldap")
		return CHECK_OK;

	if (check->value1 == "unix") {
		if (check->value2.find("%c") != std::string::npos) {
			printError(check->option2, "contains %c but this is not supported by Unix plugin");
			return CHECK_ERROR;
		}
		return CHECK_OK;
	}

	/* DB plugin, must have %c in loginname format when hosted is enabled */
	if (check->hosted) {
		if (check->value2.find("%c") == std::string::npos) {
			printError(check->option2, "multi-tenancy enabled, but value does not contain %c: \"" + check->value2 + "\"");
			return CHECK_ERROR;
		}
	} else {
		if (check->value2.find("%c") != std::string::npos) {
			printError(check->option2, "multi-tenancy disabled, but value contains %c: \"" + check->value2 + "\"");
			return CHECK_ERROR;
		}
	}

	return CHECK_OK;
}

int ServerConfigCheck::testAuthMethod(config_check_t *check)
{
	if (check->value1.empty() || check->value1 == "plugin" || check->value1 == "pam" || check->value1 == "kerberos")
		return CHECK_OK;
	printError(check->option1, "must be one of 'plugin', 'pam' or 'kerberos': " + check->value1);
	return CHECK_ERROR;
}
