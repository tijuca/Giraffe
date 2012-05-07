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
	return _("Field Chooser");
}

function getIncludes(){
	return array(
			"client/layout/js/fieldchooser.js",
		);
}

function getJavaScript_onload(){ ?>
					initFieldChooser();
<?php } // getJavaScript_onload						

function getBody(){ ?>
		<div id="fieldchooser">
			<table width="100%" border="0" cellpadding="1" cellspacing="0">
				<tr>
					<td width="38%">
						<?=_("Available fields")?>:
					</td>
					<td width="23%">&nbsp;</td>
					<td width="38%">
						<?=_("Selected fields")?>:
					</td>
				</tr>
				<tr>
					<td>
						<select id="columns" class="combobox" size="16" style="width:100%"></select>
					</td>
					<td valign="top" align="center">
						<input type="button" class="buttonsize add" value="<?=_("Add")?>" onclick="addColumn();">
						<input type="button" class="buttonsize delete" value="<?=_("Delete")?>" onclick="deleteColumn();">
					</td>
					<td>
						<select id="selected_columns" class="combobox" size="16" style="width:100%"></select>
					</td>
				</tr>
				<tr>
					<td>&nbsp;</td>
					<td>&nbsp;</td>
					<td align="center">
						<input type="button" class="buttonsize" value="<?=_("Up")?>" onclick="columnUp();">
						<input type="button" class="buttonsize" value="<?=_("Down")?>" onclick="columnDown();">
					</td>
				</tr>
			</table>
		</div>
		
		<?=createConfirmButtons("submitFields();window.close();")?>
<?php } // getBody
?>
