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
	return "restoreitemslistmodule";
}

function getModuleType(){
	return "list";
}

function getDialogTitle(){
	return _("Restore deleted Items");
}

function getIncludes(){
	return array(
			"client/layout/js/restoreitems.js",
			"client/widgets/tablewidget.js",
			"client/modules/".getModuleName().".js"
		);
}

function getJavaScript_onload(){ ?>
	
	// pass the last parameter [data] with has_no_menu = true 
	module.init(moduleID,null, null, {has_no_menu:true} );

	//sometimes the menu is there but in hidden mode, so use this method to show the main menu. [a workaround]
	webclient.menu.showMenu();
	
	var data = new Object();
	data["storeid"] = <?=get("storeid","false","'", ID_REGEX)?>;
	data["entryid"] = <?=get("parententryid","false","'", ID_REGEX)?>;
	module.setData(data);
	
	module.list();
	webclient.xmlrequest.sendRequest();
	
	//added event handler for radio buttons
	dhtml.addEvent(module, dhtml.getElementById("message"), "click", loadRecoverableData);
	dhtml.addEvent(module, dhtml.getElementById("folder"), "click", loadRecoverableData);
	
<?php } // getJavaSctipt_onload
function getJavaScript_other(){ ?>
	var tableWidget;
<?php }

function getBody(){ ?>
	<div class="restore_option_button ">
		<input  name="recover_options" type="radio" id="message" value="list" checked ><label for="message"><?=_("Message")?></label>
		<input  name="recover_options" type="radio" id="folder" value="folderlist"><label for="folder"><?=_("Folder")?></label>
	</div>
	<input id="entryid" type="hidden">
	<input id="parent_entryid" type="hidden">
	<div id="conflict"></div>
	<div id="restoreitemstable" class="restoreItemTable"></div>
	<div id="restoreitems_status"></div>
<?php }

function getMenuButtons(){
	$menuItems = array(
		array(
			'id'=>"restore_item",
			'name'=>_("Restore"),
			'title'=>_("Restore the selected items"),
			'callback'=>'restore_selected_items'
		),
		array(
			'id'=>"restoreall",
			'name'=>_("Restore All"),
			'title'=>_("Restore all"),
			'callback'=>'permanent_restore_all'
		)
	);

	// add delete buttons only when DISABLE_DELETE_IN_RESTORE_ITEMS config is set to false
	if(!DISABLE_DELETE_IN_RESTORE_ITEMS) {
		array_push($menuItems,
			array(
				'id'=>"purge",
				'name'=>_("Permanent Delete"),
				'title'=>_("Permanent delete selected items"),
				'callback'=>'permanent_delete_items'
			),
			array(
				'id'=>"deleteall",
				'name'=>_("Delete All"),
				'title'=>_("Permanent delete All"),
				'callback'=>'permanent_delete_all'
			),
			array(
				'id'=>"seperator",
				'name'=>"",
				'title'=>"",
				'callback'=>""
			)
		);
	}

	array_push($menuItems,
		array(
			'id'=>"seperator",
			'name'=>"",
			'title'=>"",
			'callback'=>""
		),
		array(
			'id'=>"close",
			'name'=>_("Close"),
			'title'=>_("Close"),
			'callback'=>"function(){window.close();}"
		)
	);

	return $menuItems;
}
?>