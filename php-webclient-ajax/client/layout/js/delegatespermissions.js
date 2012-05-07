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
 * initDelegatePermissions
 * 
 * initializes permissions dialog for delegates.
 */
function initDelegatePermissions(user)
{
	delegate = user[0];
	// Add delegate's name to window title.
	if (editDelegates.length > 1){
		window.document.title = _("Delegate Permissions: ") + _("Multiple Delegates");
	} else {
		window.document.title = _("Delegate Permissions: ") + delegate["fullname"];
	}
	// Profile template
	var profileTemplate = new Object();
	profileTemplate[_("Owner")]		= ecRightsTemplate[_("Owner")];
	profileTemplate[_("Secretary")]	= ecRightsTemplate[_("Secretary")];
	profileTemplate[_("Only read")]	= ecRightsTemplate[_("Only read")];
	profileTemplate[_("None")]		= 0;

	// Loading profiles.
	for (var foldername in delegate["permissions"]){
		// Profile element.
		var element = dhtml.getElementById(foldername);
		if (element) {
			dhtml.deleteAllChildren(element);
			for(var title in profileTemplate){
				var option = dhtml.addElement(null, "option");
				option.text = title;
				option.value = profileTemplate[title];
				element.options[element.length] = option;
			}
			// Finally set profile according to permission a delegate has.
			element.value = delegate["permissions"][foldername];

			// toggle delegate meeting rule option
			if(element.id == "calendar") {
				toggleDelegateMeetingRuleOption(element);
			}
		}
	}

	// Set flags for private items.
	var see_private = dhtml.getElementById("see_private");
	if (parseInt(delegate["see_private"])){
		see_private.checked = true;
	}

	// Set flag for delegate meeting rule.
	var delegate_meeting_rule_checkbox = dhtml.getElementById("delegate_meeting_rule_checkbox");
	// if calendar permission is not set as owner then don't set this flag
	var calendar_folder_element = dhtml.getElementById("calendar");
	if(parseInt(delegate["delegate_meeting_rule"]) && (calendar_folder_element.value == ecRightsTemplate[_("Owner")] || calendar_folder_element.value == ecRightsTemplate[_("Secretary")])) {
		delegate_meeting_rule_checkbox.checked = true;
	}
}
/**
 * submitDelegatePermissions
 *
 * Called when user has done with giving permissions to delegate and
 * clicks 'OK' to save permissions.
 */
function submitDelegatePermissions()
{
	// Retrieve permissions for all folders.
	for (var foldername in delegate["permissions"]){
		var element = dhtml.getElementById(foldername);
		if (element) {
			delegate["permissions"][foldername] = element.value;
		}
	}
	
	// Retrieve permission for private items.
	delegate["see_private"] = "0";
	var see_private = dhtml.getElementById("see_private");
	if (see_private.checked){
		delegate["see_private"] = "1";
	}

	// Retrieve flag for delegate meeting rule.
	delegate["delegate_meeting_rule"] = "0";
	var delegate_meeting_rule_checkbox = dhtml.getElementById("delegate_meeting_rule_checkbox");
	if(delegate_meeting_rule_checkbox.checked) {
		delegate["delegate_meeting_rule"] = "1";
	}
	
	for (var i = 0; i < editDelegates.length; i++){
		editDelegates[i]["permissions"] = delegate["permissions"];
		editDelegates[i]["see_private"] = delegate["see_private"];
		editDelegates[i]["delegate_meeting_rule"] = delegate["delegate_meeting_rule"];
	}

	// Save delegate permissions.
	return window.resultCallBack(editDelegates, window.windowData["newDelegate"], window.callBackData);
}

/**
 * toggleDelegateMeetingRuleOption
 *
 * This function is used to enable/disable delegate meeting rule checkbox,
 * according to selected profile
 */
function toggleDelegateMeetingRuleOption(element)
{
	if(element && element.value) {
		if(element.value == ecRightsTemplate[_("Owner")] || element.value == ecRightsTemplate[_("Secretary")]) {
			var cb = dhtml.getElementById("delegate_meeting_rule_checkbox");
			cb.disabled = false;

			var label = cb.nextSibling.nextSibling;
			dhtml.removeClassName(label, "disabled_text");
		} else {
			var cb = dhtml.getElementById("delegate_meeting_rule_checkbox");
			cb.checked = false;		// clear previous value
			cb.disabled = true;

			var label = cb.nextSibling.nextSibling;
			dhtml.addClassName(label, "disabled_text");
		}
	}
}