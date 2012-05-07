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

#ifndef ECUNKNOWN_H
#define ECUNKNOWN_H

#include "IECUnknown.h"
#include "pthread.h"

#include <list>
#include <mapi.h>

/**
 * Return interface pointer on a specific interface query.
 * @param[in]	_guid	The interface guid.
 * @param[in]	_interface	The class which implements the interface
 * @note guid variable must be named 'refiid', return variable must be named lppInterface.
 */
#define REGISTER_INTERFACE(_guid, _interface)	\
	if (refiid == (_guid)) {				 	\
		AddRef();								\
		*lppInterface = (void*)(_interface);	\
		return hrSuccess;						\
	}

/**
 * Return interface pointer on a specific interface query without incrementing the refcount.
 * @param[in]	_guid	The interface guid.
 * @param[in]	_interface	The class which implements the interface
 * @note guid variable must be named 'refiid', return variable must be named lppInterface.
 */
#define REGISTER_INTERFACE_NOREF(_guid, _interface)	\
	if (refiid == (_guid)) {				 		\
		AddRef();									\
		*lppInterface = (void*)(_interface);		\
		return hrSuccess;							\
	}

class ECUnknown : public IECUnknown {
public:
	ECUnknown(char *szClassName = NULL);
	virtual ~ECUnknown();

	virtual ULONG AddRef();
	virtual ULONG Release();
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

	virtual HRESULT AddChild(ECUnknown *lpChild);
	virtual HRESULT RemoveChild(ECUnknown *lpChild);

	class xUnknown : public IUnknown
	{
	public:
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();	
	} m_xUnknown;

	// lpParent is public because it is always thread-safe and valid
	ECUnknown				*lpParent;
	virtual BOOL IsParentOf(ECUnknown *lpObject);
	virtual BOOL IsChildOf(ECUnknown *lpObject);

protected:
	// Called by AddChild
	virtual HRESULT SetParent(ECUnknown *lpParent);

	// Kills itself when lstChildren.empty() AND m_cREF == 0
	virtual HRESULT			Suicide();

	ULONG					m_cRef;
	char					*szClassName;
	std::list<ECUnknown *>	lstChildren; 
	pthread_mutex_t mutex;

};


#endif // ECUNKNOWN_H
