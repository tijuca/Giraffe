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

#ifndef ECLICENSECLIENT_H
#define ECLICENSECLIENT_H

#include <zarafa/zcdefs.h>
#include <vector>
#include <string>

#include "ECChannelClient.h"
#include <zarafa/ZarafaCode.h>

class ECLicenseClient _zcp_final : public ECChannelClient{
public:
    ECLicenseClient(const char *szLicensePath, unsigned int ulTimeOut);
    ~ECLicenseClient();
    
    ECRESULT GetCapabilities(unsigned int ulServiceType, std::vector<std::string > &lstCapabilities);
    ECRESULT QueryCapability(unsigned int ulServiceType, const std::string &strCapability, bool *lpbResult);
    ECRESULT GetSerial(unsigned int ulServiceType, std::string &lpstrSerial, std::vector<std::string> &lstCALs);
    ECRESULT GetInfo(unsigned int ulServiceType, unsigned int *lpulUserCount);
    ECRESULT Auth(const unsigned char *lpData, unsigned int ulSize, unsigned char **lpResponse, unsigned int *lpulResponseSize);
    ECRESULT SetSerial(unsigned int ulServiceType, const std::string &strSerial, const std::vector<std::string> &lstCALs);

private:
	ECRESULT ServiceTypeToServiceTypeString(unsigned int ulServiceType, std::string &strServiceType);
};

#endif
