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
	 * Rules Module
	 */
	class RulesListModule extends ListModule
	{
		/**
		 * @var Array properties of rule item that will be used to get data
		 */
		var $properties = null;

		/**
		 * Constructor
		 * @param int $id unique id.
		 * @param array $data list of all actions.
		 */
		function RulesListModule($id, $data)
		{
			// Default Columns
			$this->tablecolumns = $GLOBALS["TableColumns"]->getRuleListTableColumns();

			parent::ListModule($id, $data, array());
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
				$store = $this->getActionStore($action);

				$this->generatePropertyTags($store, false, $action);

				switch($action["attributes"]["type"]) {
				case 'list':
					$rules = $this->ruleList($store, $action);
					
					if($rules) {
						$result = true;

						$rules["attributes"] = array("type" => "list");
						
						$storeProps = mapi_msgstore_getprops($store, array(PR_IPM_WASTEBASKET_ENTRYID));
						$wastebasket_entryid = $storeProps[PR_IPM_WASTEBASKET_ENTRYID];
						$rules["wastebasket_entryid"] = array("attributes"=>array("type"=>"binary"), "_content"=>bin2hex($wastebasket_entryid));

						array_push($this->responseData["action"], $rules);
						$GLOBALS["bus"]->addData($this->responseData);
					};
					break;
				case 'setRules':
					if (isset($action["rules"])){
						$this->setRules($store, $action["rules"]);
					}
					break;
				}
			}
			
			return $result;
		}
		
		function ruleList($store, $action)
		{
			$rules = $GLOBALS["operations"]->getRules($store, $this->properties);
			
			$data["item"] = $rules;
			
			$data["column"] = $this->tablecolumns;
			
			return $data;
		}
		
		function setRules($store, $rules)
		{
			$result = $GLOBALS["operations"]->updateRules($store, $rules, $this->properties);
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
			$this->properties = $GLOBALS["properties"]->getRulesProperties($store);

			$this->sort = array();
			$this->sort[$this->properties["rule_name"]] = TABLE_SORT_ASCEND;
		}
	}
?>
