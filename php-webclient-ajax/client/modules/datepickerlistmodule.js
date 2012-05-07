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

datepickerlistmodule.prototype = new ListModule;
datepickerlistmodule.prototype.constructor = datepickerlistmodule;
datepickerlistmodule.superclass = ListModule.prototype;

function datepickerlistmodule(id, element, title, data)
{
	if(arguments.length > 0) {
		this.init(id, element, title, data);
	}
}

datepickerlistmodule.prototype.init = function(id, element, title, data)
{
	this.elementHeight = 160;
	datepickerlistmodule.superclass.init.call(this, id, element, title, data);
	this.initializeView();
	
	var todayTitle = new Date().getDate()+" "+MONTHS[new Date().getMonth()]+" "+new Date().getFullYear();

	this.menuItems.push(webclient.menu.createMenuItem("seperator", ""));
	this.menuItems.push(webclient.menu.createMenuItem("today", _("Today"), _("Today")+": "+todayTitle, eventDatePickerContextGoToday));
	this.menuItems.push(webclient.menu.createMenuItem("seperator", ""));
	this.menuItems.push(webclient.menu.createMenuItem("day", _("Day"), _("Day"), eventDatePickerSwitchCalendarView ));
	this.menuItems.push(webclient.menu.createMenuItem("workweek", _("Workweek"), _("Workweek"), eventDatePickerSwitchCalendarView));
	this.menuItems.push(webclient.menu.createMenuItem("week", _("Week"), _("Week"), eventDatePickerSwitchCalendarView));
	this.menuItems.push(webclient.menu.createMenuItem("7days", _("7 Days"), _("7 Days"), eventDatePickerSwitchCalendarView));
	this.menuItems.push(webclient.menu.createMenuItem("month", _("Month"), _("Month"), eventDatePickerSwitchCalendarView));
    // this will add list view option in top menu bar
	this.menuItems.push(webclient.menu.createMenuItem("table", _("List"), _("List"), eventDatePickerSwitchCalendarView));
	
	if(webclient.settings.get("calendar/calendar_refresh_button", "true") == "true"){
		this.menuItems.push(webclient.menu.createMenuItem("seperator", ""));
		this.menuItems.push(webclient.menu.createMenuItem("refresh_calendar", _("Refresh"), _("Refresh"), eventDatePickerRefreshCalendarView));
	}
	webclient.menu.buildTopMenu(this.id, "appointment", this.menuItems, eventListNewMessage);
}

datepickerlistmodule.prototype.initializeView = function(month, year)
{
	this.setTitle(this.title, false, true);
	this.contentElement = dhtml.addElement(this.element, "div");
	
	this.events = new Object();
	this.events["day"] = new Object();
	this.events["day"]["mousedown"] = eventDatePickerListChangeDay;
	this.events["nextmonth"] = new Object();
	this.events["nextmonth"]["mousedown"] = eventDatePickerListNextMonth;
	this.events["previousmonth"] = new Object();
	this.events["previousmonth"]["mousedown"] = eventDatePickerListPreviousMonth;
	
}

datepickerlistmodule.prototype.loadFolder = function(folder, data)
{
	if (this.appointmentmodule){
		webclient.deleteModule(this.appointmentmodule);
	}

	this.appointmentmodule = webclient.loadModule("appointmentlist", folder["display_name"], "main", data, BORDER_LAYOUT);
	webclient.hierarchy.setNumberItems(folder["content_count"], folder["content_unread"]);
	this.appointmentmodule.datepicker = this;	
	
	// reload datepicker with new data

	this.setData(data);

	var date = new Date();
	this.changeMonth(date.getMonth()+1, date.getFullYear(), true, true);
	this.changeSelectedDate(date.getTime(), false);
}

datepickerlistmodule.prototype.changeMonth = function(month, year, previous, delay)
{
	if(previous) {
		month--;
		
		if(month < 0) {
			month = 11;
			year--;
		}
	} else {
		month++;
		
		if(month > 11) {
			month = 0;
			year++;
		}
	}
	
	var date = new Date();
	this.startdate = date.setTimeStamp(1, month+1, year) / 1000;
	
	dhtml.deleteAllChildren(this.contentElement);

	var data = new Object();
	data["month"] = month;
	data["year"] = year;
	
	if(this.timeoutid)
		clearTimeout(this.timeoutid)
	
	this.viewController.initView(this.id, "datepicker", this.contentElement, this.events, data);
	if(!delay) {
		this.list();
	} else {
		var module = this;
		this.timeoutid = setTimeout(function() {
			module.list();
			webclient.xmlrequest.sendRequest();
		}, 500);
	}
}

datepickerlistmodule.prototype.getRestrictionData = function()
{
	var restriction = new Object();
	
	if(this.startdate) {
		restriction["startdate"] = this.startdate;
	} else
		restriction = false; // cancel list, we have no startdate yet
	
	return restriction;
}

datepickerlistmodule.prototype.changeSelectedDate = function (newdate, refresh)
{
	this.oldDate = this.selectedDate;
	var oldElem = dhtml.getElementById(this.oldDate);
	var newElem = dhtml.getElementById(newdate)
	dhtml.removeClassName(oldElem,"calendarselect");
	if (!dhtml.hasClassName(newElem, "calendartoday")){
		dhtml.addClassName(newElem,"calendarselect");
	}
	
	if(this.appointmentmodule && refresh) {
		this.appointmentmodule.changeDays(newdate);
	}

	this.selectedDate = newdate;
}

datepickerlistmodule.prototype.changeView = function (view, refresh)
{
	if (this.appointmentmodule) {
		this.appointmentmodule.destructor();
		this.appointmentmodule.initializeView(view);
		if(refresh)
			this.appointmentmodule.changeDays(this.appointmentmodule.selectedDate);
	}
}

/**
 * Function which will be used to get the selected appointments.<b>
 * @param array messages array which has selected element ids.
 */
datepickerlistmodule.prototype.deleteMessages = function (messages)
{
	this.appointmentmodule.deleteMessages(messages);
}

/**
 * Function which will be used to get the selected appointments.<b> 
 * @return array array which has selected element ids.
 */
datepickerlistmodule.prototype.getSelectedMessages = function ()
{
	return this.appointmentmodule.getSelectedMessages();
}

datepickerlistmodule.prototype.showCopyMessagesDialog = function ()
{
	this.appointmentmodule.showCopyMessagesDialog();
}

datepickerlistmodule.prototype.printItem = function()
{
	moduleObject = this.appointmentmodule;
	moduleObject.printItem(moduleObject.entryids[moduleObject.selectedMessages[0]]);
}

/**
* Function to open a print dialog
* @param string entryid The entryid for the item
*/
datepickerlistmodule.prototype.printList = function(entryid) 
{
	var windowData = new Object();
	
	windowData["modulename"] = this.appointmentmodule.getModuleName();
	windowData["moduleID"] = this.appointmentmodule.id;
	windowData["startdate"] = this.appointmentmodule.startdate;
	windowData["duedate"] = this.appointmentmodule.duedate;
	windowData["selecteddate"] = this.appointmentmodule.selectedDate;
	windowData["view"] = this.appointmentmodule.selectedview;

	// temporarily disabled day/workweek/7days views
	switch(windowData["view"]) {
		case "day":
			windowData["view"] = "list";
			break;
		case "workweek":
			windowData["view"] = "week";
			break;
		case "7days":
			windowData["view"] = "week";
			break;
	}

	// please note that this url is also printed, so make it more "interesting" by first set the entryid
	webclient.openModalDialog(this, "printing", DIALOG_URL+"entryid="+entryid+"&storeid="+this.storeid+"&task=printlist_modal", 714, 800, null, null, windowData);
}

/**
 * Function which deletes one or more items in the view.
 * @param object action the action tag 
 */ 
datepickerlistmodule.prototype.deleteItems = function(action)
{
	this.viewController.viewObject.removeItemOfDay(dhtml.getXMLValue(action, "entryid"));
}

function eventDatePickerListNextMonth(moduleObject, element, event)
{
	var month = false;
	var year = false;
	var classNames = element.className.split(" ");
	
	for(var i = 0; i < classNames.length; i++)
	{
		if(classNames[i].indexOf("_") > 0) {
			month = parseInt(classNames[i].substring(classNames[i].indexOf("_") + 1, classNames[i].lastIndexOf("_")));
			year = parseInt(classNames[i].substring(classNames[i].lastIndexOf("_") + 1));
		}
	}
	
	moduleObject.changeMonth(month, year, false, true);
}

function eventDatePickerListPreviousMonth(moduleObject, element, event)
{
	var month = false;
	var year = false;
	var classNames = element.className.split(" ");
	
	for(var i = 0; i < classNames.length; i++)
	{
		if(classNames[i].indexOf("_") > 0) {
			month = parseInt(classNames[i].substring(classNames[i].indexOf("_") + 1, classNames[i].lastIndexOf("_")));
			year = parseInt(classNames[i].substring(classNames[i].lastIndexOf("_") + 1));
		}
	}
	
	moduleObject.changeMonth(month, year, true, true);
}

function eventDatePickerListChangeDay(moduleObject, element, event)
{
	moduleObject.changeSelectedDate(parseInt(element.id), true);
}

function eventDatePickerSwitchCalendarView(moduleObject, element, event)
{
	var newView = element.id;
	moduleObject.changeView(newView, true);
}

function eventDatePickerRefreshCalendarView(moduleObject, element, event)
{
	var selectedView = moduleObject.appointmentmodule.selectedview;
	moduleObject.changeView(selectedView, true);
}

function eventDatePickerContextGoToday(moduleObject, element, event)
{
	var date = new Date();
	moduleObject.changeMonth(date.getMonth()+1, date.getFullYear(), true);
	moduleObject.changeSelectedDate(date.getTime(), true);
}
