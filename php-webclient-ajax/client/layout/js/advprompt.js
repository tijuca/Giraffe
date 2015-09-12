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

function advPromptSubmit() {
	if(window.windowData && typeof window.windowData.fields == "object"){
		var result = new Array();
		for(var i in window.windowData.fields){
			var fieldData = window.windowData.fields[i];

			switch(fieldData.type){
				case "textarea":
					var field = dhtml.getElementById(fieldData.name, "textarea");
					if(field){
						// Check for required field
						if(fieldData.required && (field.value).trim().length == 0){
							alert(_("Please fill in field \"%s\"").replace(/%s/g,fieldData.label));
							field.focus();
							return false;
						}
						result[fieldData.name] = (field.value).trim();
					// Field not found
					}else{
						return false;
					}

				case "combineddatetime":
					if(fieldData.combinedDateTimePicker){
						result[fieldData.name] = {
							start: fieldData.combinedDateTimePicker.getStartValue(),
							end: fieldData.combinedDateTimePicker.getEndValue()
						};
					}
					break;
				case "select":
					if(fieldData.selectBox) {
						if(typeof fieldData.selectBox.value == "undefined") {
							result[fieldData.name] = "";
						} else {
							result[fieldData.name] = fieldData.selectBox.options[fieldData.selectBox.selectedIndex].text;
						}
					}
					break;
				case "checkbox":
					if(fieldData.checkBox) {
						if(fieldData.checkBox.checked == true) {
							result[fieldData.name] = true;
						} else {
							result[fieldData.name] = false;
						}
					}
					break;
				case "normal":
				case "email":
				default:
					var field = dhtml.getElementById(fieldData.name, "input");
					if(field){
						// Check for required field
						if(fieldData.required && (field.value).trim().length == 0){
							alert(_("Please fill in field \"%s\"").replace(/%s/g,fieldData.label));
							field.focus();
							return false;
						}
						// Check for validation of field
						switch(fieldData.type){
							case "email":
								if(!validateEmailAddress((field.value).trim(), true)){
									// Alert is done by validateEmailAddress when it fails
									field.focus();
									return false;
								}
								break;
						}
						result[fieldData.name] = (field.value).trim();
					// Field not found
					}else{
						return false;
					}
					break;
			}

		}
		// This point is only reached when all checks are passed
		window.resultCallBack(result, window.callBackData);
		return true;
	// Cannot find window.windowData or window.windowData.field is not an array
	}else{
		return false;
	}
}
