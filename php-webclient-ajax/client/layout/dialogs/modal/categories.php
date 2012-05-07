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
	return "categoriesitemmodule";
}

function getModuleType(){
	return "item";
}

function getDialogTitle() {
	return _("Categories");
}

function getIncludes(){
	return array(
			"client/layout/js/categories.js",
			"client/modules/".getModuleName().".js"
		);
}

function getJavaScript_onload(){ ?>
					var entryid = <?=get("entryid","false","'", ID_REGEX)?>;
					
					if(entryid) {
						module.init(moduleID);
						module.setData(<?=get("storeid","false","'", ID_REGEX)?>, <?=get("parententryid","false","'", ID_REGEX)?>);
						module.open(entryid);
					}else if(window.windowData && window.windowData["categories"]){
						dhtml.getElementById("categories").value = window.windowData["categories"];
					} else {
						var categories = parentwindow.dhtml.getElementById("categories");
						
						if(categories) {
							dhtml.getElementById("categories").value = categories.value;
						} 
					}
					dhtml.addEvent(module, dhtml.getElementById("categories"), "blur", eventFilterCategories);
<?php } // getJavaScript_onload						

function getBody(){ ?>
		<div id="categorylist">
			<div class="selected_categories">
				<?=_("Item belongs to these categories")?>:
				<textarea id="categories"></textarea>
			</div>
			
			<?=_("Available categories")?>:
			
			<div style="position:relative;">
				<select id="available_categories" size="12" multiple ondblclick="addCategories();">
					<option><?=_("Business")?></option>
					<option><?=_("Competition")?></option>
					<option><?=_("Favorites")?></option>
					<option><?=_("Gifts")?></option>
					<option><?=_("Goals/Objectives")?></option>
					<option><?=_("Holiday")?></option>
					<option><?=_("Holiday Cards")?></option>
					<option><?=_("Hot Contacts")?></option>
					<option><?=_("Ideas")?></option>
					<option><?=_("International")?></option>
					<option><?=_("Key Customer")?></option>
					<option><?=_("Miscellaneous")?></option>
					<option><?=_("Personal")?></option>
					<option><?=_("Phone Calls")?></option>
					<option><?=_("Status")?></option>
					<option><?=_("Strategies")?></option>
					<option><?=_("Suppliers")?></option>
					<option><?=_("Time & Expenses")?></option>
					<option><?=_("VIP")?></option>
					<option><?=_("Waiting")?></option>	
				</select>
				<input type="button" class="buttonsize" value="<?=_("Add")?>" title="<?=_("Add selected categories")?>" onclick="addCategories();">
			</div>
		</div>
		
		<?=createConfirmButtons("window.resultCallBack(dhtml.getElementById('categories').value, window.callBackData);window.close();")?>
<?php } // getBody
?>
