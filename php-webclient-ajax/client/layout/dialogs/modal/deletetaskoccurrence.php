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
	return _("Confirm Delete");
}

function getJavaScript_onload(){ ?>
	if (window.windowData) {
		var subject = (window.windowData['subject'].length > 25) ? window.windowData['subject'].substr(0, 25) +"..." : window.windowData['subject'];
		if (window.windowData["taskrequest"]) {
			dhtml.getElementById("delete_info_bar_taskreq").innerHTML = _("The task \"%s\" has not been completed. What do you want to do?").sprintf(subject).htmlEntities();
			dhtml.getElementById("taskrecurr").style.display = "none";
		} else {
			dhtml.getElementById("delete_info_bar").innerHTML = _("The task \"%s\" is set to recur in the future. Do you want to delete all future occurrences of the task or just this occurrence?").sprintf(subject).htmlEntities();
			dhtml.getElementById("taskrequest").style.display = "none";
		}
	}

<?php } // getJavaScript_onload

function getIncludes(){
	return array(
		"client/layout/css/occurrence.css"
	);
}

function getJavaScript_other(){ ?>
	function deletetask() {
		var entryid = dhtml.getElementById("entryid").value;
		var parentModule = window.windowData.parentModule;
		if (parentModule) {
			if (window.windowData['taskrequest']) {
				var send = "delete";
				if (dhtml.getElementById("decline").checked) send = "decline";
				else if (dhtml.getElementById("complete").checked) send = "complete";

				parentModule.deleteMessage(send, entryid, typeof window.windowData.softDelete != 'undefined' ? window.windowData.softDelete : false);
			} else {
				var openOcc = dhtml.getElementById("occ");
				parentModule.deleteMessage(openOcc.checked ? "occurrence" : "delete", entryid, typeof window.windowData.softDelete != 'undefined' ? window.windowData.softDelete : false);
			}
		}

	}

<? } // javascript other

function getBody(){ ?>
		<div id="taskrecurr">
			<ul>
				<li><label id="delete_info_bar"></label></li>
			</ul>
			<ul>
				<li><input id="series" name="occurrence" class="fieldsize" type="radio" value="series" checked><label for="series"><?=_("Delete all");?></label></li>
				<li><input id="occ" name="occurrence" class="fieldsize" type="radio" value="occurrence"><label for="occ"><?=_("Delete this one");?></label></li>
				<input id="entryid" type="hidden" name="entryid" value="<?=htmlentities(get("entryid", false, false, ID_REGEX))?>">
			</ul>
		</div>
		<div id="taskrequest">
			<ul>
				<li><label id="delete_info_bar_taskreq"></label></li>
			</ul>
			<ul>
				<li><input id="decline" name="taskrequest" class="fieldsize" type="radio"checked><label for="decline"><?=_("Delete and Decline");?></label></li>
				<li><input id="complete" name="taskrequest" class="fieldsize" type="radio"><label for="complete"><?=_("Mark complete and delete");?></label></li>
				<li><input id="delete" name="taskrequest" class="fieldsize" type="radio"><label for="delete"><?=_("Delete");?></label></li>
			</ul>
		</div>
		<?=createConfirmButtons("deletetask();window.close();")?>
<?php } // getBody
?>