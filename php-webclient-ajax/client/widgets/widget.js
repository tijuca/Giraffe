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
* Widget base class
* this class has no function, but it provides all widgets an unique id
* which can be used for creating HTML elements and it also provides
* a destructor
*/

Widget.prototype.constructor = Widget;

function Widget()
{
	this.init(this);
}

Widget.prototype.init = function(widgetObject)
{
	widgetObject.widgetID = widgetCounter++;
}

Widget.prototype.destructor = function(widgetObject)
{
}

/**
 * Add an event handler for internal event type 'eventname'. If 'object' is not null, the method
 * will be called in the context of that object. The parameters passed to the method are down
 * to the event source
 *
 * @param eventname name of the event to handle (eg 'openitem')
 * @param method method to call when event is triggered
 * @param object object if the method is to be called in an object's context
 */
Widget.prototype.addEventHandler = function(eventname, method, object)
{
	if(!this.internalEvents) 
		this.internalEvents = new Array();
		
	if(!this.internalEvents[eventname])
		this.internalEvents[eventname] = new Array();
		
	handlerinfo = new Object();
	handlerinfo.object = object;
	handlerinfo.method = method;
	
	this.internalEvents[eventname].push(handlerinfo);
}

/**
 * Send an event to all listeners
 *
 * @param eventname
 * @param paramN all other parameters are sent to the event handler.
 */
Widget.prototype.sendEvent = function()
{
	var args = new Array;
	
	// Convert 'arguments' into a real Array() so we can do shift()
	for(var i=0; i< arguments.length;i++) {
		args.push(arguments[i]);
	}

	var eventname = args.shift();

	if(!this.internalEvents)
		return true;
		
	if(!this.internalEvents[eventname])
		return true;
		
	for(var i=0; i< this.internalEvents[eventname].length; i++) {
		var object =  this.internalEvents[eventname][i].object || window;
		this.internalEvents[eventname][i].method.apply(object, args);
	}
}

// global widget counter
var widgetCounter = 0;

