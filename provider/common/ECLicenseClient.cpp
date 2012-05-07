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

#include "platform.h"

#include <string>
#include <vector>

#include <sys/un.h>
#include <sys/socket.h>

#include "ECDefs.h"
#include "ECChannel.h"
#include "base64.h"
#include "stringutil.h"

#include "ECLicenseClient.h"

ECLicenseClient::ECLicenseClient(char *szLicensePath, unsigned int ulTimeOut)
	: ECChannelClient(szLicensePath, " \t")
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
    ECRESULT er = erSuccess;
	std::string strServiceType;

	er = ServiceTypeToServiceTypeString(ulServiceType, strServiceType);
	if (er != erSuccess)
		goto exit;

    er = DoCmd("CAPA " + strServiceType, lstCapabilities);

exit:
    return er;
}

ECRESULT ECLicenseClient::QueryCapability(unsigned int ulServiceType, const std::string &strCapability, bool *lpbResult)
{
    ECRESULT er = erSuccess;
	std::string strServiceType;
	std::vector<std::string> vResult;

	er = ServiceTypeToServiceTypeString(ulServiceType, strServiceType);
	if (er != erSuccess)
		goto exit;

    er = DoCmd("QUERY " + strServiceType + " " + strCapability, vResult);
    if (er != erSuccess)
		goto exit;

	*lpbResult = (vResult.front().compare("ENABLED") == 0);

exit:
    return er;
}

ECRESULT ECLicenseClient::GetSerial(unsigned int ulServiceType, std::string &strSerial, std::vector<std::string> &lstCALs)
{
    ECRESULT er = erSuccess;
    std::vector<std::string> lstSerials;
	std::string strServiceType;

	er = ServiceTypeToServiceTypeString(ulServiceType, strServiceType);
	if (er != erSuccess)
		goto exit;

    er = DoCmd("SERIAL " + strServiceType, lstSerials);
    if(er != erSuccess)
        goto exit;

    if(lstSerials.empty()) {
        strSerial = "";
        goto exit;
    } else {
    	strSerial=lstSerials.front();
    	lstSerials.erase(lstSerials.begin());
    }
    
	lstCALs=lstSerials;

exit:
    return er;
}

ECRESULT ECLicenseClient::GetInfo(unsigned int ulServiceType, unsigned int *lpulUserCount)
{
    ECRESULT er = erSuccess;
    std::vector<std::string> lstInfo;
    unsigned int ulUserCount = 0;
    unsigned int ulTrialtime = 0;
	std::string strServiceType;

	er = ServiceTypeToServiceTypeString(ulServiceType, strServiceType);
	if (er != erSuccess)
		goto exit;


    er = DoCmd("INFO " + strServiceType, lstInfo);
    if(er != erSuccess)
        goto exit;

    if(lstInfo.empty()) {
        er = ZARAFA_E_INVALID_PARAMETER;
        goto exit;
    }
        
    ulUserCount = atoi(lstInfo.front().c_str());
    lstInfo.erase(lstInfo.begin());
    
/*    if(!lstInfo.empty()) {
        if(lstInfo.front() == "TRIALTIME") {
            lstInfo.erase(lstInfo.begin());
            if(!lstInfo.empty())
                ulTrialtime = atoi(lstInfo.front().c_str());
        }
    }
  */  
    if(lpulUserCount)
        *lpulUserCount = ulUserCount;
    //if(lpulTrialtime)
      //  *lpulTrialtime = ulTrialtime;

exit:    
    return er;
}

ECRESULT ECLicenseClient::Auth(unsigned char *lpData, unsigned int ulSize, unsigned char **lppResponse, unsigned int *lpulResponseSize)
{
    ECRESULT er = erSuccess;
    std::vector<std::string> lstAuth;
    std::string strDecoded;
    unsigned char *lpResponse = NULL;
    
    er = DoCmd((std::string)"AUTH " + base64_encode(lpData, ulSize), lstAuth);
    if(er != erSuccess)
        goto exit;
        
    if(lstAuth.empty()) {
        er = ZARAFA_E_INVALID_PARAMETER;
        goto exit;
    }
    
    strDecoded = base64_decode(lstAuth.front());

    lpResponse = new unsigned char [strDecoded.size()];
    memcpy(lpResponse, strDecoded.c_str(), strDecoded.size());
    
    if(lppResponse)
        *lppResponse = lpResponse;
    if(lpulResponseSize)
        *lpulResponseSize = strDecoded.size();

exit:
    return er;
}

ECRESULT ECLicenseClient::SetSerial(unsigned int ulServiceType, const std::string &strSerial, const std::vector<std::string> &lstCALs)
{
	ECRESULT er = erSuccess;
	std::string strServiceType;
	std::string strCommand;
	std::vector<std::string> lstRes;

	er = ServiceTypeToServiceTypeString(ulServiceType, strServiceType);
	if (er != erSuccess)
		goto exit;

	strCommand = "SETSERIAL " + strServiceType + " " + strSerial;
	for (std::vector<std::string>::const_iterator iCAL = lstCALs.begin(); iCAL != lstCALs.end(); ++iCAL)
		strCommand.append(" " + *iCAL);

	er = DoCmd(strCommand, lstRes);

exit:
	return er;
}
