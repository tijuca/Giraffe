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
	 * Preview Read Mail ItemModule
	 */
	class PreviewReadMailItemModule extends ItemModule
	{
		/**
		 * @var Array properties of categories that will be used to get data
		 */
		var $properties = null;

		/**
		 * Constructor
		 * @param int $id unique id.
		 * @param array $data list of all actions.
		 */
		function PreviewReadMailItemModule($id, $data)
		{
			parent::ItemModule($id, $data);
		}

		function open($store, $entryid, $action)
		{
			$result = false;

			if($store && $entryid) {
				$data = array();
				$data["attributes"] = array("type" => "item");
				$message = $GLOBALS["operations"]->openMessage($store, $entryid);
				$data["item"] = $GLOBALS["operations"]->getMessageProps($store, $message, array("message_class" => PR_MESSAGE_CLASS, "message_flags" => PR_MESSAGE_FLAGS), true);

				/** In previewpane, keep the body and recipients from task request and grab all other properties from the task message object.
				*/
				if (isset($data["item"]["message_class"]) && strpos($data["item"]["message_class"], "IPM.TaskRequest") !== false) {
					$body = $data["item"]["body"];
					$recipients = $data["item"]["recipients"];
					$msgClass = $data["item"]["message_class"];
					$msgFlags = $data["item"]["message_flags"];

					$tr = new TaskRequest($store, $message, $GLOBALS["mapisession"]->getSession());
					$properties = $GLOBALS["properties"]->getTaskProperties();
					if($tr->isTaskRequest()) {
						$tr->processTaskRequest();
						$task = $tr->getAssociatedTask(true);
						$taskProps = $GLOBALS["operations"]->getMessageProps($store, $task, $properties, true);
						$data["item"] = $taskProps;

						// notify task folder that new task has been created
						$GLOBALS["bus"]->notify($taskProps["parent_entryid"]["_content"], TABLE_SAVE, array(
							PR_ENTRYID => hex2bin($taskProps["entryid"]["_content"]),
							PR_PARENT_ENTRYID => hex2bin($taskProps["parent_entryid"]["_content"]),
							PR_STORE_ENTRYID => hex2bin($taskProps["store_entryid"]["_content"])
						));

						if (isset($data["item"]["recurring"]) && $data["item"]["recurring"] == 1) {
							// Get the recurrence information
							$recur = new Taskrecurrence($store, $task);
							$recurpattern = $recur->getRecurrence();

							if(isset($recurpattern) && is_array($recurpattern))
							$data["item"]["recurrProps"] = $recurpattern;
						}
					}
					
					if($tr->isTaskRequestResponse()) {
						$tr->processTaskResponse();
						$task = $tr->getAssociatedTask(false);

						$data["item"] = $GLOBALS["operations"]->getMessageProps($store, $task, $properties, true);
					}
					$data["item"]["body"] = $body;
					$data["item"]["recipients"] = $recipients;
					$data["item"]["message_class"] = $msgClass;
					$data["item"]["message_flags"] = $msgFlags;

					// Also send taskrequest entryids
					$data["item"]['taskrequest'] = array();
					$data["item"]['taskrequest']['entryid'] = $action['entryid'];
					$data["item"]['taskrequest']['parententryid'] = $action['parententryid'];
					$data["item"]['taskrequest']['storeid'] = $action['store'];

					array_push($this->responseData["action"], $data);
					$GLOBALS["bus"]->addData($this->responseData);

					return true;
				} else {
					parent::open($store, $entryid, $action);
				}
			}
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
			$this->properties = $GLOBALS["properties"]->getMailProperties($store);
		}
	}
?>