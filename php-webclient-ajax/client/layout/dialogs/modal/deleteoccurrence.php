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

function getDialogTitle() {
	return _("Confirm Delete");
}

function getJavaScript_onload(){ ?>
<?php } // getJavaScript_onload						

function getIncludes(){
	return array(
		"client/layout/css/occurrence.css"
	);
}

function getJavaScript_other(){ ?>
			function deleteAppointment(){
				var openOcc = dhtml.getElementById("occ");

				var entryid = dhtml.getElementById("entryid").value;
				var storeid = dhtml.getElementById("storeid").value;
				var basedate = dhtml.getElementById("basedate").value;
				var parententryid = dhtml.getElementById("parententryid").value;
				var meeting = dhtml.getElementById("meeting").value;
				var elementid = dhtml.getElementById("elementid").value;

				var uri = DIALOG_URL+"task=appointment_standard&storeid=" + storeid + "&entryid=" + entryid + "&parententryid=" + parententryid;
				var dialogname = window.name;
				if(!dialogname) {
					dialogname = window.dialogArguments.dialogName;
				}
				
				var parentModuleType = window.windowData.parentModuleType;
				if(parentModuleType && parentModuleType == "item"){
					//it handles cancelinvitation for an item module appointments
					var parentModule = window.windowData.parentModule;
					parentModule.deleteMessage(openOcc.checked ? basedate : false);
					window.resultCallBack();
				} else {
					//it handles cancleinvitation for list module appointments
					var parentModule = window.windowData.parentModule;
					parentModule.deleteAppointment(entryid, openOcc.checked ? basedate : false, elementid);
				}
			}

			function cancelDelete(){
				var parentModuleType = window.windowData.parentModuleType;
				if(parentModuleType && parentModuleType == "item"){
					// Oraganizer MR setup
					parentwindow.meetingRequestSetup(1);
				}
			}

<? } // javascript other
                                                
function getBody(){ ?>
		<div id="occurrence">
			<?=_("Do you want to delete all occurrences of this recurring appointment or just this one?");?>
			<p>
			<ul>
			<li><input id="occ" name="occurrence" class="fieldsize" type="radio" value="occurrence" checked><label for="occ"><?=_("Delete this occurrence");?></label></li>
			<li><input id="series" name="occurrence" class="fieldsize" type="radio" value="series"><label for="series"><?=_("Delete the series");?></label></li>
			<input id="entryid" type="hidden" name="entryid" value="<?=htmlentities(get("entryid", false, false, ID_REGEX))?>">
			<input id="parententryid" type="hidden" name="parententryid" value="<?=htmlentities(get("parententryid", false, false, ID_REGEX))?>">
			<input id="storeid"  type="hidden" name="storeid"  value="<?=htmlentities(get("storeid", false, false, ID_REGEX))?>">
			<input id="basedate" type="hidden" name="basedate" value="<?=htmlentities(get("basedate", false, false, ID_REGEX))?>">
			<input id="meeting" type="hidden" name="meeting" value="<?=htmlentities($_GET["meeting"])?>">
			<input id="elementid" type="hidden" name="elementid" value="<?=htmlentities(get("elementid", false, false, ID_REGEX))?>">
			</ul>

			<div class="confirmbuttons">
				<?=createButtons(array("title"=>_("Ok"),"handler"=>"deleteAppointment();window.close();"), array("title"=>_("Cancel"),"handler"=>"cancelDelete();window.close();"))?>
			</div>
		</div>
<?php } // getBody
?>
