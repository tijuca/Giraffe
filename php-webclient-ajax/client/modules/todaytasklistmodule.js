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
 * todaytasklistmodule extend from the TodayListModule.
 */
todaytasklistmodule.prototype = new TodayListModule;
todaytasklistmodule.prototype.constructor = todaytasklistmodule;
todaytasklistmodule.superclass = TodayListModule.prototype;

function todaytasklistmodule(id, element, title, data)
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
todaytasklistmodule.prototype.init = function(id, element, title, data)
{
	todaytasklistmodule.superclass.init.call(this, id, element, title, data);

	this.events = new Object();
	this.events["rowcolumn"] = new Object();
	this.events["rowcolumn"]["complete"] = new Object();
	this.events["rowcolumn"]["complete"]["click"] = eventListChangeCompleteStatus;
	this.events["rowcolumn"]["subject"] = new Object();
	this.events["rowcolumn"]["subject"]["click"] = eventTodayTaskListClickMessage;
	this.events["rowcolumn"]["subject"]["mouseover"] = eventTodayTaskListMouseOverMessage;
	this.events["rowcolumn"]["subject"]["mouseout"] = eventTodayTaskListMouseOutMessage;

	//get task folder entryid from settings
	this.entryid = webclient.settings.get("today/task/taskfolderid", this.entryid);
	this.uniqueid = "entryid";

	this.initializeView();
}

/**
 * Function which intializes the view.
 */
todaytasklistmodule.prototype.initializeView = function()
{
	if (this.title) {
		this.setTitleInTodayView(this.title);
	}

	this.contentElement = dhtml.addElement(this.element, "div", "main");
	this.viewController.initView(this.id, "todayTask", this.contentElement, this.events, null, this.uniqueid);
}

/**
 * Function which sends a request to the server, with the action "list".
 */ 
todaytasklistmodule.prototype.list = function(useTimeOut)
{
	if(this.storeid && this.entryid) {
		var data = new Object();
		data["store"] = this.storeid;
		data["entryid"] = this.entryid;
	
		webclient.xmlrequest.addData(this, "list", data);
	}
}

/**
 * Function which execute an action. This function is called by the XMLRequest object.
 * @param string type the action type
 * @param object action the action tag 
 */
todaytasklistmodule.prototype.execute = function(type, action)
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
		case "error":
			this.handleError(action);
			break;
	}
}

/**
 * Function which takes care of the list action. 
 * @param object action the action tag
 */ 
todaytasklistmodule.prototype.messageList = function(action)
{
	this.entryids = this.viewController.addItems(action.getElementsByTagName("item"), false, action);

	// remember item properties
	this.itemProps = new Object();
	var items = action.getElementsByTagName("item");
	for(var i=0;i<items.length;i++) {
		this.updateItemProps(items[i]);
	}

	this.resize();
}

todaytasklistmodule.prototype.resize = function()
{
	this.todaymodule.resize();
}

/**
 * function will call when mousedown on one task.
 * This will open task dialog box.
 */
function eventTodayTaskListClickMessage(moduleObject, element, event) 
{
	var elementId = element.parentNode.id;

	if(moduleObject.entryids[elementId]) {
		moduleObject.sendEvent("openitem", moduleObject.entryids[elementId], "task");
	}
}

function eventTodayTaskListMouseOverMessage(moduleObject, element, event)
{
	dhtml.addClassName(element, "trtextover");
}

function eventTodayTaskListMouseOutMessage(moduleObject, element, event)
{
	dhtml.removeClassName(element, "trtextover");
}

