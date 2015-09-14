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
function getModuleName(){
	return "delegatesmodule";
}
function getDialogTitle(){
	return _("Delegates");
}
function getIncludes(){
	$includes = array(
		"client/modules/".getModuleName().".js",
		"client/layout/js/delegates.js",
		"client/layout/css/delegates.css",
		"client/widgets/tablewidget.js"
	);
	return $includes;
}
function getJavaScript_onload(){
?>
	var data = new Object();
	// We need storeid to open GAB.
	data["storeid"] = parentWebclient.hierarchy.defaultstore.id;
	module.init(moduleID, data);
	module.getDelegates();
	initDelegates();
	
	// add events
	dhtml.addEvent(module, dhtml.getElementById("add_delegate"), "click", eventAddDelegate);
	dhtml.addEvent(module, dhtml.getElementById("remove_delegate"), "click", eventRemoveDelegate);
	dhtml.addEvent(module, dhtml.getElementById("edit_permissions"), "click", eventEditDelegatePermissions);
<?php }  // getJavaScript_onload

function getJavaScript_other(){
?>
	var delegatesTable;
<?php } //getJavaScript_other(){

function getBody(){ ?>
	<div class="delegates delegates_info">
		<label><?=_("Delegates can send items on your behalf, and optionally view or edit items in your store. This is setup by the 'Edit Delegates...' button on the Preferences tab of the Settings window. To enable others to access your folders without giving send-on-behalf-of privileges, is configured on the Permissions tab of the folder Preferences (accessed by right clicking the folder).")?></label>
	</div>
	<div class="delegateslist">
		<div id="delegateslist_container" class="delegates delegateslist_container"></div>
	</div>
	<div class="delegates_options">
		<table cellpadding="0px" cellspacing="0px" width="100%" align="right">
			<tr>
				<td>
					<input id="add_delegate" type="button"  class="buttonsize" value="<?=_("Add")?>...">
				</td>
			</tr>
			<tr>
				<td>
					<input id="remove_delegate" type="button"  class="buttonsize" value="<?=_("Remove")?>...">
				</td>
			</tr>
			<tr>
				<td>
					<input id="edit_permissions" type="button"  class="buttonsize" value="<?=_("Permissions")?>...">
				</td>
			</tr>
		</table>
	</div>
	<div class="delegates">&nbsp;</div>
<?php
	print createConfirmButtons("submitDelegates();"); 
} // getBody
?>