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
<?

/* This is a simple wrapper to server static content (like .js and .png) to the client
 * so that we can supply good cache headers, without having to change Apache config settings
 * etc.
 */
 
include_once("config.php"); 

if(!isset($_GET["p"])) {
	print "Invalid input";
	return;
}

$files = $_GET["p"];
if (!is_array($files)){
	$files = array($files);
}

$fileext = false;

foreach($files as $fn){

	// Check if filename is allowed (allow  ( [alnum] "/" )* "/" ([ alnum | "_" ] ".") + [alpha ] )
	if((strpos($fn, 'client/') !== 0 && strpos($fn, PATH_PLUGIN_DIR) !== 0) || !preg_match('/^([0-9A-Za-z\-]+\\/)*(([0-9A-Za-z_\-])+\.)+([A-Za-z\-])+$/', $fn)){
		print "Invalid path";
		trigger_error("Invalid path", E_USER_NOTICE);
		return;
	}

	// Find extension 
	$pos = strrchr($fn, ".");

	if(!isset($pos)) {
		print "Invalid type";
		trigger_error("Invalid type", E_USER_NOTICE);
		return;
	}

	$ext = substr($pos, 1);

	switch($ext) {
		case "js": $type = "text/javascript; charset=utf-8"; break;
		case "png": $type = "image/png"; break;
		case "jpg": $type = "image/jpeg"; break;
		case "gif": $type = "image/gif"; break;
		case "css": $type = "text/css; charset=utf-8"; break;
		case "html": $type = "text/html; charset=utf-8"; break;
		// This is important: do not server any other files! (e.g. php files)
		default: 
			print "Invalid extension \"".$ext."\"";
			trigger_error("Invalid extension \"".$ext."\"", E_USER_NOTICE);
			return;
	}

	if (!$fileext) {
		$fileext = $ext;
	}else if ($ext!=$fileext){
		print "Can't combine files";
		trigger_error("Can't combine files", E_USER_NOTICE);
		return;
	}else if($fileext!="js" && $fileext!="css"){
		print "Can only combine javascript or css files";
		trigger_error("Can only combine javascript or css files", E_USER_NOTICE);
		return;
	}

	if (!is_file($fn) || !is_readable($fn)){
		print "File \"".$fn."\" not found";
		trigger_error("File \"".$fn."\" not found", E_USER_NOTICE);
		return;
	}
}

// Output the file
header("Content-Type: ".$type);


if (defined("DEBUG_LOADER") && !DEBUG_LOADER){
	// Disable caching in debug mode
	header('Expires: -1');
	header('Cache-Control: no-cache');
}else{

	$etag = md5($_SERVER["QUERY_STRING"]);
	header("ETag: \"".$etag."\"");

	header('Expires: '.gmdate('D, d M Y H:i:s',time() + EXPIRES_TIME).' GMT');
	header('Cache-Control: max-age=' . EXPIRES_TIME.',must-revalidate');

	// check if the requested file is modified
	if (function_exists("getallheaders")){ // only with php as apache module
		$headers = getallheaders();

		if (isset($headers["If-None-Match"]) && strpos($headers['If-None-Match'], $etag)!==false){
			// ETag found, file has not changed
			header('HTTP/1.1 304 Not Modified');
			exit;
		}
	}
}


// compress output
if(!defined("DEBUG_GZIP")||DEBUG_GZIP){
	ob_start("ob_gzhandler");
}

foreach($files as $fn){

	if ($fileext!="css"){
		readfile($fn);
	}else{
		// rewrite css files
		$dir = substr($fn, 0, strrpos($fn, "/"));

		$fh = fopen($fn, "r");
		while(!feof($fh)){
			$line = fgets($fh, 4096);
			echo preg_replace("/url\(/i", "url(".$dir."/", $line);
		}
	}
	echo "\n";
}
