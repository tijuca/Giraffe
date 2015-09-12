/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

function setFlag(onlyStoreValues)
{
	onlyStoreValues = typeof onlyStoreValues == "undefined" ? false : onlyStoreValues;
	
	if(parentwindow) {
		var flag_icon = dhtml.getElementById("flag_icon");
		var flag_status = dhtml.getElementById("flag_status");

		var flag_due_by = false;
		if (parentwindow.module && parentwindow.module.flag_dtp){
			if (parentwindow.module.old_unixtime !== parentwindow.module.flag_dtp.getValue()){
				flag_due_by = parentwindow.module.flag_dtp.getValue();
			}
		}

		parentwindow.dhtml.getElementById("reply_requested").value = true;
		parentwindow.dhtml.getElementById("response_requested").value = true;
		parentwindow.dhtml.getElementById("flag_request").value = flag_icon.options[flag_icon.selectedIndex].text; // for now set the flag_request text to the color of the flag
		parentwindow.dhtml.getElementById("flag_due_by").value = "";
		parentwindow.dhtml.getElementById("reminder_time").value = "";
		parentwindow.dhtml.getElementById("reply_time").value = "";
		parentwindow.dhtml.getElementById("reminder_set").value = false;
		parentwindow.dhtml.getElementById("flag_complete_time").value = "";

		if (flag_due_by){
			parentwindow.dhtml.getElementById("reminder_time").value = flag_due_by;
			parentwindow.dhtml.getElementById("reminder_set").value = true;
			parentwindow.dhtml.getElementById("flag_due_by").value = flag_due_by;
			parentwindow.dhtml.getElementById("flag_due_by").setAttribute('unixtime', flag_due_by);
			parentwindow.dhtml.getElementById("reply_time").value = flag_due_by;
		}

		if(flag_status.checked) { // completed
			parentwindow.dhtml.getElementById("flag_status").value = olFlagComplete;
			parentwindow.dhtml.getElementById("flag_icon").value = "";
			parentwindow.dhtml.getElementById("flag_complete_time").value = parseInt((new Date()).getTime()/1000, 10);
			parentwindow.dhtml.getElementById("reminder_set").value = false;
			parentwindow.dhtml.getElementById("reply_requested").value = false;
			parentwindow.dhtml.getElementById("response_requested").value = false;
		} else {
			parentwindow.dhtml.getElementById("flag_icon").value = flag_icon.options[flag_icon.selectedIndex].value;
			parentwindow.dhtml.getElementById("flag_status").value = olFlagMarked;
		}

		if(!onlyStoreValues) {
			var props = new Object();
			props["entryid"] = parentwindow.dhtml.getElementById("entryid").value;
			props["reminder_time"] = parentwindow.dhtml.getElementById("reminder_time").value;
			props["reminder_set"] = parentwindow.dhtml.getElementById("reminder_set").value;
			props["flag_request"] = parentwindow.dhtml.getElementById("flag_request").value;
			props["flag_due_by"] = parentwindow.dhtml.getElementById("flag_due_by").value;
			props["flag_icon"] = parentwindow.dhtml.getElementById("flag_icon").value;
			props["flag_status"] = parentwindow.dhtml.getElementById("flag_status").value;
			props["flag_complete_time"] = parentwindow.dhtml.getElementById("flag_complete_time").value;
			props["reply_requested"] = parentwindow.dhtml.getElementById("reply_requested").value;
			props["reply_time"] = parentwindow.dhtml.getElementById("reply_time").value;
			props["response_requested"] = parentwindow.dhtml.getElementById("response_requested").value;

			parentwindow.module.save(props);
		}
	}
}
