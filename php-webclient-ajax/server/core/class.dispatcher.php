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
	* On-demand module loader
	*
	* The dispatcher is simply a class instance factory, returning an instance of a class. If
	* the source was not loaded yet, then the specified file is loaded.
	*
	* @package core
	*/
	class Dispatcher
	{
		function Dispatcher()
		{
			
		} 
		
		/**
		 * Load a module with a specific name
		 *
		 * If required, loads the source for the module, then instantiates a module of that type
		 * with the specified id and initial data. The $id and $data parameters are directly
		 * forwarded to the module constructor. 
		 *
		 * Source is loaded from server/modules/class.$modulename.php
		 *
		 * @param string $moduleName The name of the module which should be loaded (eg 'hierarchymodule')
		 * @param integer $id Unique id number which represents this module
		 * @param array $data Array of data which is received from the client
		 * @return object Module object on success, false on failed		 		 		 		 
		 */		 		
		function loadModule($moduleName, $id, $data)
		{
			$module = false;
			
			if(array_search($moduleName, $GLOBALS["availableModules"])!==false) {
				require_once("server/modules/class." . $moduleName . ".php");
				$module = new $moduleName($id, $data);
			}elseif(isset($GLOBALS["availablePluginModules"][ $moduleName ])) {
				require_once($GLOBALS["availablePluginModules"][ $moduleName ]['file']);
				$module = new $moduleName($id, $data);
			}else{
				trigger_error("Unknown module '".$moduleName."', id: '".$id."'", E_USER_WARNING);
			}
			return $module;
		}
	}
?>
