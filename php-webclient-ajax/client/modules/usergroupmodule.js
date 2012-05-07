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

usergroupmodule.prototype = new Module;
usergroupmodule.prototype.constructor = usergroupmodule;
usergroupmodule.superclass = Module.prototype;

function usergroupmodule(id, element, title, data)
{
	if(arguments.length > 0) {
		this.init(id, element, title, data);
	}
	
}

usergroupmodule.prototype.init = function(id, element, title, data)
{
	usergroupmodule.superclass.init.call(this, id, element, title, data);
}

usergroupmodule.prototype.getGroups = function(){
	webclient.xmlrequest.addData(this, "getgroups", {});
	webclient.xmlrequest.sendRequest();
}

usergroupmodule.prototype.getUsers = function(groupEntryID){
	webclient.xmlrequest.addData(this, "getgroupusers", {group_entry_id: groupEntryID});
	webclient.xmlrequest.sendRequest();
}

/**
 * Function which execute an action. This function is called by the XMLRequest object.
 * @param string type the action type
 * @param object action the action tag 
 */ 
usergroupmodule.prototype.execute = function(type, action)
{
	switch(type)
	{
		case "groups":
			this.listGroups(action);
			break;
		case "users":
			this.listUsers(action);
			break;
	}
}


usergroupmodule.prototype.listGroups = function(action)
{
	var select = document.getElementById("usergroup_dialog_usergroup_list");
	while(select.options.length > 0){
		select.remove(0);
	}

	dhtml.addEvent(this, select, "change", eventUserGroupModuleDialogOnChange);
	var items = action.getElementsByTagName("item");
	for(var i=0;i<items.length;i++){
		var item = items[i];
		var data = {
			group_entry_id: dhtml.getXMLValue(item, "entryid"),
			display_name: dhtml.getXMLValue(item, "subject")
		}
		select.options[select.options.length] = new Option(data.display_name, data.group_entry_id);
	}
}

usergroupmodule.prototype.listUsers = function(action)
{
	var select = document.getElementById("usergroup_dialog_usergroup_userlist", "select");
	while(select.options.length > 0){
		select.remove(0);
	}

	var items = action.getElementsByTagName("item");
	for(var i=0;i<items.length;i++){
		var item = items[i];
		var data = {
			userentryid: dhtml.getXMLValue(item, "userentryid"),
			display_name: dhtml.getXMLValue(item, "display_name"),
			emailaddress: dhtml.getXMLValue(item, "emailaddress"),
			username: dhtml.getXMLValue(item, "username"),
			access: dhtml.getXMLValue(item, "access"),
			storeid: dhtml.getXMLValue(item, "storeid"),
			calentryid: dhtml.getXMLValue(item, "calentryid")
		}
		var opt = new Option(data.display_name, data.display_name);
		opt.data = data;
		select.options[select.options.length] = opt;
		dhtml.addEvent(this, select.options[(select.options.length-1)], "click", eventUserGroupUserModuleDialogOnClick);
	}
}

function eventUserGroupModuleDialogOnChange(moduleObj, element, event){
	var select = document.getElementById("usergroup_dialog_usergroup_userlist", "select");
	while(select.options.length > 0){
		select.remove(0);
	}
	moduleObj.getUsers(element.value);
}

function eventUserGroupUserModuleDialogOnClick(moduleObj, element, event){
	
}
