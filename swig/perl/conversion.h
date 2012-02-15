/*
 * Copyright 2005 - 2009  Zarafa B.V.
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

// HACK OP_DELETE collision
#define OP_DELETE OP_DELETE2
#include <edkmdb.h>
#undef OP_DELETE

AV *			AV_from_LPSPropValue(LPSPropValue lpProps, ULONG cValues);
LPSPropValue	AV_to_LPSPropValue(AV *sv, STRLEN *cValues, void *lpBase = NULL);

AV *			AV_from_LPTSTRPtr(LPTSTR *lpStrings, ULONG cValues);

LPSPropTagArray	AV_to_LPSPropTagArray(AV *sv);
AV *			AV_from_LPSPropTagArray(LPSPropTagArray lpPropTagArray);

LPSRestriction	HV_to_LPSRestriction(HV *sv, void *lpBase = NULL);
int				HV_to_LPSRestriction(HV *sv, LPSRestriction lpsRestriction, void *lpBase = NULL);
HV *			HV_from_LPSRestriction(LPSRestriction lpRestriction);

LPSSortOrderSet	HV_to_LPSSortOrderSet(HV *sv);
HV *			HV_from_LPSSortOrderSet(LPSSortOrderSet lpSortOrderSet);

AV *			AV_from_LPSRowSet(LPSRowSet lpRowSet);
LPSRowSet		AV_to_LPSRowSet(AV *av);
AV *			AV_from_LPADRLIST(LPADRLIST lpRowSet);
LPADRLIST		AV_to_LPADRLIST(AV *av);

LPADRLIST		AV_to_LPADRLIST(AV *av);
AV *			AV_from_LPADRLIST(LPADRLIST lpAdrList);

LPADRPARM		HV_to_LPADRPARM(HV *av);

LPADRENTRY		HV_to_LPADRENTRY(HV *av);

AV *			AV_from_LPSPropProblemArray(LPSPropProblemArray lpProblemArray);
AV *			AV_from_p_LPMAPINAMEID(LPMAPINAMEID *lppMAPINameId, ULONG cNames);
LPMAPINAMEID *	AV_to_p_LPMAPINAMEID(AV *, STRLEN *lpcNames);

LPENTRYLIST		AV_to_LPENTRYLIST(AV *);
AV *			AV_from_LPENTRYLIST(LPENTRYLIST lpEntryList);

LPNOTIFICATION	AV_to_LPNOTIFICATION(AV *, STRLEN *lpcNames);
AV *			AV_from_LPNOTIFICATION(LPNOTIFICATION lpNotif, ULONG cNotifs);

NOTIFICATION *	HV_to_LPNOTIFICATION(HV *);

LPFlagList		AV_to_LPFlagList(AV *);
AV *			AV_from_LPFlagList(LPFlagList lpFlags);

HV *			HV_from_LPMAPIERROR(LPMAPIERROR lpMAPIError);

LPCIID			AV_to_LPCIID(AV *, STRLEN *lpcIID);

LPECUSER		HV_to_LPECUSER(HV *);
HV * HV_from_LPECUSER(LPECUSER lpUser);
AV * AV_from_LPECUSER(LPECUSER lpUsers, ULONG cElements);

LPECGROUP		HV_to_LPECGROUP(HV *);
HV * HV_from_LPECGROUP(LPECGROUP lpGroup);
AV * AV_from_LPECGROUP(LPECGROUP lpGroups, ULONG cElements);

LPECCOMPANY		HV_to_LPECCOMPANY(HV *);
HV * HV_from_LPECCOMPANY(LPECCOMPANY lpCompany);
AV * AV_from_LPECCOMPANY(LPECCOMPANY lpCompanies, ULONG cElements);

LPECQUOTA		HV_to_LPECQUOTA(HV *);
HV *			HV_from_LPECQUOTA(LPECQUOTA lpQuota);

LPROWLIST		AV_to_LPROWLIST(AV *);

AV *			AV_from_LPENTRYLIST(LPENTRYLIST);
HV *			HV_from_LPSRestriction(LPSRestriction);

HV *			HV_from_LPECUSERCLIENTUPDATESTATUS(LPECUSERCLIENTUPDATESTATUS lpECUCUS);

HV *			HV_from_LPECQUOTASTATUS(LPECQUOTASTATUS lpQuotaStatus);

LPECSVRNAMELIST	AV_to_LPECSVRNAMELIST(AV *);

AV *			AV_from_LPECSERVERLIST(LPECSERVERLIST);

