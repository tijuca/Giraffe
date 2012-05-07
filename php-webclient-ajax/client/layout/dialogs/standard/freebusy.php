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
<div id="appointment_freebusy_proposenewtime_container"></div>
<div id="freebusy_container" class="freebusy_container">

<div id="fbleft">
	<div class="left_header"><?=_("Zoom")?> 
		<select id="day_zoom">		
			<option value="1" selected>1 <?=_("day")?></option>
			<option value="2">2 <?=_("days")?></option>		
			<option value="3">3 <?=_("days")?></option>
			<option value="4">4 <?=_("days")?></option>
			<option value="6">6 <?=_("days")?></option>
			<option value="8">8 <?=_("days")?></option>
		</select> 
	</div>
	<div style="border-top: 1px solid #A5ACB2;" class="name_list_item"><?=_("All Attendees")?></div>
	<div class="name_list" id="name_list"></div>
	<div><input type="text" value="<?=_("Add a name")?>" class="idle" id="new_name" autocomplete="off"/></div>
</div>
<div id="screen_out" class="screen_out">
	<div id="screen_in" class="screen_in">
	</div>
</div>
<div id="freebusy_bottom_container">
<br>
<br>
<div id="fbbottom">
	<div id="autpic">
		<input type="button" onclick="fb_module.findPrevPickPoint()" value="<<"></input>
		<?=_("AutoPick")?>
		<input type="button" onclick="fb_module.findNextPickPoint()" value=">>"></input>
		<br>&nbsp; <!-- HACK -->
	</div>
	<div id="fbstartdate"></div>
	<div id="fballday">					
		<input id="checkbox_allday" type="checkbox" onclick="fb_module.onChangeAllDay();">
		<label for="checkbox_allday"><?=_("All Day Event")?></label>
	</div>
	<div id="fbenddate"></div>
	<div id="fbrecur" style="display: none">
		<p>
		<table border="0" cellpadding="1" cellspacing="0">
			<tr>
				<td>
					<?=_('Recurrence')?>: <span id="fbrecurtext"></span>
				</td>
			</tr>
		</table>
		<p>
	</div>
</div>

<div id="fblegend">
	<span class="legend_busy fb_state"></span>
	<span class="legend_text"><?=_("Busy")?></span>
	<span class="legend_tentative fb_state"></span>
	<span class="legend_text"><?=_("Tentative")?></span>
	<span class="legend_outofoffice fb_state"></span>
	<span class="legend_text"><?=_("Out of office")?></span>
	<span class="legend_unknown fb_state"></span>
	<span class="legend_text"><?=_("No Information")?></span>
</div>
</div>

</div>
