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
 * --Combo Box Widget--  
 * @type	Widget
 * @classDescription	 This widget can be used to make a combo box
 * 
 * HOWTO BUILD:
 * HOWTO USE:
 * HOWTO REACT:
 */
ComboBox.prototype = new Widget;
ComboBox.prototype.constructor = ComboBox;
ComboBox.superclass = Widget.prototype;

/**
 * @constructor This widget can be used to make a combo box
 * @param {Object} id
 * @param {Object} callbackfunction
 * @param {Object} moduleID
 */
function ComboBox(id, callbackfunction, moduleID)
{
	this.id = id;
	this.callbackfunction = callbackfunction;
	this.moduleID = moduleID;
}

/**
 * @param {Object} contentElement
 * @param {Object} options
 * @param {Object} optionSelected
 * @param {Int} width
 */
ComboBox.prototype.createComboBox = function(contentElement, options, optionSelected, width)
{
	var combobox = dhtml.addElement(contentElement, "div", "combobox");

	if (width){
		combobox.style.width = width + "px";
	} else {
		combobox.style.width = width + "100px";
	}
	
	var selected = dhtml.addElement(combobox, "div", "selected", "selected_" + this.id, optionSelected);
	dhtml.addEvent(-1, selected, "click", eventComboBoxClickArrow);
	
	var arrow = dhtml.addElement(combobox, "div", "arrow icon_arrow", "arrow_" + this.id);
	dhtml.addEvent(-1, arrow, "mouseover", eventComboBoxMouseOverArrow);
	dhtml.addEvent(-1, arrow, "mouseout", eventComboBoxMouseOutArrow);
	dhtml.addEvent(-1, arrow, "click", eventComboBoxClickArrow);
	
	var optionsElement = dhtml.addElement(document.body, "div", "combobox_options", "options_" + this.id);
	
	for(var i in options)
	{
		var option = options[i];
		var optionElement = dhtml.addElement(optionsElement, "div", "option value_" + option["id"], false, option["value"]);

		dhtml.addEvent(-1, optionElement, "mouseover", eventComboBoxMouseOverOption);
		dhtml.addEvent(-1, optionElement, "mouseout", eventComboBoxMouseOutOption);
		dhtml.addEvent(-1, optionElement, "click", eventComboBoxClickOption);
		dhtml.addEvent(this.moduleID, optionElement, "click", this.callbackfunction);
	}

	this.combobox = combobox;
}

ComboBox.prototype.destructor = function()
{
	dhtml.deleteAllChildren(dhtml.getElementById("options_"+this.id));
	dhtml.deleteElement(dhtml.getElementById("options_"+this.id));
	
	dhtml.deleteAllChildren(this.combobox);
	dhtml.deleteElement(this.combobox);
	ComboBox.superclass.destructor(this);
}

/**
 * @param {Object} moduleObject
 * @param {HtmlElement} element
 * @param {Object} event
 */
function eventComboBoxMouseOverArrow(moduleObject, element, event)
{
	element.className += " arrowover";
}

/**
 * @param {Object} moduleObject
 * @param {HtmlElement} element
 * @param {Object} event
 */
function eventComboBoxMouseOutArrow(moduleObject, element, event)
{
	if(element.className.indexOf("arrowover") > 0) {
		element.className = element.className.substring(0, element.className.indexOf("arrowover"));
	}
}

/**
 * @param {Object} moduleObject
 * @param {HtmlElement} element
 * @param {Object} event
 */
function eventComboBoxClickArrow(moduleObject, element, event)
{
	var options = dhtml.getElementById("options_"+element.id.substring(element.id.indexOf("_") + 1));
	
	if(options) {
		if(options.style.display == "none" || options.style.display == "") {
			var combobox = element.parentNode;
			options.style.width = (combobox.clientWidth - 2) + "px";
			options.style.top = (dhtml.getElementTop(combobox) + combobox.clientHeight) + "px";
			options.style.left = (dhtml.getElementLeft(combobox) + (document.all?1:2)) + "px";
			
			options.style.display = "block";
		} else {
			options.style.display = "none";
		}
	}
}

/**
 * @param {Object} moduleObject
 * @param {HtmlElement} element
 * @param {Object} event
 */
function eventComboBoxMouseOverOption(moduleObject, element, event)
{
	element.className += " optionover";
}

/**
 * @param {Object} moduleObject
 * @param {HtmlElement} element
 * @param {Object} event
 */
function eventComboBoxMouseOutOption(moduleObject, element, event)
{
	if(element.className.indexOf("optionover") > 0) {
		element.className = element.className.substring(0, element.className.indexOf("optionover"));
	}
}

/**
 * @param {Object} moduleObject
 * @param {HtmlElement} element
 * @param {Object} event
 */
function eventComboBoxClickOption(moduleObject, element, event)
{
	var selected = dhtml.getElementById("selected_" + element.parentNode.id);
	
	if(selected) {
		dhtml.deleteAllChildren(selected);
		dhtml.addTextNode(selected, element.firstChild.nodeValue);
	}	
	
	element.parentNode.style.display = "none";
}
