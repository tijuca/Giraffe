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
	* Upload Attachment
	* This file is used to upload an file.
	*/

	// required to validate input arguments
	require_once("client/layout/dialogs/utils.php");

	// Check if dialog_attachments is set
	if(isset($_POST["dialog_attachments"])) {
		// Check if the file is uploaded correctly and is not above the MAX_FILE_SIZE
		dump($_FILES["attachments"]);
		if(isset($_FILES["attachments"]) && is_array($_FILES["attachments"])) {
			$FILES = Array();
			
			if(isset($_FILES['attachments']['name']) && is_array($_FILES['attachments']['name'])){
				foreach($_FILES['attachments']['name'] as $key => $name){
					$FILE = Array(
						'name'     => $_FILES['attachments']['name'][$key],
						'type'     => $_FILES['attachments']['type'][$key],
						'tmp_name' => $_FILES['attachments']['tmp_name'][$key],
						'error'    => $_FILES['attachments']['error'][$key],
						'size'     => $_FILES['attachments']['size'][$key]
					);
					
					$FILES[] = $FILE;
				}
			} else if(isset($_FILES['attachments']['name'])){
				// SWFUpload sends attachments array already in required format.
				$FILE = $_FILES['attachments'];

				/**
				 * Get MIME type of the file, cause SWFUpload object doesn't return MIME type of the file.
				 * for more information -> http://swfupload.org/forum/generaldiscussion/166
				 */
				$FILE["type"] = getMIMEType($FILE["name"]);
				$FILES[] = $FILE;
			}

			foreach($FILES as $FILE) {
				if (!empty($FILE["size"]) && !(isset($_POST["MAX_FILE_SIZE"]) && $FILE["size"] > $_POST["MAX_FILE_SIZE"])) {
					// Create unique tmpname
					$tmpname = tempnam(TMP_PATH, stripslashes($FILE["name"]));

					// Move the uploaded file the the tmpname
					$result = move_uploaded_file($FILE["tmp_name"], $tmpname);

					if($FILE["type"] == "message/rfc822") {
						/**
						 * check content type that is send by browser because content type for
						 * eml attachments will be message/rfc822, but this content type is used 
						 * for message-in-message embedded objects, so we have to send it as 
						 * application/octet-stream.
						 */
						$FILE["type"] = "application/octet-stream";
					}

					// Check if there are no files uploaded
					if(!isset($_SESSION["files"])) {
						$_SESSION["files"] = array();
					}
					
					// Check if no files are uploaded with this dialog_attachments
					if(!isset($_SESSION["files"][$_POST["dialog_attachments"]])) {
						$_SESSION["files"][$_POST["dialog_attachments"]] = array();
					}

					// stripping path details
					$tmpname = basename($tmpname);

					// Add file information to the session
					$_SESSION["files"][$_POST["dialog_attachments"]][$tmpname] = Array(
						"name"       => utf8_to_windows1252(stripslashes($FILE["name"])),
						"size"       => $FILE["size"],
						"type"       => $FILE["type"],
						"sourcetype" => 'default'
					);
				}

			}
		} else if(isset($_POST["deleteattachment"]) && isset($_POST["type"])) { // Delete uploaded file
			// Check if the delete file is an uploaded or an attachment of a MAPI iMessage
			if($_POST["type"] == "new") {
				// Get the tmpname of the uploaded file
				$tmpname = urldecode($_POST["deleteattachment"]);

				if(isset($_SESSION["files"][$_POST["dialog_attachments"]][$tmpname])) {
					unset($_SESSION["files"][$_POST["dialog_attachments"]][$tmpname]);
					// Delete file
					if (is_file($tmpname)) {
						unlink($tmpname);
					} 
				} 
			} else { // The file is an attachment of a MAPI iMessage
				// Set the correct array structure
				if(!isset($_SESSION["deleteattachment"])) {
					$_SESSION["deleteattachment"] = array();
				}
				
				if(!isset($_SESSION["deleteattachment"][$_POST["dialog_attachments"]])) {
					$_SESSION["deleteattachment"][$_POST["dialog_attachments"]] = array();
				}
				
				// Push the number of the attachment to the array structure
				array_push($_SESSION["deleteattachment"][$_POST["dialog_attachments"]], $_POST["deleteattachment"]);
			}
		}
} else if($_GET && isset($_GET["attachment_id"])) { // this is to upload the file to server when the doc is send via OOo
	// check wheather the doc is already moved
	if(file_exists("/tmp/".$_GET["attachment_id"])){
		// Create unique tmpname	
		$tmpname = tempnam(TMP_PATH, stripslashes($_GET["name"]));
		// Move the uploaded file to tmpname loaction
		rename("/tmp/".$_GET["attachment_id"], $tmpname);
				
		// Check if there are no files uploaded
		if(!isset($_SESSION["files"])) {
			$_SESSION["files"] = array();
		}
		
		// Check if no files are uploaded with this attachmentid
		if(!isset($_SESSION["files"][$_GET["attachment_id"]])) {
			$_SESSION["files"][$_GET["attachment_id"]] = array();
		}

		// Add file information to the session
		$_SESSION["files"][$_GET["attachment_id"]][basename($tmpname)] = Array(
			"name"       => utf8_to_windows1252(stripslashes($_GET["name"])),
			"size"       => filesize($tmpname),
			"type"       => mime_content_type($tmpname),
			"sourcetype" => 'default'
		);
	}else{
		// Check if no files are uploaded with this attachmentid
		if(isset($_SESSION["files"][$_GET["attachment_id"]])) {
			$_SESSION["files"][$_GET["attachment_id"]] = array();
		}
	}
}


// Return the file data output when client has request this page through the load==upload_attachment
if(get('load', false, false, STRING_REGEX) == 'upload_attachment'){
	if(isset($_SESSION["files"]) && isset($_SESSION["files"][$_REQUEST["dialog_attachments"]])) {
		foreach($_SESSION["files"][$_REQUEST["dialog_attachments"]] as $tmpname => $file){
			// Echo tmpname | filename | size
			echo windows1252_to_utf8($tmpname) . "|" . windows1252_to_utf8($file["name"]) . "|" . $file["size"] . "||";
		}
	}
}

?>
