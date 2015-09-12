<?php
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

?>
<?php

function getJavaScript_onload(){
?>
/**
 * Required input is 
 * window.windowData = {
 * 	windowname: "##NAME##",
 * 	dialogtitle: "##TITLE##",
 * 	fields: [
 * 		{
 * 			name: "##NAME##",
 * 			label: "##LABEL##",
 * 			type: "(normal|email)",
 * 			required: (true|false),
 * 			value: ""
 * 		},
 * 		{
 * 			name: "##NAME##",
 * 			label_start: "##LABEL##",
 *			label_end: "##LABEL##",
 * 			type: "(combineddatetime)",
 * 			required: (true|false),
 * 			value_start: "",
 * 			value_end: ""
 * 		},
 * 	]
 * };
 *
 * field type can be normal, email, combineddatetime, select, checkbox
 */
if(window.windowData && window.windowData.windowname){
	document.title = window.windowData.windowname;
}

if(window.windowData && window.windowData.dialogtitle){
	// this will overwrite title set by getDialogTitle() method
	dhtml.getElementById("windowtitle").innerHTML = window.windowData.dialogtitle;
}

var fieldsTable = dhtml.getElementById("fields", "table");
if(fieldsTable && window.windowData && typeof window.windowData.fields == "object"){
	var addedFocus = false;
	for(var i in window.windowData.fields){
		var fieldData = window.windowData.fields[i];
		var row = fieldsTable.insertRow(fieldsTable.rows.length);

		if(window.windowData.fields[i].type == "combineddatetime"){
			var thCell = dhtml.addElement(row, "th", null, null, fieldData.label_start);
			var tdCell = dhtml.addElement(row, "td", null, fieldData.id_start);
			var row2 = fieldsTable.insertRow(fieldsTable.rows.length);
			var thCell2 = dhtml.addElement(row2, "th", null, null, fieldData.label_end);
			var tdCell2 = dhtml.addElement(row2, "td", null, fieldData.id_end);

			var appoint_start = new DateTimePicker(tdCell, "");
			var appoint_end = new DateTimePicker(tdCell2, "");

			// set start & end datetime values
			if(fieldData.value_start && fieldData.value_end) {
				appoint_start.setValue(fieldData.value_start);
				appoint_end.setValue(fieldData.value_end);
			}

			var appoint_dtp = new combinedDateTimePicker(appoint_start,appoint_end);
			window.windowData.fields[i].combinedDateTimePicker = appoint_dtp;

		}else if(window.windowData.fields[i].type == "textarea"){
			var thCell = dhtml.addElement(row, "th", null, null, fieldData.label);
			var tdCell = dhtml.addElement(row, "td");

			var input = dhtml.addElement(tdCell, "textarea", "text", fieldData.name);
			input.value = (fieldData.value)?fieldData.value:"";
			if(!addedFocus){
				input.focus();
				addedFocus = true;
			}

		}else if(window.windowData.fields[i].type == "select"){
			var thCell = dhtml.addElement(row, "th", null, null, fieldData.label);
			var tdCell = dhtml.addElement(row, "td");

			var selectbox = dhtml.addElement(tdCell, "select", "text", fieldData.name);
			var index = 0;

			if(typeof fieldData.value == "object") {
				for(var key in fieldData.value) {
					if(typeof fieldData.value[key] == "object" && fieldData.value[key]["selected"] == true) {
						selectbox.options[index++] = new Option(fieldData.value[key]["text"], key, true, true);
					} else {
						selectbox.options[index++] = new Option(fieldData.value[key], key, false);
					}
				}
			}
			if(!addedFocus){
				selectbox.focus();
				addedFocus = true;
			}
			window.windowData.fields[i].selectBox = selectbox;

		}else if(window.windowData.fields[i].type == "checkbox"){
			var thCell = dhtml.addElement(row, "th");
			thCell.setAttribute("colSpan", 2);

			var checkbox = dhtml.addElement(null, "input", "text", fieldData.name);
			checkbox.setAttribute("type", "checkbox");
			// add element in DOM after setting all attributes,
			// otherwise IE doesn't support changing type attribute after adding checkbox to DOM
			thCell.appendChild(checkbox);

			var label = dhtml.addElement(thCell, "a", null, null, fieldData.label);

			if(typeof fieldData.value != "undefined") {
				if(fieldData.value == true) {
					checkbox.checked = true;
				}
			}
			if(!addedFocus){
				checkbox.focus();
				addedFocus = true;
			}
			window.windowData.fields[i].checkBox = checkbox;

		}else{
			var thCell = dhtml.addElement(row, "th", null, null, fieldData.label);
			var tdCell = dhtml.addElement(row, "td");

			var input = dhtml.addElement(tdCell, "input", "text", fieldData.name);
			input.value = (fieldData.value)?fieldData.value:"";
			if(!addedFocus){
				input.focus();
				addedFocus = true;
			}
		}
	}

}

<?php
}

function getDialogTitle(){
	return _("Input");
}

function getIncludes() {
	$includes = array(
		"client/layout/js/advprompt.js",
		"client/widgets/datetimepicker.js",
		"client/widgets/combineddatetimepicker.js",
		"client/layout/js/date-picker.js",
		"client/layout/js/date-picker-language.js",
		"client/layout/js/date-picker-setup.js",
		"client/layout/css/date-picker.css",
		"client/widgets/datepicker.js",
		"client/widgets/timepicker.js"
	);
	return $includes;
}

function getBody() { ?>
	<table id="fields"></table>
	<?=createConfirmButtons("if(advPromptSubmit()) window.close();")?>
<?php } // getBody
?>
