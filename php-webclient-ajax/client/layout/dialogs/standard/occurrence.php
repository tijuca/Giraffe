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
	return _("Recurring appointment");
}

function getJavaScript_onload(){ ?>
<?php } // getJavaScript_onload

function getIncludes(){
	return array(
		"client/layout/css/occurrence.css"
	);
}

function getJavaScript_other(){ ?>
		function openAppointment(){
				var openOcc = dhtml.getElementById("occ");

				var entryid = dhtml.getElementById("entryid").value;
				var storeid = dhtml.getElementById("storeid").value;
				var basedate = dhtml.getElementById("basedate").value;
				var parententryid = dhtml.getElementById("parententryid").value;

				var uri = DIALOG_URL+"task=appointment_standard&storeid=" + storeid + "&entryid=" + entryid + "&parententryid=" + parententryid;
				if (openOcc.checked) {
					// Open the occurrence
					uri += "&basedate=" + basedate;
				}

				// Open new appointment window, and set focus on it.
				var apptWin = parentWebclient.openWindow(-1, "appointment", uri);
				apptWin.focus();
				window.close();
	}
<? } // javascript other

function getBody(){ ?>
		<div id="occurrence">
			<?=_("This is a recurring appointment. Do you want to open only this occurrence or the series?");?>
			<p>
			<ul>
			<li><input id="occ" name="occurrence" class="fieldsize" type="radio" value="occurrence" checked><label for="occ"><?=_("Open this occurrence");?></label></li>
			<li><input id="series" name="occurrence" class="fieldsize" type="radio" value="series"><label for="series"><?=_("Open the series");?></label></li>
			<input id="entryid" type="hidden" name="entryid" value="<?=htmlentities(get("entryid", false, false, ID_REGEX))?>">
			<input id="parententryid" type="hidden" name="parententryid" value="<?=htmlentities(get("parententryid", false, false, ID_REGEX))?>">
			<input id="storeid"  type="hidden" name="storeid"  value="<?=htmlentities(get("storeid", false, false, ID_REGEX))?>">
			<input id="basedate" type="hidden" name="basedate" value="<?=htmlentities(get("basedate", false, false, ID_REGEX))?>">
			</ul>
			
			<?=createConfirmButtons("openAppointment();")?>
		</div>
<?php } // getBody
?>
