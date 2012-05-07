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
* Settings Module - A global module who takes care of all the settings within the webaccess
*/

settingsmodule.prototype = new Module;
settingsmodule.prototype.constructor = settingsmodule;
settingsmodule.superclass = Module.prototype;

function settingsmodule(id)
{
	if(arguments.length > 0) {
		this.init(id);
	}
}

settingsmodule.prototype.init = function(id)
{
	settingsmodule.superclass.init.call(this, id);
}

settingsmodule.prototype.execute = function(type, action)
{
	if (type == "retrieveAll"){
		this.handleData(action);
	}
	if (type == "convert"){
		this.convertCallback(action);
	}
}

settingsmodule.prototype.handleData = function(action)
{
	this.settings = dom2array(action);
}

settingsmodule.prototype.retrieveAll = function()
{
	webclient.xmlrequest.addData(this, "retrieveAll");
	webclient.xmlrequest.sendRequest(); // don't wait, send request
}

settingsmodule.prototype.get = function(path, default_value)
{	
	path = path.split("/");
	var tmp = this.settings;
	for(var i=0; i<path.length; i++){
		var pointer = path[i];
		// Check whether value is not set to zero and if its empty then return default_value
		if (!tmp[pointer] && (tmp[pointer] != 0 || tmp[pointer] === "")) {
			if (typeof default_value == "undefined") {
				return null;
			}
			return default_value;
		}
		tmp = tmp[pointer];
	}
	return tmp;
}

settingsmodule.prototype.setArray = function(path, data)
{
	if (typeof data == 'object' || typeof data == 'array'){
		for(var key in data){
			this.setArray(path+"/"+key, data[key]);
		}
	}else {
		this.set(path, data);
	}
}

settingsmodule.prototype.set = function(path, value)
{
	// check if 'value' is an object/array
	if (typeof value == 'object' || typeof value == 'array'){
		for(var key in value){
			this.set(path+"/"+key, value[key]);
		}
	}else {
		// check if this setting really has changed
		if (this.get(path) != value){
		
				// save new value to server
				var data = new Object();
				data["path"] = path;
				data["value"] = value;
				webclient.xmlrequest.addData(this, "set", data);
		
				// set new value in this.settings (using eval)
				path = path.split("/");
				var tmp = "this.settings";
				for(var i=0; i<path.length; i++){
					tmp +='["'+path[i]+'"]';
					eval("if (typeof("+tmp+")=='undefined' || !"+tmp+") "+tmp+" = new Object();");
				}
				tmp += ' = "'+escapeJavascript(value)+'";';
				eval(tmp);
		}
	}
}

settingsmodule.prototype.deleteSetting = function(path)
{
	// only delete if exists
	if (this.get(path) != null){

		// send delete request
		var data = new Object();
		data["path"] = path;
		webclient.xmlrequest.addData(this, "delete", data);
	
		// delete local setting
		path = path.split("/");
		var tmp = "delete this.settings";
		for(var i=0; i<path.length; i++){
			tmp += "[\""+path[i]+"\"]";
		}
		eval(tmp);
	}
}

settingsmodule.prototype.save = function()
{	
	webclient.xmlrequest.sendRequest();
}

// convert HTML to plain text
settingsmodule.prototype.convert = function(htmlData, callback)
{
	if (!this.callback_functions){
		this.callback_counter = 0;
		this.callback_functions = new Object();
	}
	this.callback_counter++;
	this.callback_functions[this.callback_counter] = callback;
	
	var data = new Object();
	data["html"] = htmlData;
	data["callback"] = this.callback_counter;
	webclient.xmlrequest.addData(this, "convert", data);
	webclient.xmlrequest.sendRequest(); // don't wait, send request
}

settingsmodule.prototype.convertCallback = function(action)
{
	var text = action.getElementsByTagName("text")[0].firstChild.nodeValue;
	var callback = action.getElementsByTagName("callback")[0].firstChild.nodeValue;
	if (this.callback_functions[callback]){
		this.callback_functions[callback](text);
		delete this.callback_functions[callback];
	}
}
