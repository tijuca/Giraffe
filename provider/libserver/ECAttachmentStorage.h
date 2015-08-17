/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#ifndef EC_ATTACHMENT_STORAGE
#define EC_ATTACHMENT_STORAGE

#include "ECDatabase.h"
#include <list>
#include <set>
#include <string>

#include <dirent.h>
#include <sys/types.h>

class ECSerializer;
class ECLogger;

class ECAttachmentStorage {
public:
	ECAttachmentStorage(ECDatabase *lpDatabase, unsigned int ulCompressionLevel);

	ULONG AddRef();
	ULONG Release();

	static ECRESULT CreateAttachmentStorage(ECDatabase *lpDatabase, ECConfig *lpConfig, ECLogger *lpLogger, ECAttachmentStorage **lppAttachmentStorage);

	/* Single Instance Attachment wrappers (should not be overridden by subclasses) */
	bool ExistAttachment(ULONG ulObjId, ULONG ulPropId);
	ECRESULT LoadAttachment(struct soap *soap, ULONG ulObjId, ULONG ulPropId, size_t *lpiSize, unsigned char **lppData);
	ECRESULT LoadAttachment(ULONG ulObjId, ULONG ulPropId, size_t *lpiSize, ECSerializer *lpSink);
	ECRESULT SaveAttachment(ULONG ulObjId, ULONG ulPropId, bool bDeleteOld, size_t iSize, unsigned char *lpData, ULONG *lpulInstanceId);
	ECRESULT SaveAttachment(ULONG ulObjId, ULONG ulPropId, bool bDeleteOld, size_t iSize, ECSerializer *lpSource, ULONG *lpulInstanceId);
	ECRESULT SaveAttachment(ULONG ulObjId, ULONG ulPropId, bool bDeleteOld, ULONG ulInstanceId, ULONG *lpulInstanceId);
	ECRESULT CopyAttachment(ULONG ulObjId, ULONG ulNewObjId);
	ECRESULT DeleteAttachments(std::list<ULONG> lstDeleteObjects);
	ECRESULT DeleteAttachment(ULONG ulObjId, ULONG ulPropId);
	ECRESULT GetSize(ULONG ulObjId, ULONG ulPropId, size_t *lpulSize);

	/* Convert ObjectId (hierarchyid) into Instance Id */
	ECRESULT GetSingleInstanceId(ULONG ulObjId, ULONG ulPropId, ULONG *lpulInstanceId);
	ECRESULT GetSingleInstanceIds(std::list<ULONG> &lstObjIds, std::list<ULONG> *lstAttachIds);

	/* Request parents for a particular Single Instance Id */
	ECRESULT GetSingleInstanceParents(ULONG ulInstanceId, std::list<ULONG> *lplstObjIds);

	/* Single Instance Attachment handlers (must be overridden by subclasses) */
	virtual bool ExistAttachmentInstance(ULONG ulInstanceId) = 0;

	virtual ECRESULT Begin() = 0;
	virtual ECRESULT Commit() = 0;
	virtual ECRESULT Rollback() = 0;

protected:
	virtual ~ECAttachmentStorage();
	
	/* Single Instance Attachment handlers (must be overridden by subclasses) */
	virtual ECRESULT LoadAttachmentInstance(struct soap *soap, ULONG ulInstanceId, size_t *lpiSize, unsigned char **lppData) = 0;
	virtual ECRESULT LoadAttachmentInstance(ULONG ulInstanceId, size_t *lpiSize, ECSerializer *lpSink) = 0;
	virtual ECRESULT SaveAttachmentInstance(ULONG ulInstanceId, ULONG ulPropId, size_t iSize, unsigned char *lpData) = 0;
	virtual ECRESULT SaveAttachmentInstance(ULONG ulInstanceId, ULONG ulPropId, size_t iSize, ECSerializer *lpSource) = 0;
	virtual ECRESULT DeleteAttachmentInstances(std::list<ULONG> &lstDeleteInstances, bool bReplace) = 0;
	virtual ECRESULT DeleteAttachmentInstance(ULONG ulInstanceId, bool bReplace) = 0;
	virtual ECRESULT GetSizeInstance(ULONG ulInstanceId, size_t *lpulSize, bool *lpbCompressed = NULL) = 0;

private:
	/* Add reference between Object and Single Instance */
	ECRESULT SetSingleInstanceId(ULONG ulObjId, ULONG ulInstanceId, ULONG ulTag);

	/* Count the number of times an attachment is referenced */
	ECRESULT IsOrphanedSingleInstance(ULONG ulInstanceId, bool *bOrphan);
	ECRESULT GetOrphanedSingleInstances(std::list<ULONG> &lstInstanceIds, std::list<ULONG> *lplstOrphanedInstanceIds);

	ECRESULT DeleteAttachment(ULONG ulObjId, ULONG ulPropId, bool bReplace);

protected:
	ECDatabase *m_lpDatabase;
	bool m_bFileCompression;
	std::string m_CompressionLevel;
	ULONG m_ulRef;
};

class ECDatabaseAttachment : public ECAttachmentStorage {
public:
	ECDatabaseAttachment(ECDatabase *lpDatabase, ECLogger *lpLogger);

protected:
	virtual ~ECDatabaseAttachment();

	/* Single Instance Attachment handlers */
	virtual bool ExistAttachmentInstance(ULONG ulInstanceId);
	virtual ECRESULT LoadAttachmentInstance(struct soap *soap, ULONG ulInstanceId, size_t *lpiSize, unsigned char **lppData);
	virtual ECRESULT LoadAttachmentInstance(ULONG ulInstanceId, size_t *lpiSize, ECSerializer *lpSink);
	virtual ECRESULT SaveAttachmentInstance(ULONG ulInstanceId, ULONG ulPropId, size_t iSize, unsigned char *lpData);
	virtual ECRESULT SaveAttachmentInstance(ULONG ulInstanceId, ULONG ulPropId, size_t iSize, ECSerializer *lpSource);
	virtual ECRESULT DeleteAttachmentInstances(std::list<ULONG> &lstDeleteInstances, bool bReplace);
	virtual ECRESULT DeleteAttachmentInstance(ULONG ulInstanceId, bool bReplace);
	virtual ECRESULT GetSizeInstance(ULONG ulInstanceId, size_t *lpulSize, bool *lpbCompressed = NULL);

	virtual ECRESULT Begin();
	virtual ECRESULT Commit();
	virtual ECRESULT Rollback();
	
	ECLogger *m_lpLogger;
};

class ECFileAttachment : public ECAttachmentStorage {
public:
	ECFileAttachment(ECDatabase *lpDatabase, std::string basepath, unsigned int ulCompressionLevel, ECLogger *lpLogger, const bool force_changes_to_disk);

protected:
	virtual ~ECFileAttachment();
	
	/* Single Instance Attachment handlers */
	virtual bool ExistAttachmentInstance(ULONG ulInstanceId);
	virtual ECRESULT LoadAttachmentInstance(struct soap *soap, ULONG ulInstanceId, size_t *lpiSize, unsigned char **lppData);
	virtual ECRESULT LoadAttachmentInstance(ULONG ulObjId, size_t *lpiSize, ECSerializer *lpSink);
	virtual ECRESULT SaveAttachmentInstance(ULONG ulInstanceId, ULONG ulPropId, size_t iSize, unsigned char *lpData);
	virtual ECRESULT SaveAttachmentInstance(ULONG ulObjId, ULONG ulPropId, size_t iSize, ECSerializer *lpSource);
	virtual ECRESULT DeleteAttachmentInstances(std::list<ULONG> &lstDeleteInstances, bool bReplace);
	virtual ECRESULT DeleteAttachmentInstance(ULONG ulInstanceId, bool bReplace);
	virtual ECRESULT GetSizeInstance(ULONG ulInstanceId, size_t *lpulSize, bool *lpbCompressed = NULL);

	virtual ECRESULT Begin();
	virtual ECRESULT Commit();
	virtual ECRESULT Rollback();

private:
	std::string CreateAttachmentFilename(ULONG ulInstanceId, bool bCompressed);

	size_t attachment_size_safety_limit;

	int m_dirFd;
	DIR *m_dirp;
	bool force_changes_to_disk;

	/* helper functions for transacted deletion */
	ECRESULT MarkAttachmentForDeletion(ULONG ulInstanceId);
	ECRESULT DeleteMarkedAttachment(ULONG ulInstanceId);
	ECRESULT RestoreMarkedAttachment(ULONG ulInstanceId);

	bool VerifyInstanceSize(const ULONG ulInstanceId, const size_t expectedSize, const std::string & filename);

	std::string m_basepath;
	bool m_bTransaction;
	std::set<ULONG> m_setNewAttachment;
	std::set<ULONG> m_setDeletedAttachment;
	std::set<ULONG> m_setMarkedAttachment;
	ECLogger * m_lpLogger;
};

#endif
