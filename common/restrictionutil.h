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

#ifndef __RESTRICTION_UTIL_H
#define __RESTRICTION_UTIL_H

#include <mapi.h>
#include <mapiutil.h>
#include "Util.h"

#define CREATE_RESTRICTION(_lpRestriction) \
	{\
	hr = MAPIAllocateBuffer(sizeof(SRestriction), (LPVOID*)&(_lpRestriction)); \
	if (hr != hrSuccess) \
		goto exit; \
	}

#define CREATE_RESTRICTION_BASE(_lpBase, _lpRestriction) \
	{\
	hr = MAPIAllocateMore(sizeof(SRestriction), (_lpBase), (LPVOID*)&(_lpRestriction)); \
	if (hr != hrSuccess) \
		goto exit; \
	}

#define FREE_RESTRICTION(_lpRestriction) \
	{\
	if (_lpRestriction) {\
		MAPIFreeBuffer(_lpRestriction);\
		_lpRestriction = NULL;\
	}\
	}

#define CREATE_RES_AND(_lpBase, _lpRestriction, _values) \
	{\
	if ((_lpBase) == NULL || _lpRestriction == NULL) { \
		hr = MAPI_E_INVALID_PARAMETER; \
		goto exit; \
	} \
\
	(_lpRestriction)->rt = RES_AND; \
\
	hr = MAPIAllocateMore(sizeof(SRestriction) * _values, (_lpBase), (void**)&((_lpRestriction)->res.resAnd.lpRes)); \
	if (hr != hrSuccess) { \
		hr = MAPI_E_INVALID_PARAMETER; \
		goto exit; \
	} \
\
	(_lpRestriction)->res.resAnd.cRes = _values; \
	}

#define CREATE_RES_OR(_lpBase, _lpRestriction, _values) \
	{\
	if ((_lpBase) == NULL || _lpRestriction == NULL) { \
		hr = MAPI_E_INVALID_PARAMETER; \
		goto exit; \
	} \
\
	(_lpRestriction)->rt = RES_OR; \
\
	hr = MAPIAllocateMore(sizeof(SRestriction) * _values, (_lpBase), (void**)&((_lpRestriction)->res.resOr.lpRes)); \
	if (hr != hrSuccess) { \
		hr = MAPI_E_INVALID_PARAMETER; \
		goto exit; \
	} \
\
	(_lpRestriction)->res.resOr.cRes = _values; \
	}

#define CREATE_RES_NOT(_lpBase, _lpRestriction) \
	{\
	if ((_lpBase) == NULL || _lpRestriction == NULL) { \
		hr = MAPI_E_INVALID_PARAMETER; \
		goto exit; \
	} \
\
	(_lpRestriction)->rt = RES_NOT; \
\
	hr = MAPIAllocateMore(sizeof(SRestriction), (_lpBase), (void**)&((_lpRestriction)->res.resNot.lpRes)); \
	if (hr != hrSuccess) { \
		hr = MAPI_E_INVALID_PARAMETER; \
		goto exit; \
	} \
\
	(_lpRestriction)->res.resNot.ulReserved = 0; \
}


#define DATA_RES_PROPERTY(_lpBase, _sRestriction, _ulRelop, _ulPropTag, _lpProp) \
	{\
	(_sRestriction).rt = RES_PROPERTY; \
	(_sRestriction).res.resProperty.relop = _ulRelop; \
	(_sRestriction).res.resProperty.ulPropTag = _ulPropTag; \
\
	hr = MAPIAllocateMore(sizeof(SPropValue), (_lpBase), (void**)&(_sRestriction).res.resProperty.lpProp);\
	if(hr != hrSuccess) \
		goto exit; \
	hr = Util::HrCopyProperty((_sRestriction).res.resProperty.lpProp, _lpProp, _lpBase);\
	if(hr != hrSuccess) \
		goto exit; \
	}

#define DATA_RES_PROPERTY_CHEAP(_lpBase, _sRestriction, _ulRelop, _ulPropTag, _lpProp) \
	{\
	(_sRestriction).rt = RES_PROPERTY; \
	(_sRestriction).res.resProperty.relop = _ulRelop; \
	(_sRestriction).res.resProperty.ulPropTag = _ulPropTag; \
	(_sRestriction).res.resProperty.lpProp = _lpProp; \
	}

#define  DATA_RES_EXIST(_lpBase, _sRestriction, _ulPropTag) \
	{\
	(_sRestriction).rt = RES_EXIST; \
	(_sRestriction).res.resExist.ulReserved1 = 0; \
	(_sRestriction).res.resExist.ulPropTag = _ulPropTag; \
	(_sRestriction).res.resExist.ulReserved2 = 0; \
	}

#define  DATA_RES_COMPAREPROPS(_lpBase, _sRestriction, _relop, _ulPropTag1, _ulPropTag2) \
	{\
	(_sRestriction).rt = RES_COMPAREPROPS; \
	(_sRestriction).res.resCompareProps.relop = _relop; \
	(_sRestriction).res.resCompareProps.ulPropTag1 = _ulPropTag1; \
	(_sRestriction).res.resCompareProps.ulPropTag2 = _ulPropTag2; \
	}

#define  DATA_RES_BITMASK(_lpBase, _sRestriction, _relBMR, _ulPropTag, _ulMask) \
	{\
	(_sRestriction).rt = RES_BITMASK; \
	(_sRestriction).res.resBitMask.relBMR = _relBMR; \
	(_sRestriction).res.resBitMask.ulPropTag = _ulPropTag; \
	(_sRestriction).res.resBitMask.ulMask = _ulMask; \
	}

#define DATA_RES_CONTENT(_lpBase, _sRestriction, _ulFuzzyLevel, _ulPropTag, _lpProp) \
	{\
	(_sRestriction).rt = RES_CONTENT; \
	(_sRestriction).res.resContent.ulFuzzyLevel = _ulFuzzyLevel; \
	(_sRestriction).res.resContent.ulPropTag = _ulPropTag; \
\
	hr = MAPIAllocateMore(sizeof(SPropValue), (_lpBase), (void**)&(_sRestriction).res.resContent.lpProp);\
	if(hr != hrSuccess) \
		goto exit; \
	hr = Util::HrCopyProperty((_sRestriction).res.resContent.lpProp, _lpProp, _lpBase);\
	if(hr != hrSuccess) \
		goto exit; \
	}

#define DATA_RES_CONTENT_CHEAP(_lpBase, _sRestriction, _ulFuzzyLevel, _ulPropTag, _lpProp) \
	{\
	(_sRestriction).rt = RES_CONTENT; \
	(_sRestriction).res.resContent.ulFuzzyLevel = _ulFuzzyLevel; \
	(_sRestriction).res.resContent.ulPropTag = _ulPropTag; \
	(_sRestriction).res.resContent.lpProp = _lpProp; \
	}


// UTILS
//FP = From mapipropObject
#define DATA_FP_RES_PROPERTY(_lpBase, _sRestriction, _ulRelop, _ulPropTag, _lpFolder, _ulPropTagFromFolder) \
	{\
	LPSPropValue _lpPropValue = NULL;\
	hr = HrGetOneProp(_lpFolder, _ulPropTagFromFolder, &_lpPropValue);\
	if(hr != hrSuccess)\
		goto exit;\
\
	DATA_RES_PROPERTY(_lpBase, _sRestriction, _ulRelop, _ulPropTag, _lpPropValue)\
\
	if (_lpPropValue) { MAPIFreeBuffer(_lpPropValue); _lpPropValue = NULL; }\
	}

#endif
