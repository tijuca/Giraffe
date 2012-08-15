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

#include "platform.h"

#include "MAPIConsoleTable.h"
#include "ConsoleTable.h"
#include "mapi_ptr.h"
#include "mapi_ptr/mapi_rowset_ptr.h"
#include "stringutil.h"

std::string ToString(SPropValue *lpProp)
{
    switch(PROP_TYPE(lpProp->ulPropTag)) {
        case PT_STRING8:
            return std::string(lpProp->Value.lpszA);
        case PT_LONG:
            return stringify(lpProp->Value.ul);
        case PT_DOUBLE:
            return stringify(lpProp->Value.dbl);
        case PT_FLOAT:
            return stringify(lpProp->Value.flt);
        case PT_I8:
            return stringify_int64(lpProp->Value.li.QuadPart);
        case PT_SYSTIME:
        {
            time_t t;
            char buf[32]; // must be at least 26 bytes
            FileTimeToUnixTime(lpProp->Value.ft, &t);
            ctime_r(&t, buf);
            buf[strlen(buf)-2] = 0;
            return std::string(buf);
        }
        case PT_MV_STRING8:
        {
            std::string s;
            for(unsigned int i=0; i < lpProp->Value.MVszA.cValues; i++) {
                if(!s.empty())
                    s += ",";
                s += lpProp->Value.MVszA.lppszA[i];
            }
            
            return s;
        }
            
    }
    
    return std::string();
}

HRESULT MAPITablePrint(IMAPITable *lpTable)
{
    HRESULT hr = hrSuccess;
    SPropTagArrayPtr ptrColumns;
    SRowSetPtr ptrRows;
    ConsoleTable ct(0, 0);
    unsigned int i = 0, j = 0;
    
    hr = lpTable->QueryColumns(0, &ptrColumns);
    if(hr != hrSuccess)
        goto exit;
        
    hr = lpTable->QueryRows(-1, 0, &ptrRows);
    if(hr != hrSuccess)
        goto exit;
        
    ct.Resize(ptrRows.size(), ptrColumns->cValues);
    
    for(i=0;i<ptrColumns->cValues;i++) {
        ct.SetHeader(i, stringify(ptrColumns->aulPropTag[i], true));
    }
    
    for(i=0;i<ptrRows.size();i++) {
        for(j=0;j<ptrRows[i].cValues;j++) {
            ct.SetColumn(i, j, ToString(&ptrRows[i].lpProps[j]));
        }
    }
    
    ct.PrintTable();
        
exit:
    return hr;
}
