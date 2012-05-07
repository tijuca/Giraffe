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

function mailOptionsCallBack(result)
{
    setImportance(result.importance);
    dhtml.getElementById("sensitivity").value = result.sensitivity;
	toggleReadReceipt(!result.readreceipt);
}

function autosave(){
	if(window.changedSinceLastAutoSave && !isMessageBodyEmpty()){
		submit_createmail(false);
		window.changedSinceLastAutoSave = false;
	}
}

function setCursorPosition(message_action) {
	var htmlFormat = dhtml.getElementById("use_html").value == "true" ? true : false;
	var htmlBody = dhtml.getElementById("html_body");
	var cursorPos = webclient.settings.get("createmail/cursor_position", "start");
	var currentPos;

	switch(message_action) {
		case "reply":
		case "replyall":
		case "forward":
			if(htmlFormat == true) {
				// FCKEditor is installed
				if(typeof(FCKeditorAPI) != "undefined" && (fckEditor = FCKeditorAPI.GetInstance("html_body"))) {
					if(cursorPos == "start") {
						if(message_action != "forward")
							fckEditor.Focus();
					} else {
						if(fckEditor.EditorWindow.document && module.signature != ""){
							// Create a element and place at the end of editor
							var pEle = dhtml.addElement(fckEditor.EditorWindow.document.body, 'p', false, false, null, fckEditor.EditorWindow);
							if(message_action != "forward")
							{
								// Select newly inserted element.
								fckEditor.Selection.SelectNode(pEle);
								fckEditor.Selection.Save();
							}
							// Update scrolling
							fckEditor.EditingArea.Document.documentElement.scrollTop = fckEditor.EditingArea.Document.body.lastChild.offsetTop;
							fckEditor.EditingArea.Document.documentElement.scrollLeft = 0;

							// Add signature at the end if cursor is positioned at the end.
							var signatureElement = dhtml.addElement(fckEditor.EditorWindow.document.body, 'div', false, false, null, fckEditor.EditorWindow);
							signatureElement.innerHTML = module.signature;

							// Now we can put focus on editor.
							if(message_action != "forward")
								fckEditor.Focus();
						}else{
							break;
						}
					}
				}
			} else {
				// FCKEditor is not installed
				if(cursorPos == "start") {
					dhtml.setSelectionRange(htmlBody, 0, 0);
				} else {
					if(htmlBody && typeof htmlBody == "object") {
						currentPos = htmlBody.value.length;
						// Add signature at the end if cursor is positioned at the end.
						htmlBody.value += module.signature;
						dhtml.setSelectionRange(htmlBody, currentPos, currentPos);
					}
				}
			}
			if(message_action == "forward") dhtml.getElementById("to").focus();
			break;
		default:
			dhtml.getElementById("to").focus();
	}
}
/**
 * Function which check if message body is empty.
 *@return boolean -return true if message body is empty, false if not empty.
 */
function isMessageBodyEmpty()
{
	// Check for HTML format
	if (typeof FCKeditorAPI != "undefined" && (fckEditor = FCKeditorAPI.GetInstance("html_body"))){
		var content = fckEditor.GetXHTML();
		if (content.length > 0) return false;
		else return true;
	} else {
		// Check for PLAIN format
		var body = dhtml.getElementById("html_body");
		if (body && body.value.trim().length > 0) return false;
		else return true;
	}
}
/**
 * Callback function for checknames in mail.
 * @param Object resolveObj obj of the resolved names
 */
function checkNamesCallBackCreateMail(resolveObj)
{
	checkNamesCallBack(resolveObj, true);

	//Send mail
	if(window.resolveForSendingMessage === true){
		submit_createmail(true);
	}
}

function eventCreateMailItemKeyCtrlSubmit(moduleObject, element, event)
{
	switch(event.keyCombination)
	{
		case this.keys["mail"]["save"]:
			submit_createmail();
			break;
		case this.keys["mail"]["send"]:
			submit_createmail(true);
			break;
	}
}

/**
 * Function to set/unset the read reciept for email sending option
 * @param String value previous value of readreceipt setting
 */
function toggleReadReceipt(value)
{
   var read_receipt = dhtml.getElementById("read_receipt_requested");
   var read_receipt_button = dhtml.getElementById('read_receipt_button');
   if(read_receipt) {
		var checked = false;
		if(value == "false" || value == false) {
			read_receipt.value = "true";
			dhtml.addClassName(read_receipt_button, "menubuttonselected");
			checked = true;
		}else{
			read_receipt.value = "false";
			dhtml.removeClassName(read_receipt_button, "menubuttonselected");
		}
		read_receipt.checked = checked;
	}
}
