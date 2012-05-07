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
 * This class can be used to output a tabbar control
 *
 * HOWTO BUILD:
 * - see tabbar.class.php
 * - tabbarControl = tabbar; //tabbarControl will be used in example 
 * HOWTO USE:
 * - see tabbar.class.php
 * - tabbarControl.addExternalHandler(handler); //handler will be executed on tab switch
 * - tabbarControl.delExternalHandler(); //will remove the current handler
 * - tabbarControl.getSelectedTab(); //will return name the selected tab 
 * DEPENDS ON:
 * |------> tabbar.php
 * |------> tabbar.css
 * |------> dhtml.js 
 */

/**
 * Constructor
 *
 * @param string tabbar_id The HTML id for the tabbar (set in PHP)
 * @param array tabpages   All tabpages are specified here
 */
function TabBar(tabbar_id, tabpages)
{
	this.element = dhtml.getElementById(tabbar_id);
	this.pages = tabpages;
	
	// add event handlers
	for(var tab_id in this.pages){
		var tabbutton = dhtml.getElementById('tab_'+tab_id);
		dhtml.addEvent(this, tabbutton, "click", eventTabBarClick);
		dhtml.addEvent(this, tabbutton, "mouseover", eventTabBarMouseOver);
		dhtml.addEvent(this, tabbutton, "mouseout", eventTabBarMouseOut);

		if (tabbutton.className == "selectedtab"){
			this.selected_tab = tab_id;
		}
	}
}

/**
 * Function to add a handler which will be called when the tabbar changes view, the handler will be called with
 * a string argument, witch contains the new active tabname and the old tabname
 */
TabBar.prototype.addExternalHandler = function(handler)
{
	this.handler = handler;
}

TabBar.prototype.delExternalHandler = function()
{
	this.handler = null;
}

/**
 * Function to change the page
 *
 * @param string newPage The name of the page we want to change to.
 */
TabBar.prototype.change = function(newPage)
{
	if (this.pages[newPage]){
		var old_tab = this.selected_tab;
		this.selected_tab = newPage;
		for(tab_id in this.pages){
			if (tab_id == this.selected_tab){
				dhtml.getElementById('tab_'+tab_id).className = "selectedtab";
				dhtml.getElementById(tab_id+'_tab').className = "tabpage selectedtabpage";
			}else{
				dhtml.removeClassName(dhtml.getElementById('tab_'+tab_id), "selectedtab");
				dhtml.getElementById(tab_id+'_tab').className = "tabpage";
			}
		}

		if (this.handler && this.handler != null){
			this.handler(this.selected_tab, old_tab);
		}
	}
	
	// Resize the body.
	// The layout brakes when body is resized when it is not visible. So
	// every time a tab is clicked the body will be resized. To make sure
	// the layout won't brake.
	resizeBody();
}

/**
 * Function to get the current selected tab
 */
TabBar.prototype.getSelectedTab = function()
{
	return this.selected_tab;
}

// Event handlers

function eventTabBarClick(tabBarObject, element, event)
{
	var buttonName = element.id.substring(4);
	tabBarObject.change(buttonName);
	return false;
}

function eventTabBarMouseOver(tabBarObject, element, event)
{
	if (element.className.indexOf("hover") == -1){
		element.className += " hover";
	}
}

function eventTabBarMouseOut(tabBarObject, element, event)
{
	if (element.className.indexOf("hover") != -1){
		element.className = element.className.substring(0,element.className.indexOf("hover")-1)+element.className.substring(element.className.indexOf("hover")+5, element.className.length);
	}
}
