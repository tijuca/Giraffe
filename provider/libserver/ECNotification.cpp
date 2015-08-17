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

// ECNotification.cpp: implementation of the ECNotification class.
//
//////////////////////////////////////////////////////////////////////
#include "platform.h"

#include "ECNotification.h"
#include "ECMAPI.h"
#include "SOAPUtils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ECNotification::ECNotification()
{
	Init();
}

ECNotification::~ECNotification()
{
	FreeNotificationStruct(m_lpsNotification, true);
}

ECNotification::ECNotification(const ECNotification &x)
{
	Init();

	*this = x;
}

ECNotification::ECNotification(notification &notification)
{
	Init();

	*this = notification;
}

void ECNotification::Init()
{
	this->m_lpsNotification = new notification;

	memset(m_lpsNotification, 0, sizeof(notification));
}

ECNotification& ECNotification::operator=(const ECNotification &x)
{
	if(this != &x){
		CopyNotificationStruct(NULL, x.m_lpsNotification, *this->m_lpsNotification);
	}

	return *this;
}

ECNotification& ECNotification::operator=(const notification &srcNotification)
{

	CopyNotificationStruct(NULL, (notification *)&srcNotification, *this->m_lpsNotification);

	return *this;
}

void ECNotification::SetConnection(unsigned int ulConnection)
{
	m_lpsNotification->ulConnection = ulConnection;
}

void ECNotification::GetCopy(struct soap *soap, notification &notification)
{
	CopyNotificationStruct(soap, this->m_lpsNotification, notification);
}

/**
 * Get object size
 *
 * @return Object size in bytes
 */
unsigned int ECNotification::GetObjectSize() 
{
	return NotificationStructSize(m_lpsNotification);
}
