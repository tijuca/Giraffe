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

function shortcuts_loadSettings(settings){
	var element = dhtml.getElementById("shortcuts_tab");
	element.style.height = document.documentElement.clientHeight - element.parentNode.offsetTop - 30 + "px";

	var replace_elements = new Array();
	replace_elements['.']  = _("DOT (.)");
	replace_elements[',']  = _("COMMA (,)");
	replace_elements['LA'] = _("LEFT ARROW");
	replace_elements['RA'] = _("RIGHT ARROW");
	replace_elements['UA'] = _("UP ARROW");
	replace_elements['DA'] = _("DOWN ARROW");

	for (var type in KEYS){
		for (var key in KEYS[type]){
			var element = dhtml.getElementById(type +"_"+ key);

			if (element){
				var innerHTML = KEYS[type][key].replace(/\+/g, " + ");

				for(var replace in replace_elements){
					if (innerHTML.indexOf(replace) > 0)
						innerHTML = innerHTML.replace(replace, replace_elements[replace]);
				}
				element.innerHTML = innerHTML;
			}
		}
	}

	var shortcuts_enabled = settings.get("global/shortcuts/enabled", false);
	dhtml.getElementById("shortcuts_on").checked = shortcuts_enabled ? true : false;
	dhtml.getElementById("shortcuts_off").checked = shortcuts_enabled ? false : true;
}

function shortcuts_saveSettings(settings){
	var shortcuts_off = dhtml.getElementById("shortcuts_off");
	var shortcuts_on = dhtml.getElementById("shortcuts_on");
	var old_value = settings.get("global/shortcuts/enabled", false);
	
	if (shortcuts_on.checked){
		settings.set("global/shortcuts/enabled", true);
	} else if (shortcuts_off.checked){
		settings.set("global/shortcuts/enabled", false);
	}
	
	if (old_value != settings.get("global/shortcuts/enabled", -1)){
		// Reload Webaccess because we need settings updated in main window
		reloadNeeded = true;
	}
}

function shortcuts_addCallBack(result){
}