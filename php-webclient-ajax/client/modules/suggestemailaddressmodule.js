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

suggestEmailAddressModule.prototype = new Module;
suggestEmailAddressModule.prototype.constructor = suggestEmailAddressModule;
suggestEmailAddressModule.superclass = Module.prototype;

function suggestEmailAddressModule(id, suggestlist){
	if(arguments.length > 1) {
		this.init(id, suggestlist);
	}
}

suggestEmailAddressModule.prototype.init = function(id, suggestlist){
	this.id = id;
	this.suggestionlists = new Array();
	this.addSuggestionList(suggestlist);
	this.cache = new Array();
	suggestEmailAddressModule.superclass.init.call(this, id);
}

suggestEmailAddressModule.prototype.addSuggestionList = function(suggestlist){
	if(typeof suggestlist == "object"){
		this.suggestionlists[suggestlist.id] = suggestlist;
	}
}

suggestEmailAddressModule.prototype.getList = function(suggestlistId, str){
	if(typeof this.cache[str] == "object"){
		if(typeof this.suggestionlists[suggestlistId] == "object"){
			this.suggestionlists[suggestlistId].handleResult(str, this.cache[str]);
		}
	}else{
		webclient.xmlrequest.addData(this, "getRecipientList", {searchstring: str, returnid: suggestlistId});
		webclient.xmlrequest.sendRequest();
	}
}

suggestEmailAddressModule.prototype.deleteRecipient = function(suggestlistId, recipient){
	var emailAddresses = new Array();
	// retieve the email addresses from the text and sent to server to delete it.
	var recipients = (recipient.indexOf(";") > 1) ? recipient.split(";") : [recipient];
	for (var i=0; i<recipients.length; i++ ){
		emailAddresses.push(recipients[i].substring(recipients[i].indexOf("<")+1, recipients[i].length-1).trim());
	}
	recipient = emailAddresses.join(";");
	webclient.xmlrequest.addData(this, "deleteRecipient", {deleteRecipient: recipient, returnid: suggestlistId});
	webclient.xmlrequest.sendRequest();
	this.cache = new Array();
}

suggestEmailAddressModule.prototype.execute = function(type, action){
	var result = new Array();
	var searchstring = false;
	try{
		var resultcollection = action.getElementsByTagName("result");
		for(var i=0;i<resultcollection.length;i++){
			if(resultcollection[i].firstChild != null){
				result.push(resultcollection[i].firstChild.nodeValue);
			}
		}
		searchstring = dhtml.getXMLValue(action, "searchstring", "");
		var returnId = dhtml.getXMLValue(action, "returnid", false);

		this.cache[searchstring] = result;
	}
	catch(e){
	}

	if(typeof returnId != "undefined" && typeof this.suggestionlists[returnId] == "object"){
		this.suggestionlists[returnId].handleResult(searchstring, result);
	}
}

