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
require("client/layout/tabbar.class.php");

function getModuleName(){
	return "addressbookitemmodule";
}

function getModuleType(){
	return "item";
}

function getDialogTitle(){
	return _("GAB Details");
}

function getIncludes(){
	return array(
			"client/layout/css/tabbar.css",
			"client/layout/css/addressbookitem.css",
			"client/layout/js/tabbar.js",
			"client/widgets/tablewidget.js",
//			"client/layout/js/addressbookitem.js",
			"client/modules/itemmodule.js",
			"client/modules/".getModuleName().".js"
		);
}

function initWindow(){
	global $tabbar, $tabs;

	$tabs = array(
		"general" => _("General"),
		"memberslist" => _("Members"),
		"memberof" => _("Member Of"),
		"emailaddresses" => _("E-mail Addresses")
	);
	// Allowing to hook in and add more tabs
	$GLOBALS['PluginManager']->triggerHook("server.dialog.gab_detail_distlist.tabs.setup", array(
		'tabs' =>& $tabs
	));

	$tabbar = new TabBar($tabs, key($tabs));
}

function getJavaScript_onload(){
	global $tabbar;
	
	$tabbar->initJavascript("tabbar", "\t\t\t\t\t"); 
?>
					module.init(moduleID);
					module.setData(<?=get("storeid","false","'", ID_REGEX)?>);
					module.open(<?=get("entryid","false","'", ID_REGEX)?>);
<?php } // getJavaSctipt_onload

function getJavaScript_onresize(){ ?>

<?php } // getJavaScript_onresize	

function getBody() {
?>
<div class="addressbookitem abitem_distlist">
<?php

	global $tabbar, $tabs;
	
	$tabbar->createTabs();
	/*******************************
	 * Create General Tab          *
	 *******************************/
	$tabbar->beginTab("general");
?>

<div class="addressbookitem_left">
	<div class="abitem_distlist_fields">
		<table border="0" cellpadding="1" cellspacing="0">
			<tr>
				<td class="abitem_label"><?=_('Display name')?></td>
				<td class="abitem_value"><input id="display_name" class="abitem_field" type="text"></td>
			</tr>
			<tr>
				<td class="abitem_label"><?=_('Alias name')?></td>
				<td class="abitem_value"><input id="account" class="abitem_field" type="text"></td>
			</tr>
		</table>
	</div>
</div>
<div class="addressbookitem_right">
	<div class="headertitle"><?=_('Notes')?>:</div>
	<div class="notes"><textarea id="comment" class="abitem_field"></textarea></div>
</div>


<div class="addressbookitem_bottom">
<fieldset class="addressbookitem_fieldset hideborder">
	<div class="headertitle"><?=_('Owner')?>:</div>
	<div id="owner_table"></div>
</fieldset>
</div>

<span style="clear: both;"></span>
<?php 
	$tabbar->endTab();

	/*******************************
	 * Create Memberslist Tab      *
	 *******************************/
	$tabbar->beginTab("memberslist");
?>
<fieldset class="adsdressbookitem_fieldset">
	<legend><?=_('Members')?></legend>
	<div id="members_table" style="height: 225px;"></div>
</fieldset>

<?php 
	$tabbar->endTab();

	/*******************************
	 * Create MemberOf Tab         *
	 *******************************/
	$tabbar->beginTab("memberof");
?>
<div class="headertitle"><?=_('Group membership')?></div>
<div id="memberof_table"></div>

<?php 
	$tabbar->endTab();
	
	/*******************************
	 * Create E-mail Addresses Tab *
	 *******************************/
	$tabbar->beginTab("emailaddresses");
?>
<div id="proxy_addresses_table"></div>

<?php
	$tabbar->endTab();

	// Allowing to hook in and add more tabs
	$GLOBALS['PluginManager']->triggerHook("server.dialog.gab_detail_distlist.tabs.htmloutput", array());

?>
</div>
<?=createCloseButton("window.close();")?>
<?php

} // getBody

?>