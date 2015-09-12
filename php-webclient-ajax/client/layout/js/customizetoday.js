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
 * function which initialize the dialogbox
 * @param array taskfolder list of all task folder
 */
function init(todaymoduleobject)
{
	if(!todaymoduleobject)
		todaymoduleobject = parentWebclient.getModulesByName("todaymodule")[0];
	else
		todaymoduleobject = todaymoduleobject.modules["todaytasklistmodule"];
	
	var taskEntryID = todaymoduleobject.entryid;	

	showTask();
	
	var day = dhtml.getElementById("select_days");
	var selected_day = webclient.settings.get("today/calendar/numberofdaysloaded", "7");		
	day.selectedIndex = selected_day - 1;
	
	var task = dhtml.getElementById("select_tasks");
	var selected_task = webclient.settings.get("today/task/taskfolder", taskEntryID);
	task.selectedIndex = selected_task;
	
	var styles = dhtml.getElementById("select_styles");
	var selected_style = webclient.settings.get("today/style/styletype", "1");		
	styles.selectedIndex = selected_style - 1;
}

/**
 * get day value which is selected in dialog box
 */
function onChangeDays()
{
	this.select_days = dhtml.getElementById("select_days");
}

/**
 * assign all task folder in the combo box
 */
function showTask()
{
	var taskfolders = getAllTaskFolders();
	var folderList = dhtml.getElementById("select_tasks");
	
	for(var i = 0; i < taskfolders.length; i++) {
		var folder = taskfolders[i];

		var entryid = folder["entryid"];
		var display_name = folder["display_name"];
				
		var newOption = dhtml.addElement(folderList, "option", null, null, display_name);
		newOption.id = entryid;
		newOption.value = i;
	}
}

function getAllTaskFolders(){
	var folders = new Array();
	var allFolders = parentWebclient.hierarchy.defaultstore.folders;
	for(var i = 0; i < allFolders.length; i++)
	{
		var folder = allFolders[i];
		var container_class = folder["container_class"];
			
		if(container_class == "IPF.Task") {
			folders.push(folder);
		}
	}
	return folders;
}

/**
 * get which task is selected in dialog box
 */
function onChangeTasks()
{
	var select_tasks = dhtml.getElementById("select_tasks");
	this.selectedOption = select_tasks.options[select_tasks.value];
}

/**
 * get which style is selected in dialog box.
 */
function onChangeStyles() 
{
	this.select_styles = dhtml.getElementById("select_styles");
}

/**
 * fuction will set all value in the settings
 */
function customizeTodaySubmit()
{
	var reloadNeeded = false;
	
	if(this.select_days) {
		parentWebclient.settings.set("today/calendar/numberofdaysloaded", this.select_days.value);		
		reloadNeeded = true;
	}
	
	if(this.selectedOption) {
		parentWebclient.settings.set("today/task/taskfolderid", this.selectedOption.id);		
		parentWebclient.settings.set("today/task/taskfolder", this.selectedOption.value);		
		reloadNeeded = true;
	}

	if(this.select_styles) {
		parentWebclient.settings.set("today/style/styletype", this.select_styles.value);		
		reloadNeeded = true;
	}
	
	if (reloadNeeded){
		parentWebclient.hierarchy.selectLastFolder(true); 
	}
}

