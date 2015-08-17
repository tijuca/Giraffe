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

#ifndef CONVERSION_H
#define CONVERSION_H

#include <edkmdb.h>		// LPREADSTATE
#include "ECDefs.h"		// LPECUSER

#define CONV_COPY_SHALLOW	0
#define CONV_COPY_DEEP		1

typedef int(*TypeCheckFunc)(PyObject*);

int				Object_is_list_of(PyObject *object, TypeCheckFunc fnTypeCheck); 

FILETIME		Object_to_FILETIME(PyObject *object);
PyObject *		Object_from_FILETIME(FILETIME ft);
int				Object_is_FILETIME(PyObject *object);

LPSPropValue	Object_to_LPSPropValue(PyObject *object, ULONG ulFlags = CONV_COPY_SHALLOW, void *lpBase = NULL);
int				Object_is_LPSPropValue(PyObject *object);
PyObject *		List_from_LPSPropValue(LPSPropValue lpProps, ULONG cValues);
LPSPropValue	List_to_LPSPropValue(PyObject *sv, ULONG *cValues, ULONG ulFlags = CONV_COPY_SHALLOW, void *lpBase = NULL);

PyObject *		List_from_LPTSTRPtr(LPTSTR *lpStrings, ULONG cValues);

LPSPropTagArray	List_to_LPSPropTagArray(PyObject *sv, ULONG ulFlags = CONV_COPY_SHALLOW);
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
LPSRowSet		List_to_LPSRowSet(PyObject *av, ULONG ulFlags = CONV_COPY_SHALLOW);

LPADRLIST		List_to_LPADRLIST(PyObject *av, ULONG ulFlags = CONV_COPY_SHALLOW);
PyObject *		List_from_LPADRLIST(LPADRLIST lpAdrList);

LPADRPARM		Object_to_LPADRPARM(PyObject *av);

LPADRENTRY		Object_to_LPADRENTRY(PyObject *av);

PyObject *		List_from_LPSPropProblemArray(LPSPropProblemArray lpProblemArray);
LPSPropProblemArray List_to_LPSPropProblemArray(PyObject *, ULONG ulFlags = CONV_COPY_SHALLOW);

PyObject *		Object_from_LPMAPINAMEID(LPMAPINAMEID lpMAPINameId);
PyObject *		List_from_LPMAPINAMEID(LPMAPINAMEID *lppMAPINameId, ULONG cNames);
LPMAPINAMEID *	List_to_p_LPMAPINAMEID(PyObject *, ULONG *lpcNames, ULONG ulFlags = CONV_COPY_SHALLOW);

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
PyObject *		List_from_LPCIID(LPCIID iids, ULONG cElements);

LPECUSER		Object_to_LPECUSER(PyObject *, ULONG ulFlags);
PyObject *		Object_from_LPECUSER(LPECUSER lpUser, ULONG ulFlags);
PyObject *		List_from_LPECUSER(LPECUSER lpUser, ULONG cElements, ULONG ulFlags);

LPECGROUP		Object_to_LPECGROUP(PyObject *, ULONG ulFlags);
PyObject *		Object_from_LPECGROUP(LPECGROUP lpGroup, ULONG ulFlags);
PyObject *		List_from_LPECGROUP(LPECGROUP lpGroup, ULONG cElements, ULONG ulFlags);

LPECCOMPANY		Object_to_LPECCOMPANY(PyObject *, ULONG ulFlags);
PyObject *		Object_from_LPECCOMPANY(LPECCOMPANY lpCompany, ULONG ulFlags);
PyObject *		List_from_LPECCOMPANY(LPECCOMPANY lpCompany, ULONG cElements, ULONG ulFlags);

LPECQUOTA		Object_to_LPECQUOTA(PyObject *);
PyObject *		Object_from_LPECQUOTA(LPECQUOTA lpQuota);

PyObject *		Object_from_LPECQUOTASTATUS(LPECQUOTASTATUS lpQuotaStatus);

PyObject *		Object_from_LPECUSERCLIENTUPDATESTATUS(LPECUSERCLIENTUPDATESTATUS lpECUCUS);

LPROWLIST		List_to_LPROWLIST(PyObject *, ULONG ulFlags = CONV_COPY_SHALLOW);

LPECSVRNAMELIST List_to_LPECSVRNAMELIST(PyObject *object);

PyObject *		Object_from_LPECSERVER(LPECSERVER lpServer);
PyObject *		List_from_LPECSERVERLIST(LPECSERVERLIST lpServerList);

PyObject *		List_from_wchar_t(wchar_t **, ULONG cElements);

void			Init();

void			DoException(HRESULT hr);
int				GetExceptionError(PyObject *, HRESULT *);

void			Object_to_STATSTG(PyObject *, STATSTG *);
PyObject *		Object_from_STATSTG(STATSTG *);

PyObject *		Object_from_SYSTEMTIME(const SYSTEMTIME &time);
SYSTEMTIME		Object_to_SYSTEMTIME(PyObject *);

#endif // ndef CONVERSION_H
