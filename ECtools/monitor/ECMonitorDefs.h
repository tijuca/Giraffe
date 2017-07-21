/*
 * Copyright 2005 - 2016 Zarafa and its licensors
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

#ifndef ECMONITORDEFS_H
#define ECMONITORDEFS_H

#include <kopano/ECLogger.h>
#include <kopano/ECConfig.h>

struct ECTHREADMONITOR {
	ECLogger*		lpLogger;
	ECConfig*		lpConfig;
	bool			bShutdown;

	ECTHREADMONITOR(void)
	{
		lpLogger = NULL;
		lpConfig = NULL;
		bShutdown = false;
	};
};

#endif
