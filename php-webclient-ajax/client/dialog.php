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
	* This file is called every time the user request a dialog window
	*
	* To specify which dialog the user want, use the GET attribute 'type',
	* the types are specified in /client/layout/html
	*/
	require_once("client/layout/dialogs/utils.php");
	
	$task = false;
	if(isset($_GET["task"])) {
		$task = get("task", false, false, STRING_REGEX);
	}
	$plugin = false;
	if(isset($_GET["plugin"])) {
		$plugin = get("plugin", false, false, STRING_REGEX);
	}
	$filename = false;
	$errormsg = false;

	// Include the upload_attachment script
	if(isset($_POST["dialog_attachments"])) {
		$hookdata = array( "task" => $task, 'plugin' => $plugin);
		$GLOBALS['PluginManager']->triggerHook("server.dialog.general.upload_attachment.before", $hookdata);

		include("server/upload_attachment.php");

		$GLOBALS['PluginManager']->triggerHook("server.dialog.general.upload_attachment.after", $hookdata);
	}

	if($plugin){
		if($GLOBALS['PluginManager']->pluginExists($plugin)){
			$filename = $GLOBALS['PluginManager']->getDialogFilePath($plugin, $task);
			if(!$filename){
				$errormsg = 'Invalid plugin task';
			}
		}else{
			$errormsg = 'Invalid plugin';
		}
	}elseif($task) {
		// check for invalid characters in filename
		if (!isset($task) || preg_match("/[^a-z0-9\-\_]/i", $task)){
			trigger_error("invalid input", E_USER_ERROR);
		}
		
		$type = false;
		if(strpos($task, "_") > 0) {
			$type = substr($task, strrpos($task, "_") + 1);
			$task = substr($task, 0, strrpos($task, "_"));
		}

		if($type) {
			// check if file exists
			$filename = "client/layout/dialogs/" . $type . "/" . $task . ".php";
			
			if (!is_readable($filename)){
				// fallback to IPM.Note if message_class is IPM.Note.*
				if (substr($task,0,4)=="note"){
					$filename = "client/layout/dialogs/" . $type . "/readmail.php";
				}else{
					$filename = "client/layout/dialogs/not_implemented.php";
				}
			}
		}else{
			$errormsg = 'Invalid tasktype';
		}
	}else{
		$errormsg = 'Invalid task';
	}


	if($filename){
		include($filename);
		include("client/layout/dialogs/window.php");
	}else{
		echo '<html><head><title>'.$errormsg.'</title></head><body><h1>'.$errormsg.'</h1></body></html>';
	}
?>
