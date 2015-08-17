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

#ifndef ECMEMTABLEPUBLIC_H
#define ECMEMTABLEPUBLIC_H

#include "zcdefs.h"
#include "ECMemTable.h"
#include "ECMAPIFolderPublic.h"

#include <mapidefs.h>

class ECMemTablePublic _final : public ECMemTable
{
public:
	typedef struct _sRelation {
		unsigned int ulRowID;
		IMAPIFolder* lpFolder;
		LPMAPIADVISESINK lpAdviseSink;
		ULONG ulAdviseConnectionId;
		ULONG cbEntryID; // Folder entryid
		LPENTRYID lpEntryID;
	}t_sRelation;

	typedef std::map<std::string, t_sRelation> ECMAPFolderRelation; // <instancekey, relation>

protected:
	ECMemTablePublic(ECMAPIFolderPublic *lpECParentFolder, SPropTagArray *lpsPropTags, ULONG ulRowPropTag);
	virtual ~ECMemTablePublic(void);

public:
	static HRESULT Create(ECMAPIFolderPublic *lpECParentFolder, ECMemTablePublic **lppECMemTable);
	
	static void FreeRelation(t_sRelation* lpRelation);

	HRESULT QueryInterface(REFIID refiid, void **lppInterface) _override;

	//virtual ULONG AddRef(void) _override;
	//virtual ULONG Release(void) _override;

	HRESULT Init(ULONG ulFlags);

	HRESULT ModifyRow(SBinary* lpInstanceKey, LPSRow lpsRow);
	HRESULT DelRow(SBinary* lpInstanceKey);


	HRESULT AdviseFolder(ULONG cbSourceKey, LPBYTE lpbSourceKey, LPMAPIFOLDER lpFolder);

	ECMAPIFolderPublic *m_lpECParentFolder;
	LPMAPIADVISESINK	m_lpShortCutAdviseSink;
	ULONG				m_ulFlags; //UNICODE flags
	LPMAPITABLE			m_lpShortcutTable;

	ULONG				m_ulRowId;
	ECMAPFolderRelation	m_mapRelation; //Relation between shortcut instancekey and rowid

};

#endif //#ifndef ECMEMTABLEPUBLIC_H
