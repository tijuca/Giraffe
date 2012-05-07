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
	* Function to include JavaScript files in the client from the specified directory
	* It outputs one or more <script>-tags.
	*
	*@param $dirName Name of the client directory to include
	*@param $excludeFiles Array with filenames we don't want to include
	*@returns Array with the files that are included;
	*/
	function includeJavaScriptFilesFromDir($dirName, $excludeFiles=array())
	{
		$dir = opendir("client/".$dirName);
		$files = array();
		$names = array();
		
		while(($file = readdir($dir)) !== false)
		{	
			if (substr($file,1,1)!="." && !in_array($file,$excludeFiles) && substr($file,-3)==".js"){
				$files[] = "client/".$dirName."/".$file;
				$names[] = $file;
			}
		}
		
		includeFiles("js", $files);

		return $names;
	}

	/**
	* Function to include JavaScript or CSS files in the client
	* It outputs one or more <script>-tags.
	*
	*@param $type The type of files that are included ("js" or "css")
	*@param $files Array with the files to include
	*/
	function includeFiles($type = "js", $files)
	{
		$singleRequest = false;
		if (defined("DEBUG_LOADER") && DEBUG_LOADER){
			$singleRequest = true;
		}

		// If we have IE6 with Windows 2000, always load the include files in seperate request because of a caching problem
		if (isset($_SERVER["HTTP_USER_AGENT"]) && strpos($_SERVER["HTTP_USER_AGENT"],"MSIE 6.0; Windows NT 5.0")!==false){
			$singleRequest = true;
		}
		
		switch($type){
			case "js":
				if (!$singleRequest){
					echo "\t\t<script type=\"text/javascript\" src=\"static.php?version=".phpversion("mapi")."&";
				}
				foreach($files as $file){
					if ($singleRequest){
						echo "\t\t<script type=\"text/javascript\" src=\"static.php?version=".phpversion("mapi")."&p[]=".$file."\"></script>\n";
					}else{
						echo "p[]=".$file."&";
					}
				}
				if (!$singleRequest){
					echo "\"></script>\n";
				}
				break;

			case "css":
				if (!$singleRequest){
					echo "\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"static.php?version=".phpversion("mapi")."&";
				}
				foreach($files as $file){
					if ($singleRequest){
						echo "\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"static.php?version=".phpversion("mapi")."&p[]=".$file."\">\n";
					}else{
						echo "p[]=".$file."&";
					}
				}
				if (!$singleRequest){
					echo "\">\n";
				}
				break;
		}
	}
?>
