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
	return 'mailoptionsitemmodule';
}

function getModuleType() {
	return 'item';
}

function getIncludes(){
	return array(
			"client/modules/".getModuleName().".js",
			"client/layout/css/messageoptions.css"
		);
}

function getDialogTitle() {
	return _("Message Options");
}

function getJavaScript_onload(){ ?>
					var data = new Object();
					module.init(moduleID);
					module.setData(<?=get("storeid","false","'", ID_REGEX)?>, <?=get("parententryid","false","'", ID_REGEX)?>);
					module.open(<?=get("entryid","false","'", ID_REGEX)?>);
<?php } // getJavaScript_onload						

function getJavaScript_onresize() { ?>
	// Internet headers element.
	var headersElement = dhtml.getElementById("transport_message_headers");

	var dialogContentElement = dhtml.getElementById("dialog_content");
	var internetHeadersLabelElements = dhtml.getElementsByClassNameInElement(dialogContentElement, "internet_headers", "tr");
	var propertywidthElements = dhtml.getElementsByClassNameInElement(internetHeadersLabelElements[0], "propertywidth", "td");

	// Set the width of Internet headers element.
	headersElement.style.width = (document.documentElement.offsetWidth * 0.98) - propertywidthElements[0].offsetWidth +"px";

	// Set the height of Internet headers element.
	if(document.documentElement.offsetHeight > 300)
		headersElement.style.height = document.documentElement.offsetHeight - 250 +"px";
	else
		headersElement.style.height = "50px";
<?php } // getJavaScript_onresize

function getJavaScript_other(){?>

function saveProps()
{
	if(module) {
		var props = getPropsFromDialog();
		module.save(props);
		window.close();
	}
}

function categoryCallBack(categories)
{
    if(categories) {
        dhtml.getElementById("categories").value = categories;
    }
}

<?php }

function getBody(){ ?>
		<input id="entryid" type="hidden" value="">
		<input id="parent_entryid" type="hidden" value="">
		<div>
			<div class="propertytitle"><?=_("Message Settings")?></div>
			<table cellpadding="2" cellspacing="0">
				<tr>
					<td class="propertynormal propertywidth" nowrap>
						<?=_("Importance")?>:
					</td>
					<td>
						<select id="importance" class="combobox">
							<option value="0"><?=_("Low")?></option>
							<option value="1" selected><?=_("Normal")?></option>
							<option value="2"><?=_("High")?></option>
						</select>
					</td>
				</tr>
				<tr>
					<td class="propertynormal propertywidth" nowrap>
						<?=_("Sensitivity")?>:
					</td>
					<td>
						<select id="sensitivity" class="combobox">
							<option value="0" selected><?=_("Normal")?></option>
							<option value="1"><?=_("Personal")?></option>
							<option value="2"><?=_("Private")?></option>
							<option value="3"><?=_("Confidential")?></option>
						</select>
					</td>
				</tr>
			</table>
			
			<div class="propertytitle"><?=_("Tracking Options")?></div>
			<table cellpadding="2" cellspacing="0">
				<tr>
					<td width="25">
						<input id="read_receipt_requested" type="checkbox" disabled>
					</td>
					<td class="propertynormal" nowrap>
						<?=_("Request a read receipt for this message")?>.
					</td>
				</tr>
			</table>
			
			<div class="propertytitle"></div>
			<table width="100%" border="0" cellpadding="2" cellspacing="0">
				<tr>
					<td class="propertynormal propertywidth">
						<input class="button" type="button" value="<?=_("Categories")?>:" onclick="webclient.openModalDialog(module, 'categories', DIALOG_URL+'task=categories_modal', 350, 370, categoryCallBack);">
					</td>
					<td>
						<input id="categories" class="field" type="text">
					</td>
				</tr>

				<tr class="internet_headers">
					<td class="propertynormal propertywidth">
						<?=_("Internet headers")?>:
					</td>
					<td>
						<div id="transport_message_headers"></div>
					</td>
			</table>

			<?=createConfirmButtons("saveProps()")?>
		</div>
<?php } // getBody
?>
