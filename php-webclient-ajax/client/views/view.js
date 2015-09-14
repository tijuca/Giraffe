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

/**
 * --View--
 * @classDescription	View base class
 *  
 * DEPENDS ON:
 * |------> view.js
 * |----+-> *listmodule.js
 * |    |----> listmodule.js
 */

/**
 * TODO: implement default functions
 */

View.prototype.constructor = View;

/**
 * @constructor
 * @param {Int} moduleID
 * @param {HtmlElement} element
 * @param {Object} events
 * @param {XmlElement} data
 */
function View(moduleID, element, events, data)
{
}

/**
 * Function will render the view and execute this.resizeView when done
 */
View.prototype.initView = function()
{
}

View.prototype.destructor = function()
{
	// Unregister module from InputManager
	webclient.inputmanager.removeObject(webclient.getModule(this.moduleID));
}

View.prototype.execute = function()
{
}

View.prototype.showEmptyView = function()
{
}

/**
 * Function will resize all elements in the view
 */
View.prototype.resizeView = function()
{
}

/**
 * Function will show Loading text in view
 */
View.prototype.loadMessage = function()
{
	if (this.element){
		dhtml.removeEvents(this.element);
		dhtml.deleteAllChildren(this.element);
	
		this.element.innerHTML = "<center>" + _("Loading") + "...</center>";
		document.body.style.cursor = "wait";
	}
}

/**
 * Function will delete load text in view
 */
View.prototype.deleteLoadMessage = function()
{
	if (this.element){
		dhtml.deleteAllChildren(this.element);
		document.body.style.cursor = "default";
	}
}

/**
 * Get/Set cursor position, default is to ignore
 */
 
View.prototype.setCursorPosition = function(id)
{
}

View.prototype.getCursorPosition = function()
{
	return;
}

/*
 * Modify the data set in the constructor
 */
View.prototype.setData = function(data)
{
}

View.prototype.getRowNumber = function(elemid)
{
	return;
}

View.prototype.getElemIdByRowNumber = function(rownum)
{
	return;
}

View.prototype.getRowCount = function()
{
	return;
}

