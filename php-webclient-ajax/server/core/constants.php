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
	/**
	 * This file contains only some constants used anyware in the WebApp
	 */

	// These are the events, which a module can register for.
	define("OBJECT_SAVE", 				0);
	define("OBJECT_DELETE", 			1);
	define("TABLE_SAVE",				2);
	define("TABLE_DELETE",				3);
	define("REQUEST_START",				4);
	define("REQUEST_END",				5);
	define("DUMMY_EVENT",				6);

	// dummy entryid, used for the REQUEST events
	define("REQUEST_ENTRYID",			"dummy_value");
	
	// used in operations->getHierarchyList
	define("HIERARCHY_GET_ALL",			0);
	define("HIERARCHY_GET_DEFAULT",		1);
	define("HIERARCHY_GET_SPECIFIC",	2);

	// check to see if the HTML editor is installled
	if (is_dir(FCKEDITOR_PATH.'/editor') && is_file(FCKEDITOR_PATH.'/editor/fckeditor.html')){
		define('FCKEDITOR_INSTALLED',true);
	}else{
		define('FCKEDITOR_INSTALLED',false);
	}
    
	// used with fields/columns to specify a autosize column
	define("PERCENTAGE", "percentage");


	// used by distribution lists
	define("DL_GUID",    pack("H*", "C091ADD3519DCF11A4A900AA0047FAA4"));
	define("DL_USER",    0xC3);		//	195
	define("DL_USER2",   0xD3);		//	211
	define("DL_USER3",   0xE3);		//	227
	define("DL_DIST",    0xB4);		//	180
	define("DL_USER_AB", 0xB5);		//	181
	define("DL_DIST_AB", 0xB6);		//	182

				

?>
