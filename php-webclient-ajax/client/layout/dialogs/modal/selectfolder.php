<?php
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

?>
<?php

function getDialogTitle(){
	return isset($_GET["title"]) ? htmlentities(get("title", false, false, STRING_REGEX)) : _("Select folder");
}


function getIncludes(){
	return array(
			"client/widgets/tree.js",
			"client/layout/css/tree.css",
			"client/modules/hierarchymodule.js",
			"client/modules/".getModuleName().".js",
			"client/layout/js/selectfolder.js"
		);
}

function getModuleName(){
	return "hierarchyselectmodule";
}

function getJavaScript_onload(){ ?>
					var elem = dhtml.getElementById("targetfolder");

					// check that this dialog should implement multiple selection or not
					var multipleSelection = "<?=get("multipleSelection", false, false, STRING_REGEX)?>";
					var validatorType = "<?=get("validatorType", false, false, STRING_REGEX)?>";

					module.init(moduleID, elem, false, multipleSelection, validatorType);
					module.list();

					var dialogname = window.name;
					if(!dialogname) {
						dialogname = window.dialogArguments.dialogName;
					}

					var subtitle = dhtml.getElementById("subtitle");
					dhtml.deleteAllChildren(subtitle);
					subtitle.appendChild(document.createTextNode("<?=get("subtitle", "", false, STRING_REGEX)?>" ));

					var entryid = "<?=get("entryid", "none", false, ID_REGEX)?>";
					if(entryid == '' ||  entryid == 'none' || entryid == 'undefined'){
						entryid = false;
					}
					if(!entryid && typeof window.windowData != "undefined") {
						entryid = window.windowData.entryid;
					}

					if(module.multipleSelection && module.validatorType == "search") {
						var subfoldersElement = dhtml.getElementById("subfolders");
						subfoldersElement.style.display = "block";
						subfoldersElement.style.visibility = "visible";

						if(typeof window.windowData != "undefined" && typeof window.windowData["subfolders"] != "undefined") {
							// check subfolders option
							var subFoldersCheckboxElem = dhtml.getElementById("subfolders_checkbox");
							if(subFoldersCheckboxElem != null) {
								subFoldersCheckboxElem.checked = Boolean(window.windowData["subfolders"]);
							}
						}
					}

					setTimeout(function(){
						if(module.multipleSelection) {
							module.selectFolderCheckbox(entryid);
						} else {
							module.selectFolder(entryid);
						}
					}, 1000);

					
<?php } // getJavaSctipt_onload	

function getJavaScript_other(){ ?>
<?php }

function getBody(){
?>
		<dl id="copymovemessages" class="folderdialog">
			<dt><?=_("Please select a folder")?>:</dt>
			<dd id="targetfolder" class="dialog_hierarchy" onmousedown="return false;"></dd>

			<dd><span id="sourcemessages"></span></dd>
		</dl>
		<div id="subfolders">
			<input type="checkbox" id="subfolders_checkbox" disabled>
			<label id="subfolders_label" class="disabled_text" for="subfolders_checkbox"><?=_("Search subfolders")?></label>
		</div>

		<?=createConfirmButtons("if(submitFolder()) window.close; else window.focus();")?>
<?
}
?>
