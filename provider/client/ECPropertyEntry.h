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

#ifndef ECPROPERTYENTRY_H
#define ECPROPERTYENTRY_H

#include <mapidefs.h>
#include <mapicode.h>

#include "ECInvariantChecker.h"

// C++ class to represent a property in the property list.
class ECProperty {
public:
	ECProperty(const ECProperty &Property);
	ECProperty(LPSPropValue lpsProp);
	~ECProperty();

	HRESULT CopyFrom(LPSPropValue lpsProp);
	HRESULT CopyTo(LPSPropValue lpsProp, void *lpBase, ULONG ulPropTag);
	HRESULT CopyToByRef(LPSPropValue lpsProp);
	
	bool operator==(const ECProperty &property) const;
	SPropValue GetMAPIPropValRef();

	ULONG GetSize() const { return ulSize; }
	ULONG GetPropTag() const { return ulPropTag; }
	DWORD GetLastError() const { return dwLastError; }

	DECL_INVARIANT_CHECK

private:
	DECL_INVARIANT_GUARD(ECProperty)
	HRESULT CopyFromInternal(LPSPropValue lpsProp);

private:
	ULONG ulSize;
	ULONG ulPropTag;
	union _PV Value;

	DWORD dwLastError;
};

// A class representing a property we have in-memory, a list of which is held by ECMAPIProp
// Deleting a property just sets the property as deleted
class ECPropertyEntry {
public:
	ECPropertyEntry(ULONG ulPropTag);
	ECPropertyEntry(ECProperty *property);
	~ECPropertyEntry();

	HRESULT			HrSetProp(ECProperty *property);
	HRESULT			HrSetProp(LPSPropValue lpsPropValue);
	HRESULT			HrSetClean();

	ECProperty *	GetProperty() { return lpProperty; }
	ULONG			GetPropTag() const { return ulPropTag; }
	void			DeleteProperty();
	BOOL			FIsDirty() const { return fDirty; }
	BOOL			FIsLoaded() const { return lpProperty != NULL; }

	DECL_INVARIANT_CHECK

private:
	DECL_INVARIANT_GUARD(ECPropertyEntry)

	ECProperty		*lpProperty;
	ULONG			ulPropTag;
	BOOL			fDirty;
};

#endif
