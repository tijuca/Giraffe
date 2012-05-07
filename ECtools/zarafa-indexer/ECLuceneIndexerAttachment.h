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

#ifndef ECLUCENEINDEXERATTACHMENT_H
#define ECLUCENEINDEXERATTACHMENT_H

#include <string>
#include <CLucene.h>
#include <ECUnknown.h>
#include "zarafa-indexer.h"
#include "tstring.h"

class ECLuceneIndexer;

/**
 * Message Attachment indexer
 */
class ECLuceneIndexerAttachment : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECLuceneIndexerAttachment must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	lpIndexer
	 */
	ECLuceneIndexerAttachment(ECThreadData *lpThreadData, ECLuceneIndexer *lpIndexer);

public:
	/**
	 * Create new ECLuceneIndexerAttachment object
	 *
	 * @note Creating a new ECLuceneIndexerAttachment object must always occur through this function.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to ECThreadData object.
	 * @param[in]	lpIndexer
	 *					Reference to ECLuceneIndexer object.
	 * @param[out]	lppIndexerAttach
	 *					Reference to the created ECLuceneIndexerAttachment object.
	 * @return HRESULT
	 */
	static HRESULT Create(ECThreadData *lpThreadData, ECLuceneIndexer *lpIndexer, ECLuceneIndexerAttachment **lppIndexerAttach);

	/**
	 * Destructor
	 */
	~ECLuceneIndexerAttachment();
	/**
	 * Parse all attachments for a certain message and add all fields into the given Document.
	 *
	 * @param[in]	lpDocument
	 *					Lucene Document to which all parsed data should be written to
	 * @param[in]	lpMessage
	 *					Reference to IMessage object for which all attachments should be indexed.
	 * @return HRESULT
	 */
	HRESULT ParseAttachments(lucene::document::Document *lpDocument, IMessage *lpMessage);

	/**
	 * Parse all attachments for a certain message and add all fields into the given Document.
	 *
	 * @param[in]   lpDocument
	 *					Lucene Document to which all parsed data should be written to
	 * @param[in]	lpSerializer
	 *					Serializer containing the stream with all attachments which should be indexed
	 * @return HRESULT
	 */
	HRESULT ParseAttachments(lucene::document::Document *lpDocument, ECSerializer *lpSerializer);

private:
	/**
	 * Check if given attachment has been cached by ECLucene
	 *
	 * This function will try to obtain the Single Instance ID for the attachment,
	 * and will check with ECLucene to see if the attachment has previously been parsed.
	 * If that is the case, the cache should be used rather then parsing it again.
	 *
	 * @param[in]	lpAttach
	 *					Reference to IAttach object which should be parsed.
	 * @param[out]	lpstrAttachData
	 *					String containing the plain-text version of the attachment.
	 * @return HRESULT
	 */
	HRESULT GetCachedAttachment(IAttach *lpAttach, std::wstring *lpstrAttachData);

	/**
	 * Add given parsed attachment data to ECLucene
	 *
	 * After the attachment has been parsed the result should be given to ECLucene
	 * for caching so it won't need to be parsed again.
	 *
	 * @param[in]	lpAttach
	 *					Reference to IAttach object which has been parsed.
	 * @param[in]	strAttachData
	 *					String containing the plain-text version of the attachment.
	 * @return HRESULT
	 */
	HRESULT SetCachedAttachment(IAttach *lpAttach, std::wstring &strAttachData);

	/**
	 * Copy single block from stream to parser
	 *
	 * @param[in]	lpStream
	 *					Reference to IStream object which should be read.
	 * @param[in]	ulFpWrite
	 *					File descriptor with write access to parser.
	 * @param[out]	lpulSize
	 *					Number of bytes actually written to parser. Always returned despite return value.
	 * @return HRESULT
	 * @retval hrSuccess Block of bytes written to parser, or end of attachment, or parser command stopped parsing early
	 * @retval MAPI_E_CALL_FAILED Error during write to parser process
	 */
	HRESULT CopyBlockToParser(IStream *lpStream, int ulFpWrite, ULONG *lpulSize);

	/**
	 * Copy single block from parser to std::wstring
	 *
	 * @param[in]	ulFpRead
	 *					File descriptor with read access from parser.
	 * @param[out]	strInput
	 *					Data read from parser.
	 * @return HRESULT
	 */
	HRESULT CopyBlockFromParser(int ulFpRead, std::wstring *strInput);

	/**
	 * Copy all data from stream to parser
	 *
	 * This will copy all data from IStream object into the parser by
	 * making multiple calls to CopyBlockToParser().  When data is
	 * ready from the parser, read the data from the pipe into
	 * strInput.
	 *
	 * Note: This function always closes the given pipe filedescriptors.
	 *
	 * @param[in]	lpStream
	 *					Reference to IStream object which should be read.
	 * @param[in]	ulFpWrite
	 *					File descriptor with write access to parser. Closed on return.
	 * @param[in]	ulFpRead
	 *					File descriptor with read access from parser. Closed on return.
	 * @param[out]	strInput
	 *					Data read from parser.
	 * @return HRESULT
	 */
	HRESULT CopyStreamToParser(IStream *lpStream, int ulFpWrite, int ulFpRead, std::wstring *strInput);

	/**
	 * Execute Shell command to start parser for given attachment
	 *
	 * @param[in]	strFilename
	 *					Filename of the attachment
	 * @param[in]	strCommand
	 *					Shell command which must be executed
	 * @param[in]	lpStream
	 *					Reference to IStream object which should be parsed.
	 * @param[out]	lpstrParsed
	 *					Data read from parser.
 	 * @return HRESULT	 
	 */
	HRESULT ParseAttachmentCommand(tstring &strFilename, std::string &strCommand, IStream *lpStream, std::wstring *lpstrParsed);
	/**
	 * Handler for attachments which were attached with ATTACH_EMBEDDED_MSG
	 *
	 * This will recursively call back into ECLuceneIndexer to parse the message and possible attachments
	 *
	 * @param[in]	lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	lpAttach
	 *					Reference to IAttach object which contains the attachment which should be parsed.
	 * @param[in]	ulProps
	 *					The number of SPropValue elements in lpProps.
	 * @param[in]	lpProps
	 *					Array of SPropValue elements containing all Attachment properties required for indexing.
	 * @return HRESULT
	 */
	HRESULT ParseEmbeddedAttachment(lucene::document::Document *lpDocument, IAttach *lpAttach, ULONG ulProps, LPSPropValue lpProps);

	/**
	 * Handler for attachments which were attached with ATTACH_EMBEDDED_MSG
	 *
	 * This will recursively call back into ECLuceneIndexer to parse the message and possible attachments
	 *
	 * @param[in]	lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 *	@param[in]	lpSerializer
	 *					Reference to ECSerializer object which contains the stream which points to the attachment to index
	 * @return HRESULT
	 */
	HRESULT ParseEmbeddedAttachment(lucene::document::Document *lpDocument, ECSerializer *lpSerializer);

	/**
	 * Handler for attachments which were attached with ATTACH_BY_VALUE
	 *
	 * This will create a Shell command which will be executed by ParseAttachmentCommand() after which it will
	 * send all attachment data into the stream for that command.
	 *
	 * @param[in]	lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	lpStream
	 *					Reference to IStream object which contains the stream with all attachment data
	 * @param[in]	strMimeTag
	 *					MIME tag of attachment
	 * @param[in]	strExtension
	 *					File extension of attachment
	 * @param[in]	strFilename
	 *					Filename of attachment
	 * @param[out]	lpstrParsed
	 *					Parsed data string
	 * @return HRESULT
	 */
	HRESULT ParseValueAttachment(lucene::document::Document *lpDocument, IStream *lpStream,
								 tstring &strMimeTag, tstring &strExtension, tstring &strFilename,
								 std::wstring *lpstrParsed = NULL);

	/**
	 * Handler for attachments which were attached with ATTACH_BY_VALUE
	 *
	 * This handler will read the properties and call the generic ParseValueAttachment()
	 *
	 * @param[in]	lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	lpAttach
	 *					Reference to IAttach object which contains the attachment which should be parsed.
	 * @param[in]	ulProps
	 *					The number of SPropValue elements in lpProps.
	 * @param[in]	lpProps
	 *					Array of SPropValue elements containing all Attachment properties required for indexing.
	 * @return HRESULT
	 */
	HRESULT ParseValueAttachment(lucene::document::Document *lpDocument, IAttach *lpAttach, ULONG ulProps, LPSPropValue lpProps);

	/**
	 * Parse single attachment from message
	 *
	 * Open a single attachment from the given message and check attach method to determine
	 * the method by which the attachment should be parsed.
	 *
	 * @param[in]	lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	lpMessage
	 *					Reference to IMessage object for which all attachments should be indexed.
	 * @param[in]	ulProps
	 *					The number of SPropValue elements in lpProps.
	 * @param[in]	lpProps
	 *					Array of SPropValue elements containing all Attachment properties required for indexing.
	 * @return HRESULT
	 */
	HRESULT ParseAttachment(lucene::document::Document *lpDocument, IMessage *lpMessage, ULONG ulProps, LPSPropValue lpProps);

	/**
	 * Parse single attachment from Stream serializer
	 *
	 * Read a single subobject from the serializer and check attach method to determine
	 * the method by which the attachment should be parsed.
	 *
	 * @param[in]	lpDocument
	 *					Reference to CLucene Document object to which all parsed data should be written to.
	 * @param[in]	lpSerializer
	 *					Reference to ECSerializer object which contains the stream which points to the attachment to index
	 * @return HRESULT
	 */
	HRESULT ParseAttachment(lucene::document::Document *lpDocument, ECSerializer *lpSerializer);

private:
	ECThreadData *m_lpThreadData;
	ECLuceneIndexer *m_lpIndexer;

	LPBYTE m_lpCache;
	ULONG m_ulCache;

	std::string m_strCommand;

	std::wstring m_strFieldName;
	ULONG m_ulFieldStorage;
};

#endif /* ECLUCENEINDEXERATTACHMENT_H */
