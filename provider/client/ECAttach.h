/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#ifndef ECATTACH_H
#define ECATTACH_H

#include <kopano/zcdefs.h>
#include <mapidefs.h>
#include <kopano/Util.h>
#include "ECMessage.h"
#include "ECMAPIProp.h"
#include "Mem.h"


class ECMsgStore;

class ECAttach : public ECMAPIProp, public IAttach {
protected:
	ECAttach(ECMsgStore *, ULONG obj_type, BOOL modify, ULONG attach_num, const ECMAPIProp *root);
	virtual ~ECAttach(void) = default;
public:
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _kc_override;
	static HRESULT Create(ECMsgStore *, ULONG obj_type, BOOL modify, ULONG attach_num, const ECMAPIProp *root, ECAttach **);

	// Override for SaveChanges
	virtual HRESULT SaveChanges(ULONG ulFlags);

	// Override for OpenProperty
	virtual HRESULT OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN *lppUnk);

	static  HRESULT	GetPropHandler(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);
	static HRESULT SetPropHandler(ULONG ulPropTag, void *lpProvider, const SPropValue *lpsPropValue, void *lpParam);

	// Override for CopyTo
	virtual HRESULT CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, const SPropTagArray *lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray *lppProblems);

	// Override for HrSetRealProp - should reset instance ID when changed
	virtual HRESULT HrSetRealProp(const SPropValue *lpsPropValue);
	
	virtual HRESULT HrSaveChild(ULONG ulFlags, MAPIOBJECT *lpsMapiObject);

private:
	ULONG ulAttachNum;
	ALLOC_WRAP_FRIEND;
};

class ECAttachFactory _kc_final : public IAttachFactory {
public:
	HRESULT Create(ECMsgStore *, ULONG obj_type, BOOL modify, ULONG attach_num, const ECMAPIProp *, ECAttach **) const;
};


#endif // ECATTACH_H
