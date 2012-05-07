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

#ifndef ECDATABASEUTILS_H
#define ECDATABASEUTILS_H

#include "ECMAPI.h"
#include "Zarafa.h"
#include "ZarafaCode.h"
#include "ECDatabase.h"
#include "ECDatabaseFactory.h"
#include "ECLogger.h"

#include <string>

#define MAX_PROP_SIZE 8192	
#define MAX_QUERY 4096

#define PROPCOL_ULONG	"val_ulong"
#define PROPCOL_STRING	"val_string"
#define PROPCOL_BINARY	"val_binary"
#define PROPCOL_DOUBLE	"val_double"
#define PROPCOL_LONGINT	"val_longint"
#define PROPCOL_HI		"val_hi"
#define PROPCOL_LO		"val_lo"

#define _PROPCOL_ULONG(_tab)	#_tab "." PROPCOL_ULONG
#define _PROPCOL_STRING(_tab)	#_tab "." PROPCOL_STRING
#define _PROPCOL_BINARY(_tab)	#_tab "." PROPCOL_BINARY
#define _PROPCOL_DOUBLE(_tab)	#_tab "." PROPCOL_DOUBLE
#define _PROPCOL_LONGINT(_tab)	#_tab "." PROPCOL_LONGINT
#define _PROPCOL_HI(_tab)		#_tab "." PROPCOL_HI
#define _PROPCOL_LO(_tab)		#_tab "." PROPCOL_LO

#define PROPCOL_HILO		PROPCOL_HI "," PROPCOL_LO
#define _PROPCOL_HILO(_tab)	PROPCOL_HI(_tab) "," PROPCOL_LO(_tab)

/* make string of define value */
#ifndef __STRING
#define __STRING(x) #x
#endif
#define STR(macro) __STRING(macro)

// Warning! Code references the ordering of these values! Do not change unless you know what you're doing!
#define PROPCOLVALUEORDER(_tab) 			_PROPCOL_ULONG(_tab) "," _PROPCOL_STRING(_tab) "," _PROPCOL_BINARY(_tab) "," _PROPCOL_DOUBLE(_tab) "," _PROPCOL_LONGINT(_tab) "," _PROPCOL_HI(_tab) "," _PROPCOL_LO(_tab)
#define PROPCOLVALUEORDER_TRUNCATED(_tab) 	_PROPCOL_ULONG(_tab) ", LEFT(" _PROPCOL_STRING(_tab) ","STR(TABLE_CAP_STRING)"),LEFT(" _PROPCOL_BINARY(_tab) ","STR(TABLE_CAP_BINARY)")," _PROPCOL_DOUBLE(_tab) "," _PROPCOL_LONGINT(_tab) "," _PROPCOL_HI(_tab) "," _PROPCOL_LO(_tab)
enum { VALUE_NR_ULONG=0, VALUE_NR_STRING, VALUE_NR_BINARY, VALUE_NR_DOUBLE, VALUE_NR_LONGINT, VALUE_NR_HILO, VALUE_NR_MAX };

#define PROPCOLORDER "0,properties.tag,properties.type," PROPCOLVALUEORDER(properties)
#define PROPCOLORDER_TRUNCATED "0,properties.tag,properties.type," PROPCOLVALUEORDER_TRUNCATED(properties)
#define MVPROPCOLORDER "count(*),mvproperties.tag,mvproperties.type,group_concat(length(mvproperties.val_ulong),':', mvproperties.val_ulong ORDER BY mvproperties.orderid SEPARATOR ''), group_concat(length(mvproperties.val_string),':', mvproperties.val_string ORDER BY mvproperties.orderid SEPARATOR ''), group_concat(length(mvproperties.val_binary),':', mvproperties.val_binary ORDER BY mvproperties.orderid SEPARATOR ''), group_concat(length(mvproperties.val_double),':', mvproperties.val_double ORDER BY mvproperties.orderid SEPARATOR ''), group_concat(length(mvproperties.val_longint),':', mvproperties.val_longint ORDER BY mvproperties.orderid SEPARATOR ''), group_concat(length(mvproperties.val_hi),':', mvproperties.val_hi ORDER BY mvproperties.orderid SEPARATOR ''), group_concat(length(mvproperties.val_lo),':', mvproperties.val_lo ORDER BY mvproperties.orderid SEPARATOR '')"
#define MVIPROPCOLORDER "0,mvproperties.tag,mvproperties.type | 0x2000," PROPCOLVALUEORDER(mvproperties)
#define MVIPROPCOLORDER_TRUNCATED "0,mvproperties.tag,mvproperties.type | 0x2000," PROPCOLVALUEORDER_TRUNCATED(mvproperties)

enum { FIELD_NR_ID=0, FIELD_NR_TAG, FIELD_NR_TYPE, FIELD_NR_ULONG, FIELD_NR_STRING, FIELD_NR_BINARY, FIELD_NR_DOUBLE, FIELD_NR_LONGINT, FIELD_NR_HI, FIELD_NR_LO, FIELD_NR_MAX };

ULONG GetColOffset(ULONG ulPropTag);
std::string GetPropColOrder(unsigned int ulPropTag, std::string strSubQuery);
unsigned int GetColWidth(unsigned int ulPropType);
ECRESULT	GetPropSize(DB_ROW lpRow, DB_LENGTHS lpLen, unsigned int *lpulSize);
ECRESULT	CopySOAPPropValToDatabasePropVal(struct propVal *lpPropVal, unsigned int *ulColId, std::string &strColData, ECDatabase *lpMySQL, bool bTruncate);
ECRESULT	CopyDatabasePropValToSOAPPropVal(struct soap *soap, DB_ROW lpRow, DB_LENGTHS lpLen, propVal *lpPropVal);

ULONG GetMVItemCount(struct propVal *lpPropVal);
ECRESULT CopySOAPPropValToDatabaseMVPropVal(struct propVal *lpPropVal, int nItem, std::string &strColName, std::string &strColData, ECDatabase *lpDatabase);

ECRESULT ParseMVProp(char* lpRowData, ULONG ulSize, unsigned int *lpulLastPos, std::string *lpstrData);
ECRESULT ParseMVPropCount(char* lpRowData, ULONG ulSize, unsigned int *lpulLastPos, int *lpnItemCount);

unsigned int NormalizeDBPropTag(unsigned int ulPropTag);
bool CompareDBPropTag(unsigned int ulPropTag1, unsigned int ulPropTag2);

#endif // ECDATABASEUTILS_H
