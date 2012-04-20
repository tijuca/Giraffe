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

#ifndef WSSTORETABLEVIEW_H
#define WSSTORETABLEVIEW_H

#include "WSTableView.h"

class WSStoreTableView : public WSTableView
{
protected:
    WSStoreTableView(ULONG ulType, ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport);
	virtual ~WSStoreTableView();

public:
	static HRESULT Create(ULONG ulType, ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport, WSTableView **lppTableView);
	virtual	HRESULT	QueryInterface(REFIID refiid, void **lppInterface);

};

class WSTableOutGoingQueue : public WSStoreTableView
{
protected:
    WSTableOutGoingQueue(ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport);

public:
	static HRESULT Create(ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport, WSTableOutGoingQueue **lppTableOutGoingQueue);
	virtual	HRESULT	QueryInterface(REFIID refiid, void **lppInterface);

public:
	virtual HRESULT HrOpenTable();
};

class WSTableMultiStore : public WSStoreTableView
{
protected:
    WSTableMultiStore(ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport);
    virtual ~WSTableMultiStore();

public:
	static HRESULT Create(ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport, WSTableMultiStore **lppTableMultiStore);

	virtual HRESULT HrOpenTable();

	virtual HRESULT HrSetEntryIDs(LPENTRYLIST lpMsgList);
private:
    struct entryList m_sEntryList;
};

/* not really store tables, but the code is the same.. */
class WSTableMisc : public WSStoreTableView
{
protected:
	WSTableMisc(ULONG ulTableType, ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport);
	virtual ~WSTableMisc();

public:
	static HRESULT Create(ULONG ulTableType, ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport, WSTableMisc **lppTableMisc);

	virtual HRESULT HrOpenTable();

private:
	ULONG m_ulTableType;
};

/**
 * MailBox table which shows all the stores
 */
class WSTableMailBox : public WSStoreTableView
{
protected:
	WSTableMailBox(ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ECMsgStore *lpMsgStore, WSTransport *lpTransport);
	virtual ~WSTableMailBox();

public:
	static HRESULT Create(ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ECMsgStore *lpMsgStore, WSTransport *lpTransport, WSTableMailBox **lppTableMisc);
};
#endif
