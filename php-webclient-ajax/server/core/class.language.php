<?php
/*
 * Copyright 2005 - 2009  Zarafa B.V.
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
	 * Language handling class
	 *
	 * @package core
	 */
	class Language {
		var $languages;
		var $languagetable;
		var $lang;
		var $loaded;
			
        /**
        * Default constructor
        *
        * By default, the Language class only knows about en_EN (English). If you want more languages, you
        * must call loadLanguages().
        *
        */
		function Language()
		{
			$this->languages = array("en_EN"=>"English");
			$this->languagetable = array("en_EN"=>"eng_ENG");
			$this->loaded = false;
		}
		
		/**
		* Loads languages from disk
		*
		* loadLanguages() reads the languages from disk by reading LANGUAGE_DIR and opening all directories
		* in that directory. Each directory must contain a 'language.txt' file containing:
		*
		* <language display name>
		* <win32 language name>
		*
		* For example:
		* <code>
		* Nederlands
		* nld_NLD
		* </code>
		*
		* Also, the directory names must have a name that is:
		* 1. Available to the server's locale system
		* 2. In the UTF-8 charset
		*
		* For example, nl_NL.UTF-8
		*
		*/
		function loadLanguages()
		{
			if($this->loaded)
				return;

			$languages = explode(";", ENABLED_LANGUAGES);
			$dh = opendir(LANGUAGE_DIR);
			while (($entry = readdir($dh))!==false){
				$langcode = str_ireplace(".UTF-8", "", $entry);
				if(in_array($langcode, $languages) || in_array($entry, $languages)) {
					if (is_dir(LANGUAGE_DIR.$entry."/LC_MESSAGES") && is_file(LANGUAGE_DIR.$entry."/language.txt")){
						$fh = fopen(LANGUAGE_DIR.$entry."/language.txt", "r");
						$lang_title = fgets($fh);
						$lang_table = fgets($fh);
						fclose($fh);
						$this->languages[$entry] = $lang_title;
						$this->languagetable[$entry] = $lang_table;
					}
				}
			}
			$this->loaded = true;		
		}

		/**
		* Attempt to set language
		*
		* setLanguage attempts to set the language to the specified language. The language passed
		* is the name of the directory containing the language.
		*
		* For setLanguage() to success, the language has to have been loaded via loadLanguages() AND
		* the gettext system on the system must 'know' the language specified.
		*
		* @param string $lang Language (eg nl_NL.UTF-8)
		*/
		function setLanguage($lang)
		{
			$lang = (empty($lang)||$lang=="C")?LANG:$lang; // default language fix

			/**
			 * for backward compatibility
			 * convert all locale from utf-8 strings to UTF-8 string because locales are case sensitive and
			 * older WA settings can contain utf-8, so we have to convert it to UTF-8 before using
			 * otherwise languiage will be reseted to english
			 */
			if(strpos($lang, "utf-8") !== false) {
				$lang = str_replace("utf-8", "UTF-8", $lang);		// case sensitive replace

				// change it in settings also
				if(strcmp($GLOBALS["settings"]->get("global/language", ""), $lang)) {		// case insensitive compare
					$GLOBALS["settings"]->set("global/language", $lang);
				}
			}

			if ($this->is_language($lang)){
				$this->lang = $lang;
		
				if (strtoupper(substr(PHP_OS, 0, 3)) === "WIN"){
					$this->loadLanguages(); // we need the languagetable for windows...
					setlocale(LC_MESSAGES,$this->languagetable[$lang]);
					setlocale(LC_TIME,$this->languagetable[$lang]);
				}else{
					setlocale(LC_MESSAGES,$lang);
					setlocale(LC_TIME,$lang);
				}

				bindtextdomain('zarafa' , LANGUAGE_DIR);

				// All text from gettext() and _() is in UTF-8 so if you're saving to
				// MAPI directly, don't forget to convert to windows-1252 if you're writing
				// to PT_STRING8
				bind_textdomain_codeset('zarafa', "UTF-8");

				if(isset($GLOBALS['PluginManager'])){
					// What we did above, we are also now going to do for each plugin that has translations.
					$pluginTranslationPaths = $GLOBALS['PluginManager']->getTranslationFilePaths();
					foreach($pluginTranslationPaths as $pluginname => $path){
						bindtextdomain('plugin_'.$pluginname , $path);
						bind_textdomain_codeset('plugin_'.$pluginname, "UTF-8");
					}
				}

				textdomain('zarafa');
			}else{
				trigger_error("Unknown language: '".$lang."'", E_USER_WARNING);
			}
		}

		/**
		* Return a list of supported languages
		*
		* Returns an associative array in the format langid -> langname, for example "nl_NL.utf8" -> "Nederlands"
		*
		* @return array List of supported languages
		*/
		function getLanguages()
		{
			$this->loadLanguages();
			return $this->languages;
		}
	
		/**
		* Returns the ID of the currently selected language
		*
		* @return string ID of selected language
		*/
		function getSelected()
		{
			return $this->lang;
		}
	
		/**
		* Returns if the specified language is valid or not
		*
		* @param string $lang 
		* @return boolean TRUE if the language is valid
		*/
		function is_language($lang)
		{
			return $lang=="en_EN" || is_dir(LANGUAGE_DIR . "/" . $lang);
		}

		function getTranslations(){
			$translations = Array();

			$translations['zarafa'] = $this->getTranslationsFromFile(LANGUAGE_DIR.$this->getSelected().'/LC_MESSAGES/zarafa.mo');
			if(!$translations['zarafa']) $translations['zarafa'] = Array();

			if(isset($GLOBALS['PluginManager'])){
				// What we did above, we are also now going to do for each plugin that has translations.
				$pluginTranslationPaths = $GLOBALS['PluginManager']->getTranslationFilePaths();
				foreach($pluginTranslationPaths as $pluginname => $path){
					$plugin_translations = $this->getTranslationsFromFile($path.'/'.$this->getSelected().'/LC_MESSAGES/plugin_'.$pluginname.'.mo');
					if($plugin_translations){
						$translations['plugin_'.$pluginname] = $plugin_translations;
					}
				}
			}
			
			return $translations;
		}

		/**
		 * getTranslationsFromFile
		 * 
		 * This file reads the translations from the binary .mo file and returns
		 * them in an array containing the original and the translation variant.
		 * The .mo file format is described on the following URL.
		 * http://www.gnu.org/software/gettext/manual/gettext.html#MO-Files
		 * 
		 *          byte
		 *               +------------------------------------------+
		 *            0  | magic number = 0x950412de                |
		 *               |                                          |
		 *            4  | file format revision = 0                 |
		 *               |                                          |
		 *            8  | number of strings                        |  == N
		 *               |                                          |
		 *           12  | offset of table with original strings    |  == O
		 *               |                                          |
		 *           16  | offset of table with translation strings |  == T
		 *               |                                          |
		 *           20  | size of hashing table                    |  == S
		 *               |                                          |
		 *           24  | offset of hashing table                  |  == H
		 *               |                                          |
		 *               .                                          .
		 *               .    (possibly more entries later)         .
		 *               .                                          .
		 *               |                                          |
		 *            O  | length & offset 0th string  ----------------.
		 *        O + 8  | length & offset 1st string  ------------------.
		 *                ...                                    ...   | |
		 *  O + ((N-1)*8)| length & offset (N-1)th string           |  | |
		 *               |                                          |  | |
		 *            T  | length & offset 0th translation  ---------------.
		 *        T + 8  | length & offset 1st translation  -----------------.
		 *                ...                                    ...   | | | |
		 *  T + ((N-1)*8)| length & offset (N-1)th translation      |  | | | |
		 *               |                                          |  | | | |
		 *            H  | start hash table                         |  | | | |
		 *                ...                                    ...   | | | |
		 *    H + S * 4  | end hash table                           |  | | | |
		 *               |                                          |  | | | |
		 *               | NUL terminated 0th string  <----------------' | | |
		 *               |                                          |    | | |
		 *               | NUL terminated 1st string  <------------------' | |
		 *               |                                          |      | |
		 *                ...                                    ...       | |
		 *               |                                          |      | |
		 *               | NUL terminated 0th translation  <---------------' |
		 *               |                                          |        |
		 *               | NUL terminated 1st translation  <-----------------'
		 *               |                                          |
		 *                ...                                    ...
		 *               |                                          |
		 *               +------------------------------------------+
		 * 
		 * @param $filename string Name of the .mo file.
		 * @return array|boolean false when file is missing otherwise array with
		 *                             translations.
		 */

		function getTranslationsFromFile($filename){
			if(!is_file($filename)) return false;

			$fp = fopen($filename, 'r');
			if(!$fp){return false;}

			// Get number of strings in .mo file
			fseek($fp, 8, SEEK_SET);
			$num_of_str = unpack('Lnum', fread($fp, 4));
			$num_of_str = $num_of_str['num'];

			// Get offset to table with original strings
			fseek($fp, 12, SEEK_SET);
			$offset_orig_tbl = unpack('Loffset', fread($fp, 4));
			$offset_orig_tbl = $offset_orig_tbl['offset'];

			// Get offset to table with translation strings
			fseek($fp, 16, SEEK_SET);
			$offset_transl_tbl = unpack('Loffset', fread($fp, 4));
			$offset_transl_tbl = $offset_transl_tbl['offset'];

			// The following arrays will contain the length and offset of the strings
			$data_orig_strs = Array();
			$data_transl_strs = Array();

			/**
			 * Get the length and offset to the original strings by using the table 
			 * with original strings
			 */
			// Set pointer to start of orig string table
			fseek($fp, $offset_orig_tbl, SEEK_SET);
			for($i=0;$i<$num_of_str;$i++){
				// Length 4 bytes followed by offset 4 bytes
				$length = unpack('Llen', fread($fp, 4));
				$offset = unpack('Loffset', fread($fp, 4));
				$data_orig_strs[$i] = Array( 'length' => $length['len'], 'offset' => $offset['offset'] );
			}

			/**
			 * Get the length and offset to the translation strings by using the table 
			 * with translation strings
			 */
			// Set pointer to start of translations string table
			fseek($fp, $offset_transl_tbl, SEEK_SET);
			for($i=0;$i<$num_of_str;$i++){
				// Length 4 bytes followed by offset 4 bytes
				$length = unpack('Llen', fread($fp, 4));
				$offset = unpack('Loffset', fread($fp, 4));
				$data_transl_strs[$i] = Array( 'length' => $length['len'], 'offset' => $offset['offset'] );
			}

			// This array will contain the actual original and translation strings
			$translation_data = Array();

			// Get the original strings using the length and offset
			for($i=0;$i<count($data_orig_strs);$i++){
				$translation_data[$i] = Array();

				// Set pointer to the offset of the string
				fseek($fp, $data_orig_strs[$i]['offset'], SEEK_SET);
				if($data_orig_strs[$i]['length'] > 0){	// fread does not accept length=0
					$length = $data_orig_strs[$i]['length'];
					$orig_str = unpack('a'.$length.'str', fread($fp, $length));
					$translation_data[$i]['orig'] = $orig_str['str'];	// unpack converts to array :S
				}else{
					$translation_data[$i]['orig'] = '';
				}
			}

			// Get the translation strings using the length and offset
			for($i=0;$i<count($data_transl_strs);$i++){
				// Set pointer to the offset of the string
				fseek($fp, $data_transl_strs[$i]['offset'], SEEK_SET);
				if($data_transl_strs[$i]['length'] > 0){	// fread does not accept length=0
					$length = $data_transl_strs[$i]['length'];
					$trans_str = unpack('a'.$length.'str', fread($fp, $length));
					$translation_data[$i]['trans'] = $trans_str['str'];	// unpack converts to array :S
				}else{
					$translation_data[$i]['trans'] = '';
				}
			}

			return $translation_data;
		}
	}
?>
