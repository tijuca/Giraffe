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

#ifndef ECNAMEDPROP_H
#define ECNAMEDPROP_H

#include <mapidefs.h>
#include <map>

class WSTransport;

// Sort function
//
// Doesn't really matter *how* it sorts, as long as it's reproduceable
struct ltmap {
	bool operator()(MAPINAMEID *a, MAPINAMEID *b) const
	{
	    int r = memcmp(a->lpguid, b->lpguid, sizeof(GUID));
	    
        if(r<0)
            return false;
        else if(r>0)
            return true;
        else {	    
            if(a->ulKind != b->ulKind)
                return a->ulKind > b->ulKind;

            switch(a->ulKind) {
            case MNID_ID:
                return a->Kind.lID > b->Kind.lID;
            case MNID_STRING:
                return wcscmp(a->Kind.lpwstrName, b->Kind.lpwstrName) < 0;
            default:
                return false;
            }
        }
	}
};

class ECNamedProp {
public:
	ECNamedProp(WSTransport *lpTransport);
	virtual ~ECNamedProp();

	virtual HRESULT GetNamesFromIDs(LPSPropTagArray FAR * lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG FAR * lpcPropNames, LPMAPINAMEID FAR * FAR * lpppPropNames);
	virtual HRESULT GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID FAR * lppPropNames, ULONG ulFlags, LPSPropTagArray FAR * lppPropTags);

private:
	std::map<MAPINAMEID *,ULONG,ltmap>		mapNames;
	WSTransport	*							lpTransport;

	HRESULT			ResolveLocal(MAPINAMEID *lpName, ULONG *ulId);
	HRESULT			ResolveCache(MAPINAMEID *lpName, ULONG *ulId);

	HRESULT			ResolveReverseLocal(ULONG ulId, LPGUID lpGuid, ULONG ulFlags, void *lpBase, MAPINAMEID **lppName);
	HRESULT			ResolveReverseCache(ULONG ulId, LPGUID lpGuid, ULONG ulFlags, void *lpBase, MAPINAMEID **lppName);

	HRESULT			UpdateCache(ULONG ulId, MAPINAMEID *lpName);
	HRESULT			HrCopyNameId(LPMAPINAMEID lpSrc, LPMAPINAMEID *lppDst, void *lpBase);

};

#endif // ECNAMEDPROP_H
