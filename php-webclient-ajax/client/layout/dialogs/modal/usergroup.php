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

function getDialogTitle(){
	return _("User group"); // use gettext here
}

function getModuleName() { 
	return "usergroupmodule"; 
}

function getIncludes() {
	$includes = array(
		"client/layout/js/usergroup.js",
		"client/modules/".getModuleName().".js"
	);
	return $includes;
}

function getJavaScript_onload(){ ?>
	module.init(moduleID);
	module.getGroups();

	var addButtonGroup = dhtml.getElementById("usergroup_dialog_usergroup_userlist_addgroup", "span");
	if(addButtonGroup){
		dhtml.addEvent(-1, addButtonGroup, "mouseover", eventMenuMouseOverTopMenuItem);
		dhtml.addEvent(-1, addButtonGroup, "mouseout", eventMenuMouseOutTopMenuItem);
	}

	var delButtonGroup = dhtml.getElementById("usergroup_dialog_usergroup_userlist_delgroup", "span");
	if(addButtonGroup){
		dhtml.addEvent(-1, delButtonGroup, "mouseover", eventMenuMouseOverTopMenuItem);
		dhtml.addEvent(-1, delButtonGroup, "mouseout", eventMenuMouseOutTopMenuItem);
	}

	var addButtonUser = dhtml.getElementById("usergroup_dialog_usergroup_userlist_adduser", "span");
	if(addButtonGroup){
		dhtml.addEvent(-1, addButtonUser, "mouseover", eventMenuMouseOverTopMenuItem);
		dhtml.addEvent(-1, addButtonUser, "mouseout", eventMenuMouseOutTopMenuItem);
	}

	var delButtonUser = dhtml.getElementById("usergroup_dialog_usergroup_userlist_deluser", "span");
	if(addButtonGroup){
		dhtml.addEvent(-1, delButtonUser, "mouseover", eventMenuMouseOverTopMenuItem);
		dhtml.addEvent(-1, delButtonUser, "mouseout", eventMenuMouseOutTopMenuItem);
	}
<?php } // getJavaScript_onload


function getBody() { ?>
	<div id="usergroup">
		<div class="usergroup_container">
			<div class="usergroupslist">
				<span class="usergroup_dialog_list_header"><?=_("Groups")?></span>
				<select id="usergroup_dialog_usergroup_list" multiple="multiple" size="5" class="usergroupslist"></select>
			</div>
			<div class="usergroup_options">
				<span class="menubutton icon icon_insert" id="usergroup_dialog_usergroup_userlist_addgroup" onclick="addGroup();" title="<?=_("Add Group")?>"></span>
				<span class="menubutton icon icon_remove" style="clear: both;" id="usergroup_dialog_usergroup_userlist_delgroup" onclick="removeGroup();" title="<?=_("Remove Group")?>"></span>
			</div>


			<div class="usergroup_content">
				<span class="usergroup_dialog_list_header"><?=_("Users")?></span>
				<select id="usergroup_dialog_usergroup_userlist" multiple="multiple" size="5" class="usergroupslist"></select>
			</div>
			<div class="usergroup_options">
				<span class="menubutton icon icon_insert" id="usergroup_dialog_usergroup_userlist_adduser" onclick="addUser();" title="<?=_("Add User")?>"></span>
				<span class="menubutton icon icon_remove" style="clear: both;" id="usergroup_dialog_usergroup_userlist_deluser" onclick="removeUser();" title="<?=_("Remove User")?>"></span>
			</div>

		</div>
		<div class="usergroup_footer_container">
			<?=createConfirmButtons("if(multiusercalendarSubmit()) window.close();")?>
		</div>
	</div>
<?php } // getBody
?>
