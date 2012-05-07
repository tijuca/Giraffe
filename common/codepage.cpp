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

#include <mapidefs.h>
#include <mapicode.h>

#include "codepage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// These charset should all be supported by iconv

// @see http://msdn.microsoft.com/en-us/library/dd317756(VS.85).aspx
// this list is incomplete
struct CPMAP {
    char *charset;
    ULONG codepage;
} CPMAP[] = {
  { "DIN_66003",		20106 },
  { "NS_4551-1", 		20108 },
  { "SEN_850200_B",		20107 },
  { "big5",			 	950 },
  { "csISO2022JP",		50221 },
  { "euc-jp",			51932 },
  { "euc-cn",			51936 },
  { "euc-kr",		 	51949 },
  { "euc-kr",	 		949 },	 // euc-kr is compatible with cp949 according to some sources (some horde tickets say this)
  { "cp949",			949 },
  { "ks_c_5601-1987",	949 },	 // ks_c_5601-1987 == cp949, but this charset is not recognized by iconv
  { "gb18030",		 	936 },	 // was gb2312, but cp936 is gb3212 + more, which is superseded by gb18030 (is codepage 54936?)
  { "gb2312",		 	936 },	 // entry for reverse lookup
  { "GBK",			 	936 },	 // entry for reverse lookup
  { "csgb2312",	 		52936 }, // not sure, hz-cn-2312 according to MS, iconv has this one
  { "ibm852",		 	852 },
  { "ibm866",		 	866 },
  { "iso-2022-jp",	 	50220 },
  { "iso-2022-jp",	 	50222 },
  { "iso-2022-kr",	 	50225 },
  { "windows-1252",	 	1252 }, // ordering is important.. for outgoing message we want to send windows-1252, not iso
  { "iso-8859-1",	 	1252 },
  { "iso-8859-1",	 	28591 },
  { "iso-8859-2",	 	28592 },
  { "iso-8859-3", 		28593 },
  { "iso-8859-4",	 	28594 },
  { "iso-8859-5",		28595 },
  { "iso-8859-6",		28596 },
  { "iso-8859-7",		28597 },
  { "iso-8859-8", 		28598 },
  { "iso-8859-8-i",		28598 },
  { "iso-8859-9", 		28599 },
  { "iso-8859-13", 		28603 },
  { "iso-8859-15", 		28605 },
  { "koi8-r",		 	20866 },
  { "koi8-u",			21866 },
  { "shift-jis",	 	932 },
  { "unicode",		 	1200 },
  { "unicodebig",	 	1201 },
  { "utf-7",		 	65000 },
  { "utf-8",		 	65001 },
  { "windows-1250",	 	1250 },
  { "windows-1251",		1251 },
  { "windows-1253", 	1253 },
  { "windows-1254", 	1254 },
  { "windows-1255", 	1255 },
  { "windows-1256", 	1256 },
  { "windows-1257", 	1257 },
  { "windows-1258", 	1258 },
  { "windows-874",	 	874 },
  { "us-ascii",	 		20127 }
};

/**
 * Converts a Windows codepage to a valid iconv charset string.
 *
 * @param[in]	codepage	Windows codepage number (eg. from PR_INTERNET_CPID)
 * @param[out]	lppszCharset	Pointer to internal structure containing iconv charset string
 * @retval MAPI_E_NOT_FOUND on unknown codepage, lppszCharset will be unchanged.
 */
HRESULT HrGetCharsetByCP(ULONG codepage, char **lppszCharset)
{
    HRESULT hr = hrSuccess;
    unsigned int i = 0;
    
    for(i=0;i<sizeof(CPMAP)/sizeof(CPMAP[0]);i++) {
        if(CPMAP[i].codepage == codepage) {
            *lppszCharset = CPMAP[i].charset;
            return hrSuccess;
        }
    }
    
    hr = MAPI_E_NOT_FOUND;
    
    return hr;
}

/**
 * Converts a Windows codepage to a valid iconv charset string.
 *
 * @param[in]	codepage	Windows codepage number (eg. from PR_INTERNET_CPID)
 * @param[out]	lppszCharset	Pointer to internal structure containing iconv charset string
 * @retval MAPI_E_NOT_FOUND on unknown codepage, lppszCharset will be unchanged.
 */
HRESULT HrGetCPByCharset(const char *lpszCharset,ULONG *codepage)
{
    HRESULT hr = hrSuccess;
    unsigned int i = 0;
    
    for(i=0;i<sizeof(CPMAP)/sizeof(CPMAP[0]);i++) {
        if(stricmp(CPMAP[i].charset, lpszCharset) == 0) {
            *codepage = CPMAP[i].codepage;
            return hrSuccess;
        }
    }
    
    hr = MAPI_E_NOT_FOUND;
    
    return hr;
}
