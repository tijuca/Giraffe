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
* Notification Module
*/

notificationmodule.prototype = new Module;
notificationmodule.prototype.constructor = notificationmodule;
notificationmodule.superclass = Module.prototype;

function notificationmodule(id)
{
	if(arguments.length > 0) {
		this.init(id);
	}
	this.quotaBar = new QuotaWidget(dhtml.getElementById("quota_footer"),_("Quota"));
}

notificationmodule.prototype.init = function(id)
{
	notificationmodule.superclass.init.call(this, id);

	if (webclient.xmlrequest) {
		var data = new Object();
		webclient.xmlrequest.addData(this, "checkmail", data);
	}
	
	this.infobox = new InfoBox(null, 10000, "notification");
}


notificationmodule.prototype.execute = function(type, action)
{
	if (type == "newmail"){
		var count = parseInt(action.getAttribute("content_count"),10);
		var unread = parseInt(action.getAttribute("content_unread"),10);
		this.showNotification(count,unread);
		
		var mailmodules = webclient.getModulesByName("maillistmodule");
		if (mailmodules.length>0){
			var entryids = action.getElementsByTagName("parent_entryid");
			
			for(var i=0;i<mailmodules.length;i++){
				if (mailmodules[i].rowstart == 0){ // only when the maillist module is on the first page
					var folder_entryid = mailmodules[i].entryid;
					var reload_needed = false;

					for(var j=0;j<entryids.length;j++){ // loop through all new messages
						var entryid = dhtml.getTextNode(entryids[j],"");
						if (entryid == folder_entryid){
							reload_needed = true;
						}
					}
					
					// because we don't know where to put the new message we need to reload the maillistmodule here
					if (reload_needed){
						// reload the maillistmodule.
						mailmodules[i].list(false, false, true);
					}
				}
			}
		}
	}

	if (type == "update"){
		//mail notification
		var count = dhtml.getTextNode(action.getElementsByTagName("count")[0],"");
		var unread = dhtml.getTextNode(action.getElementsByTagName("unread")[0],"");
		
		//quota display
		var store_size = parseInt(dhtml.getTextNode(action.getElementsByTagName("store_size")[0], -1),10);
		if (store_size != -1){
			var quota_warning = parseInt(dhtml.getTextNode(action.getElementsByTagName("quota_warning")[0]),10);
			var quota_soft = parseInt(dhtml.getTextNode(action.getElementsByTagName("quota_soft")[0]),10);
			var quota_hard = parseInt(dhtml.getTextNode(action.getElementsByTagName("quota_hard")[0]),10);
			this.quotaBar.update(store_size,quota_warning,quota_soft,quota_hard);
		}
	}
}

notificationmodule.prototype.showNotification = function(count, unread)
{
	var msg = _("You have a new message in your inbox");
	if (unread>1){
		msg = _("There are %s unread messages in your inbox").sprintf(unread);
	}

	this.infobox.show(msg);
}


