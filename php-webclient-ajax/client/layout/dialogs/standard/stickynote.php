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
	return "stickynoteitemmodule";
}

function getModuleType(){
	return "item";
}

function getDialogTitle(){
	return _("Note");
}

function getIncludes(){
	return array(
			"client/layout/js/stickynote.js",
			"client/modules/".getModuleName().".js"
		);
}

function getJavaScript_onload(){ ?>
					module.init(moduleID);
					module.setData(<?=get("storeid","false","'", ID_REGEX)?>, <?=get("parententryid","false","'", ID_REGEX)?>);

					var attachNum = false;
					<? if(isset($_GET["attachNum"]) && is_array($_GET["attachNum"])) { ?>
						attachNum = new Array();
					
						<? foreach($_GET["attachNum"] as $attachNum) { 
							if(preg_match_all(NUMERIC_REGEX, $attachNum, $matches)) {
							?>
								attachNum.push(<?=intval($attachNum)?>);
						<?	}
						} ?>
					
					<? } ?>
					module.open(<?=get("entryid","false","'", ID_REGEX)?>, <?=get("rootentryid","false","'", ID_REGEX)?>, attachNum);
					
					resizeBody();
					
					dhtml.addEvent(false, dhtml.getElementById("html_body"), "contextmenu", forceDefaultActionEvent);

					// check if we need to send the request to convert the selected message as stickyNotes
					if(window.windowData && window.windowData["action"] == "convert_item") {
						module.sendConversionItemData(windowData);
					}

					//set focus on body of the note
					dhtml.getElementById("html_body").focus();
<?php } // getJavaSctipt_onload
			
function getBody() { ?>
		<input id="entryid" type="hidden">
		<input id="parent_entryid" type="hidden">
		<input id="message_class" type="hidden" value="IPM.StickyNote">
		<input id="icon_index" type="hidden" value="771">
		<input id="color" type="hidden" value="3">
		<input id="subject" type="hidden" value="">
		
		<div id="conflict"></div>
		
		<div class="stickynote_color">
			<table border="0" cellpadding="1" cellspacing="0">
				<tr>
					<td class="propertybold" width="60" nowrap>
						<?=_("Color")?>:
					</td>
					<td>
						<select id="select_color" class="combobox" onchange="onChangeColor();">
							<option value="0"><?=_("Blue")?></option>
							<option value="1"><?=_("Green")?></option>
							<option value="2"><?=_("Pink")?></option>
							<option value="3" selected><?=_("Yellow")?></option>
							<option value="4"><?=_("White")?></option>
						</select>
					</td>
				</tr>
			</table>
		</div>
		
		<textarea id="html_body" class="stickynote_yellow" cols="60" rows="12"></textarea>
<?php } // getBody

function getMenuButtons(){
	return array(
			array(
				'id'=>"save",
				'name'=>_("Save"),
				'title'=>_("Save"),
				'callback'=>'submitStickyNote'
			),
			array(
				'id'=>"seperator",
				'name'=>"",
				'title'=>"",
				'callback'=>""
			),
			array(
				'id'=>"delete",
				'name'=>"",
				'title'=>_("Delete"),
				'callback'=>"function(){delete_item()}"
			)
		);
}

?>
