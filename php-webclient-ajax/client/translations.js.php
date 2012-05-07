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
	header("Content-Type: text/javascript; charset=utf-8");

	header('Expires: '.gmdate('D, d M Y H:i:s',time() + EXPIRES_TIME).' GMT');
	header('Cache-Control: max-age=' . EXPIRES_TIME . ',must-revalidate');
	
	// Pragma: cache doesn't really exist. But since session_start() automatically
	// outputs a Pragma: no-cache, the only way to override that is to output something else
	// in that header. So that's what we do here. It might as well have read 'Pragma: foo'.
	header('Pragma: cache');
	
	// compress output
	ob_start("ob_gzhandler");

	$translations = $GLOBALS['language']->getTranslations();
?>
	/**
	 * Function which returns a translation from gettext.
	 * @param string key english text
	 * @param string domain optional gettext domain
	 * @return string translated text
	 */
	function _(key, domain)
	{
		var translations = new Object();

		// BEGIN TRANSLATIONS
<?php
foreach($translations as $domain => $translation_list){

	// Find the translation that contains the charset.
	foreach($translations[$domain] as $key => $translation_strings){
		$charset = 'UTF-8';
		if($translation_strings['orig'] == ''){
			preg_match('/charset=([a-zA-Z0-9_-]+)/', $translation_strings['trans'], $matches);
			if(count($matches) > 0 && isset($matches[1])){
				$charset = strtoupper($matches[1]);
			}
			break;
		}
	}
?>
		translations["<?=$domain?>"] = new Object();
<?php
	foreach($translations[$domain] as $key => $translation_strings){
		if($translation_strings['orig'] == '') continue;

		// Escape the \n slash to prevent the translation file out breaking.
		$translation_strings['orig'] = str_replace("\n","\\n", addslashes($translation_strings['orig']));
		$translation_strings['trans'] = str_replace("\n","\\n", addslashes($translation_strings['trans']));

		if($charset != 'UTF-8'){
			$orig = iconv($charset, 'UTF-8//TRANSLIT', $translation_strings['orig']);
			$trans = iconv($charset, 'UTF-8//TRANSLIT', $translation_strings['trans']);
		}else{
			$orig = $translation_strings['orig'];
			$trans = $translation_strings['trans'];
		}
?>
		translations["<?=$domain?>"]["<?=$orig?>"] = "<?=$trans?>";
<?php
	}
}
?>
		if(typeof domain != "string") domain = "zarafa_webaccess";
		if(typeof translations[domain] == "undefined") return key;

		if(translations[domain][key]) {
			return translations[domain][key];
		} else {
			return key; // return the english text as fallback
		}
}

/**
 * This function is used as an alias to the _() function for developers who hate
 * to have the arguments in reverse order in respect to the dgettext() function 
 * in PHP.
 * @param string domain gettext domain
 * @param string key english text
 * @return string translated text
 */
function dgettext(domain, key){ return _(key, domain); }
