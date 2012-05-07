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
include_once('mapi/class.taskrecurrence.php');
include_once("mapi/class.taskrequest.php");

	/**
	 * Task ItemModule
	 * Module which openes, creates, saves and deletes an item. It 
	 * extends the Module class.
	 */
	class TaskItemModule extends ItemModule
	{
		/**
		 * @var Array properties of task item that will be used to get data
		 */
		var $properties = null;

		var $plaintext;

		/**
		 * Constructor
		 * @param int $id unique id.
		 * @param array $data list of all actions.
		 */
		function TaskItemModule($id, $data)
		{
			$this->plaintext = true;

			parent::ItemModule($id, $data);
		}

		function open($store, $entryid, $action)
		{
			$message = mapi_msgstore_openentry($store, $entryid);
			$tr = new TaskRequest($store, $message, $GLOBALS["mapisession"]->getSession());
			
			if($tr->isTaskRequest()) {
				$tr->processTaskRequest();
				$task = $tr->getAssociatedTask(false);
				
				$taskprops = mapi_getprops($task, array(PR_ENTRYID));
				$entryid = $taskprops[PR_ENTRYID];
			}
			
			if($tr->isTaskRequestResponse()) {
				$tr->processTaskResponse();
				$task = $tr->getAssociatedTask(false);
				
				$taskprops = mapi_getprops($task, array(PR_ENTRYID));
				$entryid = $taskprops[PR_ENTRYID];
			}

			$result = false;
			if($store && $entryid) {
				$data = array();
				$data["attributes"] = array("type" => "item");
				$message = $GLOBALS["operations"]->openMessage($store, $entryid);
				$data["item"] = $GLOBALS["operations"]->getMessageProps($store, $message, $this->properties, true);
				// Get the recurrence information
				$recur = new Taskrecurrence($store, $message);
				$recurpattern = $recur->getRecurrence();

				// Add the recurrence pattern to the data
				if(isset($recurpattern) && is_array($recurpattern))
					$data["item"] += $recurpattern;

				array_push($this->responseData["action"], $data);
				$GLOBALS["bus"]->addData($this->responseData);
				
				$result = true;
			}
			
			return $result;
		}

		/**
		 * Function which saves an item.
		 * @param object $store MAPI Message Store Object
		 * @param string $parententryid parent entryid of the message
		 * @param array $action the action data, sent by the client
		 * @return boolean true on success or false on failure		 		 
		 */
		function save($store, $parententryid, $action)
		{
			$result = false;

			if(isset($action["props"])) {
				$props = $action["props"];
				
				if(!$store && !$parententryid) {
					if(isset($action["props"]["message_class"])) {
						$store = $GLOBALS["mapisession"]->getDefaultMessageStore();
						$parententryid = $this->getDefaultFolderEntryID($store, $action["props"]["message_class"]);
					}
				}

				if ($store && $parententryid) {
					$messageProps = $GLOBALS["operations"]->saveTask($store, $parententryid, $action);

					if($messageProps) {
						$result = true;
						$GLOBALS["bus"]->notify(bin2hex($parententryid), TABLE_SAVE, $messageProps);
					}
				}
			}
			
			return $result;
		}

		/**
		 * Function which deletes an item.
		 * @param object $store MAPI Message Store Object
		 * @param string $parententryid parent entryid of the message
		 * @param string $entryid entryid of the message		 
		 * @param array $action the action data, sent by the client
		 * @return boolean true on success or false on failure		 		 
		 */
		function delete($store, $parententryid, $entryids, $action)
		{
			$result = false;
			$send = isset($action["send"]) ? $action["send"] : false;

			if($store && $parententryid) {
				$props = array();
				$props[PR_PARENT_ENTRYID] = $parententryid;
				$props[PR_ENTRYID] = $entryids;

				$storeprops = mapi_getprops($store, array(PR_ENTRYID));
				$props[PR_STORE_ENTRYID] = $storeprops[PR_ENTRYID];

				$result = $GLOBALS["operations"]->deleteTask($store, $parententryid, $entryids, $action);

				if (isset($result['occurrenceDeleted']) && $result['occurrenceDeleted']) {
					// Occurrence deleted, update item
					$GLOBALS["bus"]->notify(bin2hex($parententryid), TABLE_SAVE, $props);
				} else {
					$GLOBALS["bus"]->notify(bin2hex($parententryid), TABLE_DELETE, $props);
				}
			}

			return $result;
		}

		/**
		 * Function will generate property tags based on passed MAPIStore to use
		 * in module. These properties are regenerated for every request so stores
		 * residing on different servers will have proper values for property tags.
		 * @param MAPIStore $store store that should be used to generate property tags.
		 * @param Binary $entryid entryid of message/folder
		 * @param Array $action action data sent by client
		 */
		function generatePropertyTags($store, $entryid, $action)
		{
			$this->properties = $GLOBALS["properties"]->getTaskProperties($store);
		}
	}
?>
