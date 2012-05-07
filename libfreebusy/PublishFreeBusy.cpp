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
#include "PublishFreeBusy.h"
#include "namedprops.h"
#include "mapiguidext.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include "ECLogger.h"
#include "recurrence.h"

#include "restrictionutil.h"
#include "ECFreeBusyUpdate.h"
#include "freebusyutil.h"
#include "ECFreeBusySupport.h"

using namespace std;

#define START_TIME 0
#define END_TIME 1

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/** 
 * Publish free/busy information from the default calendar
 * 
 * @param[in] lpSession Session object of user
 * @param[in] lpDefStore Store of user
 * @param[in] tsStart Start time to publish data of
 * @param[in] ulMonths Number of months to publish
 * @param[in] lpLogger Log object to send log messages to
 * 
 * @return MAPI Error code
 */
HRESULT HrPublishDefaultCalendar(IMAPISession *lpSession, IMsgStore *lpDefStore, time_t tsStart, ULONG ulMonths, ECLogger *lpLogger)
{
	HRESULT hr = hrSuccess;
	PublishFreeBusy *lpFreeBusy = NULL;
	IMAPITable *lpTable = NULL;
	FBBlock_1 *lpFBblocks = NULL;
	ULONG cValues = 0;
	ECLogger *lpNullLogger = NULL;

	if (!lpLogger) {
		lpNullLogger = new ECLogger_Null();
		lpLogger = lpNullLogger;
	}

	lpLogger->Log(EC_LOGLEVEL_DEBUG, "current time %d", (int)tsStart);

	lpFreeBusy = new PublishFreeBusy(lpSession, lpDefStore, tsStart, ulMonths, lpLogger);
	
	hr = lpFreeBusy->HrInit();
	if (hr != hrSuccess)
		goto exit;

	hr = lpFreeBusy->HrGetResctItems(&lpTable);
	if (hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_INFO, "Error while finding messages for free/busy publish, error code: 0x%08X", hr);
		goto exit;
	}

	hr = lpFreeBusy->HrProcessTable(lpTable, &lpFBblocks, &cValues);
	if(hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_INFO, "Error while finding free/busy blocks, error code: 0x%08X  ", hr);
		goto exit;
	}

	if (cValues == 0) {
		lpLogger->Log(EC_LOGLEVEL_DEBUG, "No messages for free/busy publish");
		goto exit;
	}

	hr = lpFreeBusy->HrMergeBlocks(&lpFBblocks, &cValues);
	if(hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_INFO, "Error while merging free/busy blocks, entries: %d, error code: 0x%08X  ", cValues, hr);
		goto exit;
	}
	lpLogger->Log(EC_LOGLEVEL_DEBUG, "Publishing %d free/busy blocks", cValues);

	hr = lpFreeBusy->HrPublishFBblocks(lpFBblocks, cValues);
	if(hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_INFO, "Error while publishing free/busy blocks, entries: %d, error code: 0x%08X  ", cValues, hr);
		goto exit;
	}
	
exit:
	if(lpTable)
		lpTable->Release();

	if(lpFreeBusy)
		delete lpFreeBusy;

	if(lpFBblocks)
		MAPIFreeBuffer(lpFBblocks);

	if(lpNullLogger)
		lpNullLogger->Release();

	return hr;
}

/** 
 * Class handling free/busy publishing.
 * @todo validate input time & months.
 *
 * @param[in] lpSession 
 * @param[in] lpDefStore 
 * @param[in] tsStart 
 * @param[in] ulMonths 
 * @param[in] lpLogger 
 */
PublishFreeBusy::PublishFreeBusy(IMAPISession *lpSession, IMsgStore *lpDefStore, time_t tsStart, ULONG ulMonths, ECLogger *lpLogger)
{
	m_lpSession = lpSession;
	m_lpDefStore = lpDefStore;
	m_lpLogger = lpLogger;
	m_tsStart = tsStart;
	m_tsEnd = tsStart + (ulMonths * (30*24*60*60));

	UnixTimeToFileTime(m_tsStart, &m_ftStart);
	UnixTimeToFileTime(m_tsEnd , &m_ftEnd);
}

PublishFreeBusy::~PublishFreeBusy()
{
}

/** 
 * Initialize object. Get named properties required for publishing
 * free/busy data.
 * 
 * @return MAPI Error code
 */
HRESULT PublishFreeBusy::HrInit()
{
	HRESULT hr = hrSuccess;
	
	PROPMAP_INIT_NAMED_ID (APPT_STARTWHOLE, 	PT_SYSTIME, PSETID_Appointment, dispidApptStartWhole)
	PROPMAP_INIT_NAMED_ID (APPT_ENDWHOLE, 		PT_SYSTIME, PSETID_Appointment, dispidApptEndWhole)
	PROPMAP_INIT_NAMED_ID (APPT_CLIPEND,		PT_SYSTIME, PSETID_Appointment, dispidClipEnd)
	PROPMAP_INIT_NAMED_ID (APPT_ISRECURRING,	PT_BOOLEAN, PSETID_Appointment, dispidRecurring)
	PROPMAP_INIT_NAMED_ID (APPT_FBSTATUS,		PT_LONG, PSETID_Appointment,	dispidBusyStatus)
	PROPMAP_INIT_NAMED_ID (APPT_RECURRINGSTATE,	PT_BINARY, PSETID_Appointment,	dispidRecurrenceState)
	PROPMAP_INIT_NAMED_ID (APPT_TIMEZONESTRUCT,	PT_BINARY, PSETID_Appointment,	dispidTimeZoneData)
	PROPMAP_INIT (m_lpDefStore)
	;

exit:
	return hr;
}

/** 
 * Create a contents table with items to be published for free/busy
 * times.
 * 
 * @todo rename function 
 * @todo use restriction class to create restrictions
 *
 * @param[out] lppTable Default calendar contents table
 * 
 * @return MAPI Error code
 */
HRESULT PublishFreeBusy::HrGetResctItems(IMAPITable **lppTable)
{
	HRESULT hr = hrSuccess;
	IMAPIFolder *lpDefCalendar = NULL;
	IMAPITable *lpTable = NULL;
	SRestriction *lpsRestrict = NULL;
	SRestriction *lpsRestrictOR = NULL;
	SRestriction *lpsRestrictAND = NULL;
	SRestriction *lpsRestrictSubOR = NULL;
	SRestriction *lpsRestrictNot = NULL;
	SPropValue lpsPropStart;
	SPropValue lpsPropEnd;
	SPropValue lpsPropIsRecc;
	SPropValue lpsPropReccEnd;
		
	hr = HrOpenDefaultCalendar(m_lpDefStore, m_lpLogger, &lpDefCalendar);
	if(hr != hrSuccess)
		goto exit;

	hr = lpDefCalendar->GetContentsTable(0, &lpTable);
	if(hr != hrSuccess)
		goto exit;
	
	CREATE_RESTRICTION(lpsRestrict);
	CREATE_RES_OR(lpsRestrict, lpsRestrict, 4);
	lpsRestrictOR = lpsRestrict->res.resOr.lpRes;
	
	lpsPropStart.ulPropTag = PROP_APPT_STARTWHOLE;
	lpsPropStart.Value.ft = m_ftStart;

	lpsPropEnd.ulPropTag = PROP_APPT_ENDWHOLE;
	lpsPropEnd.Value.ft = m_ftEnd;

	lpsPropIsRecc.ulPropTag = PROP_APPT_ISRECURRING;
	lpsPropIsRecc.Value.b = true;

	lpsPropReccEnd.ulPropTag = PROP_APPT_CLIPEND;
	lpsPropReccEnd.Value.ft = m_ftStart;

	CREATE_RES_AND(lpsRestrict, (&lpsRestrictOR[0]), 2);
	lpsRestrictAND = lpsRestrictOR[0].res.resAnd.lpRes;	
	//ITEM[START] >= START && ITEM[START] <= END;
	DATA_RES_PROPERTY(lpsRestrict, lpsRestrictAND[0], RELOP_GE, PROP_APPT_STARTWHOLE, &lpsPropStart);//item[start]
	DATA_RES_PROPERTY(lpsRestrict, lpsRestrictAND[1], RELOP_LE, PROP_APPT_STARTWHOLE, &lpsPropEnd);//item[start]

	CREATE_RES_AND(lpsRestrict, (&lpsRestrictOR[1]), 2);
	lpsRestrictAND = lpsRestrictOR[1].res.resAnd.lpRes;
	//ITEM[END] >= START && ITEM[END] <= END;
	DATA_RES_PROPERTY(lpsRestrict, lpsRestrictAND[0], RELOP_GE, PROP_APPT_ENDWHOLE, &lpsPropStart);//item[end]
	DATA_RES_PROPERTY(lpsRestrict, lpsRestrictAND[1], RELOP_LE, PROP_APPT_ENDWHOLE, &lpsPropEnd);//item[end]

	CREATE_RES_AND(lpsRestrict, (&lpsRestrictOR[2]), 2);
	lpsRestrictAND = lpsRestrictOR[2].res.resAnd.lpRes;
	//ITEM[START] < START && ITEM[END] > END;
	DATA_RES_PROPERTY(lpsRestrict, lpsRestrictAND[0], RELOP_LT, PROP_APPT_STARTWHOLE, &lpsPropStart);//item[start]
	DATA_RES_PROPERTY(lpsRestrict, lpsRestrictAND[1], RELOP_GT, PROP_APPT_ENDWHOLE, &lpsPropEnd);//item[end]

	CREATE_RES_OR(lpsRestrict,(&lpsRestrictOR[3]),2);
	lpsRestrictSubOR = lpsRestrictOR[3].res.resOr.lpRes;
	
	CREATE_RES_AND(lpsRestrict,(&lpsRestrictSubOR[0]),3);
	lpsRestrictAND = lpsRestrictSubOR[0].res.resAnd.lpRes;
	
	DATA_RES_EXIST(lpsRestrict, lpsRestrictAND[0], PROP_APPT_CLIPEND);
	DATA_RES_PROPERTY(lpsRestrict, lpsRestrictAND[1], RELOP_EQ, PROP_APPT_ISRECURRING, &lpsPropIsRecc);
	DATA_RES_PROPERTY(lpsRestrict, lpsRestrictAND[2], RELOP_GE, PROP_APPT_CLIPEND, &lpsPropReccEnd);

	CREATE_RES_AND(lpsRestrict,(&lpsRestrictSubOR[1]),3);
	lpsRestrictAND = lpsRestrictSubOR[1].res.resAnd.lpRes;

	CREATE_RES_NOT(lpsRestrict,(&lpsRestrictAND[0]));
	lpsRestrictNot = lpsRestrictAND[0].res.resNot.lpRes;

	DATA_RES_EXIST(lpsRestrict, lpsRestrictNot[0], PROP_APPT_CLIPEND);
	DATA_RES_PROPERTY(lpsRestrict, lpsRestrictAND[1], RELOP_LE, PROP_APPT_STARTWHOLE, &lpsPropEnd);
	DATA_RES_PROPERTY(lpsRestrict, lpsRestrictAND[2], RELOP_EQ, PROP_APPT_ISRECURRING, &lpsPropIsRecc);

	
	hr = lpTable->Restrict(lpsRestrict, TBL_BATCH);
	if(hr != hrSuccess)
		goto exit;
	
	*lppTable = lpTable;
	lpTable = NULL;

exit:
	if(lpsRestrict)
		FREE_RESTRICTION(lpsRestrict);

	if(lpTable)
		lpTable->Release();

	if(lpDefCalendar)
		lpDefCalendar->Release();

	return hr;
}

/**
 * Calculates the freebusy blocks from the rows of the table.
 * It also adds the occurrences of the recurrence in the array of blocks.
 *
 * @param[in]	lpTable			restricted mapi table containing the rows
 * @param[out]	lppfbBlocks		array of freebusy blocks
 * @param[out]	lpcValues		number of freebusy blocks in lppfbBlocks array
 *
 * @return		MAPI Error code
 */
HRESULT PublishFreeBusy::HrProcessTable(IMAPITable *lpTable, FBBlock_1 **lppfbBlocks, ULONG *lpcValues)
{
	HRESULT hr = hrSuccess;
	SRowSet *lpRowSet = NULL;
	SPropTagArray *lpsPrpTagArr = NULL;
	OccrInfo *lpOccrInfo = NULL;
	FBBlock_1 *lpfbBlocks = NULL;
	recurrence lpRecurrence;
	ULONG ulFbStatus = 0;

	// @todo make static block and move this when to the table is created
	hr = MAPIAllocateBuffer(CbNewSPropTagArray(7), (void **)&lpsPrpTagArr);
	if(hr != hrSuccess)
		goto exit;

	lpsPrpTagArr->cValues = 7;
	lpsPrpTagArr->aulPropTag[0] = PROP_APPT_STARTWHOLE;
	lpsPrpTagArr->aulPropTag[1] = PROP_APPT_ENDWHOLE;
	lpsPrpTagArr->aulPropTag[2] = PROP_APPT_FBSTATUS;
	lpsPrpTagArr->aulPropTag[3] = PROP_APPT_ISRECURRING;
	lpsPrpTagArr->aulPropTag[4] = PROP_APPT_RECURRINGSTATE;
	lpsPrpTagArr->aulPropTag[5] = PROP_APPT_CLIPEND;
	lpsPrpTagArr->aulPropTag[6] = PROP_APPT_TIMEZONESTRUCT;

	hr = lpTable->SetColumns((LPSPropTagArray)lpsPrpTagArr, 0);
	if(hr != hrSuccess)
		goto exit;

	while (true)
	{
		hr = lpTable->QueryRows(50, 0, &lpRowSet);
		if(hr != hrSuccess)
			goto exit;

		if(lpRowSet->cRows == 0)
			break;
		
		for(ULONG i = 0; i < lpRowSet->cRows ; i++)
		{	
			TIMEZONE_STRUCT ttzInfo = {0};
			
			ulFbStatus = 0;

			if(lpRowSet->aRow[i].lpProps[3].ulPropTag == PROP_APPT_ISRECURRING 
				&& lpRowSet->aRow[i].lpProps[3].Value.b == true)
			{
				if(lpRowSet->aRow[i].lpProps[4].ulPropTag == PROP_APPT_RECURRINGSTATE) 
				{
					hr = lpRecurrence.HrLoadRecurrenceState((char *)(lpRowSet->aRow[i].lpProps[4].Value.bin.lpb),lpRowSet->aRow[i].lpProps[4].Value.bin.cb, 0);
					if(FAILED(hr)) {
						m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error loading recurrence state, error code : 0x%08X", hr);
						continue;
					}

					if (lpRowSet->aRow[i].lpProps[6].ulPropTag == PROP_APPT_TIMEZONESTRUCT)
						ttzInfo = *(TIMEZONE_STRUCT*)lpRowSet->aRow[i].lpProps[6].Value.bin.lpb;

					if (lpRowSet->aRow[i].lpProps[2].ulPropTag == PROP_APPT_FBSTATUS)
						ulFbStatus = lpRowSet->aRow[i].lpProps[2].Value.ul;

					hr = lpRecurrence.HrGetItems( m_tsStart, m_tsEnd, m_lpLogger, ttzInfo, ulFbStatus, &lpOccrInfo, lpcValues);
					if (hr != hrSuccess || !lpOccrInfo) {
						m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error expanding items for recurring item, error code : 0x%08X", hr);
						continue;
					}
				}
			}
			else
			{
				OccrInfo sOccrBlock;
				
				if (lpRowSet->aRow[i].lpProps[0].ulPropTag == PROP_APPT_STARTWHOLE) 
					FileTimeToRTime(&lpRowSet->aRow[i].lpProps[0].Value.ft, &sOccrBlock.fbBlock.m_tmStart);

				if (lpRowSet->aRow[i].lpProps[1].ulPropTag == PROP_APPT_ENDWHOLE) {
				
					FileTimeToRTime(&lpRowSet->aRow[i].lpProps[1].Value.ft, &sOccrBlock.fbBlock.m_tmEnd);
					FileTimeToUnixTime(lpRowSet->aRow[i].lpProps[1].Value.ft, &sOccrBlock.tBaseDate);
				}
				if (lpRowSet->aRow[i].lpProps[2].ulPropTag == PROP_APPT_FBSTATUS) 
					sOccrBlock.fbBlock.m_fbstatus = (FBStatus)lpRowSet->aRow[i].lpProps[2].Value.ul;
				
				hr = HrAddFBBlock(sOccrBlock, &lpOccrInfo, lpcValues);
				if (hr != hrSuccess) {
					m_lpLogger->Log( EC_LOGLEVEL_DEBUG, "Error adding occurrence block to list, error code : 0x%08X", hr);
					goto exit;
				}
			}
	
		}

		if(lpRowSet)
			FreeProws(lpRowSet);
		lpRowSet = NULL;
	}
	
	if (lpcValues != 0) {
		hr = MAPIAllocateBuffer(sizeof(FBBlock_1)* (*lpcValues), (void**)&lpfbBlocks);
		if(hr != hrSuccess)
			goto exit;

		for (ULONG i = 0 ; i < *lpcValues; i++)
			lpfbBlocks[i]  = lpOccrInfo[i].fbBlock;

		*lppfbBlocks = lpfbBlocks;
		lpfbBlocks = NULL;
	}

exit:
	if (lpOccrInfo)
		MAPIFreeBuffer(lpOccrInfo);

	if (lpsPrpTagArr)
		MAPIFreeBuffer(lpsPrpTagArr);

	if (lpRowSet)
		FreeProws(lpRowSet);

	return hr;
}

/** 
 * Merge overlapping free/busy blocks.
 * 
 * @param[in,out] lppfbBlocks In: generated blocks, Out: merged blocks
 * @param[in,out] lpcValues Number of blocks in lppfbBlocks
 * 
 * @return MAPI Error code
 */
HRESULT PublishFreeBusy::HrMergeBlocks(FBBlock_1 **lppfbBlocks, ULONG *lpcValues)
{
	HRESULT hr = hrSuccess;
	FBBlock_1 *lpFbBlocks = NULL;
	ULONG cValues = *lpcValues;
	ULONG ulLevel = 0;
	time_t tsLastTime = 0;
	TSARRAY sTsitem = {0,0,0};
	std::map<time_t , TSARRAY> mpTimestamps;
	std::map<time_t , TSARRAY>::iterator iterTs;
	std::vector <ULONG> vctStatus;
	std::vector <ULONG>::iterator iterStatus;
	std::vector <FBBlock_1> vcFBblocks;
	std::vector <FBBlock_1>::iterator iterVcBlocks;
	time_t tTemp = 0;

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Input blocks %ul", cValues);

	lpFbBlocks = *lppfbBlocks;
	for (ULONG i = 0; i< cValues; i++)
	{
		sTsitem.ulType = START_TIME;
		sTsitem.ulStatus = lpFbBlocks[i].m_fbstatus;
		sTsitem.tsTime = lpFbBlocks[i].m_tmStart;
		RTimeToUnixTime(sTsitem.tsTime, &tTemp);

		// @note ctime adds \n character
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Blocks start %s", ctime(&tTemp));

		mpTimestamps[sTsitem.tsTime] = sTsitem;
		
		sTsitem.ulType = END_TIME;
		sTsitem.ulStatus = lpFbBlocks[i].m_fbstatus;
		sTsitem.tsTime = lpFbBlocks[i].m_tmEnd;

		mpTimestamps[sTsitem.tsTime] = sTsitem;
	}
	
	for (iterTs = mpTimestamps.begin(); iterTs != mpTimestamps.end(); iterTs++)
	{
		FBBlock_1 fbBlockTemp;

		sTsitem = iterTs->second;
		switch(sTsitem.ulType)
		{
		case START_TIME:
			if (ulLevel != 0 && tsLastTime != sTsitem.tsTime)
			{
				std::sort(vctStatus.begin(),vctStatus.end());
				fbBlockTemp.m_tmStart = tsLastTime;
				fbBlockTemp.m_tmEnd = sTsitem.tsTime;
				fbBlockTemp.m_fbstatus = (enum FBStatus)(vctStatus.size()> 0 ? vctStatus.back(): 0);// sort it to get max of status
				if(fbBlockTemp.m_fbstatus != 0)
					vcFBblocks.push_back(fbBlockTemp);
			}
			ulLevel++;
			vctStatus.push_back(sTsitem.ulStatus);
			tsLastTime = sTsitem.tsTime;
			break;
		case END_TIME:
			if(tsLastTime != sTsitem.tsTime)
			{
				std::sort(vctStatus.begin(),vctStatus.end());// sort it to get max of status
				fbBlockTemp.m_tmStart = tsLastTime;
				fbBlockTemp.m_tmEnd = sTsitem.tsTime;
				fbBlockTemp.m_fbstatus = (enum FBStatus)(vctStatus.size()> 0 ? vctStatus.back(): 0);
				if(fbBlockTemp.m_fbstatus != 0)
					vcFBblocks.push_back(fbBlockTemp);
			}
			ulLevel--;
			if(!vctStatus.empty()){
				iterStatus = std::find(vctStatus.begin(),vctStatus.end(),sTsitem.ulStatus);
				if(iterStatus != vctStatus.end())
					vctStatus.erase(iterStatus);
			}
			tsLastTime = sTsitem.tsTime;
			break;
		}
	}

	// Free previously allocated memory
	if(lppfbBlocks && *lppfbBlocks) {
		MAPIFreeBuffer(*lppfbBlocks);
		*lppfbBlocks = NULL;
	}

	hr = MAPIAllocateBuffer(sizeof(FBBlock_1) * vcFBblocks.size(), (void **)&lpFbBlocks);
	if (hr != hrSuccess)
		goto exit;
	iterVcBlocks = vcFBblocks.begin();

	for(ULONG i = 0; iterVcBlocks != vcFBblocks.end(); i++, iterVcBlocks++)
		lpFbBlocks[i] = *iterVcBlocks;		

	*lppfbBlocks = lpFbBlocks;
	*lpcValues = vcFBblocks.size();

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Output blocks %d", *lpcValues);

exit:
	return hr;
}

/** 
 * Save free/busy blocks in public for current user
 * 
 * @param[in] lpfbBlocks new blocks to publish
 * @param[in] cValues number of blocks to publish
 * 
 * @return MAPI Error code
 */
HRESULT PublishFreeBusy::HrPublishFBblocks(FBBlock_1 *lpfbBlocks, ULONG cValues)
{
	HRESULT hr = hrSuccess;
	ECFreeBusyUpdate *lpFBUpdate = NULL;
	IMessage *lpMessage = NULL;
	IMsgStore *lpPubStore = NULL;
	LPSPropValue lpsPrpUsrMEid = NULL;
	time_t tsStart = 0;
	
	hr = HrOpenECPublicStore(m_lpSession, &lpPubStore);
	if(hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(m_lpDefStore, PR_MAILBOX_OWNER_ENTRYID, &lpsPrpUsrMEid);
	if(hr != hrSuccess)
		goto exit;

	hr = GetFreeBusyMessage(m_lpSession, lpPubStore, m_lpDefStore, lpsPrpUsrMEid[0].Value.bin.cb, (LPENTRYID)lpsPrpUsrMEid[0].Value.bin.lpb, true, &lpMessage);
	if(hr != hrSuccess)
		goto exit;

	hr = ECFreeBusyUpdate::Create(lpMessage,&lpFBUpdate);
	if(hr != hrSuccess)
		goto exit;
	
	hr = lpFBUpdate->ResetPublishedFreeBusy();
	if(hr != hrSuccess)
		goto exit;

	hr = lpFBUpdate->PublishFreeBusy(lpfbBlocks, cValues);
	if(hr != hrSuccess)
		goto exit;

	FileTimeToUnixTime(m_ftStart, &tsStart);
	// @todo use a "start of day" function?
	tsStart = tsStart - 86400; // 24*60*60 = 86400 include current day.
	UnixTimeToFileTime(tsStart, &m_ftStart);

	hr = lpFBUpdate->SaveChanges(m_ftStart, m_ftEnd);
	if( hr != hrSuccess)
		goto exit;

exit:
	if(lpsPrpUsrMEid)
		MAPIFreeBuffer(lpsPrpUsrMEid);

	if(lpFBUpdate)
		lpFBUpdate->Release();

	if(lpMessage)
		lpMessage->Release();

	if(lpPubStore)
		lpPubStore->Release();

	return hr;

}
