<?php
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

?>
<?php

function getDialogTitle(){
	return _("New folder");
}


function getIncludes(){
	return array(
			"client/widgets/tree.js",
			"client/layout/css/tree.css",
			"client/modules/hierarchymodule.js",
			"client/modules/".getModuleName().".js"
		);
}

function getModuleName(){
	return "hierarchyselectmodule";
}

function getJavaScript_onload(){ ?>
					//set focus on foldername field for create folder dialog
					dhtml.getElementById("newfolder").focus();

					var elem = dhtml.getElementById("parentfolder");
					module.init(moduleID, elem, true);
					module.list();
					var dialogname = window.name;

					if(!dialogname) {
						dialogname = window.dialogArguments.dialogName;
					}

					parentModule = windowData.parentModule;

					module.selectFolder("<?=get("parent_entryid", "none", false, ID_REGEX)?>");

					if (parentWebclient && parentWebclient.hierarchy && module.selectedFolder){
						var parentFolder = parentWebclient.hierarchy.getFolder(module.selectedFolder);
						if (parentFolder && parentFolder.container_class){
							dhtml.getElementById("newfoldertype").value = parentFolder.container_class;
						}
					}

<?php } // getJavaSctipt_onload	

function getJavaScript_other(){ ?>
			var parent_entryid = "<?=get("parent_entryid","none", false, ID_REGEX)?>";
			var parentModule;
			
			function submit()
			{
				var name = dhtml.getElementById("newfolder").value;

				if(name.length > 0) {
					var type = dhtml.getElementById("newfoldertype").options[dhtml.getElementById("newfoldertype").selectedIndex].value;
					parent_entryid = module.selectedFolder;
					
					parentModule.createFolder(name, type, parent_entryid);	
				}
				window.close();
			}
<?php }

function getBody(){
?>
		<dl id="createfolder" class="folderdialog">
			<dt><?=_("New foldername")?>:</dt>
			<dd><input id="newfolder" type="text"></dd>
			
			<dt><?=_("Folder contains")?>:</dt>
			<dd>
				<select id="newfoldertype">
					<option value="IPF.Note" selected><?=_("E-Mail")?></option>
					<option value="IPF.Contact"><?=_("Contact")?></option>
					<option value="IPF.Task"><?=_("Task")?></option>
					<option value="IPF.Appointment"><?=_("Appointment")?></option>
					<option value="IPF.StickyNote"><?=_("Note")?></option>
				</select>
			</dd>
			<dt><?=_("Select where to place the new folder")?>:</dt>
			<dd id="parentfolder" class="dialog_hierarchy" onmousedown="return false;"></dd>

		
		
		</dl>

	<?=createConfirmButtons("submit()")?>

<?
}
?>
