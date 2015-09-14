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

function eventRulesOpenItem(ruleid) {
	editRule(ruleid);
}

function ruleCallBack(result, callBackData)
{
	var internalid = false;
	if (typeof(callBackData)!="undefined" && typeof(callBackData.internalid)!="undefined"){
	    internalid = callBackData.internalid;
	}
	module.setRule(internalid, result);
    
    return true;
}

// Called by rule dialog to get rule information
function getRule(internalid) {
    return window.rules[internalid];
}

// UI buttons
function addRule() {
	var callBackData = new Object;

    webclient.openModalDialog(module, "rule", DIALOG_URL+"task=rule_modal", 520, 550, ruleCallBack, callBackData);
}

function editRule(internalid) {
	if (typeof(ruleid)=="undefined"){
	    var selected = module.getSelectedIDs();
		internalid = selected[0];
	}

	if (typeof(internalid)!="undefined"){
		var callBackData = new Object;
	    callBackData.internalid = internalid;
    
	    webclient.openModalDialog(module, "rule", DIALOG_URL+"task=rule_modal&internalid=" + internalid, 520, 550, ruleCallBack, callBackData);
	}
}

function deleteRule() {
    var selected = module.getSelectedIDs();

    if (selected.length>0)
	    module.deleteRules(selected);
}

function moveUp() {
    var selected = module.getSelectedRowNumber(); 
    
    if(selected == 0) {
        // Already at top
        return;
    }
    
    var internalid1 = module.getMessageByRowNumber(selected);
    var internalid2 = module.getMessageByRowNumber(selected-1);
    
    module.swap(internalid1, internalid2);
}

function moveDown() {
    var selected = module.getSelectedRowNumber();
    
    if(selected == module.getRowCount() - 1)
        return; // Already at bottom
        
    var internalid1 = module.getMessageByRowNumber(selected);
    var internalid2 = module.getMessageByRowNumber(selected+1);
  
    module.swap(internalid2, internalid1);
}

function submitRules() {
    module.submitRules();
	return true;
}
