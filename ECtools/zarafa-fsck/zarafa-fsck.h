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

#ifndef ZARAFA_FSCK
#define ZARAFA_FSCK

#include "platform.h"
#include <string>
using namespace std;

#include <mapidefs.h>

/*
 * Global configuration
 */
extern string auto_fix;
extern string auto_del;

class ZarafaFsck {
private:
	ULONG ulFolders;
	ULONG ulEntries;
	ULONG ulProblems;
	ULONG ulFixed;
	ULONG ulDeleted;

	virtual HRESULT ValidateItem(LPMESSAGE lpMessage, string strClass) = 0;

public:
	ZarafaFsck();
	virtual ~ZarafaFsck() { }

	HRESULT ValidateMessage(LPMESSAGE lpMessage,
				string strName, string strClass);
	HRESULT ValidateFolder(LPMAPIFOLDER lpFolder, string strName);

	HRESULT AddMissingProperty(LPMESSAGE lpMessage, std::string strName,
				   ULONG ulTag,__UPV Value);
	HRESULT ReplaceProperty(LPMESSAGE lpMessage, std::string strName,
				ULONG ulTag, std::string strError, __UPV Value);
	HRESULT DeleteMessage(LPMAPIFOLDER lpFolder,
			      LPSPropValue lpItemProperty);

	void PrintStatistics(string title);
};

class ZarafaFsckCalendar : public ZarafaFsck {
private:
	HRESULT ValidateItem(LPMESSAGE lpMessage, string strClass);
	HRESULT ValidateMinimalNamedFields(LPMESSAGE lpMessage);
	HRESULT ValidateTimestamps(LPMESSAGE lpMessage);
	HRESULT ValidateRecurrence(LPMESSAGE lpMessage);
};

class ZarafaFsckContact : public ZarafaFsck {
private:
	HRESULT ValidateItem(LPMESSAGE lpMessage, string strClass);
	HRESULT ValidateContactNames(LPMESSAGE lpMessage);
};

class ZarafaFsckTask : public ZarafaFsck {
private:
	HRESULT ValidateItem(LPMESSAGE lpMessage, string strClass);
	HRESULT ValidateMinimalNamedFields(LPMESSAGE lpMessage);
	HRESULT ValidateTimestamps(LPMESSAGE lpMessage);
	HRESULT ValidateCompletion(LPMESSAGE lpMessage);
};

/*
 * Helper functions.
 */
HRESULT allocNamedIdList(ULONG ulSize, LPMAPINAMEID **lpppNameArray);
void freeNamedIdList(LPMAPINAMEID *lppNameArray);

HRESULT ReadProperties(LPMESSAGE lpMessage, ULONG ulCount,
		       ULONG *lpTag, LPSPropValue *lppPropertyArray);
HRESULT ReadNamedProperties(LPMESSAGE lpMessage, ULONG ulCount,
			    LPMAPINAMEID *lppTag,
			    LPSPropTagArray *lppPropertyTagArray,
			    LPSPropValue *lppPropertyArray);

#endif /* ZARAFA_FSCK */
