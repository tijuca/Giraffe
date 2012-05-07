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


function getDialogTitle() {
	return _("Send Meeting Request Mail Confirmation");
}

function getIncludes(){
	return array(
			"client/layout/css/sendMRMailConfirmation.css",
			"client/layout/css/occurrence.css"
	);
}

function getJavaScript_onload(){ ?>
	var windowData = typeof window.windowData != "undefined" ? window.windowData : false;
	var sendMRMailConfirmationEle = dhtml.getElementById("sendMRMailConfirmation");
	var confirmDeleteEle = dhtml.getElementById("confirmDelete");

	if (windowData) {
		if (windowData.confirmDelete) {
			sendMRMailConfirmationEle.style.display = 'none';
			confirmDeleteEle.style.display = 'block';

			var delete_info_bar = dhtml.getElementById("delete_info_bar");
			var subjectString = (window.windowData["subject"].length > 30) ? window.windowData["subject"].substr(0, 30)+"..." : window.windowData["subject"];
			delete_info_bar.innerHTML = _("The meeting \"%s\" was already accepted").sprintf(subjectString).htmlEntities();
		} else {
			confirmDeleteEle.style.display = 'none';
		}
	}
<?php }


function getJavaScript_other(){ ?>
	function editResponse(){
		var body = dhtml.getElementById("body");
		if(dhtml.getElementById('editResponse').checked){
			body.removeAttribute("disabled");
		}else{
			body.setAttribute("disabled","disabled");
		}
	}

	function sendMRMailConfirmation(){
		if(dhtml.getElementById('editResponse').checked){
			var response = new Object;
			response.body = dhtml.getElementById('body').value;
			response.type = false;
			window.resultCallBack(response, window.callBackData);
		}else{
			window.resultCallBack(dhtml.getElementById('noResponse').checked, window.callBackData);
		}
	}

	function sendConfirmDelete() {
		window.resultCallBack(window.callBackData, dhtml.getElementById('sendResponseAsDelete').checked, window.windowData["basedate"] ? window.windowData["basedate"] : false, window.windowData);
	}
<?}

function getBody(){ ?>
		<div id="sendMRMailConfirmation">
			<div>
				<input type="radio" name="action" id="editResponse" onclick ="editResponse()" />
				<label for="editResponse"><?=_("Edit a response before Sending")?></label>
				<div id="responseBody">
					<label for="body"><?=_("Type Response Message").":"?></label><br/>
					<textarea cols=20 rows=5 name="body" id="body" disabled></textarea>
				</div>
			</div>
			<div>
				<input type="radio" name="action" id="sendResponse" onclick ="editResponse()" checked/>
				<label for="sendResponse"><?=_("Send a response")?></label>
			</div>
			<div>
				<input type="radio" name="action" id="noResponse" onclick ="editResponse()"/>
				<label for="noResponse"><?=_("Don't send a response")?></label>
			</div>
			<?=createConfirmButtons("sendMRMailConfirmation(); window.close();")?>
		</div>

		<div id="confirmDelete">
			<label id="delete_info_bar"></label>
			<p>
			<ul>
				<li><input type="radio" name="delete" id="sendResponseAsDelete" checked/><label for="sendResponseAsDelete"><?=_("Delete and send a response to the meeting organizer")."."?></label></li>
				<li><input type="radio" name="delete" id="deletenoResponse"/><label for="deletenoResponse"><?=_("Delete without sending a response")."."?></label></li>
			</ul>
			<?=createConfirmButtons("sendConfirmDelete(); window.close();")?>
		</div>
<?php } // getBody
?>
