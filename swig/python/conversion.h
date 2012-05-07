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

#ifndef CONVERSION_H
#define CONVERSION_H

#include <edkmdb.h>		// LPREADSTATE
#include "ECDefs.h"		// LPECUSER


LPSPropValue	Object_to_LPSPropValue(PyObject *object, void *lpBase = NULL);
PyObject *		List_from_LPSPropValue(LPSPropValue lpProps, ULONG cValues);
LPSPropValue	List_to_LPSPropValue(PyObject *sv, ULONG *cValues, void *lpBase = NULL);

PyObject *		List_from_LPTSTRPtr(LPTSTR *lpStrings, ULONG cValues);

LPSPropTagArray	List_to_LPSPropTagArray(PyObject *sv);
PyObject *		List_from_LPSPropTagArray(LPSPropTagArray lpPropTagArray);

LPSRestriction	Object_to_LPSRestriction(PyObject *sv, void *lpBase = NULL);
void			Object_to_LPSRestriction(PyObject *sv, LPSRestriction lpsRestriction, void *lpBase = NULL);
PyObject *		Object_from_LPSRestriction(LPSRestriction lpRestriction);

PyObject *		Object_from_LPACTION(LPACTION lpAction);
PyObject *		Object_from_LPACTIONS(ACTIONS *lpsActions);
void			Object_to_LPACTION(PyObject *object, ACTION *lpAction, void *lpBase);
void			Object_to_LPACTIONS(PyObject *object, ACTIONS *lpActions, void *lpBase = NULL);

LPSSortOrderSet	Object_to_LPSSortOrderSet(PyObject *sv);
PyObject *		Object_from_LPSSortOrderSet(LPSSortOrderSet lpSortOrderSet);

PyObject *		List_from_LPSRowSet(LPSRowSet lpRowSet);
LPSRowSet		List_to_LPSRowSet(PyObject *av);

LPADRLIST		List_to_LPADRLIST(PyObject *av);
PyObject *		List_from_LPADRLIST(LPADRLIST lpAdrList);

LPADRPARM		Object_to_LPADRPARM(PyObject *av);

LPADRENTRY		Object_to_LPADRENTRY(PyObject *av);

PyObject *		List_from_LPSPropProblemArray(LPSPropProblemArray lpProblemArray);
PyObject *		List_from_LPMAPINAMEID(LPMAPINAMEID *lppMAPINameId, ULONG cNames);
LPMAPINAMEID *	List_to_p_LPMAPINAMEID(PyObject *, ULONG *lpcNames);

LPENTRYLIST		List_to_LPENTRYLIST(PyObject *);
PyObject *		List_from_LPENTRYLIST(LPENTRYLIST lpEntryList);

LPNOTIFICATION	List_to_LPNOTIFICATION(PyObject *, ULONG *lpcNames);
PyObject *		List_from_LPNOTIFICATION(LPNOTIFICATION lpNotif, ULONG cNotifs);
PyObject *		Object_from_LPNOTIFICATION(NOTIFICATION *lpNotif);
NOTIFICATION *	Object_to_LPNOTIFICATION(PyObject *);

LPFlagList		List_to_LPFlagList(PyObject *);
PyObject *		List_from_LPFlagList(LPFlagList lpFlags);

LPMAPIERROR		Object_to_LPMAPIERROR(PyObject *);
PyObject *		Object_from_LPMAPIERROR(LPMAPIERROR lpMAPIError);

LPREADSTATE		List_to_LPREADSTATE(PyObject *, ULONG *lpcElements);
PyObject *		List_from_LPREADSTATE(LPREADSTATE lpReadState, ULONG cElements);

LPCIID 			List_to_LPCIID(PyObject *, ULONG *);

LPECUSER		Object_to_LPECUSER(PyObject *, ULONG);
PyObject *		Object_from_LPECUSER(LPECUSER lpUser);
PyObject *		List_from_LPECUSER(LPECUSER lpUser, ULONG cElements);

LPECGROUP		Object_to_LPECGROUP(PyObject *, ULONG);
PyObject *		Object_from_LPECGROUP(LPECGROUP lpGroup);
PyObject *		List_from_LPECGROUP(LPECGROUP lpGroup, ULONG cElements);

LPECCOMPANY		Object_to_LPECCOMPANY(PyObject *, ULONG);
PyObject *		Object_from_LPECCOMPANY(LPECCOMPANY lpCompany);
PyObject *		List_from_LPECCOMPANY(LPECCOMPANY lpCompany, ULONG cElements);

LPECQUOTA		Object_to_LPECQUOTA(PyObject *);
PyObject *		Object_from_LPECQUOTA(LPECQUOTA lpQuota);

PyObject *		Object_from_LPECQUOTASTATUS(LPECQUOTASTATUS lpQuotaStatus);

PyObject *		Object_from_LPECUSERCLIENTUPDATESTATUS(LPECUSERCLIENTUPDATESTATUS lpECUCUS);

LPROWLIST		List_to_LPROWLIST(PyObject *);

LPECSVRNAMELIST List_to_LPECSVRNAMELIST(PyObject *object);

PyObject *		Object_from_LPECSERVER(LPECSERVER lpServer);

PyObject *		List_from_LPECSERVERLIST(LPECSERVERLIST lpServerList);

void			Init();

void			DoException(HRESULT hr);
int				GetExceptionError(PyObject *, HRESULT *);

#endif // ndef CONVERSION_H
