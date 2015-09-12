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
function getModuleName(){
	return "foldersizemodule";
}

function getDialogTitle()
{
	return _("Folder Size");
}

function getIncludes(){
	return array("client/layout/css/tabbar.css",
			"client/widgets/widget.js",
			"client/widgets/tablewidget.js",
			"client/layout/js/foldersize.js",
			"client/modules/".getModuleName().".js"
			);
}

function getJavaScript_onload()
{
?>					var data = new Object();
					data["store"] = <?=get("storeid","false", "'", ID_REGEX)?>;
					data["entryid"] = <?=get("entryid","false", "'", ID_REGEX)?>;

					module.init(moduleID, dhtml.getElementById("folder_size"), false, data);
					initFolderSize(module);
					module.getFolderSize();

<?
}					

function getBody(){
?>
<table id="folder_size">
	<tbody>
		<tr>
			<td><?=_("Folder Name")?>:</td>
			<td id="folder_name">&nbsp;</td>
		</tr>
		<tr>
			<td><?=_("Size (without subfolders)")?>:</td>
			<td id="size_excl">&nbsp;</td>
		</tr>
		<tr>
			<td><?=_("Total size (including subfolders)")?>:</td>
			<td id="size_incl">&nbsp;</td>
		</tr>
	</tbody>
</table>
<hr />
<div id="foldersize_table"></div>
<hr />
<?php 

	print (createCloseButton("window.close();"));
} // getBody
?>
