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

function getDialogTitle() {
	return _("Flag");
}

function getIncludes(){
	return array(
			"client/layout/js/flag.js",
			"client/layout/js/date-picker.js",
			"client/layout/js/date-picker-language.js",
			"client/layout/js/date-picker-setup.js",
			"client/layout/css/date-picker.css",
			"client/widgets/datetimepicker.js",
			"client/widgets/combineddatetimepicker.js",
			"client/widgets/datepicker.js",
			"client/widgets/timepicker.js"
		);
}


function getJavaScript_onload(){ ?>
					var flag_icon = dhtml.getElementById("flag_icon");
					for(var i = 0; i < flag_icon.options.length; i++)
					{
						if(flag_icon.options[i].value == parentwindow.dhtml.getElementById("flag_icon").value) {
							flag_icon.options[i].selected = true;
						}
					}
					
					var flag_status = dhtml.getElementById("flag_status");
					if(parentwindow.dhtml.getElementById("flag_status").value == "1") {
						flag_status.checked = true;
						flag_icon.disabled = true;
					}
					
					var flag_due_by = parentwindow.dhtml.getElementById("flag_due_by").getAttribute("unixtime");
					if(flag_due_by !== null) {
						dhtml.getElementById("reminderdate").setAttribute("unixtime", parseInt(flag_due_by, 10));
					}

					// Make DTP object for flag.
					parentwindow.module.flag_dtp = new DateTimePicker(dhtml.getElementById("text_reminderdate"));
					if (parentwindow.module.flag_dtp){
						// Set time
						var unixtime = dhtml.getElementById("reminderdate").getAttribute("unixtime");
						if (unixtime !== null) {
							parentwindow.module.flag_dtp.setValue(parseInt(unixtime, 10));
						}
						
						/*
						 * Save old unixtime, because if item is not having reminder
						 * then DTP will set current time.
						 */
						parentwindow.module.old_unixtime = parentwindow.module.flag_dtp.getValue();
					}

					
<?php } // getJavaScript_onload						

function getBody(){ ?>
		<div>
			<table cellpadding="2" cellspacing="0">
				<tr>
					<td class="propertybold propertywidth" nowrap>
						<?=_("Color of Flag")?>:
					</td>
					<td>
						<select id="flag_icon" class="combobox">
							<option value="0"><?=_("None")?></option>
							<option class="label_red" value="6" selected><?=_("Red")?></option>
							<option class="label_blue" value="5"><?=_("Blue")?></option>
							<option class="label_yellow" value="4"><?=_("Yellow")?></option>
							<option class="label_green" value="3"><?=_("Green")?></option>
							<option class="label_orange" value="2"><?=_("Orange")?></option>
							<option class="label_purple" value="1"><?=_("Purple")?></option>
						</select>
					</td>
				</tr>
				<tr>
					<td class="propertybold propertywidth" nowrap>
						<?=_("End date")?>:
					</td>
					<td>
						<table cellspacing="0" cellpadding="0" border="0">
							<tr>
								<td>
									<input id="reminderdate" type="hidden">
									<div id="text_reminderdate"></div>
								</td>
							</tr>
						</table>
					</td>
				<tr>
					<td class="propertybold propertywidth" nowrap>
						<?=_("Complete")?>:
					</td>
					<td>
						<input id="flag_status" type="checkbox" onclick="if(this.checked) { dhtml.getElementById('flag_icon').disabled=true; } else { dhtml.getElementById('flag_icon').disabled=false; }">
					</td>
				</tr>
			</table>
			
			<?php
				$onlyStoreValues = isset($_GET['store_values']) ? $_GET['store_values'] : 'false';
				echo createConfirmButtons("setFlag($onlyStoreValues); window.close();");
			?>
		</div>
<?php } // getBody
?>
