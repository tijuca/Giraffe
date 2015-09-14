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

function addCategories()
{
	var available_categories = dhtml.getElementById("available_categories");
	var selected_categories = dhtml.getElementById("categories");
	
	var categories = selected_categories.value.split(";");

	for(var i = 0; i < available_categories.length; i++)
	{
		var option = available_categories.options[i];
		if(option.selected == true) {
			if(selected_categories.value.indexOf(";") > 0){
				if(selected_categories.value.indexOf(option.text + ";") < 0) 
					categories.push(option.text);
			}else{
				 if(selected_categories.value.indexOf(option.text) < 0)
				 	categories.push(option.text);
			}
		}
	}

	var tmpcategories = categories;
	categories = new Array();
	for(var i = 0; i < tmpcategories.length; i++) {
		if(tmpcategories[i] != "" && tmpcategories[i] != " ") {
			if(tmpcategories[i].indexOf(" ") == 0) {
				tmpcategories[i] = tmpcategories[i].substring(1);
			}
			
			categories.push(tmpcategories[i]);
		}
	}

	categories.sort(sortCategories);
	selected_categories.value = "";

	for(var i = 0; i < categories.length; i++) {
		selected_categories.value += categories[i] + "; ";
	}

	if (module)
		module.filtercategories(selected_categories, selected_categories.value, available_categories);
}

function sortCategories(a, b)
{
	if(a > b) return 1;
	if(a < b) return -1;
	return 0;
}
