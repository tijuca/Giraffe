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
 * initDelegates
 */
function initDelegates()
{
	// Setup Delegate list using a TableWidget.
	delegatesTable = new TableWidget(dhtml.getElementById("delegateslist_container"), true);
	delegatesTable.addColumn("delegate_name", _("Name"), false, 1);
	delegatesTable.addRowListener(selectDelegate, "select");
	delegatesTable.addRowListener(deselectDelegate, "deselect");
}

/**
 * populateDelegateList
 * 
 * Filling the lists and tables that contain the delegates list with the latest 
 * data from the global Data Object. The old data is removed and then the new 
 * data is added.
 * @param array delegates  -delegates information
 */
function populateDelegateList(delegates)
{
	// Setting up the data for the tableWidget
	var tableData = new Array();
	for(var i in delegates){
		tableData.push({
			rowID: i,	// Defined as userEntryID.
			data: delegates[i],
			delegate_name: {innerHTML: delegates[i]["fullname"]}
		});
	}

	// Generate delegate list.
	delegatesTable.generateTable(tableData);
}
/**
 * selectDelegate
 * 
 * Called when user selects a row in the delegate list.
 * @param tblWidget object reference to tablewidget
 * @param type string type of action (select)
 * @param selected array list of rowIDs of selected rows
 * @param select list of rowIDs of newly selected rows
 */
function selectDelegate(tblWidget, type, selected, select)
{
	module.selectedDelegates = new Array();
	for (var i in selected){
		module.selectedDelegates.push(i);
	}
}
/**
 * deselectDelegate
 * 
 * Called when user deselects a row in the Delegate table.
 * @param tblWidget object reference to tablewidget
 * @param type string  type of action (deselect)
 * @param selected array list of rowIDs of selected rows
 * @param deselect array list of deselected rowIDs 
 */
function deselectDelegate(tblWidget, type, selected, deselect)
{
}
/**
 * delegatesFromABCallBack
 *
 * Function which called from addressbook after a user is selected.
 * @param array userdata array of selected users.
 * @param object module module object.
 */
function delegatesFromABCallBack(userdata)
{
	newDelegatePermissions(userdata);
}
/**
 * newDelegatePermissions
 *
 * Function which opens permissions for selected user.
 * @param array result array of selected users.
 * @param object callbackData module object.
 */
function newDelegatePermissions(result)
{
	var newDelegate = new Object();
	var delegate = new Object();
	
	if(result){
		
		var windowData = new Array();
		// Check if selected user is existing delegate.
		if (typeof module.delegateProps[result["entryid"]] == "undefined"){
			newDelegate["entryid"] = result["entryid"];
			newDelegate["fullname"] = result["display_name"];
			newDelegate["see_private"] = "0";
			newDelegate["delegate_meeting_rule"] = "0";
			newDelegate["permissions"] = ({"calendar":"0", 
											"contacts":"0", 
											"inbox":"0", 
											"journal":"0", 
											"notes":"0", 
											"tasks":"0"
										 });
			windowData["newDelegate"] = true;
		} else {
			newDelegate = module.delegateProps[result["entryid"]];
			windowData["newDelegate"] = false;
		}
		windowData["delegate"] = new Array(newDelegate);
		webclient.openModalDialog(module, 'delegatespermission', DIALOG_URL+'task=delegatespermission_modal', 430, 375, setPermissionsCallBack, false, windowData);
	}
}
/**
 * submitDelegates
 *
 * Function which submits all delegates to save them.
 */
function submitDelegates()
{
	var delegates = new Array();
	if (module) {
		for (var i in module.delegateProps) {
			delegates.push(module.delegateProps[i]);
		}
		module.save(delegates);
	}
}
/**
 * setPermissionsCallBack
 *
 * Callback function for permissions dialog. Called
 * when user is finished with selecting profiles and 
 * clicks 'ok'.
 * @param Object delegates list of delegates whoes setting are changed.
 * @param boolean newDelegate true if new delegate, false if existing delegate.
 */
function setPermissionsCallBack(delegates, newDelegate)
{
	if (module) {
		if (newDelegate) {
			module.delegateProps[delegates[0]["entryid"]] = new Object();
			module.delegateProps[delegates[0]["entryid"]] = delegates[0];
		} else {
			for (var i = 0; i < delegates.length; i++) {
				if (typeof module.delegateProps[delegates[i]["entryid"]] == "undefined") {
					module.delegateProps[delegates[i]["entryid"]] = new Object();
				}
				module.delegateProps[delegates[i]["entryid"]] = delegates[i];
			}
		}
		populateDelegateList(module.delegateProps);
	}
}