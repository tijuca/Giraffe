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
	 * Folder Properties Module
	 */
	class FolderSizeModule extends Module
	{
		/**
		 * Constructor
		 * @param int $id unique id.
		 * @param string $folderentryid Entryid of the folder. Data will be selected from this folder.
		 * @param array $data list of all actions.
		 */
		function FolderSizeModule($id, $data)
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
						case "foldersize":
							$subfolderdata = array();
							$store = $GLOBALS["mapisession"]->openMessageStore(hex2bin($action["store"]));
							$storeprops = mapi_getprops($store, array(PR_IPM_SUBTREE_ENTRYID, PR_DISPLAY_NAME));
							
							$folder = mapi_msgstore_openentry($store, hex2bin($action["entryid"]));
							$folderprops = mapi_getprops($folder, array(PR_ENTRYID, PR_DISPLAY_NAME));
							
							if ($folderprops[PR_ENTRYID] == $storeprops[PR_IPM_SUBTREE_ENTRYID]) {
								$foldername = w2u($storeprops[PR_DISPLAY_NAME]);
							} else {
								$foldername = w2u($folderprops[PR_DISPLAY_NAME]);
							}

							// Get the folder size
							$sizemainfolder = $GLOBALS["operations"]->calcFolderMessageSize($folder, false)/1024;
							
							// Get the subfolders in depth
							$subfoldersize = $this->getFolderSize($store, $folder, null, $subfolderdata);
							
							// Sort the folder names
							usort($subfolderdata, array("FolderSizeModule", "cmp_sortfoldername"));
							
							$data = array( 
										"mainfolder" => array(
													"name" => $foldername,
													"size" => round($sizemainfolder),
													"totalsize" => round($sizemainfolder + $subfoldersize)),
										"subfolders" => array("folder" => $subfolderdata) );
							
							// return response
							$data["attributes"] = array("type"=>"foldersize");
							array_push($this->responseData["action"], $data);
							$GLOBALS["bus"]->addData($this->responseData);
							$result = true;
							break;

					}
				}
			}
			
			return $result;
		}

		/**
		* Calculate recursive folder sizes with the folder name and subfolder sizes
		*
		* @param object $store store of the selected folder
		* @param object $folder selected folder 
		* @param string $parentfoldernames recursive the folder names seperated with a slash
		* @param array $subfolderdata array with folder information
		* @return integer total size of subfolders 
		*/
		function getFolderSize($store, $folder, $parentfoldernames, &$subfolderdata)
		{
			$totalsize = 0;
			
			$hierarchyTable = mapi_folder_gethierarchytable($folder);
			if (mapi_table_getrowcount($hierarchyTable) == 0) {
				return $totalsize;
			}
			
			mapi_table_sort($hierarchyTable, array(PR_DISPLAY_NAME => TABLE_SORT_ASCEND));

			$rows = mapi_table_queryallrows($hierarchyTable, Array(PR_ENTRYID, PR_SUBFOLDERS, PR_DISPLAY_NAME));
			foreach($rows as $subfolder)
			{
				$foldernames = (($parentfoldernames==null)?"":"$parentfoldernames \\ ") .w2u($subfolder[PR_DISPLAY_NAME]);
				$subfoldersize = 0;

				$folderObject = mapi_msgstore_openentry($store, $subfolder[PR_ENTRYID]);
				if ($folderObject) {
					$foldersize = $GLOBALS["operations"]->calcFolderMessageSize($folderObject, false)/1024;

					if($subfolder[PR_SUBFOLDERS]) {
						$subfoldersize = $this->getFolderSize($store, $folderObject, $foldernames, $subfolderdata);
					}
				}
				$subfolderdata[] = array(
									"name" => $foldernames,
									"size" => round($foldersize),
									"totalsize" => round($subfoldersize + $foldersize));
									
				$totalsize += $subfoldersize + $foldersize;
			}
			
			return $totalsize;
		}

		/**
		 * Sort the folder names
		 */
		function cmp_sortfoldername($a, $b)
		{
			return strnatcmp($a["name"], $b["name"]);
		}

	}
?>
