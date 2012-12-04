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

	// Include backwards compatibility
	require_once("server/sys_get_temp_dir.php");

	// Get Attachment data from state
	$attachment_state = new AttachmentState();
	$attachment_state->open();

	// Check if dialog_attachments is set
	if(isset($_POST["dialog_attachments"])) {
		// Check if the file is uploaded correctly and is not above the MAX_FILE_SIZE
		if(isset($_FILES["attachments"]) && is_array($_FILES["attachments"])) {
			$FILES = Array();

			if(isset($_FILES['attachments']['name'])) {
				if(is_array($_FILES['attachments']['name'])){
					foreach($_FILES['attachments']['name'] as $key => $name){
						$FILE = Array(
							'name'     => $_FILES['attachments']['name'][$key],
							'type'     => $_FILES['attachments']['type'][$key],
							'tmp_name' => $_FILES['attachments']['tmp_name'][$key],
							'error'    => $_FILES['attachments']['error'][$key],
							'size'     => $_FILES['attachments']['size'][$key]
						);

						/**
						 * check content type that is send by browser because content type for
						 * eml attachments will be message/rfc822, but this content type is used 
						 * for message-in-message embedded objects, so we have to send it as 
						 * application/octet-stream.
						 */
						if ($FILE["type"] == "message/rfc822") {
							$FILE["type"] = "application/octet-stream";
						}

						$FILES[] = $FILE;
					}
				} else {
					// SWFUpload sends attachments array already in required format.
					$FILE = $_FILES['attachments'];

					/**
					 * Get MIME type of the file, cause SWFUpload object doesn't return MIME type of the file.
					 * for more information -> http://swfupload.org/forum/generaldiscussion/166
					 */
					$FILE["type"] = getMIMEType($FILE["name"]);
					$FILES[] = $FILE;
				}
			}

			foreach($FILES as $FILE) {
				if (!empty($FILE["size"]) && !(isset($_POST["MAX_FILE_SIZE"]) && $FILE["size"] > $_POST["MAX_FILE_SIZE"])) {
					// Parse the filename, strip it from
					// any illegal characters.
					$filename = basename(stripslashes($FILE["name"]));

					// Move the uploaded file into the attachment state
					$attachid = $attachment_state->addUploadedAttachmentFile($_REQUEST["dialog_attachments"], $filename, $FILE["tmp_name"], array(
						"name"       => $filename,
						"size"       => $FILE["size"],
						"type"       => $FILE["type"],
						"sourcetype" => 'default'
					));
				}

			}
		} else if(isset($_POST["deleteattachment"]) && isset($_POST["type"])) { // Delete uploaded file
			// Parse the filename, strip it from
			// any illegal characters
			$filename = basename(stripslashes(urldecode($_POST["deleteattachment"])));

			// Check if the delete file is an uploaded or an attachment of a MAPI iMessage
			if ($_POST["type"] == "new") {
				// Delete the file instance and unregister the file
				$attachment_state->deleteUploadedAttachmentFile($_REQUEST["dialog_attachments"], $filename);
			} else { // The file is an attachment of a MAPI iMessage
				// Set the correct array structure
				$attachment_state->addDeletedAttachment($_REQUEST["dialog_attachments"], $filename);
			}
		}
	} else if($_GET && isset($_GET["attachment_id"])) { // this is to upload the file to server when the doc is send via OOo
		$providedFile = sys_get_temp_dir() . DIRECTORY_SEPARATOR . $_GET['attachment_id'];

		// check wheather the doc is already moved
		if (file_exists($providedFile)) {
			$filename = basename(stripslashes($_GET["name"]));

			// Move the uploaded file to the session
			$attachment_state->addProvidedAttachmentFile($_REQUEST["attachment_id"], $filename, $providedFile, array(
				"name"       => $filename,
				"size"       => filesize($tmpname),
				"type"       => mime_content_type($tmpname),
				"sourcetype" => 'default'
			));
		}else{
			// Check if no files are uploaded with this attachmentid
			$attachment_state->clearAttachmentFiles($_GET["attachment_id"]);
		}
	}

	// Return the file data output when client has request this page through the load==upload_attachment
	if(get('load', false, false, STRING_REGEX) == 'upload_attachment'){
		if(isset($_REQUEST["dialog_attachments"])) {
			$files = $attachment_state->getAttachmentFiles($_REQUEST["dialog_attachments"]);
			foreach($files as $tmpname => $file) {
				// Echo tmpname | filename | size
				echo windows1252_to_utf8($tmpname) . "|" . windows1252_to_utf8($file["name"]) . "|" . $file["size"] . "||";
			}
		}
	}

	$attachment_state->close();
?>