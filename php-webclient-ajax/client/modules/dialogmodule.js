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
* Dialog Module
* 
* This module is available in every dialog module and contains
* some general functions (requests) that can be used when there is 
* no other module available to do this.
*/

dialogmodule.prototype = new Module;
dialogmodule.prototype.constructor = dialogmodule;
dialogmodule.superclass = Module.prototype;

function dialogmodule(id)
{
	if(arguments.length > 0) {
		this.init(id);
	}
}

dialogmodule.prototype.init = function(id)
{
	this.callbackCounter = 0;
	this.callbackFunctions = new Object();

	dialogmodule.superclass.init.call(this, id);
}

dialogmodule.prototype.execute = function(type, action)
{
	var data = dom2array(action)
	if (data["callbackid"]){
		var callback = this.getCallback(data["callbackid"]);
		callback.call(callback, data);
	}
}

/**
* addCallback - this functions stores the callback function for future use
*               it returns the callbackid which should be send in the request
*/
dialogmodule.prototype.addCallback = function(callback)
{
	this.callbackFunctions[++this.callbackCounter] = callback;
	return this.callbackCounter;
}

/**
* getCallback - returns the callback function for the given callbackid, 
				please note that the reference to the callback function
				is removed after calling this function
*/
dialogmodule.prototype.getCallback = function(callbackid)
{
	var callback = this.callbackFunctions[callbackid];
	delete this.callbackFunctions[callbackid];
	return callback;
}

/**
* loadABHierachy - requests a list of addressbook folders
*
*
*/
dialogmodule.prototype.loadABHierarchy = function(hierarchy, callback)
{
	var data = new Object();
	data["callbackid"] = this.addCallback(callback);
	data["gab"] = "all";
	data["contacts"] = {stores:{store:new Array(),folder:new Array()}}
	for(var i=0;i<hierarchy.stores.length;i++){
		var store = hierarchy.stores[i];
		switch(store.foldertype){
			case "contact":
				data["contacts"]["stores"]["folder"].push(store.id);
				break;
			case "all":
				data["contacts"]["stores"]["store"].push(store.id);
				break;
		}
	}

	webclient.xmlrequest.addData(this, "abhierarchy",data);
	webclient.xmlrequest.sendRequest();
}
