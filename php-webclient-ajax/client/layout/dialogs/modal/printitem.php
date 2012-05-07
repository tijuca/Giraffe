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

function getModuleName() {
	return 'printitemmodule';
}

function getDialogTitle() {
	return _("Print preview");
}

function getIncludes(){
	return array(
			"client/modules/".getModuleName().".js",
			"client/modules/printmessageitem.js",
		);
}

function getJavaScript_onload(){ ?>
					var data = new Object();
					var elem = dhtml.getElementById("printing_frame");
					module.init(moduleID, elem);

					module.setData(<?=get("storeid","false","'", ID_REGEX)?>, <?=get("parententryid","false","'", ID_REGEX)?>);
					
					var attachNum = false;
					<? if(isset($_GET["attachNum"]) && is_array($_GET["attachNum"])) { ?>
						attachNum = new Array();
					
						<? foreach($_GET["attachNum"] as $attachNum) { 
							if(preg_match_all(NUMERIC_REGEX, $attachNum, $matches)) {
							?>
								attachNum.push(<?=intval($attachNum)?>);
						<?	}
					} ?>
					
					<? } ?>
					var entryid = "<?=get("entryid","", "", ID_REGEX)?>";
					if(entryid != ""){
						module.open(entryid, entryid, attachNum);
					}else{
						// if entryid is not there that means, the page is called to show unsaved data in printpreview.
						module.printFromUnsaved(parentwindow);
					}

					window.onresize();
<?php } // getJavaScript_onload						

function getJavaScript_onresize() {?>
	var top = dhtml.getElementById("dialog_content").offsetTop;
	var frame = dhtml.getElementById("printing_frame");

	var window_height = window.innerHeight;
	if(!window_height){ // fix for IE
		window_height = document.body.offsetHeight;
	}
	frame.height = (window_height - top - 10) + "px";
<?php } // onresize

function getBody(){ ?>
	<iframe id="printing_frame" name="printing_frame" width="100%" height="150" frameborder="0" scrolling="auto" style="border: 1px solid #000;"></iframe>
<?php } // getBody

function getMenuButtons(){
	return array(
			"close"=>array(
				'id'=>"close",
				'name'=>_("Close"),
				'title'=>_("Close preview"),
				'callback'=>'function(){window.close();}'
			),
			
			"print"=>array(
				'id'=>'print',
				'name'=>_("Print"),
				'title'=>_("Print"),
				'callback'=>'function(){printing_frame.focus();printing_frame.print();}'
			),
			
		);
}

?>
