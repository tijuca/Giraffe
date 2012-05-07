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
* InfoBox widget - shows a message in the webclient
*/

InfoBox.prototype = new Widget;
InfoBox.prototype.constructor = InfoBox;
InfoBox.superclass = Widget.prototype;

function InfoBox(message, timeout, className)
{
	this.init(message, timeout, className);
}

InfoBox.prototype.init = function(message, timeout, className)
{
	InfoBox.superclass.init(this);
	

	this.message = "";
	this.timeout = 10000; // timeout in msec
	this.timer = null;
	this.className = "infobox";

	this.element = dhtml.addElement(document.body, "div", this.className, "infobox_"+this.widgetID);
	this.element.widgetObject = this;
	dhtml.addEvent(-1, this.element, "click", eventInfoBoxClick);

	if (arguments.length > 0 && message != null){
		this.show(message, timeout, className);
	}
}

InfoBox.prototype.destructor = function(widgetObject)
{
	// stop timer
	window.clearTimeout(this.timer);

	// if visible hide the infobox
	this.hide();

	// remove element
	dhtml.deleteAllChildren(this.element);
	dhtml.removeElement(this.element);

	InfoBox.superclass.destructor(this);
}

InfoBox.prototype.show = function(message, timeout, className)
{
	if (typeof message != "undefined"){
		this.message = message;
	}
	if (typeof timeout != "undefined"){
		this.timeout = timeout;
	}
	if (typeof className != "undefined"){
		this.className = "infobox "+className;
	}

	if (this.element){
		this.element.className = this.className;
		dhtml.deleteAllChildren(this.element);

		this.element.textBox = dhtml.addElement(this.element, "span", false, false);
		this.element.textBox.innerHTML = this.message;

		this.visible = true;		
		this.element.style.display = "block";
		
		if (this.timeout>0){
			// hiding of the info box is initial called from a static function
			this.timer = window.setTimeout('eventInfoBoxTimeout("'+this.element.id+'");', this.timeout);
		}
	}
}

InfoBox.prototype.hide = function()
{
	this.element.style.display = "none";
	this.visible = false;
	// make sure the timer is stopped (in case we call this function ourself)
	window.clearTimeout(this.timer);
}

InfoBox.prototype.click = function(event)
{
	this.hide();
}

function eventInfoBoxTimeout(infoBoxID)
{
	var element = dhtml.getElementById(infoBoxID);
	if (element && element.widgetObject){
		element.widgetObject.hide();
	}
}

function eventInfoBoxClick(moduleObject, element, event)
{
	if (typeof element.widgetObject != "undefined" && element.widgetObject instanceof InfoBox){
		element.widgetObject.click(event);
	}
}
