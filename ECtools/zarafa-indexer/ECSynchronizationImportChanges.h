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

#ifndef ECINDEXERIMPORTCHANGES_H
#define ECINDEXERIMPORTCHANGES_H

#include <ECUnknown.h>
#include <IECImportAddressbookChanges.h>
#include <IECImportContentsChanges.h>

#include "ECChangeData.h"
#include "zarafa-indexer.h"

/**
 * Main importer class for synchronization classes
 *
 * This class offers the interfaces:
 *     IID_IECImportAddressbookChanges
 *     IID_IExchangeImportHierarchyChanges
 *     IID_IExchangeImportContentsChanges
 *
 * With these intefaces all required forms of synchronization for
 * the indexing process can be offered. The main synchronization
 * handler can be found in ECIndexerSyncer.
 */
class ECSynchronizationImportChanges : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * Objects of ECSynchronizationImportChanges must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	lpFolderSourceKey
	 * @param[in]	lpChanges
	 */
	ECSynchronizationImportChanges(ECThreadData *lpThreadData, SBinary *lpFolderSourceKey,
								   ECChangeData *lpChanges);

public:
	/**
	 * Create new ECSynchronizationImportChanges and open it with given interface.
	 *
	 * @note Creating a new ECSynchronizationImportChanges object must always occur through this function.
	 *		 Instead of returning the ECSynchronizationImportChanges object itself, it will directly
	 *		 return the proper interface provided by QueryInterface().
	 *
	 * @param[in]	lpThreadData
	 *					Reference to the ECThreadData object.
	 * @param[in]	lpFolderSourceKey
	 *					Source key of the folder which will be synchronized by importer.
	 * @param[in]	lpChanges
	 *					Reference to sync_change_t object with the create/delete lists.
	 * @param[in]	refiid
	 *					Interface which should be returned by this function.
	 * @param[out]	lppIndexerImportChanges
	 *					Interface of the type as indicated by refiid.
	 * @return HRESULT
	 */
	static  HRESULT Create(ECThreadData *lpThreadData, SBinary *lpFolderSourceKey,
						   ECChangeData *lpChanges, REFIID refiid,
						   LPVOID *lppIndexerImportChanges);

	/**
	 * Destructor
	 */
	virtual ~ECSynchronizationImportChanges();

	/**
	 * Return requested interface on this object
	 *
	 * @param[in]	refiid
	 *					The requested interface type.
	 * @param[out]	lppvoid
	 *					The requested interface reference.
	 * @return HRESULT
	 */
	virtual HRESULT QueryInterface(REFIID refiid, LPVOID *lppvoid);

	/**
	 * Convert HRESULT into LPMAPIERROR
	 *
	 * @param[in]	hResult
	 *					The HRESULT value to convert
	 * @param[in]	ulFlags
	 *					Optional flags
	 * @param[out]	lppMAPIError
	 *					Reference to the LPMAPIERROR object with information about the hResult.
	 * @return HRESULT
	 */
	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);

	/**
	 * Configure importer
	 *
	 * This function will do nothing.
	 *
	 * @param[in]	lpStream
	 *					Reference to synchronization stream
	 * @param[in]	ulFlags
	 *					Optional flags for configuration
	 * @return HRESULT
	 */
	virtual HRESULT Config(LPSTREAM lpStream, ULONG ulFlags);

	/**
	 * Update synchronization stream
	 *
	 * This function will do nothing.
	 *
	 * @param[in]	lpStream
	 *					Reference to synchronization stream
	 * @return HRESULT
	 */
	virtual HRESULT UpdateState(LPSTREAM lpStream);

	/**
	 * Import Address Book change
	 *
	 * This function is called by exporter for all newly created or changed address book items
	 *
	 * @param[in]	type
	 *					Address Book object type (MAPI_MAILUSER, MAPI_DISTLIST, ...).
	 * @param[in]	cbObjId
	 *					Number of bytes in the lpObjId parameter.
	 * @param[in]	lpObjId
	 *					Array of bytes containing the entryid of the Address Book object.
	 * @return HRESULT
	 */
	virtual HRESULT ImportABChange(ULONG type, ULONG cbObjId, LPENTRYID lpObjId);

	/**
	 * Import Address Book deletion
	 *
	 * This function is called by exporter for deleted address book items
	 *
	 * @param[in]	type
	 *					Address Book object type (MAPI_MAILUSER, MAPI_DISTLIST, ...).
	 * @param[in]	cbObjId
	 *					Number of bytes in the lpObjId parameter.
	 * @param[in]	lpObjId
	 *					Array of bytes containing the entryid of the Address Book object.
	 * @return HRESULT
	 */
	virtual HRESULT ImportABDeletion(ULONG type, ULONG cbObjId, LPENTRYID lpObjId);

	/**
	 * Import Folder change
	 *
	 * This function is called by exporter for all newly created or changed folders.
	 *
	 * @param[in]	cValue
	 *					Number of SPropValue objects in the lpPropArray array.
	 * @param[in]	lpPropArray
	 *					Array of SPropValue elements containing the properties of the folder.
	 * @return HRESULT
	 */
	virtual HRESULT ImportFolderChange(ULONG cValue, LPSPropValue lpPropArray);

	/**
	 * Import folder deletions
	 *
	 * This function is called by exporter for deleted folders.
	 *
	 * @param[in]	ulFlags
	 *					Flags for deletion action.
	 * @param[in]	lpSourceEntryList
	 *					List of entryids for all folders which must be deleted.
	 * @return HRESULT
	 */
	virtual HRESULT ImportFolderDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList);

	/**
	 * Import message change
	 *
	 * This function is called by exporter for all newly created or changed messages.
	 *
	 * @param[in]	cValue
	 *					Number of SPropValue objects in the lpPropArray array.
	 * @param[in]	lpPropArray
	 *					Array of SPropValue elements containing the properties of the message.
	 * @param[in]	ulFlags
	 *					Optional flags for importing message change
	 * @param[out]	lppMessage
	 *					Reference to constructed IMessage object based on information from lpPropArray.
	 * @return HRESULT
	 */
	virtual HRESULT ImportMessageChange(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPMESSAGE * lppMessage);

	/**
	 * Import message deletions
	 *
	 * This function is called by exporter for deleted messages.
	 *
	 * @param[in]	ulFlags
	 *					Flags for deletion action.
	 * @param[in]	lpSourceEntryList
	 *					List of entryids for all messages which must be deleted.
	 * @return HRESULT
	 */
	virtual HRESULT ImportMessageDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList);

	/**
	 * Import message read status changes
	 *
	 * This function will do nothing
	 *
	 * @param[in]	cElements
	 *					Number of READSTATE elements in the lpReadState parameter.
	 * @param[in]	lpReadState
	 *					Array of READSTATE elements containing the read status for all messages.
	 * @return HRESULT
	 */
	virtual HRESULT ImportPerUserReadStateChange(ULONG cElements, LPREADSTATE lpReadState);

	/**
	 * Import message move to different folder
	 *
	 * This function will add a message deletion from source folder, and a creation for the destination
	 * folder for this message. CLucene doesn't support changing a single document field, which means
	 * that a full delete & create must be performed to index the message move.
	 *
	 * @param[in]	cbSourceKeySrcFolder
	 *					Number of bytes in the pbSourceKeySrcFolder parameter.
	 * @param[in]	pbSourceKeySrcFolder
	 *					Array of bytes containing the sourcekey of the source folder.
	 * @param[in]	cbSourceKeySrcMessage
	 *					 Number of bytes in the pbSourceKeySrcMessage parameter.
	 * @param[in]	pbSourceKeySrcMessage
	 *					Array of bytes containing the sourcekey of the source message.
	 * @param[in]	cbPCLMessage
						Number of bytes in the pbPCLMessage parameter.
	 * @param[in]	pbPCLMessage
	 *					Arrat of bytes containing the PCL Message
	 * @param[in]	cbSourceKeyDestMessage
	 *					Number of bytes in the pbSourceKeyDestMessage parameter.
	 * @param[in]	pbSourceKeyDestMessage
	 *					Array of bytes containing the sourcekey of the destination message.
	 * @param[in]	cbChangeNumDestMessage
	 *					Number of bytes in the pbChangeNumDestMessage parameter.
	 * @param[in]	pbChangeNumDestMessage
	 *					Array of bytes containing the change number destination message.
	 * @return HRESULT
	 */
	virtual HRESULT ImportMessageMove(ULONG cbSourceKeySrcFolder, BYTE FAR * pbSourceKeySrcFolder,
									  ULONG cbSourceKeySrcMessage, BYTE FAR * pbSourceKeySrcMessage,
									  ULONG cbPCLMessage, BYTE FAR * pbPCLMessage,
									  ULONG cbSourceKeyDestMessage, BYTE FAR * pbSourceKeyDestMessage,
									  ULONG cbChangeNumDestMessage, BYTE FAR * pbChangeNumDestMessage);

	/**
	 * Configure importer to make use if syncstream
	 *
	 * This function will do nothing.
	 *
	 * @param[in]	lpStream
	 *					Reference to synchronization stream
	 * @param[in]	ulFlags
	 *					Optional flags for configuration
	 * @param[in]	cValuesConversion
	 *					Number of SPropValue entries in lpPropArrayConversion array
	 * @param[in]	lpPropArrayConversion
	 *					Array of SPropValue entries
	 * @return HRESULT
	 */ 
	virtual HRESULT ConfigForConversionStream(LPSTREAM lpStream, ULONG ulFlags,
											  ULONG cValuesConversion, LPSPropValue lpPropArrayConversion);
	/**
	 * Import changes by providing a stream in which messages will be stored
	 *
	 * Read information from message change and download changes as part of a stream.
	 *
	 * @param[in]	cValue
	 *					Number of SPropValue objects in the lpPropArray array.
	 * @param[in]	lpPropArray
	 *					Array of SPropValue elements containing the properties of the message.
	 * @param[in]	ulFlags
	 *					Optional flags for importing message change
	 * @param[out]	lppstream
	 *					Stream to which the message was written
	 * @return HRESULT
	 */
	virtual HRESULT ImportMessageChangeAsAStream(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPSTREAM *lppstream);

	/**
	 * Set the message interface to be used when opening messages.
	 *
	 * This has no meaning as the data is never written to an actual obejct in
	 * out implementation.
	 *
	 * @param[in]	refiid
	 * 					The IID of the interface to use.
	 * @return HRESUL
	 */
	virtual HRESULT SetMessageInterface(REFIID refiid);

private:
	/**
	 * Create sourceid_t object containing all provided sourcekey information.
	 *
	 * @param[in]	ulProps
	 *					Number of entries in lpProps array
	 * @param[in]	lpProps
	 *					Array of SPropValue entries
	 * @param[out]	lppSourceId
	 *					Reference to the created sourceid_t object.
	 * @return HRESULT
	 */
	HRESULT CreateSourceId(ULONG ulProps, LPSPropValue lpProps, sourceid_t **lppSourceId);

	/**
	 * IECImportAddressbookChanges interface
	 */
	class xECImportAddressbookChanges : public IECImportAddressbookChanges {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, LPVOID *lppInterface);

		// IECImportAddressbookChanges
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
		virtual HRESULT __stdcall Config(LPSTREAM lpStream, ULONG ulFlags);
		virtual HRESULT __stdcall UpdateState(LPSTREAM lpStream);
		virtual HRESULT __stdcall ImportABChange(ULONG type, ULONG cbObjId, LPENTRYID lpObjId);
		virtual HRESULT __stdcall ImportABDeletion(ULONG type, ULONG cbObjId, LPENTRYID lpObjId);
	} m_xECImportAddressbookChanges;

	/**
	 * IExchangeImportHierarchyChanges interface
	 */
	class xExchangeImportHierarchyChanges : public IExchangeImportHierarchyChanges {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, LPVOID *lppInterface);

		// IExchangeImportHierarchyChanges
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
		virtual HRESULT __stdcall Config(LPSTREAM lpStream, ULONG ulFlags);
		virtual HRESULT __stdcall UpdateState(LPSTREAM lpStream);
		virtual HRESULT __stdcall ImportFolderChange(ULONG cValue, LPSPropValue lpPropArray);
		virtual HRESULT __stdcall ImportFolderDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList);
	} m_xExchangeImportHierarchyChanges;

	/**
	 * IExchangeImportContentsChanges interface
	 */
	class xExchangeImportContentsChanges : public IExchangeImportContentsChanges {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, LPVOID *lppInterface);

		// IExchangeImportContentsChanges
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
		virtual HRESULT __stdcall Config(LPSTREAM lpStream, ULONG ulFlags);
		virtual HRESULT __stdcall UpdateState(LPSTREAM lpStream);
		virtual HRESULT __stdcall ImportMessageChange(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPMESSAGE * lppMessage);
		virtual HRESULT __stdcall ImportMessageDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList);
		virtual HRESULT __stdcall ImportPerUserReadStateChange(ULONG cElements, LPREADSTATE lpReadState);
		virtual HRESULT __stdcall ImportMessageMove(ULONG cbSourceKeySrcFolder, BYTE FAR * pbSourceKeySrcFolder,
													ULONG cbSourceKeySrcMessage, BYTE FAR * pbSourceKeySrcMessage,
													ULONG cbPCLMessage, BYTE FAR * pbPCLMessage,
													ULONG cbSourceKeyDestMessage, BYTE FAR * pbSourceKeyDestMessage,
													ULONG cbChangeNumDestMessage, BYTE FAR * pbChangeNumDestMessage);
	} m_xExchangeImportContentsChanges;

	class xExchangeImportContentsChanges2 : public IECImportContentsChanges {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, LPVOID *lppInterface);

		// IExchangeImportContentsChanges	
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
		virtual HRESULT __stdcall Config(LPSTREAM lpStream, ULONG ulFlags);
		virtual HRESULT __stdcall UpdateState(LPSTREAM lpStream);
		virtual HRESULT __stdcall ImportMessageChange(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPMESSAGE * lppMessage);
		virtual HRESULT __stdcall ImportMessageDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList);
		virtual HRESULT __stdcall ImportPerUserReadStateChange(ULONG cElements, LPREADSTATE lpReadState);
		virtual HRESULT __stdcall ImportMessageMove(ULONG cbSourceKeySrcFolder, BYTE FAR * pbSourceKeySrcFolder,
													ULONG cbSourceKeySrcMessage, BYTE FAR * pbSourceKeySrcMessage,
													ULONG cbPCLMessage, BYTE FAR * pbPCLMessage,
													ULONG cbSourceKeyDestMessage, BYTE FAR * pbSourceKeyDestMessage,
													ULONG cbChangeNumDestMessage, BYTE FAR * pbChangeNumDestMessage);

		// IExchangeImportContentsChanges2
		virtual HRESULT __stdcall ConfigForConversionStream(LPSTREAM lpStream, ULONG ulFlags,
															ULONG cValuesConversion, LPSPropValue lpPropArrayConversion);
		virtual HRESULT __stdcall ImportMessageChangeAsAStream(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPSTREAM *lppstream);
		virtual HRESULT __stdcall SetMessageInterface(REFIID refiid);
	} m_xExchangeImportContentsChanges2;

private:
	ECThreadData *m_lpThreadData;
	SPropValue m_sParentSourceKey;
	ECChangeData *m_lpChanges;
};

#endif /* ECINDEXERIMPORTCHANGES_H */
