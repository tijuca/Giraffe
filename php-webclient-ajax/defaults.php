<?php
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

?>
<?php
/**
 * This file is used to set configuration options to a default value that have 
 * not been set in the config.php.Each definition of a configuration value must 
 * be preceeded by "if(!defined('KEY'))"
 */
if(!defined('CONFIG_CHECK')) define('CONFIG_CHECK', TRUE);
if(!defined('CONFIG_CHECK_COOKIES_HTTP')) define('CONFIG_CHECK_COOKIES_HTTP', FALSE);
if(!defined('CONFIG_CHECK_COOKIES_SSL')) define('CONFIG_CHECK_COOKIES_SSL', FALSE);

if(!defined('STATE_FILE_MAX_LIFETIME')) define('STATE_FILE_MAX_LIFETIME', 28*60*60);
if(!defined('UPLOADED_ATTACHMENT_MAX_LIFETIME')) define('UPLOADED_ATTACHMENT_MAX_LIFETIME', 6*60*60);
if(!defined('DISABLE_FULL_CONTACTLIST_THRESHOLD')) define('DISABLE_FULL_CONTACTLIST_THRESHOLD', -1);
if(!defined('ENABLE_PUBLIC_FOLDERS')) define('ENABLE_PUBLIC_FOLDERS', true);
if(!defined('FILE_UPLOAD_LIMIT')) define('FILE_UPLOAD_LIMIT', 50);
if(!defined('FILE_QUEUE_LIMIT')) define('FILE_QUEUE_LIMIT', 20);

/**
 * When set to true this disables the fitlering of the HTML body.
 */
if(!defined('DISABLE_HTMLBODY_FILTER')) define('DISABLE_HTMLBODY_FILTER', false);
/**
 * When set to true this disables the login with the REMOTE_USER set by apache.
 */
if(!defined('DISABLE_REMOTE_USER_LOGIN')) define('DISABLE_REMOTE_USER_LOGIN', false);
if(!defined('DISABLE_DELETE_IN_RESTORE_ITEMS')) define('DISABLE_DELETE_IN_RESTORE_ITEMS', false);

/**
 * When set to true, enable the multi-upload feature of the attachment dialog. This has the following caveats:
 * - In FireFox, you can only upload to HTTPS when the certificate is recognized as an official (not self-signed
 *   SSL certificate)
 * - In Linux, some versions of flash do not support this feature and can crash during upload. Updating to the latest
 *   version of flash should fix the issue.
 * - In Windows, upload fails if the internet status is 'offline' - open internet explorer to reconnect
 */
if(!defined('ENABLE_MULTI_UPLOAD')) define('ENABLE_MULTI_UPLOAD', false);

/**
 * Limit the amount of members shown in the addressbook details dialog for a distlist. If the list 
 * is too great the browser will hang loading and rendereing all the items. By default set to 0 
 * which means it loads all members.
 */
if(!defined('ABITEMDETAILS_MAX_NUM_DISTLIST_MEMBERS')) define('ABITEMDETAILS_MAX_NUM_DISTLIST_MEMBERS', 0);

/**
 * Use direct booking by default (books resources directly in the calendar instead of sending a meeting
 * request)
 */
if(!defined('ENABLE_DIRECT_BOOKING')) define('ENABLE_DIRECT_BOOKING', true);

if(!defined('ENABLED_LANGUAGES')) define("ENABLED_LANGUAGES", "de;en;en_US;es;fi;fr;he;hu;it;nb;nl;pl;pt_BR;ru;zh_CN");

// Standard password key for session password. We recommend to change the default value for security reasons 
// and a length of 16 characters. Passwords are only encrypted when the openssl module is installed
if(!defined('PASSWORD_KEY')) define('PASSWORD_KEY','a75356b0d1b81b7');
if(!defined('PASSWORD_IV')) define('PASSWORD_IV','b3f5a483');

?>
