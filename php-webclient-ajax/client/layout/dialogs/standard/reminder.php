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

function getDialogTitle(){
	return _("Reminders");
}

function getIncludes(){
	return array(
			"client/layout/css/reminder.css",
			"client/widgets/tablewidget.js",
		);
}

function initWindow(){

}

function getJavaScript_onload(){
?>
	dhtml.addEvent(parentWebclient.reminder, dhtml.getElementById("dismissall"), "click", parentWebclient.reminder.eventDismissAll);
	dhtml.addEvent(parentWebclient.reminder, dhtml.getElementById("openitem"), "click", parentWebclient.reminder.eventOpenItem);
	dhtml.addEvent(parentWebclient.reminder, dhtml.getElementById("dismiss"), "click", parentWebclient.reminder.eventDismiss);
	dhtml.addEvent(parentWebclient.reminder, dhtml.getElementById("snooze"), "click", parentWebclient.reminder.eventSnooze);

	parentWebclient.reminder.showData();
<?php } // getJavaSctipt_onload						

function getJavaScript_onresize(){ ?>

<?php } // getJavaScript_onresize	

function getBody() {
?>
<table id="reminderinfo">
	<tr><td><div id="remindericon" class="icon">&nbsp;</div></td><td><div id="remindersubject"></div></td></tr>
	<tr><td>&nbsp;</td><td><div id="reminderdate"><?=sprintf(_("%d reminders are selected"),0)?></div></td></tr>
</table>
<div id="remindertable"></div>
<table id="remindercontrols">
	<tr>
		<td><input type="button" id="dismissall" value="<?=_("Dismiss All")?>"></td>
		<td>&nbsp;</td>
		<td><input type="button" id="openitem" value="<?=_("Open Item")?>"></td>
		<td><input type="button" id="dismiss" value="<?=_("Dismiss")?>"></td>
	</tr>
	<tr>
		<td colspan="4"><?=_("Click Snooze to be reminded again in:")?></td>
	</tr>
	<tr>
		<td colspan="3">
			<select id="snoozetime">
				<option value="5">5 <?=_("minutes")?></option>
				<option value="10">10 <?=_("minutes")?></option>
				<option value="15">15 <?=_("minutes")?></option>
				<option value="30">30 <?=_("minutes")?></option>
				<option value="60">1 <?=_("hour")?></option>
				<option value="120">2 <?=_("hours")?></option>
				<option value="240">4 <?=_("hours")?></option>
				<option value="480">8 <?=_("hours")?></option>
				<option value="720">0,5 <?=_("days")?></option>
				<option value="1440">1 <?=_("day")?></option>
				<option value="2880">2 <?=_("days")?></option>
				<option value="4320">3 <?=_("days")?></option>
				<option value="5760">4 <?=_("days")?></option>
				<option value="10080">1 <?=_("week")?></option>
				<option value="20160">2 <?=_("weeks")?></option>
			</select>
		</td>
		<td><input type="button" id="snooze" value="<?=_("Snooze")?>"></td>
	</tr>
</table>

<?
} // getBody

?>
