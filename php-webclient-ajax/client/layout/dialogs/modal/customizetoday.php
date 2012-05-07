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
function getModuleType(){
	return "item";
}

function getDialogTitle(){
	return _("Customize WebAccess Today");
}

function getIncludes(){
	return array(
			"client/layout/js/customizetoday.js"
		);
}

function getJavaScript_onload(){ ?>
	
	var todayModuleObject = window.windowData;
	init(todayModuleObject);

<?php } // getJavaSctipt_onload						
			
function getBody() { 

?>
		<div class="customize">
				
			<fieldset class="customizetoday_fieldset">
				<legend><?=_("Calendar")?></legend>
				
				<table border="0" cellpadding="1" cellspacing="0">
					<tr>
						<td class="">
							<?=_("Show this number of days in my calendar")?> : &nbsp;
						</td>
						<td>
							<select id="select_days" class="combobox" onchange="onChangeDays();">
								<option value="1">1</option>
								<option value="2">2</option>
								<option value="3">3</option>
								<option value="4">4</option>
								<option value="5">5</option>
								<option value="6">6</option>
								<option value="7">7</option>
							</select>
						</td>
					</tr>
				</table>
			</fieldset>
	
			<fieldset class="customizetoday_fieldset">
				<legend><?=_("Tasks")?></legend>
				
				<table border="0" cellpadding="1" cellspacing="0">
					<tr>
						<td class="">
							<?=_("Show this task by default")?> : &nbsp;
						</td>
						<td>
							<select id="select_tasks" class="combobox" onchange="onChangeTasks();">
							</select>
						</td>
					</tr>
				</table>
			</fieldset>
			
			<fieldset class="customizetoday_fieldset">
				<legend><?=_("Styles")?></legend>
				
				<table border="0" cellpadding="1" cellspacing="0">
					<tr>
						<td class="">
							<?=_("Show WebAccess Today in this style")?> : &nbsp;
						</td>
						<td>
							<select id="select_styles" class="combobox" onchange="onChangeStyles();">
								<option value="1">Standard</option>
								<option value="2">Standard(two column)</option>
								<option value="3">Standard(one column)</option>
							</select>
						</td>
					</tr>
				</table>
			</fieldset>
			
			<br/>
			<br/>
						
			<?=createConfirmButtons("customizeTodaySubmit();window.close();")   //see addressBookSubmit() for submition ?>  
		</div>

<?php
} // getBody 
?>
