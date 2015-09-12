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
* Folder properties module
*/

foldersizemodule.prototype = new Module;
foldersizemodule.prototype.constructor = foldersizemodule;
foldersizemodule.superclass = Module.prototype;

function foldersizemodule(id, element)
{
	if(arguments.length > 0) {
		this.init(id, element);
	}
}

foldersizemodule.prototype.init = function(id, element, title, data)
{
	if(data) {
		for(var property in data)
		{
			this[property] = data[property];
		}
	}
	
	foldersizemodule.superclass.init.call(this, id, element, title, data);
}

foldersizemodule.prototype.execute = function(type, action)
{
	switch(type)
	{
		case "foldersize":
			this.setFolderSizeData(action);
			break;
	}
}

/**
* make a XML request for folder size data
* data must be an array with the store and folder entryid
*/
foldersizemodule.prototype.getFolderSize = function()
{
	var data = {
		"store": this.store,
		"entryid": this.entryid
	};
	webclient.xmlrequest.addData(this, "foldersize", data);
	webclient.xmlrequest.sendRequest();
}

foldersizemodule.prototype.setFolderSizeData = function(action){
	var mainfolder = action.getElementsByTagName("mainfolder");
	// Make sure to only get folder-items belong the subfolders-item
	var subfolderlist = action.getElementsByTagName("subfolders");
	subfolderlist = action.getElementsByTagName("folder");

	// Get the properies for the selected folder
	dialogData = {
		'name': dhtml.getXMLValue(mainfolder[0], "name").htmlEntities(),
		'size': parseInt(dhtml.getXMLValue(mainfolder[0], "size"),10) + _("KB"),
		'totalsize': parseInt(dhtml.getXMLValue(mainfolder[0], "totalsize"),10) + _("KB"),
		'subfolders': new Array()
	
	};
	// Build data for subfolder list
	for(var i=0;i<subfolderlist.length;i++){
		dialogData['subfolders'].push({
			name: {
				innerHTML: dhtml.getXMLValue(subfolderlist[i], "name").htmlEntities()
			},
			size: {
				innerHTML: parseInt(dhtml.getXMLValue(subfolderlist[i], "size"),10) + _("KB")
			},
			totalsize: {
				innerHTML: parseInt(dhtml.getXMLValue(subfolderlist[i], "totalsize"),10) + _("KB")
			}
		});
		
	}
	this.size_data = dialogData;
	parseFolderSizeData(dialogData);
}
