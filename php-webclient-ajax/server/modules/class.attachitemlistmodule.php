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
	class AttachItemListModule extends ListModule
	{
		/**
		 * Constructor
		 * @param int $id nique id.
		 * @param array		$data list of all actions.
		 */
		function AttachItemListModule($id, $data)
		{
			parent::ListModule($id, $data, array(OBJECT_SAVE, TABLE_SAVE, TABLE_DELETE));

			$this->sort = array();
		}

		/**
		 * Executes all the actions in the $data variable.
		 * @return boolean true on success or false on failure.
		 */
		function execute()
		{
			$result = false;
			
			foreach($this->data as $action)
			{
				if(isset($action["attributes"]) && isset($action["attributes"]["type"])) {
					$store = $this->getActionStore($action);
					$parententryid = $this->getActionParentEntryID($action);
					$entryid = $this->getActionEntryID($action);

					switch($action["attributes"]["type"])
					{
						case "list":
							// get properties and column info
							$this->getColumnsAndPropertiesForMessageType($store, $entryid, $action);
							// @TODO:	get the sort data from sort settings
							//			not working now, as of sorting is not suppported by table widget.
							$this->sort[$this->properties["subject"]] = TABLE_SORT_ASCEND;
	
							$result = $this->messageList($store, $entryid, $action);
							break;
						
						case "attach_items":
							$this->attach($store, $parententryid, $action);
							break;
					}
				}
			}
			return $result;
		}

		

		/**
		 * Function will set properties and table columns for particular message class
		 * @param Object $store MAPI Message Store Object
		 * @param HexString $entryid entryid of the folder
		 * @param Array $action the action data, sent by the client
		 */
		function getColumnsAndPropertiesForMessageType($store, $entryid, $action)
		{
			if(isset($action["container_class"])) {
				$messageType = $action["container_class"];

				switch($messageType) {
					case "IPF.Appointment":
						$this->properties = $GLOBALS["properties"]->getAppointmentProperties();
						$this->tablecolumns = $GLOBALS["TableColumns"]->getAppointmentListTableColumns();
						break;
					case "IPF.Contact":
						$this->properties = $GLOBALS["properties"]->getContactProperties();
						$this->tablecolumns = $GLOBALS["TableColumns"]->getContactListTableColumns();
						break;
					case "IPF.Journal":		// not implemented
						break;
					case "IPF.Task":
						$this->properties = $GLOBALS["properties"]->getTaskProperties();
						$this->tablecolumns = $GLOBALS["TableColumns"]->getTaskListTableColumns();
						break;
					case "IPF.StickyNote":
						$this->properties = $GLOBALS["properties"]->getStickyNoteProperties();
						$this->tablecolumns = $GLOBALS["TableColumns"]->getStickyNoteListTableColumns();
						break;
					case "IPF.Note":
					default:
						$this->properties = $GLOBALS["properties"]->getMailProperties();
						$this->tablecolumns = $GLOBALS["TableColumns"]->getMailListTableColumns();

						// enable columns that are by default disabled
						$GLOBALS["TableColumns"]->changeColumnPropertyValue($this->tablecolumns, "message_delivery_time", "visible", true);
						$GLOBALS["TableColumns"]->changeColumnPropertyValue($this->tablecolumns, "sent_representing_name", "visible", true);
						break;
				}

				// add folder name column
				if($GLOBALS["TableColumns"]->getColumn($this->tablecolumns, "parent_entryid") === false) {
					$GLOBALS["TableColumns"]->addColumn($this->tablecolumns, "parent_entryid", true, 6, _("In Folder"), _("Sort Folder"), 90, "folder_name");
				}
			}
		}

		/**
		 * Function which sets selected messages as attachment to the item
		 * @param object $store MAPI Message Store Object
		 * @param string $parententryid entryid of the message
		 * @param array $action the action data, sent by the client
		 * @return boolean true on success or false on failure 
		 */
		function attach($store, $parententryid, $action)
		{
			$result = false;
			
			if($store){
				if(isset($action["entryids"]) && $action["entryids"]){
					
					if(!is_array($action["entryids"])){
						$action["entryids"] = array($action["entryids"]);
					}
					if($action["type"] == "attachment"){

						//add attach items to session variables
						$attachments = $GLOBALS["operations"]->setAttachmentInSession($store, $action["entryids"], $action["dialog_attachments"]);

						$data = array();
						$data["attributes"] = array("type" => "attach_items");
						$data["item"] = array();
						$data["item"]["parententryid"] = bin2hex($parententryid);
						$data["item"]["attachments"] = $attachments;
						array_push($this->responseData["action"], $data);
						$GLOBALS["bus"]->addData($this->responseData);
						$result = true;
					}else{
						$data = array();
						$data["attributes"] = array("type" => "attach_items_in_body");
						$data["item"] = array();

						foreach($action["entryids"] as $key => $item){
							$message = mapi_msgstore_openentry($store, hex2bin($item));
							$items = array();
							$items = $GLOBALS["operations"]->getMessageProps($store, $message, $this->properties, true);

							array_push($data["item"], $items);
						}
						array_push($this->responseData["action"], $data);
						$GLOBALS["bus"]->addData($this->responseData);
						$result = true;
					}
				}
			}
			return $result;
		}
	}
?>
