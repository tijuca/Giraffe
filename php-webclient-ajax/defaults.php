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

if(!defined('ENABLED_LANGUAGES')) define("ENABLED_LANGUAGES", "de_DE;en_EN;en_US;es_ES;fi_FI;fr_FR;he_IL;hu_HU;it_IT;nb_NO;nl_NL;pl_PL;pt_BR;ru_RU;zh_CN");


?>
