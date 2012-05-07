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
	
	class TodayAppointmentListModule extends ListModule
	{
		/**
		 * @var Array properties of appointment item that will be used to get data
		 */
		var $properties = null;

		/**
		 * Constructor
		 * @param int $id unique id.
		 * @param array $data list of all actions.
		 */
		function TodayAppointmentListModule($id, $data)
		{
			$this->start = 0;
			parent::ListModule($id, $data, array(OBJECT_SAVE, TABLE_SAVE, TABLE_DELETE));	
		}	
	
		/**
		 * Executes actions in the $data variable.
		 * @return boolean true on success of false on fialure.
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

					$this->generatePropertyTags($store, $entryid, $action);
				
					switch($action["attributes"]["type"])
					{
						case "list":
							$this->getDelegateFolderInfo($store);
							$result = $this->messageList($store, $entryid, $action);
							break;
					}
				}
			}
			
			return $result;
		}
	
	
		/**
		 * Function which retrieves a list of messages in a folder
		 * @param object $store MAPI Message Store Object
		 * @param string $entryid entryid of the folder
		 * @param array $action the action data, sent by the client
		 * @return boolean true on success or false on failure		 		 
		 */
		function messageList($store, $entryid, $action)
		{
			$result = false;
			$items = array();

			if($store && $entryid) {

				if(isset($action["restriction"])) {
					if(isset($action["restriction"]["startdate"])) {
						$startDate = (int) $action["restriction"]["startdate"];
					}
				}
				if(isset($action["restriction"])) {
					if(isset($action["restriction"]["duedate"])) {
						$endDate = (int) $action["restriction"]["duedate"];
					}
				}
				
				// Restriction for calendar
				$appointmentRestriction = 
						// OR
						// (item[end] > start && item[start] < end) ||
						Array(RES_OR,
								 Array(
									   Array(RES_AND,
											 Array(
												   Array(RES_PROPERTY,
														 Array(RELOP => RELOP_GT,
															   ULPROPTAG => $this->properties["duedate"],
															   VALUE => $startDate
															   )
														 ),
												   Array(RES_PROPERTY,
														 Array(RELOP => RELOP_LT,
															   ULPROPTAG => $this->properties["startdate"],
															   VALUE => $endDate
															   )
														 )
												   )
											 ),
										// OR
										// (item[end] == start == item[start]) ZERO minute appointment
										Array(RES_AND,
											 Array(
												   Array(RES_PROPERTY,
														 Array(RELOP => RELOP_EQ,
															   ULPROPTAG => $this->properties["duedate"],
															   VALUE => $startDate
															   )
														 ),
												   Array(RES_PROPERTY,
														 Array(RELOP => RELOP_EQ,
															   ULPROPTAG => $this->properties["startdate"],
															   VALUE => $startDate
															   )
														 )
												   )
											 ),
										//OR
										//(item[isRecurring] == true)
										Array(RES_PROPERTY,
											Array(RELOP => RELOP_EQ,
												ULPROPTAG => $this->properties["recurring"],
												VALUE => true
													)
											)		
									)
							);// global OR
							
				// Create the data array, which will be send back to the client
				$data = array();
				$data["attributes"] = array("type" => "list");
								
				$folder = mapi_msgstore_openentry($store, $entryid);
				$table = mapi_folder_getcontentstable($folder);
				$calendaritems = mapi_table_queryallrows($table, $this->properties, $appointmentRestriction);
				$storeProviderGuid = mapi_getprops($store, array(PR_MDB_PROVIDER));

				foreach($calendaritems as $calendaritem)
				{
					$item = null;
					if (isset($calendaritem[$this->properties["recurring"]]) && $calendaritem[$this->properties["recurring"]]) 
					{				
						$recurrence = new Recurrence($store, $calendaritem);
						$recuritems = $recurrence->getItems($startDate, $endDate);

						foreach($recuritems as $recuritem)
						{
							$item = Conversion::mapMAPI2XML($this->properties, $recuritem);
							
							if(isset($recuritem["exception"])) {
								$item["exception"] = true;
							}
	
							if(isset($recuritem["basedate"])) {
								$item["basedate"] = array();
								$item["basedate"]["attributes"] = array();
								$item["basedate"]["attributes"]["unixtime"] = $recuritem["basedate"];
								$item["basedate"]["_content"] = strftime("%a %d-%m-%Y %H:%M", $recuritem["basedate"]);
							}
							$item = $this->disablePrivateItem($item, true);
							array_push($items, $item);
						}
					} else {
						$item = Conversion::mapMAPI2XML($this->properties, $calendaritem);
						$item = $this->disablePrivateItem($item, true);
						array_push($items,$item);
					}
				}
				
				usort($items, array("TodayAppointmentListModule","compareCalendarItems"));
				
				$data["item"] = $items;

				array_push($this->responseData["action"], $data);
				$GLOBALS["bus"]->addData($this->responseData);

				$result = true;
			}
			
			return $result;
		}
		
		/**
		 * Function will sort items for the month view
		 * small startdate->attributes->unixtime on top	 
		 */		 		
		function compareCalendarItems($a, $b)
		{
			$start_a = $a["startdate"]["attributes"]["unixtime"];
			$start_b = $b["startdate"]["attributes"]["unixtime"];
		
		   if ($start_a == $start_b) {
		       return 0;
		   }
		   return ($start_a < $start_b) ? -1 : 1;
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
			$this->properties = $GLOBALS["properties"]->getAppointmentProperties($store);
		}
	}
?>
