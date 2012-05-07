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
			"client/modules/itemmodule.js",
			"client/modules/".getModuleName().".js"
		);
}

function initWindow(){
	global $tabbar, $tabs;

	$tabs = array(
		"general" => _("General"),
		"organization" => _("Organization"),
		"phone_notes" => _("Phone/Notes"),
		"memberof" => _("Member Of"),
		"emailaddresses" => _("E-mail Addresses")
	);
	// Allowing to hook in and add more tabs
	$GLOBALS['PluginManager']->triggerHook("server.dialog.gab_detail_mailuser.tabs.setup", array(
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
<div class="addressbookitem abitem_mailuser">
<?php

	global $tabbar, $tabs;
	
	/*******************************
	 * Create General Tab          *
	 *******************************/
	$tabbar->createTabs();
	$tabbar->beginTab("general");
?>

<fieldset class="addressbookitem_fieldset">
	<legend><?=_('Name')?></legend>

	<table border="0" cellpadding="1" cellspacing="0">
		<tr>
			<td class="abitem_label"><?=_('First')?></td>
			<td class="abitem_value given_name"><input id="given_name" class="abitem_field" type="text"></td>
			<td class="abitem_label initials"><?=_('Initials')?></td>
			<td class="abitem_value initials"><input id="initials" class="abitem_field" type="text"></td>
			<td class="abitem_label"><?=_('Last')?></td>
			<td class="abitem_value"><input id="surname" class="abitem_field" type="text"></td>
		</tr>
		<tr>
			<td class="abitem_label"><?=_('Display')?></td>
			<td class="abitem_value" colspan="3"><input id="display_name" class="abitem_field" type="text"></td>
			<td class="abitem_label"><?=_('Alias')?></td>
			<td class="abitem_value"><input id="account" class="abitem_field" type="text"></td>
		</tr>
	</table>

</fieldset>

<fieldset class="addressbookitem_fieldset hideborder">

	<table border="0" cellpadding="1" cellspacing="0">
		<tr>
			<td class="abitem_label" rowspan="2"><?=_('Address')?></td>
			<td class="abitem_value" rowspan="2">
				<textarea id="street_address" class="abitem_field"></textarea>
			</td>
			<td class="abitem_label"><?=_('Title')?></td>
			<td class="abitem_value"><input id="title" class="abitem_field" type="text"></td>
		</tr>
		<tr>
			<td class="abitem_label"><?=_('Company')?></td>
			<td class="abitem_value"><input id="company_name" class="abitem_field" type="text"></td>
		</tr>
		<tr>
			<td class="abitem_label"><?=_('City')?></td>
			<td class="abitem_value"><input id="locality" class="abitem_field" type="text"></td>
			<td class="abitem_label"><?=_('Department')?></td>
			<td class="abitem_value"><input id="department_name" class="abitem_field" type="text"></td>
		</tr>
		<tr>
			<td class="abitem_label"><?=_('State')?></td>
			<td class="abitem_value"><input id="state_or_province" class="abitem_field" type="text"></td>
			<td class="abitem_label"><?=_('Office')?></td>
			<td class="abitem_value"><input id="office_location" class="abitem_field" type="text"></td>
		</tr>
		<tr>
			<td class="abitem_label"><?=_('Zip code')?></td>
			<td class="abitem_value"><input id="postal_code" class="abitem_field" type="text"></td>
			<td class="abitem_label"><?=_('Assistant')?></td>
			<td class="abitem_value"><input id="assistant" class="abitem_field" type="text"></td>
		</tr>
		<tr>
			<td class="abitem_label"><?=_('Country/Region')?></td>
			<td class="abitem_value"><input id="country" class="abitem_field" type="text"></td>
			<td class="abitem_label"><?=_('Phone')?></td>
			<td class="abitem_value"><input id="phone" class="abitem_field" type="text"></td>
		</tr>
	</table>

</fieldset>

<?php 
	$tabbar->endTab();

	/*******************************
	 * Create Organization Tab     *
	 *******************************/
	$tabbar->beginTab("organization");
?>
<div class="headertitle"><?=_('Manager')?></div>
<div id="manager_table"></div>

<div class="headertitle"><?=_('Direct reports')?></div>
<div id="directreports_table"></div>

<?php 
	$tabbar->endTab();

	/*******************************
	 * Create Phone/Notes Tab      *
	 *******************************/
	$tabbar->beginTab("phone_notes");
?>

<fieldset class="addressbookitem_fieldset">
	<legend><?=_('Phone numbers')?></legend>

	<table border="0" cellpadding="1" cellspacing="0">
		<tr>
			<td class="abitem_label"><?=_('Business')?></td>
			<td class="abitem_value"><input id="business_telephone_number" class="abitem_field" type="text"></td>
			<td class="abitem_label"><?=_('Home')?></td>
			<td class="abitem_value"><input id="home_telephone_number" class="abitem_field" type="text"></td>
		</tr>
		<tr>
			<td class="abitem_label"><?=_('Business 2')?></td>
			<td class="abitem_value"><select id="business2_telephone_number" class="abitem_field"></select></td>
			<td class="abitem_label"><?=_('Home 2')?></td>
			<td class="abitem_value"><select id="home2_telephone_number" class="abitem_field"></select></td>
		</tr>
		<tr>
			<td class="abitem_label"><?=_('Fax')?></td>
			<td class="abitem_value"><input id="primary_fax_number" class="abitem_field" type="text"></td>
			<td class="abitem_label"><?=_('Mobile')?></td>
			<td class="abitem_value"><input id="mobile_telephone_number" class="abitem_field" type="text"></td>
		</tr>
		<tr>
			<td class="abitem_label"><?=_('Assistant')?></td>
			<td class="abitem_value"><input id="assistant_copy" class="abitem_field" type="text"></td>
			<td class="abitem_label"><?=_('Pager')?></td>
			<td class="abitem_value"><input id="pager_telephone_number" class="abitem_field" type="text"></td>
		</tr>
	</table>

</fieldset>

<fieldset class="addressbookitem_fieldset hideborder">

	<div class="headertitle"><?=_('Notes')?>:</div>
	<div class="notes"><textarea id="comment" class="abitem_field"></textarea></div>

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
	$GLOBALS['PluginManager']->triggerHook("server.dialog.gab_detail_mailuser.tabs.htmloutput", array());

?>
</div>
<?=createCloseButton("window.close();")?>
<?php

} // getBody

?>