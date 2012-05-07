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

#ifndef ICALMAPI_VCONVERTER_H
#define ICALMAPI_VCONVERTER_H

#include "vtimezone.h"
#include "icalitem.h"
#include "RecurrenceState.h"
#include "charset/convert.h"

#include <mapidefs.h>
#include <libical/ical.h>

class VConverter {
public:
	/* lpNamedProps must be the GetIDsFromNames() of the array in nameids.h */
	VConverter(LPADRBOOK lpAdrBook, timezone_map *mapTimeZones, LPSPropTagArray lpNamedProps, const std::string& strCharset, bool blCensor, bool bNoRecipients, IMailUser *lpImailUser);
	virtual ~VConverter();

	virtual HRESULT HrICal2MAPI(icalcomponent *lpEventRoot /* in */, icalcomponent *lpEvent /* in */, icalitem *lpPrevItem /* in */, icalitem **lppRet /* out */);
	virtual HRESULT HrMAPI2ICal(LPMESSAGE lpMessage /* in */, icalproperty_method *lpicMethod /* out */, std::list<icalcomponent*> *lpEventList /* out */);

protected:
	LPADRBOOK m_lpAdrBook;
	timezone_map *m_mapTimeZones;
	timezone_map_iterator m_iCurrentTimeZone;
	LPSPropTagArray m_lpNamedProps;
	std::string m_strCharset;
	IMailUser *m_lpMailUser;
	bool m_bCensorPrivate;
	bool m_bNoRecipients;

	ULONG m_ulUserStatus;

	convert_context m_converter;

	virtual HRESULT HrGetUID(icalcomponent *lpEvent, std::string *strUid);
	virtual HRESULT HrMakeBinaryUID(const std::string &strUid, void *base, SPropValue *lpPropValue);
	virtual HRESULT HrResolveUser(void *base, std::list<icalrecip> *lplstIcalRecip);
	virtual bool bIsUserLoggedIn(const std::wstring &strUser);

	/* ical -> mapi helper functions */
	virtual HRESULT HrCompareUids(icalitem *lpIcalItem, icalcomponent *lpicEvent);
	virtual HRESULT HrAddUids(icalcomponent *lpicEvent, icalitem *lpIcalItem);
	virtual HRESULT HrHandleExceptionGuid(icalcomponent *lpiEvent, void *base, SPropValue *lpsProp);
	virtual HRESULT HrAddRecurrenceID(icalcomponent *lpiEvent, icalitem *lpIcalItem);
	virtual HRESULT HrAddBaseProperties(icalproperty_method icMethod, icalcomponent *lpicEvent, void *base, bool bIsException, std::list<SPropValue> *lplstMsgProps) = 0; /* pure, must be overloaded */
	virtual HRESULT HrAddStaticProps(icalproperty_method icMethod, icalitem *lpIcalItem);
	virtual HRESULT HrAddSimpleHeaders(icalcomponent *lpicEvent, icalitem *lpIcalItem);
	virtual HRESULT HrAddBusyStatus(icalcomponent *lpicEvent, icalproperty_method icMethod, icalitem *lpIcalItem);
	virtual HRESULT HrAddXHeaders(icalcomponent *lpicEvent, icalitem *lpIcalItem);
	virtual HRESULT HrAddCategories(icalcomponent *lpicEvent, icalitem *lpIcalItem);
	virtual HRESULT HrAddTimes(icalcomponent *lpicEventRoot, icalcomponent *lpicEvent, bool bIsAllday, icalitem *lpIcalItem) = 0; /* pure, must be overloaded */
	virtual HRESULT HrAddRecipients(icalcomponent *lpicEvent, icalitem *lpIcalItem, std::list<SPropValue> *lplstMsgProps, std::list<icalrecip> * lplstIcalRecip);
	virtual HRESULT HrAddReplyRecipients(icalcomponent *lpicEvent, icalitem *lpIcalItem);
	virtual HRESULT HrAddReminder(icalcomponent *lpicEventRoot, icalcomponent *lpicEvent, icalitem *lpIcalItem);
	virtual HRESULT HrAddRecurrence(icalcomponent *lpicEventRoot, icalcomponent *lpicEvent, bool bIsAllday, icalitem *lpIcalItem);
	virtual HRESULT HrAddException(icalcomponent *lpEventRoot, icalcomponent *lpEvent, bool bIsAllday, icalitem *lpPrevItem);
	virtual HRESULT HrAddTimeZone(icalproperty *lpicProp, icalitem *lpIcalItem);
	virtual HRESULT HrRetrieveAlldayStatus(icalcomponent *lpicEvent, bool *blIsAllday);

	/* mapi -> ical helper functions */
	virtual HRESULT HrMAPI2ICal(LPMESSAGE lpMessage, icalproperty_method *lpicMethod, icaltimezone **lppicTZinfo, std::string *lpstrTZid, icalcomponent **lppEvent) = 0; /* pure */
	virtual HRESULT HrMAPI2ICal(LPMESSAGE lpMessage, icalproperty_method *lpicMethod, icaltimezone **lppicTZinfo, std::string *lpstrTZid, icalcomponent *lpEvent);
	virtual HRESULT HrFindTimezone(ULONG ulProps, LPSPropValue lpProps, std::string *lpstrTZid, TIMEZONE_STRUCT *lpTZinfo, icaltimezone **lppicTZinfo);
	virtual HRESULT HrSetTimeProperty(time_t tStamp, bool bDateOnly, icaltimezone *lpicTZinfo, const std::string &strTZid, icalproperty_kind icalkind, icalcomponent *lpicEvent);
	virtual HRESULT HrSetOrganizerAndAttendees(LPMESSAGE lpParentMsg /* if exception*/, LPMESSAGE lpMessage, ULONG ulProps, LPSPropValue lpProps, icalproperty_method *lpicMethod, icalcomponent *lpicEvent);
	virtual HRESULT HrSetTimeProperties(LPSPropValue lpMsgProps, ULONG ulMsgProps, icaltimezone *lpicTZinfo, const std::string &strTZid, icalcomponent *lpEvent);
	virtual HRESULT HrSetICalAttendees(LPMESSAGE lpMessage, const std::wstring &strOrganizer, icalcomponent *lpicEvent);
	virtual HRESULT HrSetBusyStatus(LPMESSAGE lpMessage, ULONG ulBusyStatus, icalcomponent *lpicEvent);
	virtual HRESULT HrSetXHeaders(ULONG ulProps, LPSPropValue lpProps, icalcomponent *lpicEvent);
	virtual HRESULT HrSetVAlarm(ULONG ulProps, LPSPropValue lpProps, icalcomponent *lpicEvent);
	virtual HRESULT HrSetBody(LPMESSAGE lpMessage, icalproperty **lppicProp);
	virtual HRESULT HrSetItemSpecifics(ULONG ulProps, LPSPropValue lpProps, icalcomponent *lpicEvent);
	virtual HRESULT HrSetRecurrenceID(LPSPropValue lpMsgProps, ULONG ulMsgProps, icaltimezone *lpicTZinfo, const std::string &strTZid, icalcomponent *lpiEvent);
	/* recurrence + exceptions */
	virtual HRESULT HrSetRecurrence(LPMESSAGE lpMessage, icalcomponent *lpicEvent, icaltimezone *lpicTZinfo, const std::string &strTZid, std::list<icalcomponent*> *lpEventList);
	virtual HRESULT HrUpdateReminderTime(icalcomponent *lpicEvent, LONG lReminder);
	virtual HRESULT HrGetExceptionMessage(LPMESSAGE lpMessage, time_t tStart, LPMESSAGE *lppMessage);
};

HRESULT HrCopyString(convert_context& converter, std::string& strCharset, void *base, const char* lpszSrc, WCHAR** lppszDst);
HRESULT HrCopyString(void *base, const WCHAR* lpwszSrc, WCHAR** lppwszDst);

#endif
