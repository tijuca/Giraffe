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

/**
 * --DateTimePicker widget--   
 * @type	Widget
 * @classDescription	 This widget will create 1 datetime picker
 *
 * HOWTO BUILD:
 * - make one div with unique id like "startpick" without undersquare
 * - make a object: this.dtp = new DateTimePicker(dhtml.getElementById("startpick"),"Start time");
 * HOWTO USE:
 * - to change the time/date: this.dtp.setValue(1159164000);//use timestamps
 * - to get the time/date: this.dtp.getValue();//will return a timestamp
 * HOWTO REACT:
 * - it is possible to overwrite the onchange function
 *   this function will be exectute when the time or date
 *   are changed: this.dtp.onchange = dtpOnchange;
 * DEPENDS ON:
 * |----+-> datepicker.js
 * |    |----> date-picker.js
 * |    |----> date-picker-language.js
 * |    |----> date-picker-setup.js
 * |------> timepicker.js
 * |------> dhtml.js
 * |------> date.js
 */

DateTimePicker.prototype = new Widget;
DateTimePicker.prototype.constructor = DateTimePicker;
DateTimePicker.superclass = Widget.prototype;

//PUBLIC
/**
 * @constructor	This widget will create 1 datetime picker
 * @param {HtmlElement} element with a unique id without undersquare
 * @param {String} picTitle (optional) title that will be placed before the pickers
 * @param {Date} dateobj (optional) start date/time of the picker  
 */ 
function DateTimePicker(element,picTitle,dateobj)
{
	this.element = element;
	this.picTitle = picTitle;
	this.changed = 0;
	this.id = this.element.id;
	if(dateobj){
		this.value = dateobj;
	}
	else{
		this.value = new Date();
	}
	this.timeElement = null;
	this.dateElement = null;
	this.onchange = null;

	this.render();
}

/**
 * Function will return the value "timestamp" of the pickers
 * @return {Int} date/time in as timestamp 
 */
DateTimePicker.prototype.getValue = function()
{
	var result = 0;

	result = addUnixTimestampToUnixTimeStamp(this.dateElement.getValue(), this.timeElement.getValue());

	return result;
}

/**
 * Function will set "this.value" time/date and the value of the pickers
 * @param {Int} unixtime timestamp
 */
DateTimePicker.prototype.setValue = function(unixtime)
{
	var oldValue = parseInt(Math.floor(this.value/1000));
	var newValue = parseInt(unixtime);
	if(oldValue != newValue){
		this.value = new Date(newValue*1000);
		this.changed++;
		this.timeElement.setValue(timeToSeconds(this.value.getHours()+":"+this.value.getMinutes()));
		this.dateElement.setValue(newValue);
		this.changed--;
	}
}

//PRIVATE
/**
 * Function will build the date and the time picker in "this.element"
 */
DateTimePicker.prototype.render = function()
{
	//this.element.datetimepickerobject = this;
	var tableElement = dhtml.addElement(this.element,"table");
	tableElement = dhtml.addElement(tableElement,"tbody");
	tableElement = dhtml.addElement(tableElement,"tr");
	
	var r1Element = dhtml.addElement(tableElement,"td");
	this.dateElement = new datePicker(this.id,r1Element,this.picTitle,this.value);
	this.dateElement.dateTimePickerObj = this;
	this.dateElement.onchange = dateTimePickerOnchange;
	
	var r2Element = dhtml.addElement(tableElement,"td");
	this.timeElement = new timePicker(r2Element,"",(timeToSeconds(this.value.getHours()+":"+this.value.getMinutes())));
	this.timeElement.dateTimePickerObj = this;
	this.timeElement.onchange = dateTimePickerOnchange;
}

/**
 * Function will check if the value of the picker have been changed and will call
 * "this.onchange(this)" if there is a change
 * @param {Int} dayValue
 */
DateTimePicker.prototype.updateValue = function(dayValue)
{
	var oldValue = Math.floor(this.value.getTime()/1000);
	var newValue = this.getValue();

	this.changed++;

	if(dayValue == undefined){
		dayValue =0;
	}
	if(dayValue == 1){
		this.dateElement.setValue(newValue+(ONE_DAY/1000));
	}
	if(dayValue == -1){
		this.dateElement.setValue(newValue-(ONE_DAY/1000));
	}
	
	if(newValue != oldValue && dayValue == 0){
		this.setValue(newValue);
	}
	this.changed--;

	if(!this.changed && this.onchange){
		this.onchange(this,oldValue);
	}
}

/**
 * Function will call the "DateTimePicker.updateValue()" function
 * @param {HtmlElement} obj
 * @param {Int} oldValue
 * @param {Int} dayValue
 */
function dateTimePickerOnchange(obj,oldValue,dayValue)
{
	obj.dateTimePickerObj.updateValue(dayValue);
}
