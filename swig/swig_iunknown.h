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

#ifndef SWIG_IUNKNOWN_H
#define SWIG_IUNKNOWN_H

#include <set>
#include <cstdio>

/**
 * IUnknownImplementor takes care of the IUnknown part of an IUnknown
 * derived interface.
 */
template <typename _Interface>
class IUnknownImplementor : public _Interface {
public:
	IUnknownImplementor(ULONG cInterfaces, LPCIID lpInterfaces)
	: m_interfaces(lpInterfaces, lpInterfaces + cInterfaces, &IIDLess)
	{ }

	IUnknownImplementor()
	: m_interfaces(&IID_IUnknown, &IID_IUnknown + 1, &IIDLess)
	{ }

	virtual ~IUnknownImplementor() { 
	}

	ULONG __stdcall AddRef() {
		PyGILState_STATE gstate;
		gstate = PyGILState_Ensure();
		
		Swig::Director *director = dynamic_cast<Swig::Director *>(this);
		PyObject *o = director->swig_get_self();
		Py_INCREF(o);

		PyGILState_Release(gstate);
		return o->ob_refcnt;
	}

	ULONG __stdcall Release() {
		PyGILState_STATE gstate;
		gstate = PyGILState_Ensure();
		
		Swig::Director *director = dynamic_cast<Swig::Director *>(this);
		PyObject *o = director->swig_get_self();
		ULONG cnt = o->ob_refcnt;
		Py_DECREF(o); // Will delete this because python object will have refcount 0, which deletes this object

		PyGILState_Release(gstate);
		return cnt-1;
	}

	HRESULT __stdcall QueryInterface(REFIID iid , void** ppvObject) {
		if (m_interfaces.find(iid) == m_interfaces.end())
			return MAPI_E_INTERFACE_NOT_SUPPORTED;
			
		AddRef();
		*ppvObject = (void*)this;
		return hrSuccess;
	}
	

private:
	static bool IIDLess(REFIID lhs, REFIID rhs) {
		return memcmp(&lhs, &rhs, sizeof(IID)) < 0;
	}

private:
	std::set<IID, bool(*)(REFIID,REFIID)> m_interfaces;
};

#endif // ndef SWIG_IUNKNOWN_H
