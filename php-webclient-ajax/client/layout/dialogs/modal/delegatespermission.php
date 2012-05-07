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
function getModuleName(){
	return "delegatesmodule";
}
function getDialogTitle(){
	return _("Delegate Permissions");
}
function getIncludes(){
	$includes = array(
		"client/modules/".getModuleName().".js",
		"client/layout/js/delegatespermissions.js",
		"client/layout/css/delegates.css",
		"client/widgets/tablewidget.js"
	);
	return $includes;
}

function getJavaScript_onload(){
?>
	module.init(moduleID);
	if (window.windowData){
		var user = window.windowData["delegate"];
		editDelegates = user;
		if (window.windowData["newDelegate"] == true)
			module.getNewUserPermissions(user[0]["entryid"]);
		initDelegatePermissions(user);
	}
<?php } // getJavaScript_onload
function getJavaScript_other(){
?>
	var delegate;
	var editDelegates;
	
<?php }
function getBody(){ ?>
<div id="delegatespermissions">
	<fieldset>
		<legend><?=_("This delegate has the following permissions")?></legend>
		<table cellpadding="8" cellspacing="0" width="100%">
			<tr>
				<td class="delegatesfolders" style="padding-bottom: 0px;"><label><?=_("Calendar")?>:</label></td>
				<td style="padding-bottom: 0px;">
					<select id="calendar" onchange="toggleDelegateMeetingRuleOption(this);" onkeyup="toggleDelegateMeetingRuleOption(this);"></select>
				</td>
			</tr>
			<tr>
				<td colspan="2" style="padding: 0px 0px 0px 32px;">
					<input type="checkbox" id="delegate_meeting_rule_checkbox" class="checkbox" disabled />
					<label for="delegate_meeting_rule_checkbox" class="disabled_text"><?=_("Delegate receives copies of meeting-related messages sent to me")?></label>
				</td>
			</tr>
			<tr>
				<td class="delegatesfolders"><label><?=_("Tasks")?>:</label></td>
				<td>
					<select id="tasks"></select>
				</td>
			</tr>
			<tr>
				<td class="delegatesfolders"><label><?=_("Inbox")?>:</label></td>
				<td>
					<select id="inbox"></select>
				</td>
			</tr>
			<tr>
				<td class="delegatesfolders"><label><?=_("Contacts")?>:</label></td>
				<td>
					<select id="contacts"></select>
				</td>
			</tr>
			<tr>
				<td class="delegatesfolders"><label><?=_("Notes")?>:</label></td>
				<td>
					<select id="notes"></select>

				</td>				
			</tr>
			<tr>
				<td class="delegatesfolders"><label><?=_("Journal")?>:</label></td>
				<td>
					<select id="journal"></select>
				</td>				
			</tr>
		</table>
	</fieldset>
</div>
<div>
	<input id="see_private" type="checkbox" class="checkbox">
	<label for="see_private"><?=_("Delegate can see my private items")?></label>
</div>
<?
	print (createConfirmButtons("submitDelegatePermissions();window.close();"));
} ?>