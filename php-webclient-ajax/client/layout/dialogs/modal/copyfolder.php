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
	return _("Copy/Move folder");
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
					var elem = dhtml.getElementById("targetfolder");
					module.init(moduleID, elem);
					module.list();
					var dialogname = window.name;

					if(!dialogname) {
						dialogname = window.dialogArguments.dialogName;
					}

					parentModule = windowData.parentModule;
					
					dhtml.getElementById("sourcefolder").appendChild(document.createTextNode(parentModule.getFolder(source_entryid).display_name));
					
<?php } // getJavaSctipt_onload	

function getJavaScript_other(){ ?>
			var source_entryid = "<?=get("source_entryid","none", false, ID_REGEX)?>";
			var parent_entryid;
			var parentModule;
			
			function submit(type)
			{
				if (!type) type = "copy";
				
				var target_entryid = module.selectedFolder;
				var target_storeid = false;
				if(dhtml.getElementById(module.selectedFolder)){
					target_storeid = dhtml.getElementById(module.selectedFolder).storeid;
				}

				parentModule.copyFolder(target_entryid, target_storeid, type, source_entryid);
				
				window.close();
			}
<?php }

function getBody(){
?>
		<dl id="copymovefolder" class="folderdialog">
			<dt><?=_("Folder")?>:</dt>
			<dd><span id="sourcefolder"></span></dd>
			
			<dt><?=_("Destination folder")?>:</dt>
			<dd id="targetfolder" class="dialog_hierarchy" onmousedown="return false;"></dd>

		
		
		</dl>

	<?=createButtons(array("title"=>_("Copy"),"handler"=>"submit('copy');"), array("title"=>_("Move"),"handler"=>"submit('move');"), array("title"=>_("Cancel"),"handler"=>"window.close();"))?>

<?
}
?>
