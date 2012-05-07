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
	return "ruleslistmodule";
}

function getModuleType(){
	return "list";
}

function getDialogTitle() {
	return _("Rules");
}

function getIncludes(){
	return array(
			"client/layout/js/rules.js",
			"client/layout/css/rules.css",
			"client/modules/".getModuleName().".js"
		);
}

function getJavaScript_onload(){ ?>
		var data = new Object;
		data.storeid = "<?=get("storeid", "", false, ID_REGEX)?>";
		module.init(moduleID, dhtml.getElementById("rules"), false, data);
		module.setData(data);
		module.list();

		module.addEventHandler("openitem", null, eventRulesOpenItem);
		
<?php } // getJavaScript_onload

function getJavaScript_onresize(){ ?>
		var tableContentElement = dhtml.getElementById("divelement");

		// Get all dialog elemnts to get their offsetHeight.
		var titleElement = dhtml.getElementById("windowtitle");
		var subTitleElement = dhtml.getElementById("subtitle");
		var tableHeaderElement = dhtml.getElementById("columnbackground");

		// Count the height for table contents.
		var tableContentElementHeight = dhtml.getBrowserInnerSize()["y"] - titleElement.offsetHeight - subTitleElement.offsetHeight - tableHeaderElement.offsetHeight - 80;

		if(tableContentElementHeight < 110)
			tableContentElementHeight = 110;

		// Set the height for table contents.
		tableContentElement.style.height = tableContentElementHeight +"px";
<?php } // getJavaScript_onresize

function getBody(){ ?>
		<div id="ruleslist">
			<?=_("The following rules have been defined:")?>
			<table>
				<tr>
					<td width="90%">
					<div id="rules" onmousedown="return false;">
					</div>
					</td>
					<td id="rulebuttons" width="10%">
					<input class="buttonsize rulebutton" type="button" value="<?=_("New") . "..."?>" onclick="addRule();"/><br/>
					<input class="buttonsize rulebutton" type="button" value="<?=_("Edit") . "..."?>" onclick="editRule();"/><br/>
					<input class="buttonsize rulebutton" type="button" value="<?=_("Delete")?>" onclick="deleteRule();"/><br/>
					<p>
					<input class="buttonsize rulebutton" type="button" value="<?=_("Up")?>" onclick="moveUp();"/><br/>
					<input class="buttonsize rulebutton" type="button" value="<?=_("Down")?>" onclick="moveDown();"/><br/>
					</td>
				</tr>
			</table>
		</div>
		
		<?=createConfirmButtons("if(submitRules()) window.close(); else window.focus();")?>
<?php } // getBody
?>
