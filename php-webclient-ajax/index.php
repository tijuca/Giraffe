<?php
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

?>
<?php
	/**
	* This is the entry point for every request that should return HTML
	* 
	* (one exception is that it also returns translated text for javascript)
	*/

	// load config file
	if (!file_exists("config.php")){
		die("<strong>config.php is missing!</strong>");
	}
	include("config.php");
	include("defaults.php");

	// This checks whether the Multi-User Calendar is present
	define('MUC_AVAILABLE', (is_file('client/modules/multiusercalendarmodule.js')));
	
	ob_start();
	setlocale(LC_CTYPE, "en_US.UTF-8");

	// Set timezone
	if(function_exists("date_default_timezone_set")) {
		if(defined('TIMEZONE') && TIMEZONE) {
			date_default_timezone_set(TIMEZONE);
		} else if(!ini_get('date.timezone')) {
			date_default_timezone_set('Europe/London');
		}
	}

	// Start the session
	session_name(COOKIE_NAME);

	if($_POST && array_key_exists(COOKIE_NAME, $_POST))
		session_id($_POST[COOKIE_NAME]);

	session_start();
	
	// check if config is correct
	if (defined("CONFIG_CHECK")){
		include("server/class.configcheck.php");
		new ConfigCheck(CONFIG_CHECK);
	}

	// Include the files
	require("mapi/mapi.util.php");
	require("mapi/mapicode.php");
	require("mapi/mapidefs.php");
	require("mapi/mapitags.php");
	require("mapi/mapiguid.php");
	require("server/util.php");

	require("server/core/constants.php");
	require("server/core/class.conversion.php");
	require("server/core/class.mapisession.php");
	
	require("server/core/class.request.php");
	require("server/core/class.settings.php");
	require("server/core/class.language.php");

	require("server/core/class.state.php");

	require("server/core/class.pluginmanager.php");
	require("server/core/class.plugin.php");
	
	// Destroy session if an user loggs out
	if($_GET && array_key_exists("logout", $_GET)) {
		$_SESSION = array();
		
		if (isset($_COOKIE[session_name()])) {
			setcookie(session_name(), '', time()-42000, '/');
		}
		session_destroy();

		header("Refresh: 0; url=index.php");
		exit;
	}

	// Set the session variables if it is posted
	if($_POST && array_key_exists("username", $_POST) && array_key_exists("password", $_POST)) {
		$_SESSION["username"] = utf8_to_windows1252($_POST["username"]);
		$_SESSION["password"] = utf8_to_windows1252($_POST["password"]);
	}
	if(!DISABLE_REMOTE_USER_LOGIN){
		// REMOTE_USER is set when apache has authenticated the user
		if( ! $_POST && $_SERVER && array_key_exists("REMOTE_USER", $_SERVER)) {
			$_SESSION["username"] = utf8_to_windows1252($_SERVER['REMOTE_USER']);
			if (LOGINNAME_STRIP_DOMAIN) {
				$_SESSION["username"] = ereg_replace('@.*', '', $_SESSION["username"]);
			}
			$_SESSION["password"] = "";
		}
	}


	// Create global mapi object. This object is used in many other files
	$GLOBALS["mapisession"] = new MAPISession();
	if (isset($_SESSION["username"]) && isset($_SESSION["password"])) {
		$sslcert_file = defined('SSLCERT_FILE') ? SSLCERT_FILE : null;
		$sslcert_pass = defined('SSLCERT_PASS') ? SSLCERT_PASS : null;
		$hresult = $GLOBALS["mapisession"]->logon($_SESSION["username"], $_SESSION["password"], DEFAULT_SERVER, $sslcert_file, $sslcert_pass);
		if ($hresult != NOERROR){
			// login failed, remove session
			$_SESSION = array();
			$_SESSION["hresult"] = $hresult;
		}
	}

	// Check if user is authenticated
	if ($GLOBALS["mapisession"]->isLoggedOn()) {
		// Authenticated

		// Instantiate Plugin Manager
		$GLOBALS['PluginManager'] = new PluginManager();
		$GLOBALS['PluginManager']->detectPlugins();
		$GLOBALS['PluginManager']->initPlugins();
		
		// Create globals settings object
		$GLOBALS["settings"] = new Settings();
		
		// Create global language object
		$GLOBALS["language"] = new Language();

		// Set session settings (language & style)
		foreach($GLOBALS["settings"]->getSessionSettings() as $key=>$value){
			$_SESSION[$key] = $value;
		}
		
		// Get settings from post or session or settings
		if (isset($_REQUEST["language"]) && $GLOBALS["language"]->is_language($_REQUEST["language"])) {
			$lang = $_REQUEST["language"];
			$GLOBALS["settings"]->set("global/language", $lang);
		} else if(isset($_SESSION["lang"])) {
			$lang = $_SESSION["lang"];
		} else $lang = $GLOBALS["settings"]->get("global/language", LANG);
		
		$GLOBALS["language"]->setLanguage($lang);
		if($_GET && array_key_exists("logon", $_GET)) {
			$GLOBALS['PluginManager']->triggerHook("server.index.login.success");
			if(isset($_POST) && isset($_POST["action_url"]) && $_POST["action_url"] != "") {
				// if action attributes are passed in POST variable then append it to URL for
				// further processing
				$url = "index.php" . $_POST["action_url"];
				header("Refresh: 0; url=$url");
				exit;
			} else {
				header("Refresh: 0; url=index.php");
				exit;
			}
		}

		// add extra header
		header("X-Zarafa: ".phpversion("mapi").(defined("SVN") ? "-".SVN:""));
		// Temporary fix for Internet Explorer 9, will make it work under IE8 compatibility mode
		header("X-UA-Compatible: IE=8");

		// external files who need our login
		if ($_GET && array_key_exists("load", $_GET)) {

			// Get Attachment data from state and put it into the $_SESSION
			$attachment_state = new State('attachments');
			$attachment_state->open();
			$_SESSION['files'] = $attachment_state->read("files");
			$_SESSION['deleteattachment'] = $attachment_state->read("deleteattachment");
			$attachment_state->close();
			switch ($_GET["load"]) {
				case "translations.js":
					$GLOBALS['PluginManager']->triggerHook("server.index.load.jstranslations.before");
					include("client/translations.js.php");
					$GLOBALS['PluginManager']->triggerHook("server.index.load.jstranslations.after");
					break;
				case "dialog":
					// If the dialog is called with attachid in URL we want to call the upload attachment file 
					// first to upload the files to the server, and add them to session files.
					if($_GET && array_key_exists("attachment_id", $_GET)){
						$GLOBALS['PluginManager']->triggerHook("server.index.load.upload_attachment.before");
						include("server/upload_attachment.php");
						$GLOBALS['PluginManager']->triggerHook("server.index.load.upload_attachment.after");
						// Get Attachment data from $_SESSION and put it into the state
						// Do it now before outputting the dialog.php data, so that zarafa.php cannot be called
						// before we have written the data
						$attachment_state = new State('attachments');
						$attachment_state->open();
						$attachment_state->write("files", $_SESSION['files']);
						$attachment_state->write("deleteattachment", $_SESSION['deleteattachment']);
						$attachment_state->close();
					}
					$GLOBALS['PluginManager']->triggerHook("server.index.load.dialog.before");
					include("client/dialog.php");
					$GLOBALS['PluginManager']->triggerHook("server.index.load.dialog.after");
					break;
				case "upload_attachment":
					$GLOBALS['PluginManager']->triggerHook("server.index.load.upload_attachment.before");
					include("server/upload_attachment.php");
					$GLOBALS['PluginManager']->triggerHook("server.index.load.upload_attachment.after");
					break;
				case "download_attachment":
					$GLOBALS['PluginManager']->triggerHook("server.index.load.download_attachment.before");
					include("client/download_attachment.php");
					$GLOBALS['PluginManager']->triggerHook("server.index.load.download_attachment.after");
					break;
				case "download_message":
					$GLOBALS['PluginManager']->triggerHook("server.index.load.download_message.before");
					include("client/download_message.php");
					$GLOBALS['PluginManager']->triggerHook("server.index.load.download_message.after");
					break;
				default:
					// These hooks are defined twice (also when no "load" argument is supplied)
					$GLOBALS['PluginManager']->triggerHook("server.index.load.main.before");
					include("client/webclient.php");
					$GLOBALS['PluginManager']->triggerHook("server.index.load.main.after");
					break;
			}

			// Get Attachment data from $_SESSION and put it into the state
			$attachment_state = new State('attachments');
			$attachment_state->open();
			$attachment_state->write("files", $_SESSION['files']);
			$attachment_state->write("deleteattachment", $_SESSION['deleteattachment']);
			$attachment_state->close();
			// Prevent the SESSION data to be stored elsewhere
			unset($_SESSION['files'], $_SESSION['deleteattachment']);

		} else {
			// Clean up old state files in tmp/session/
			$state = new State("index");
			$state->clean();

			// Clean up old attachments in tmp
			cleanTemp(TMP_PATH, UPLOADED_ATTACHMENT_MAX_LIFETIME);
					
			// clean search folders
			cleanSearchFolders();

			// These hooks are defined twice (also when there is a "load" argument supplied)
			$GLOBALS['PluginManager']->triggerHook("server.index.load.main.before");
			// Include webclient
			include("client/webclient.php");
			$GLOBALS['PluginManager']->triggerHook("server.index.load.main.after");
		}
	} else { // Not authenticated, goto login page
		// Create global language object
		$GLOBALS["language"] = new Language();
		$GLOBALS["language"]->setLanguage(LANG);

		include("client/login.php");
	}
?>
