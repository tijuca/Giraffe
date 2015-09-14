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
	 * Hierarchy Copy/Move Module
	 */ 
	class HierarchySelectModule extends Module
	{
		/**
		 * Constructor
		 * @param int $id unique id.
		 * @param string $folderentryid Entryid of the folder. Data will be selected from this folder.
		 * @param array $data list of all actions.
		 */
		function HierarchySelectModule($id, $data)
		{
			$this->columns = array();
			$this->columns["entryid"] = PR_ENTRYID;
			$this->columns["store_entryid"] = PR_STORE_ENTRYID;
			$this->columns["parent_entryid"] = PR_PARENT_ENTRYID;
			$this->columns["display_name"] = PR_DISPLAY_NAME;
			$this->columns["container_class"] = PR_CONTAINER_CLASS;
			$this->columns["access"] = PR_ACCESS;
			$this->columns["rights"] = PR_RIGHTS;
			$this->columns["content_count"] = PR_CONTENT_COUNT;
			$this->columns["content_unread"] = PR_CONTENT_UNREAD;
			$this->columns["subfolders"] = PR_SUBFOLDERS;
			$this->columns["store_support_mask"] = PR_STORE_SUPPORT_MASK;
		
			parent::Module($id, $data);
		}
		
		/**
		 * Executes all the actions in the $data variable.
		 * @return boolean true on success or false on fialure.
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
							$this->hierarchyList();
							$result = true;
							break;
					}
				}
			}
			
			return $result;
		}
		
		/**
		 * Generates the hierarchy list. All folders and subfolders are added to response data.
		 */
		function hierarchyList()
		{
			$data = $GLOBALS["operations"]->getHierarchyList($this->columns, HIERARCHY_GET_ALL);
			array_push($this->responseData["action"], $data);
			$GLOBALS["bus"]->addData($this->responseData);
		}
	}
?>
