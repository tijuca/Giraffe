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
 * todaylistmodule extend from the Listmodule.
 */
TodayListModule.prototype = new ListModule;
TodayListModule.prototype.constructor = TodayListModule;
TodayListModule.superclass = ListModule.prototype;

function TodayListModule(id, element, title, data)
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
TodayListModule.prototype.init = function(id, element, title, data)
{
	TodayListModule.superclass.init.call(this, id, element, title, data);
}

/**
 * Function which sets the title for the todaymodule's sub module
 * @param string title the title
 * @param object events events
 */ 
TodayListModule.prototype.setTitleInTodayView = function(title)
{
	var titleElement = dhtml.addElement(this.element, "div", "heading");
	var header = dhtml.addElement(titleElement, "a", false, false, title);
    header.href ="#";
	dhtml.addEvent(this, titleElement, "click", eventTodayTitleClick);
}



// function open the view when click on the header. 
function eventTodayTitleClick(moduleObject, element, event)
{
	webclient.hierarchy.selectFolder(true, moduleObject.entryid);
}

function eventTodayMessageMouseOver(moduleObject, element, event){
		dhtml.addClassName(element, "trtextover");
}

function eventTodayMessageMouseOut(moduleObject, element, event){
		dhtml.removeClassName(element, "trtextover");
}
