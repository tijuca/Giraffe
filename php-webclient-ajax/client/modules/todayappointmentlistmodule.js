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
 * todayappointmentlistmodule extend from the TodayListModule.
 */
todayappointmentlistmodule.prototype = new TodayListModule;
todayappointmentlistmodule.prototype.constructor = todayappointmentlistmodule;
todayappointmentlistmodule.superclass = TodayListModule.prototype;

function todayappointmentlistmodule(id, element, title, data)
{
	if(arguments.length > 0) {
		this.init(id, element, title, data);
	}
}

/**
 * Function which intializes the module.
 * @param integer id id
 * @param object element the element for the module
 * @param string title the title of the module
 * @param object data the data (storeid, entryid, ...)  
 */
todayappointmentlistmodule.prototype.init = function(id, element, title, data)
{
	todayappointmentlistmodule.superclass.init.call(this, id, element, title, data);
	
	this.events = new Object();
	this.events["click"] = eventTodayAppointmentClick;
	this.events["mouseover"] = eventTodayMessageMouseOver;
	this.events["mouseout"] = eventTodayMessageMouseOut;
	
	//get days value from the customize webaccess today settings dialog box.
	var duedays = webclient.settings.get("today/calendar/numberofdaysloaded", "7");
	
	var date = new Date();
	date.setTimeStamp(date.getDate(), date.getMonth()+1, date.getFullYear()); 

	this.startdate = date.getTime();
	this.duedate = date.getTime() + (duedays * ONE_DAY);

	this.initializeView();
}

/**
 * Function which intializes the view.
 */
todayappointmentlistmodule.prototype.initializeView = function()
{
	if (this.title) {
		this.setTitleInTodayView(this.title);
	}	
	this.contentElement = dhtml.addElement(this.element, "div", "appointmentmain");
	this.viewController.initView(this.id, "todayAppointment", this.contentElement, this.events);
}

/**
 * Function which execute an action. This function is called by the XMLRequest object.
 * @param string type the action type
 * @param object action the action tag 
 */
todayappointmentlistmodule.prototype.execute = function(type, action)
{
	switch(type)
	{
		case "list":
			this.messageList(action);
			break;
		case "item":
			this.item(action);
			break;
		case "delete":
			this.deleteItems(action);
			break;
	}
}
/**
 * Function which sends a request to the server, with the action "list".
 */ 
todayappointmentlistmodule.prototype.list = function()
{
	if(this.storeid && this.entryid) {
		this.data = new Object();
		this.data["store"] = this.storeid;
		this.data["entryid"] = this.entryid;
		this.data["restriction"] = new Object();
		this.data["restriction"]["startdate"] = this.startdate / 1000;
		this.data["restriction"]["duedate"] = this.duedate / 1000;
	
		webclient.xmlrequest.addData(this, "list", this.data);
	}
}

/**
 * Function which takes care of the list action. 
 * @param object action the action tag
 */ 
todayappointmentlistmodule.prototype.messageList = function(action)
{
	this.viewController.addItems(action.getElementsByTagName("item"), false, action);
	
	// remember item properties
	this.itemProps = new Object();
	var items = action.getElementsByTagName("item");
	for(var i=0;i<items.length;i++){
		this.updateItemProps(items[i]);
	}

	this.resize();	
}

todayappointmentlistmodule.prototype.resize = function()
{
	this.todaymodule.resize();
}

/**
 * function will call when mousedown on the appointment's subject or time. 
 * This will open appointment dialog box.
 */
function eventTodayAppointmentClick(moduleObject, element, event) 
{		
	if(typeof(element)!="undefined" && element.id) {
		//for EVENT attached on tr.
		var elementEntryId = element.id;
	}
	
	if(moduleObject.storeid && moduleObject.entryid && elementEntryId) {
		moduleObject.sendEvent("openitem", elementEntryId, "appointment");	
	}
}

