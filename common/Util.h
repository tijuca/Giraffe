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

#ifndef UTIL_H
#define UTIL_H

#include <mapix.h>
#include "edkmdb.h"
#include <string>

#include "ECDefs.h"

#include "ustringutil.h"

class ECLogger;

class Util {
public:
	static HRESULT	HrAddToPropertyArray(LPSPropValue lpSrc, ULONG cValues, LPSPropValue lpAdd, LPSPropValue *lppDest, ULONG *cDestValues);
	static HRESULT	HrMergePropertyArrays(LPSPropValue lpSrc, ULONG cValues, LPSPropValue lpAdds, ULONG cAddValues, LPSPropValue *lppDest, ULONG *cDestValues);

	static HRESULT	HrCopyPropertyArray(LPSPropValue lpSrc, ULONG cValues, LPSPropValue *lppDest, ULONG *cDestValues, bool bExcludeErrors = false);
	static HRESULT	HrCopyPropertyArrayByRef(LPSPropValue lpSrc, ULONG cValues, LPSPropValue *lppDest, ULONG *cDestValues, bool bExcludeErrors = false);
	static HRESULT	HrCopyPropertyArray(LPSPropValue lpSrc, ULONG cValues, LPSPropValue lpDest, void *lpBase);
	static HRESULT	HrCopyPropertyArrayByRef(LPSPropValue lpSrc, ULONG cValues, LPSPropValue lpDest);
	static HRESULT	HrCopyProperty(LPSPropValue lpDest, LPSPropValue lpSrc, void *lpBase, ALLOCATEMORE * lpfAllocMore = NULL);
	static HRESULT	HrCopyPropertyByRef(LPSPropValue lpDest, LPSPropValue lpSrc);
	static HRESULT	HrCopySRestriction(LPSRestriction lpDest, LPSRestriction lpSrc, void *lpBase);
	static HRESULT  HrCopySRestriction(LPSRestriction *lppDest, LPSRestriction lpSrc);
	static HRESULT	HrCopyActions(ACTIONS *lpDest, ACTIONS *lpSrc, void *lpBase);
	static HRESULT	HrCopyAction(ACTION *lpDest, ACTION *lpSrc, void *lpBase);
	static HRESULT	HrCopySRowSet(LPSRowSet lpDest, LPSRowSet lpSrc, void *lpBase);
	static HRESULT	HrCopySRow(LPSRow lpDest, LPSRow lpSrc, void *lpBase);

	static HRESULT	HrCopyPropTagArray(LPSPropTagArray lpSrc, LPSPropTagArray *lppDest);
	static HRESULT	HrCopyUnicodePropTagArray(ULONG ulFlags, LPSPropTagArray lpSrc, LPSPropTagArray *lppDest);

	static HRESULT	HrCopyBinary(ULONG ulSize, LPBYTE lpSrc, ULONG *lpulDestSize, LPBYTE *lppDest, LPVOID lpBase = NULL);
	static HRESULT	HrCopyEntryId(ULONG ulSize, LPENTRYID lpSrc, ULONG *lpulDestSize, LPENTRYID* lppDest, LPVOID lpBase = NULL);

	static int		CompareSBinary(const SBinary &sbin1, const SBinary &sbin2);
	static HRESULT	CompareProp(LPSPropValue lpProp1, LPSPropValue lpProp2, const ECLocale &locale, int* lpCompareResult);
	static unsigned int PropSize(LPSPropValue lpProp);

	static LONG		FindPropInArray(LPSPropTagArray lpPropTags, ULONG ulPropTag);

	static HRESULT HrStreamToString(IStream *sInput, std::string &strOutput);
	static HRESULT HrStreamToWString(IStream *sInput, std::wstring &strWOutput);

	static HRESULT HrConvertStreamToWString(IStream *sInput, ULONG ulCodepage, std::wstring *wstrOutput);

	static HRESULT	HrTextToRtf(IStream *text, IStream *rtf);
	static HRESULT	HrTextToHtml(IStream *text, IStream *html, ULONG ulCodepage);
	static HRESULT	HrTextToHtml(const WCHAR *text, std::string &strHTML, ULONG ulCodepage);

	static HRESULT	HrHtmlToText(IStream *html, IStream *text, ULONG ulCodepage);

	static HRESULT	HrHtmlToRtf(IStream *html, IStream *rtf, unsigned int ulCodepage);
	static HRESULT	HrHtmlToRtf(const WCHAR *lpwHTML, std::string& strRTF);

	static ULONG GetBestBody(IMAPIProp *lpPropObj, ULONG ulFlags);

	static HRESULT HrMAPIErrorToText(HRESULT hr, LPTSTR *lppszError, void *lpBase = NULL);

	static bool ValidatePropTagArray(LPSPropTagArray lpPropTagArray);

	static HRESULT bin2hex(ULONG inLength, LPBYTE input, char **output, void *parent = NULL);
	static HRESULT hex2bin(const char *input, size_t len, ULONG *outLength, LPBYTE *output, void *parent = NULL);

	template <size_t N>
	static bool StrCaseCompare(const WCHAR *lpString, const WCHAR (&lpFind)[N], size_t pos = 0) {
		return wcsncasecmp(lpString + pos, lpFind, N-1) == 0;
	}

	/* DoCopyTo/DoCopyProps functions & their helpers */
	static HRESULT FindInterface(LPCIID lpIID, ULONG ulIIDs, LPCIID lpIIDs);
	static HRESULT CopyStream(LPSTREAM lpSrc, LPSTREAM lpDest);
	static HRESULT CopyRecipients(LPMESSAGE lpSrc, LPMESSAGE lpDest);
	static HRESULT CopyInstanceIds(LPMAPIPROP lpSrc, LPMAPIPROP lpDst);
	static HRESULT CopyAttachmentProps(LPATTACH lpSrcAttach, LPATTACH lpDestAttach, LPSPropTagArray lpExcludeProps = NULL);
	static HRESULT CopyAttachments(LPMESSAGE lpSrc, LPMESSAGE lpDest);
	static HRESULT CopyHierarchy(LPMAPIFOLDER lpSrc, LPMAPIFOLDER lpDest, ULONG ulFlags, ULONG ulUIParam, LPMAPIPROGRESS lpProgress);
	static HRESULT CopyContents(ULONG ulWhat, LPMAPIFOLDER lpSrc, LPMAPIFOLDER lpDest, ULONG ulFlags, ULONG ulUIParam, LPMAPIPROGRESS lpProgress);
	static HRESULT TryOpenProperty(ULONG ulPropType, ULONG ulSrcPropTag, LPMAPIPROP lpPropSrc, ULONG ulDestPropTag, LPMAPIPROP lpPropDest,
								   LPSTREAM *lppSrcStream, LPSTREAM *lppDestStream);
	static HRESULT AddProblemToArray(LPSPropProblem lpProblem, LPSPropProblemArray *lppProblems);

	static HRESULT DoCopyTo(LPCIID lpSrcInterface, LPVOID lpSrcObj, ULONG ciidExclude, LPCIID rgiidExclude,
							LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface,
							LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray * lppProblems);
	static HRESULT DoCopyProps(LPCIID lpSrcInterface, LPVOID lpSrcObj, LPSPropTagArray lpIncludeProps, ULONG ulUIParam,
							   LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags,
							   LPSPropProblemArray * lppProblems);

	static HRESULT HrCopyIMAPData(LPMESSAGE lpSrcMsg, LPMESSAGE lpDstMsg);
	static HRESULT HrDeleteIMAPData(LPMESSAGE lpMsg);

	static HRESULT HrGetQuotaStatus(IMsgStore *lpMsgStore, LPECQUOTA lpsQuota, LPECQUOTASTATUS *lppsQuotaStatus);

	static HRESULT HrDeleteResidualProps(LPMESSAGE lpDestMsg, LPMESSAGE lpSourceMsg, LPSPropTagArray lpsValidProps);
	static HRESULT ValidMapiPropInterface(LPCIID lpInterface);
	static HRESULT QueryInterfaceMapiPropOrValidFallback(LPUNKNOWN lpInObj, LPCIID lpInterface, LPUNKNOWN *lppOutObj);

	static HRESULT HrFindEntryIDs(ULONG cbEID, LPENTRYID lpEID, ULONG cbEntryIDs, LPSPropValue lpEntryIDs, BOOL *lpbFound, ULONG* lpPos);

	static HRESULT HrDeleteAttachments(LPMESSAGE lpMsg);
	static HRESULT HrDeleteRecipients(LPMESSAGE lpMsg);

	static HRESULT HrDeleteMessage(IMAPISession *lpSession, IMessage *lpMessage);

	static bool FHasHTML(IMAPIProp *lpProp);

	struct SBinaryLess {
		bool operator()(const SBinary &left, const SBinary &right) const {
			return CompareSBinary(left, right) < 0;
		}
	};
	
	static HRESULT ReadProperty(IMAPIProp *lpProp, ULONG ulPropTag, std::string &strData);
	static HRESULT WriteProperty(IMAPIProp *lpProp, ULONG ulPropTag, const std::string &strData);
};

#define RTF_TAG_TYPE_TEXT	0x0000
#define RTF_TAG_TYPE_HTML	0x0010
#define RTF_TAG_TYPE_HEAD	0x0020
#define RTF_TAG_TYPE_BODY	0x0030
#define RTF_TAG_TYPE_P		0x0040
#define RTF_TAG_TYPE_STARTP	0x0050
#define RTF_TAG_TYPE_ENDP	0x0060
#define RTF_TAG_TYPE_BR		0x0070
#define RTF_TAG_TYPE_PRE	0x0080
#define RTF_TAG_TYPE_FONT	0x0090
#define RTF_TAG_TYPE_HEADER	0x00A0
#define RTF_TAG_TYPE_TITLE	0x00B0
#define RTF_TAG_TYPE_PLAIN	0x00C0
#define RTF_TAG_TYPE_UNK	0x00F0

#define RTF_INBODY			0x0000
#define RTF_INHEAD			0x0001
#define RTF_INHTML			0x0002
#define RTF_OUTHTML			0x0003

#define RTF_FLAG_INPAR		0x0004
#define RTF_FLAG_CLOSE		0x0008
#define RTF_FLAG_MHTML		0x0100

#endif // UTIL_H
