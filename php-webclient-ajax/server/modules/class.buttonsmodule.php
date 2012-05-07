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
	 * Buttons Module
	 */
	class ButtonsModule extends Module
	{
		/**
		 * Constructor
		 * @param int $id unique id.
		 * @param string $folderentryid Entryid of the folder. Data will be selected from this folder.
		 * @param array $data list of all actions.
		 */
		function ButtonsModule($id, $data)
		{
			parent::Module($id, $data);
		}
		
		/**
		 * Executes all the actions in the $data variable.
		 * @return boolean true on success of false on fialure.
		 */
		function execute()
		{
			$result = false;
			foreach($this->data as $action)
			{
				if(isset($action["attributes"]) && isset($action["attributes"]["type"])) {
					switch($action["attributes"]["type"])
					{
						case "list":
							$store = $GLOBALS["mapisession"]->getDefaultMessageStore();
							$inbox = mapi_msgstore_getreceivefolder($store);
							$inboxProps = array();
							if($inbox) {
								$inboxProps = mapi_getprops($inbox, array(PR_ENTRYID, PR_IPM_APPOINTMENT_ENTRYID, PR_IPM_CONTACT_ENTRYID, PR_IPM_TASK_ENTRYID, PR_IPM_NOTE_ENTRYID));
							}
							
							$data = array();
							$data["attributes"] = array("type" => "list");
							$data["folders"] = array();
							
							$button_counter = 1;
							foreach($inboxProps as $prop){
								$folder = mapi_msgstore_openentry($store, $prop);
								$folderProps = mapi_getprops($folder, array(PR_DISPLAY_NAME, PR_CONTAINER_CLASS));
								
								$data["folders"]["button".$button_counter] = array();
								$data["folders"]["button".$button_counter]["entryid"] = bin2hex($prop);
								$data["folders"]["button".$button_counter]["title"] = windows1252_to_utf8($folderProps[PR_DISPLAY_NAME]);
								
								$icon = "folder";
								if (isset($folderProps[PR_CONTAINER_CLASS])) {
									$icon = strtolower(substr($folderProps[PR_CONTAINER_CLASS],4));
								} else {
									// fix for inbox icon
									if ($prop == $inboxProps[PR_ENTRYID]){
										$icon = "inbox";
									}
								}
								$data["folders"]["button".$button_counter]["icon"] = $icon;
								$button_counter++;
							}
							
							array_push($this->responseData["action"], $data);
							$GLOBALS["bus"]->addData($this->responseData);
							$result = true;
							break;
					}
				}
			}
			
			return $result;
		}
	}
?>
