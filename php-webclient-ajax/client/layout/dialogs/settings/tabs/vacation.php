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

function vacation_settings_title(){
	return _("Out of Office");
}

function vacation_settings_order(){
	return 2;
}

function vacation_settings_html(){ ?>
	<fieldset>
		<legend><?=_("Out of Office Assistant")?></legend>
		<table class="options">
			<tr>
				<th colspan="2"><input id="vacation_notsignable" type="radio" name="vacation_option" onclick="vacation_signableChange();" class="checkbox"><label for="vacation_notsignable"><?=_("I am currently In the Office")?></label></th>
			</tr>
			<tr>
				<th colspan="2"><input id="vacation_signable" type="radio" name="vacation_option" onclick="vacation_signableChange();" class="checkbox"><label for="vacation_signable"><?=_("I am currently Out of the Office")?></label></th>
			</tr>
		</table>
		<table class="textinput" style="margin-top: 5px">
			<tr>
				<th><label for="vacation_subject"><?=_("Subject")?>:</label></th>
				<td><input id="vacation_subject" type="text" class="text"></td>
			</tr>
			<tr>
				<th colspan="2"><label for="vacation_message"><?=_("AutoReply only once to each sender with the following text")?>:</label></th>
			</tr>
			<tr>
				<th>&nbsp;</th>
				<td><textarea id="vacation_message" rows="6" cols="60"></textarea></td>
			</tr>
		</table>
	</fieldset>
<?php } ?>
