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

function vacation_loadSettings(settings){
	var field;
	var value;
	
	// signable
	value = settings.get("outofoffice/set","false");
	field = dhtml.getElementById("vacation_signable");
	field.checked = (value=="true");
	field = dhtml.getElementById("vacation_notsignable");
	field.checked = (value!="true");
	vacation_signableChange();

	// subject, set with default value
	value = settings.get("outofoffice/subject", _("Out of Office") );
	field = dhtml.getElementById("vacation_subject");
	field.value = value;


	// message, set with default value
	value = settings.get("outofoffice/message", _("User is currently out of office") +".");
	field = dhtml.getElementById("vacation_message");
	field.value = value;
}

function vacation_saveSettings(settings){
	var field;

	// signable
	field = dhtml.getElementById("vacation_signable");
	settings.set("outofoffice/set",field.checked?"true":"false");
	// Store current sessionid in settings, to get when setting for out of office was saved.
	settings.set("outofoffice_change_id", parentWebclient.sessionid);

	// subject
	field = dhtml.getElementById("vacation_subject");
	settings.set("outofoffice/subject",field.value);

	// message
	field = dhtml.getElementById("vacation_message");
	settings.set("outofoffice/message",field.value);
}

function vacation_signableChange(){
	var signable = dhtml.getElementById("vacation_signable").checked;	

	dhtml.getElementById("vacation_subject").disabled = !signable;
	dhtml.getElementById("vacation_message").disabled = !signable;
}
