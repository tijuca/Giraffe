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
 * suggestEmailAddressModule
 */

class suggestEmailAddressModule extends Module
{
	function suggestEmailAddressModule($id, $data)
	{
		parent::Module($id, $data);
	}

	function execute()
	{
		// Retrieve the recipient history
		$stream = mapi_openpropertytostream($GLOBALS["mapisession"]->getDefaultMessageStore(), PR_EC_RECIPIENT_HISTORY);
		$hresult = mapi_last_hresult();

		if($hresult == NOERROR){
			$stat = mapi_stream_stat($stream);
			mapi_stream_seek($stream, 0, STREAM_SEEK_SET);
			$xmlstring = '';
			for($i=0;$i<$stat['cb'];$i+=1024){
				$xmlstring .= mapi_stream_read($stream, 1024);
			}

			if($xmlstring !== "") {
				// don't pass empty string to xml parser otherwise it will give error
				$xml = new XMLParser();
				// convert the XMLstring using w2u, so that high characters wont give error in xml parsing.
				$recipient_history = $xml->getData(w2u($xmlstring));
			}
		}

		/**
		 * Check to make sure the recipient history is returned in array format 
		 * and not a PEAR error object.
		 */
		if(!isset($recipient_history) || !is_array($recipient_history) || !is_array($recipient_history['recipients'])){
			$recipient_history = Array(
				'recipients' => Array(
					'recipient' => Array()
				)
			);
		}else{
			/**
			 * When only one recipient is found in the XML it is saved as a single dimensional array
			 * in $recipient_history['recipients']['recipient'][RECIPDATA]. When multiple recipients
			 * are found, a multi-dimensional array is used in the format 
			 * $recipient_history['recipients']['recipient'][0][RECIPDATA].
			 */
			if($recipient_history['recipients']['recipient']){
				if(!is_numeric(key($recipient_history['recipients']['recipient']))){
					$recipient_history['recipients']['recipient'] = Array(
						0 => $recipient_history['recipients']['recipient']
					);
				}
			}
		}


		$data["attributes"] = array("type" => "none");
		foreach($this->data as $action){
			if(isset($action["attributes"]) && isset($action["attributes"]["type"])) {
				switch($action["attributes"]["type"]){
					case 'deleteRecipient':

						$l_aEmailAddresses = explode(";", $action['deleteRecipient']);
						for($i=0;$i<count($l_aEmailAddresses);$i++){
							/**
							 * A foreach is used instead of a normal for-loop to
							 * prevent the loop from finishing before the end of
							 * the array, because of the unsetting of elements 
							 * in that array.
							 **/
							foreach($recipient_history['recipients']['recipient'] as $key => $val){
								if($l_aEmailAddresses[$i] == $val['email']){
									unset($recipient_history['recipients']['recipient'][$key]);
								}
							}

							// Write new recipient history to property
							$xml = new XMLBuilder();
							$l_sNewRecipientHistoryXML = u2w($xml->build($recipient_history));
							
							$stream = mapi_openpropertytostream($GLOBALS["mapisession"]->getDefaultMessageStore(), PR_EC_RECIPIENT_HISTORY, MAPI_CREATE | MAPI_MODIFY);
							mapi_stream_write($stream, $l_sNewRecipientHistoryXML);
							mapi_stream_commit($stream);
							mapi_savechanges($GLOBALS["mapisession"]->getDefaultMessageStore());
						}
						break;

					case 'getRecipientList':
						if(strlen($action["searchstring"]) > 0 && is_array($recipient_history['recipients']) && count($recipient_history['recipients']) > 0){
							// Setup result array with match levels
							$l_aResult = Array(
									0 => Array(),	// Matches on whole string
									1 => Array()	// Matches on part of string
								);
							// Loop through all the recipients
							if(is_array($recipient_history['recipients']['recipient'])) {
                                for($i=0;$i<count($recipient_history['recipients']['recipient']);$i++){
                                    // Prepare strings for case sensitive search
                                    $l_sName = strtolower($recipient_history['recipients']['recipient'][$i]['name']);
                                    $l_sEmail = strtolower($recipient_history['recipients']['recipient'][$i]['email']);
                                    $l_sSearchString = strtolower($action["searchstring"]);

                                    // Check for the presence of the search string
                                    $l_ibPosName = strpos($l_sName, $l_sSearchString);
                                    $l_ibPosEmail = strpos($l_sEmail, $l_sSearchString);

                                    // Check if the string is present in name or email fields
                                    if($l_ibPosName !== false || $l_ibPosEmail !== false){
                                        // Setup dispay name
                                        if(!isset($recipient_history['recipients']['recipient'][$i]['objecttype']) || $recipient_history['recipients']['recipient'][$i]['objecttype'] == MAPI_MAILUSER) {
	                                        $l_sDisplayName = $recipient_history['recipients']['recipient'][$i]['name'];
    	                                    $l_sDisplayName .= ' <'.$recipient_history['recipients']['recipient'][$i]['email'].'>';
	                                        //$l_sDisplayName .= ' ['.$recipient_history['recipients']['recipient'][$i]['count'].']';
										} else {
	                                        $l_sDisplayName = '['.$recipient_history['recipients']['recipient'][$i]['name'].']';
										}
                                        // Check if the found string matches from the start of the word
                                        if($l_ibPosName === 0 || substr($l_sName, ($l_ibPosName-1), 1) == ' ' || $l_ibPosEmail === 0 || substr($l_sEmail, ($l_ibPosEmail-1), 1) == ' '){
                                            array_push($l_aResult[0], Array(
                                                'name' => $recipient_history['recipients']['recipient'][$i]['name'],
                                                'email' => $recipient_history['recipients']['recipient'][$i]['email'],
                                                'count' => $recipient_history['recipients']['recipient'][$i]['count'],
                                                'last_used' => $recipient_history['recipients']['recipient'][$i]['last_used'],
                                                'displayname' => $l_sDisplayName
                                                ));
                                        // Does not match from start of a word, but start in the middle
                                        }else{
                                            array_push($l_aResult[1], Array(
                                                'name' => $recipient_history['recipients']['recipient'][$i]['name'],
                                                'email' => $recipient_history['recipients']['recipient'][$i]['email'],
                                                'count' => $recipient_history['recipients']['recipient'][$i]['count'],
                                                'last_used' => $recipient_history['recipients']['recipient'][$i]['last_used'],
                                                'displayname' => $l_sDisplayName
                                                ));
                                        }
                                    }
                                }
                            }

							// Prevent the displaying of the exact match of the whole email address when only one item is found.
							if(count($l_aResult[0]) == 1 && count($l_aResult[1]) == 0 && $l_sSearchString == strtolower($l_aResult[0][0]['email'])){
								$l_aSortedList = Array();
							}else{
								/**
								 * Sort lists
								 *
								 * This block of code sorts the two lists and creates one final list. 
								 * The first list holds the matches based on whole words or words 
								 * beginning with the search string and the second list contains the 
								 * partial matches that start in the middle of the words. 
								 * The first list is sorted on count (the number of emails sent to this 
								 * email address), name and finally on the email address. This is done 
								 * by a natural sort. When this first list already contains the maximum 
								 * number of returned items the second list needs no sorting. If it has 
								 * less, then the second list is sorted and included in the first list 
								 * as well. At the end the final list is sorted on name and email again.
								 * 
								 */
								$l_iMaxNumListItems = 10;
								$l_aSortedList = Array();
								usort($l_aResult[0], Array($this, 'cmpSortResultList'));
								for($i=0;$i<min($l_iMaxNumListItems, count($l_aResult[0]));$i++){
									$l_aSortedList[] = $l_aResult[0][$i]['displayname'];
								}
								if(count($l_aSortedList) < $l_iMaxNumListItems){
									$l_iMaxNumRemainingListItems = $l_iMaxNumListItems - count($l_aSortedList);
									usort($l_aResult[1], Array($this, 'cmpSortResultList'));
									for($i=0;$i<min($l_iMaxNumRemainingListItems, count($l_aResult[1]));$i++){
										$l_aSortedList[] = $l_aResult[1][$i]['displayname'];
									}
								}
								natcasesort($l_aSortedList);
							}

							$data = Array(
								'searchstring' => $action["searchstring"],
								'returnid' => $action["returnid"],
								'results' => Array(
									'result' => $l_aSortedList
									)
								);
						}else{
							$data = Array(
								'searchstring' => $action["searchstring"],
								'returnid' => $action["returnid"],
								'results' => Array(
									'result' => Array()
									)
								);
						}
						break;
				}
			}
		}

		// Pass data on to be returned to the client
		$this->responseData["attributes"]["type"] = "none";
		array_push($this->responseData["action"], $data);
		$GLOBALS["bus"]->addData($this->responseData);

		return true;
	}


	function cmpSortResultList($a, $b){
		if($a['count'] < $b['count']){
			return 1;
		}elseif($a['count'] > $b['count']){
			return -1;
		}else{
			$l_iReturnVal = strnatcasecmp($a['name'], $b['name']);
			if($l_iReturnVal == 0){
				$l_iReturnVal = strnatcasecmp($a['email'], $b['email']);
			}
			return $l_iReturnVal;
		}
	}
}
?>
