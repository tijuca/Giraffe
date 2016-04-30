/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
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

#ifndef ECECSERVERENTRYPOINT_H
#define ECECSERVERENTRYPOINT_H

#include <zarafa/ECLogger.h>
#include <zarafa/ECConfig.h>
#include "ECDatabase.h"
#include "ZarafaUtil.h"


#define ZARAFA_SERVER_INIT_SERVER		0
#define ZARAFA_SERVER_INIT_OFFLINE		1

#define SOAP_CONNECTION_TYPE_NAMED_PIPE(soap)	\
	((soap) && ((soap)->user) && ((((SOAPINFO*)(soap)->user)->ulConnectionType == CONNECTION_TYPE_NAMED_PIPE) || (((SOAPINFO*)(soap)->user)->ulConnectionType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)))

#define SOAP_CONNECTION_TYPE(soap)	\
	(((SOAPINFO*)(soap)->user)->ulConnectionType)


extern ECRESULT zarafa_init(ECConfig *lpConfig, ECLogger *lpAudit, bool bHostedZarafa, bool bDistributedZarafa);
ECRESULT zarafa_exit();
void zarafa_removeallsessions();

//Internal used functions
void AddDatabaseObject(ECDatabase* lpDatabase);

// server init function
extern ECRESULT zarafa_initlibrary(const char *lpDatabaseDir, const char *lpConfigFile); // Init mysql library
extern ECRESULT zarafa_unloadlibrary(void); // Unload mysql library

// Exported functions
ZARAFA_API ECRESULT GetDatabaseObject(ECDatabase **lppDatabase);

// SOAP connection management
void zarafa_new_soap_connection(CONNECTION_TYPE ulType, struct soap *soap);
void zarafa_end_soap_connection(struct soap *soap);

void zarafa_new_soap_listener(CONNECTION_TYPE ulType, struct soap *soap);
void zarafa_end_soap_listener(struct soap *soap);
    
void zarafa_disconnect_soap_connection(struct soap *soap);


#endif //ECECSERVERENTRYPOINT_H
