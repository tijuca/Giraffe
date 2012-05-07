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

function initFolderProperties(module)
{
	var elements = new Object();
	elements["profile"] = dhtml.getElementById("profile");
	elements["ecRightsCreate"] = dhtml.getElementById("ecRightsCreate");
	elements["ecRightsReadAny"] = dhtml.getElementById("ecRightsReadAny");
	elements["ecRightsCreateSubfolder"] = dhtml.getElementById("ecRightsCreateSubfolder");
	elements["ecRightsFolderAccess"] = dhtml.getElementById("ecRightsFolderAccess");
	elements["ecRightsFolderVisible"] = dhtml.getElementById("ecRightsFolderVisible");

	elements["edit_items"] = document.getElementsByName("edit_items");	
	for(var i=0;i<elements["edit_items"].length;i++){
		elements[elements["edit_items"][i].id] = elements["edit_items"][i];
	}

	elements["del_items"] = document.getElementsByName("del_items");	
	for(var i=0;i<elements["del_items"].length;i++){
		elements[elements["del_items"][i].id] = elements["del_items"][i];
	}

	elements["userlist"] = dhtml.getElementById("userlist");
	dhtml.addEvent(module.id, elements["userlist"], "change", eventPermissionsUserlistChange);

	for(var title in ecRightsTemplate){
		var option = dhtml.addElement(null, "option");
		option.text = title;
		option.value = ecRightsTemplate[title];
		elements["profile"].options[elements["profile"].length] = option;
	}
	var option = dhtml.addElement(null, "option");
	option.text = _("Other");
	option.value = -1;
	elements["profile"].options[elements["profile"].length] = option;
	dhtml.addEvent(module.id, elements["profile"], "change", eventPermissionsProfileChange);


	for(var name in elements){
		if (name.substr(0, 8) == "ecRights"){
			dhtml.addEvent(module.id, elements[name], "click", eventPermissionChange);
		}
	}

	module.permissionElements = elements;

	dhtml.addEvent(module.id, dhtml.getElementById("add_user"), "click", eventPermissionAddUser);
	dhtml.addEvent(module.id, dhtml.getElementById("del_user"), "click", eventPermissionDeleteUser);
	dhtml.addEvent(module.id, dhtml.getElementById("username"), "change", eventPermissionUserElementChanged);
}



/**
 * Function will set all varables in the properties dialog
 */
function setFolderProperties(properties)
{
	dhtml.getElementById("display_name").firstChild.nodeValue = properties["display_name"];

	// set container class
	var container_class = "";
	switch (properties["container_class"]){
		case "IPF.Note":
			container_class = _("Mail and Post");
			break;
		case "IPF.Appointment":
			container_class = _("Calendar");
			break;
		case "IPF.Contact":
			container_class = _("Contact");
			break;
		case "IPF.Journal":
			container_class = _("Journal");
			break;
		case "IPF.StickyNote":
			container_class = _("Note");
			break;
		case "IPF.Task":
			container_class = _("Task");
			break;
		default:
			container_class = properties["container_class"];
	}
	dhtml.getElementById("container_class").firstChild.nodeValue = _("Folder containing")+" "+container_class+" "+_("Items");
	
	// set folder icon
	var folder_icon = properties["container_class"].substr(4).replace(/\./,"_").toLowerCase();
	dhtml.getElementById("folder_icon").className = "folder_big_icon folder_big_icon_folder folder_big_icon_"+folder_icon;

	dhtml.getElementById("parent_display_name").firstChild.nodeValue = properties["parent_display_name"];
	dhtml.getElementById("comment").value = properties["comment"];
	dhtml.getElementById("content_count").firstChild.nodeValue = properties["content_count"];
	dhtml.getElementById("content_unread").firstChild.nodeValue = properties["content_unread"];
	dhtml.getElementById("message_size").firstChild.nodeValue = properties["message_size"];
}

function submitProperties()
{
	if (module){
		var props = new Object();
		props["comment"] = dhtml.getElementById("comment").value;
		
		//Update permissions if changed
		if (module.permissionChanged == true) {
			props["permissions"] = module.getPermissionData();
		}
		module.save(props);
	} else {
		window.close();
	}
}
