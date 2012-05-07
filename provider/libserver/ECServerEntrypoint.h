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

#ifndef ECECSERVERENTRYPOINT_H
#define ECECSERVERENTRYPOINT_H

#include "ECLogger.h"
#include "ECConfig.h"
#include "ECDatabase.h"
#include "ZarafaUtil.h"


#define ZARAFA_SERVER_INIT_SERVER		0
#define ZARAFA_SERVER_INIT_OFFLINE		1

typedef void (*THREADSTATUSCALLBACK)(pthread_t id, std::string strStatus, void* ulParam);

struct SOAPINFO {
	 CONNECTION_TYPE ulConnectionType;
	 THREADSTATUSCALLBACK lpCallBack;
	 void* ulCallBackParam;
};

#define SOAP_CONNECTION_TYPE_NAMED_PIPE(soap)	\
	((soap) && ((soap)->user) && ((((SOAPINFO*)(soap)->user)->ulConnectionType == CONNECTION_TYPE_NAMED_PIPE) || (((SOAPINFO*)(soap)->user)->ulConnectionType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)))

#define SOAP_CONNECTION_TYPE(soap)	\
	(((SOAPINFO*)(soap)->user)->ulConnectionType)

#define SOAP_CALLBACK(soap, thread, name)										\
	if ((soap) && ((soap)->user) && (((SOAPINFO*)(soap)->user)->lpCallBack))	\
		((SOAPINFO*)(soap)->user)->lpCallBack(thread, name, ((SOAPINFO*)(soap)->user)->ulCallBackParam)


ECRESULT zarafa_init(ECConfig *lpConfig, ECLogger *lpLogger, ECLogger* lpAudit, bool bHostedZarafa, bool bDistributedZarafa);
ECRESULT zarafa_exit();
void zarafa_removeallsessions();

//Internal used functions
void AddDatabaseObject(ECDatabase* lpDatabase);

// server init function
ECRESULT zarafa_initlibrary(char *lpDatabaseDir, char *lpConfigFile, ECLogger *lpLogger); // Init mysql library
ECRESULT zarafa_unloadlibrary();					// Unload mysql library

// Exported functions
ZARAFA_API ECRESULT GetDatabaseObject(ECDatabase **lppDatabase);

// SOAP connection management
void zarafa_new_soap_connection(CONNECTION_TYPE ulType, struct soap *soap, THREADSTATUSCALLBACK lpCallBack, void* ulParam);
void zarafa_disconnect_soap_connection(struct soap *soap);
void zarafa_end_soap_connection(struct soap *soap);
bool zarafa_get_soap_connection_type(struct soap *soap, CONNECTION_TYPE *lpulType);


#endif //ECECSERVERENTRYPOINT_H
