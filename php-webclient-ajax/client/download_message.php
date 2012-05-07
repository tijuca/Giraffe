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
	* Download e-mail in eml format.
	* This file is used to download any e-mail in eml format.
	*/
	require_once("client/layout/dialogs/utils.php");

	// Get store id
	$storeid = false;
	if(isset($_GET["store"])) {
		$storeid = get("store", false, false, ID_REGEX);
	}

	// Get message entryid
	$entryid = false;
	if(isset($_GET["entryid"])) {
		$entryid = get("entryid", false, false, ID_REGEX);
	}

	// Get download file type
	$fileType = false;
	if(isset($_GET["fileType"])) {
		$fileType = get("fileType", false, false, STRING_REGEX);
	}

	// open already saved attachment
	if($storeid && $entryid) {
		$errorMsg = downloadMessageAsFile($storeid, $entryid, $fileType);
		if($errorMsg) {
			//Error Handling.
			echo "<script type='text/javascript'>alert(\"" . $errorMsg . "\");</script>";	
		}
	}

	/**
	 * Function will open email message as inet object and will return email message in eml format to client.
	 * 
	 * @param HexString $storeid store id
	 * @param HexString $entryid entryid of message
	 * @param String $fileType type of the download file e.g. eml for e-mails
	 * 
	 * @return Boolean true on success, false on failure
	 */
	function downloadMessageAsFile($storeid, $entryid, $fileType) {
		// Open the store
		$store = $GLOBALS["mapisession"]->openMessageStore(hex2bin($storeid));

		if($store) {
			// Open the message
			$message = mapi_msgstore_openentry($store, hex2bin($entryid));
			
			if($message) {
				// get message properties.
				$messageProps = mapi_getprops($message, array(PR_SUBJECT));

				switch($fileType){
					case "eml":
						// Save e-mail message as .eml format
						$addrBook = $GLOBALS["mapisession"]->getAddressbook();
						// Read the message as RFC822-formatted e-mail stream.
						$stream = mapi_inetmapi_imtoinet($GLOBALS["mapisession"]->getSession(), $addrBook, $message, array());
						$extension = ".eml";
						break;

					default:
						return _("Can't save this message as a file") . "."; // return error message
					break;
				}

				if(mapi_last_hresult() == NOERROR) {
					// Set filename for saving email.
					if($messageProps[PR_SUBJECT] && $messageProps[PR_SUBJECT] != "") {
						/**
						 * Maximum no of filename characters allowed by Firefox is 206.
						 * Firefox wil show an error "The file name is invalid.",
						 * while saving a file having filename greater than on 206 characters.
						 * So here I set limit of maximum 200 characters.
						 */
						$filename = substr($messageProps[PR_SUBJECT], 0, 200) . $extension;
					} else {
						$filename = _("Untitled") . $extension;
					}
					
					// Set the headers
					header("Pragma: public");
					header("Expires: 0"); // set expiration time
					header("Cache-Control: must-revalidate, post-check=0, pre-check=0");
					header("Content-Disposition: attachment; filename=\"" . browserDependingHTTPHeaderEncode($filename) . "\"");
					header("Content-Transfer-Encoding: binary");

					// Set content type header
					header("Content-Type: " . "application/octet-stream");

					$stat = mapi_stream_stat($stream);

					// Set the file length
					header("Content-Length: " . $stat["cb"]);

					// Read whole message and echo it.
					for($i = 0; $i < $stat["cb"]; $i += BLOCK_SIZE) {
						// Print stream
						echo mapi_stream_read($stream, BLOCK_SIZE);
					}
					return false;		// NO Error
				}
			}
		}
		return _("Unable to Save the Message") . ".";		// return error
	}
?>