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

#include <zarafa/platform.h>

#include <string>
#include <vector>

#ifdef LINUX
#include <sys/un.h>
#include <sys/socket.h>
#endif

#include <zarafa/ECDefs.h>
#include <zarafa/ECChannel.h>
#include <zarafa/base64.h>
#include <zarafa/stringutil.h>

#include "ECLicenseClient.h"

ECLicenseClient::ECLicenseClient(const char *szLicensePath,
    unsigned int ulTimeOut) :
	ECChannelClient(szLicensePath, " \t")
{
	m_ulTimeout = ulTimeOut;
}

ECLicenseClient::~ECLicenseClient()
{
}

ECRESULT ECLicenseClient::ServiceTypeToServiceTypeString(unsigned int ulServiceType, std::string &strServiceType)
{
    ECRESULT er = erSuccess;
    switch(ulServiceType)
    {
        case 0 /*SERVICE_TYPE_ZCP*/:
            strServiceType = "ZCP";
            break;
        case 1 /*SERVICE_TYPE_ARCHIVE*/:
            strServiceType = "ARCHIVER";
            break;
        default:
            er = ZARAFA_E_INVALID_TYPE;
            break;
    }

    return er;
}
    
ECRESULT ECLicenseClient::GetCapabilities(unsigned int ulServiceType, std::vector<std::string > &lstCapabilities)
{
	ECRESULT er;
	std::string strServiceType;

	er = ServiceTypeToServiceTypeString(ulServiceType, strServiceType);
	if (er != erSuccess)
		return er;
	return DoCmd("CAPA " + strServiceType, lstCapabilities);
}

ECRESULT ECLicenseClient::QueryCapability(unsigned int ulServiceType, const std::string &strCapability, bool *lpbResult)
{
	ECRESULT er;
	std::string strServiceType;
	std::vector<std::string> vResult;

	er = ServiceTypeToServiceTypeString(ulServiceType, strServiceType);
	if (er != erSuccess)
		return er;

	er = DoCmd("QUERY " + strServiceType + " " + strCapability, vResult);
	if (er != erSuccess)
		return er;

	*lpbResult = (vResult.front().compare("ENABLED") == 0);
	return erSuccess;
}

ECRESULT ECLicenseClient::GetSerial(unsigned int ulServiceType, std::string &strSerial, std::vector<std::string> &lstCALs)
{
	ECRESULT er;
	std::vector<std::string> lstSerials;
	std::string strServiceType;

	er = ServiceTypeToServiceTypeString(ulServiceType, strServiceType);
	if (er != erSuccess)
		return er;

	er = DoCmd("SERIAL " + strServiceType, lstSerials);
	if (er != erSuccess)
		return er;

    if(lstSerials.empty()) {
        strSerial = "";
		return erSuccess;
    } else {
    	strSerial=lstSerials.front();
    	lstSerials.erase(lstSerials.begin());
    }
    
	lstCALs=lstSerials;
	return erSuccess;
}

ECRESULT ECLicenseClient::GetInfo(unsigned int ulServiceType, unsigned int *lpulUserCount)
{
	ECRESULT er;
    std::vector<std::string> lstInfo;
    unsigned int ulUserCount = 0;
	std::string strServiceType;

	er = ServiceTypeToServiceTypeString(ulServiceType, strServiceType);
	if (er != erSuccess)
		return er;

	er = DoCmd("INFO " + strServiceType, lstInfo);
	if (er != erSuccess)
		return er;

	if (lstInfo.empty())
		return ZARAFA_E_INVALID_PARAMETER;
        
    ulUserCount = atoi(lstInfo.front().c_str());
    lstInfo.erase(lstInfo.begin());
    
	if (lpulUserCount != NULL)
		*lpulUserCount = ulUserCount;
	return erSuccess;
}

ECRESULT ECLicenseClient::Auth(const unsigned char *lpData,
    unsigned int ulSize, unsigned char **lppResponse,
    unsigned int *lpulResponseSize)
{
	ECRESULT er;
    std::vector<std::string> lstAuth;
    std::string strDecoded;
    unsigned char *lpResponse = NULL;
    
	er = DoCmd((std::string)"AUTH " + base64_encode(lpData, ulSize), lstAuth);
	if (er != erSuccess)
		return er;
	if (lstAuth.empty())
		return ZARAFA_E_INVALID_PARAMETER;
    
	strDecoded = base64_decode(lstAuth.front());

	if (lppResponse != NULL) {
		lpResponse = new unsigned char [strDecoded.size()];
		memcpy(lpResponse, strDecoded.c_str(), strDecoded.size());
		*lppResponse = lpResponse;
	}
	if (lpulResponseSize != NULL)
		*lpulResponseSize = strDecoded.size();
	return erSuccess;
}

ECRESULT ECLicenseClient::SetSerial(unsigned int ulServiceType, const std::string &strSerial, const std::vector<std::string> &lstCALs)
{
	ECRESULT er;
	std::string strServiceType;
	std::string strCommand;
	std::vector<std::string> lstRes;

	er = ServiceTypeToServiceTypeString(ulServiceType, strServiceType);
	if (er != erSuccess)
		return er;

	strCommand = "SETSERIAL " + strServiceType + " " + strSerial;
	for (std::vector<std::string>::const_iterator iCAL = lstCALs.begin(); iCAL != lstCALs.end(); ++iCAL)
		strCommand.append(" " + *iCAL);

	return DoCmd(strCommand, lstRes);
}
