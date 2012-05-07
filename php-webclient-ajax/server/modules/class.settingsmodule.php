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
	* Settings Module
	*/
	class SettingsModule extends Module
	{
		/**
		* Constructor
		* @param int $id unique id.
		* @param array $data list of all actions.
		*/
		function SettingsModule($id, $data)
		{
			parent::Module($id, $data, array());
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
						case "retrieveAll":
							$this->retrieveAll($action["attributes"]["type"]);
							$result = true;
							break;
						case "set":
							if (isset($action["path"]) && isset($action["value"])){
								$this->set($action["path"], $action["value"]);
								$result = true;
							}else{
								$result = false;
							}
							break;
						case "delete":
							if (isset($action["path"])){
								$this->deleteSetting($action);
								$result = true;
							}else{
								$result = false;
							}
							break;
						case "convert":
							if (isset($action["html"])&&isset($action["callback"])){
								$this->convertHTML2Text($action);
								$result = true;
							}else{
								$result = false;
							}
							break;
					}
				}
			}
			return $result;
		}
		
		function retrieveAll($type)
		{
			$data = $GLOBALS['settings']->get();
			$data["attributes"] = array("type" => $type);
			
			array_push($this->responseData["action"], $data);
			$GLOBALS["bus"]->addData($this->responseData);
		}
		
		function set($path, $value)
		{
			$GLOBALS['settings']->set($path, $value);
		}

		function deleteSetting($action)
		{
			$GLOBALS['settings']->delete($action["path"]);
		}
		
		function convertHTML2Text($action)
		{
			$filter = new filter();
			
			$data = array();
			$data["text"] = $filter->html2text($action["html"]);
			$data["callback"] = $action["callback"];
			$data["attributes"] = array("type" => $action["attributes"]["type"]);
			array_push($this->responseData["action"], $data);
			$GLOBALS["bus"]->addData($this->responseData);
		}
	}
?>
