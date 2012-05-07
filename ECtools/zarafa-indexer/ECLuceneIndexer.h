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

#ifndef ECLUCENEINDEXER_H
#define ECLUCENEINDXER_H

#include <string>

#include <CLucene.h>

#include <ECUnknown.h>

#include "ECChangeData.h"
#include "zarafa-indexer.h"
#include "archiver-common.h"

class ECLuceneAccess;
class ECLuceneIndexerAttachment;
class ECSerializer;

/**
 * Main class to perform indexing by CLucene
 */
class ECLuceneIndexer : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECLuceneIndexer must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	lpLuceneAccess
	 * @param[in]	lpAdminSession		Session needed to process stubs
	 */
	ECLuceneIndexer(ECThreadData *lpThreadData, ECLuceneAccess *lpLuceneAccess, IMAPISession *lpAdminSession);

public:
	/**
	 * Create new ECLuceneIndexer object.
	 *
	 * @note Creating a new ECLuceneIndexer object must always occur through this function.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to the ECThreadData object.
	 * @param[in]	lpLuceneAccess
	 *					Reference to the ECLuceneAccess object.
	 * @param[in]	lpMsgStore
	 *					Reference to the IMsgStore object for the store which will be indexed.
	 * @param[in]	lpAdminSession
	 * 					The MAPI admin session needed to process stubs.
	 * @param[out]	lppIndexer
	 *					The created ECLuceneIndexer object.
	 * @return HRESULT
	 */
	static HRESULT Create(ECThreadData *lpThreadData, ECLuceneAccess *lpLuceneAccess, IMsgStore *lpMsgStore, IMAPISession *lpAdminSession, ECLuceneIndexer **lppIndexer);

	/**
	 * Destructor
	 */
	~ECLuceneIndexer();

	/**
	 * Update store index with the provided creations and deletions
	 *
	 * @param[in]	listCreateEntries
	 *					List containing all sourceid_t entries which should be added to the index.
	 * @param[in]	listChangeEntries
	 *					List containing all sourceid_t entries which should be changed in the index.
	 * @param[in]	listDeleteEntries
	 *					List containing all sourceid_t entries which should be deleted from the index.
	 * @return HRESULT
	 */
	HRESULT IndexEntries(sourceid_list_t &listCreateEntries, sourceid_list_t &listChangeEntries, sourceid_list_t &listDeleteEntries);

private:
	/**
	 * Process all message deletions from index
	 *
	 * @param[in]   listSourceId
	 *					List containing all sourceid_t entries which should be deleted from the index.
	 * @return HRESULT
	 */
	HRESULT IndexDeleteEntries(sourceid_list_t &listSourceId);

	/**
	 * Process all message creations for index using the IStream
	 *
	 * @param[in]	listSourceId
	 *					List containing all sourceid_t entries which should be added to the index.
	 * @return HRESULT
	 */
	HRESULT IndexStreamEntries(sourceid_list_t &listSourceId);
	/**
	 * Process all properties from message and add all data into the CLucene Document
	 *
	 * @param[in]	lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	cValues
	 *					The number of SPropValue elements in lpProps.
	 * @param[in]	lpProps
	 *					Array of SPropValue elements containing all message properties required for indexing.
	 * @param[in]	lpMessage
	 *					Reference to IMessage object which is being indexed.
	 * @param[in]	bDefaultField
	 *					True if all parsed data should be written to the default field, rather then putting
	 *					each property into its own dedicated field. (default FALSE)
	 * @return HRESULT
	 */
	HRESULT ParseDocument(lucene::document::Document *lpDocument, ULONG cValues, LPSPropValue lpProps, IMessage *lpMessage, BOOL bDefaultField = FALSE);

	/**
	 * Process all properties from stream and add all data into the CLucene Document
	 *
	 * @param[in]	lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	cValues
	 *					The number of SPropValue elements in lpProps.
	 * @param[in]	lpProps
	 *					Array of SPropValue elements containing all message properties required for indexing.
	 * @param[in]	lpSerializer
	 *					Serializer containing the stream with the message which is being indexed
	 * @param[in]	bDefaultField
	 *					True if all parsed data should be written to the default field, rather then putting
	 *					each property into its own dedicated field. (default FALSE)
	 * @return HRESULT
	 */
	HRESULT ParseStream(lucene::document::Document *lpDocument, ULONG cValues, LPSPropValue lpProps, ECSerializer *lpSerializer, BOOL bDefaultField = FALSE);

	/**
	 * Handle message attachments
	 *
	 * This function will check if attachment indexing has been enabled, and if so will
	 * send the message to ECLuceneIndexer::ParseAttachments().
	 *
	 * @param[in]	lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	lpEntryId
	 *					EntryID of the message for which we want the attachment, might be NULL if lpOrigMessage is provided.
	 * @param[in]	lpOrigMessage
	 *					Reference to IMessage object, might be NULL if lpEntryId is provided.
	 * @return HRESULT
	 */
	HRESULT ParseAttachments(lucene::document::Document *lpDocument, LPSPropValue lpEntryId, IMessage *lpOrigMessage);

	/**
	 * Handle message attachments from stream
	 *
	 * This function will check if attachment indexing has been enabled, and if so will
	 * send the stream to ECLuceneIndexer::ParseAttachments().
	 *
	 * @param[in]   lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	lpSerializer
	 *					Serializer containing the stream from which attachments will be read.
	 * @return HRESULT
	 */
	HRESULT ParseStreamAttachments(lucene::document::Document *lpDocument, ECSerializer *lpSerializer);

	/**
	 * Process all properties from message and add all data into the CLucene Document
	 *
	 * @param[in]	lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	cValues
	 *					The number of SPropValue elements in lpProps.
	 * @param[in]	lpProps
	 *					Array of SPropValue elements containing all message properties required for indexing.
	 * @param[in]	bDefaultField
	 *					True if all parsed data should be written to the default field, rather then putting
	 *					each property into its own dedicated field. (default FALSE)
	 * @return HRESULT
	 */
	HRESULT ParseStub(lucene::document::Document *lpDocument, ULONG cValues, LPSPropValue lpProps, BOOL bDefaultField);

	/**
	 * Add given property to the document as new field
	 *
	 * @param[in]   lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	lpProp
	 *					Property which should be added to the document
	 * @param[in]	lpNameID
	 *					Pointer to named property identifier for lpProp when a named property is passed in lpProp
	 * @param[in]	bDefaultField
	 *					True if all parsed data should be written to the default field, rather then putting
	 *					each property into its own dedicated field. (default FALSE)
	 * @return HRESULT
	 */
	HRESULT AddPropertyToDocument(lucene::document::Document *lpDocument, LPSPropValue lpProp, LPMAPINAMEID lpNameID, BOOL bDefaultField);

	/**
	 * Get the name of a dynamic field
	 *
	 * This is the name of a field that is not in the default field list (eg subject). The name is determined as
	 * follows:
	 *
	 * - If the property ID is under 0x8500, the field name is simply the ID in hex (0x8XXX)
	 * - If the property ID is 0x8500 or higher, the field name is the named GUID in hex, followed by a period '.', followed by the ID in hex or the string itself
	 *
	 * @param lpProp Property value to represent
	 * @param lpNameID Named property from GetNamesFromIDs or NULL of not applicable
	 * @return String representing the property tag
	 */
	std::wstring GetDynamicFieldName(LPSPropValue lpProp, LPMAPINAMEID lpNameID);
	

private:
	ECThreadData *m_lpThreadData;
	ECLuceneAccess *m_lpLuceneAccess;
	ECLuceneIndexerAttachment *m_lpIndexerAttach;

	IMsgStore *m_lpMsgStore;
	IMAPISession *m_lpAdminSession;

	LPSPropTagArray m_lpIndexedProps;

	friend class ECLuceneIndexerAttachment;
};

#endif /* ECLUCENEINDXER_H */
