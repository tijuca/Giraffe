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

/*
 *	Test routine for RFC5322 input message character set recognition,
 *	derivation and transformation.
 */
#include "zcdefs.h"
#include "platform.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <inetmapi.h>
#include <mapiutil.h>
#include <mapiext.h>
#include <edkmdb.h>
#include "ECLogger.h"
#include "MAPIErrors.h"
#include "CommonUtil.h"
#include "codepage.h"
#include "options.h"

enum {
	TEST_OK = 0,
	TEST_FAIL = 1
};

static const char *t_default_charset = "us-ascii";
ECLogger_File t_logger(EC_LOGLEVEL_DEBUG, false, "-", 0, 0);

class ictx _final {
	public:
	void complete_init(void);
	const SPropValue *find(unsigned int) const;

	SPropValue *props;
	ULONG count;
	const char *file;

	unsigned int cpid;
	const char *codepage;
	wchar_t *data;
};

const SPropValue *ictx::find(unsigned int tag) const
{
	return PpropFindProp(const_cast<SPropValue *>(props), count, tag);
}

void ictx::complete_init(void)
{
	const SPropValue *prop = find(PR_INTERNET_CPID);
	cpid = (prop != NULL) ? prop->Value.ul : 0;
	if (HrGetCharsetByCP(cpid, &codepage) != hrSuccess)
		codepage = "<unknown>";
	prop = find(PR_BODY_W);
	data = (prop != NULL) ? prop->Value.lpszW : NULL;
}

static int slurp_file(const char *file, std::string &msg)
{
	std::ifstream fp(file);
	if (fp.fail()) {
		fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
		return -errno;
	}
	fp.seekg(0, std::ios::end);
	if (fp.good()) {
		long res = fp.tellg();
		if (res > 0)
			msg.reserve(res);
		fp.seekg(0, std::ios::beg);
	}
	msg.assign(std::istreambuf_iterator<char>(fp),
	           std::istreambuf_iterator<char>());
	return 1;
}

/**
 * pfile - read and process a file
 * @file:	path to file containing RFC 5322 message
 * @analyze:	a function to analyze the MAPI message resulting from
 * 		VMIMEToMAPI conversion
 */
static int dofile(const char *file, int (*analyze)(const struct ictx &))
{
	std::string rfcmsg;
	int ret = slurp_file(file, rfcmsg);
	if (ret <= 0)
		return TEST_FAIL;

	struct ictx ictx;
	IMAPISession *session = NULL;
	IMsgStore *store = NULL;
	IMAPIFolder *root_folder = NULL;
	IMessage *imsg = NULL;
	HRESULT hr = hrSuccess;
	ULONG type = 0;
	delivery_options delivery_opt;

	/*
	 * Sucky libinetmapi requires that we have a session open
	 * just to test VMIMEToMAPI's functions :-(
	 */
	memset(&delivery_opt, 0, sizeof(delivery_opt));
	delivery_opt.default_charset = t_default_charset;
	ret = 0;
	HrSetLogger(&t_logger);
	hr = HrOpenECSession(&t_logger, &session, "app_vers", "app_misc",
	     L"SYSTEM", L"", "file:///var/run/zarafa", 0,0,0);
	if (hr != hrSuccess) {
		fprintf(stderr, "OpenECSession: %s\n", GetMAPIErrorMessage(hr));
		goto exit;
	}
	hr = HrOpenDefaultStore(session, &store);
	if (hr != hrSuccess) {
		fprintf(stderr, "OpenDefaultStore: %s\n", GetMAPIErrorMessage(hr));
		goto exit;
	}
	hr = store->OpenEntry(0, NULL, NULL, MAPI_MODIFY, &type,
	     reinterpret_cast<IUnknown **>(&root_folder));
	if (hr != hrSuccess) {
		fprintf(stderr, "OpenEntry: %s\n", GetMAPIErrorMessage(hr));
		goto exit;
	}
	hr = root_folder->CreateMessage(NULL, 0, &imsg);
	if (hr != hrSuccess) {
		fprintf(stderr, "CreateMessage: %s\n", GetMAPIErrorMessage(hr));
		goto exit;
	}
	hr = IMToMAPI(NULL, NULL, NULL, imsg, rfcmsg, delivery_opt, &t_logger);
	if (hr != hrSuccess) {
		fprintf(stderr, "IMToMAPI: %s\n", GetMAPIErrorMessage(hr));
		goto exit;
	}
	hr = HrGetAllProps(imsg, MAPI_UNICODE, &ictx.count, &ictx.props);
	if (hr == MAPI_W_ERRORS_RETURNED) {
	} else if (hr != hrSuccess) {
		fprintf(stderr, "GetAllProps: %s\n", GetMAPIErrorMessage(hr));
		goto exit;
	}
	ictx.file = file;
	ictx.complete_init();
	ret = (analyze != NULL) ? (*analyze)(ictx) : TEST_OK;

 exit:
	if (ictx.props != NULL)
		MAPIFreeBuffer(ictx.props);
	if (imsg != NULL)
		imsg->Release();
	if (root_folder != NULL)
		root_folder->Release();
	if (store != NULL)
		store->Release();
	if (session != NULL)
		session->Release();
	HrSetLogger(NULL);
	return ret;
}

static size_t chkfile(const char *file, int (*analyze)(const struct ictx &))
{
	fprintf(stderr, "=== %s ===\n", file);
	int ret = dofile(file, analyze);
	if (ret != TEST_OK) {
		fprintf(stderr, "FAILED: %s\n\n", file);
		return ret;
	}
	fprintf(stderr, "\n");
	return 0;
}

/*
 * Important properties one may want to inspect: PR_SUBJECT_W,
 * (PR_SUBJECT_PREFIX_W), PR_BODY_W, PR_BODY_HTML, PR_INTERNET_CPID,
 * (PR_RTF_COMPRESSED)
 */

static int test_mimecset01(const struct ictx &ctx)
{
	return (strcmp(ctx.codepage, "utf-8") == 0 &&
	        wcscmp(ctx.data, L"t\xE6st") == 0) ? TEST_OK : TEST_FAIL;
}

static int test_mimecset03(const struct ictx &ctx)
{
	return strcmp(ctx.codepage, "us-ascii") == 0 ? TEST_OK : TEST_FAIL;
}

static int test_vmime_cte(const struct ictx &ctx)
{
	/* Since the CTE is *decidedly not* QP, it will be as-is */
	return (strcmp(ctx.codepage, "us-ascii") == 0 &&
	        wcscmp(ctx.data, L"=E2=98=BA") == 0) ? TEST_OK : TEST_FAIL;
}

static int test_zcp_11699(const struct ictx &ctx)
{
	const SPropValue *prop = ctx.find(PR_SUBJECT_W);
	if (prop == NULL)
		return TEST_FAIL;
	const wchar_t *subj = prop->Value.lpszW;
	static const wchar_t matchsubj[] = L"\x263A dum";
	static const wchar_t matchbody[] = L"\x263A dummy \x263B";
	if (wcscmp(subj, matchsubj) != 0 || wcscmp(ctx.data, matchbody) != 0)
		return TEST_FAIL;
	return TEST_OK;
}

static int test_zcp_11713(const struct ictx &ctx)
{
	/*
	 * ISO-2022-JP (50220, 50222) is a valid outcome of any decoder.
	 * SHIFT_JIS (932) is a possible outcome of ZCP's IMToMAPI, but not
	 * strictly RFC-conformant.
	 */
	if (strcmp(ctx.codepage, "iso-2022-jp") != 0 &&
	    strcmp(ctx.codepage, "shift-jis") != 0) {
		fprintf(stderr, "zcp-11713: unexpected charset %s (%d)\n",
		        ctx.codepage, ctx.cpid);
		return TEST_FAIL;
	}
	/* "メッセージ" */
	if (wcsstr(ctx.data, L"\x30E1\x30C3\x30BB\x30FC\x30B8") == NULL) {
		fprintf(stderr, "zcp-11713: expected text piece not found\n");
		return TEST_FAIL;
	}
	return TEST_OK;
}

static int test_zcp_12930(const struct ictx &ctx)
{
	if (strcmp(ctx.codepage, "us-ascii") != 0) {
		fprintf(stderr, "zcp-12930: expected us-ascii, got %s\n", ctx.codepage);
		return TEST_FAIL;
	}
	if (wcsstr(ctx.data, L"simply dummy t ext") == NULL) {
		fprintf(stderr, "zcp-12930: verbatim body extraction incorrect\n");
		return TEST_FAIL;
	}
	return TEST_OK;
}

static int test_zcp_13036_0d(const struct ictx &ctx)
{
	return (strcmp(ctx.codepage, "utf-8") == 0 &&
	        wcsstr(ctx.data, L"zg\x142osze\x144") != NULL) ?
	        TEST_OK : TEST_FAIL;
}

static int test_zcp_13036_69(const struct ictx &ctx)
{
	return (strcmp(ctx.codepage, "iso-8859-1") == 0 &&
	        wcsstr(ctx.data, L"J\xE4nner") != NULL) ? TEST_OK : TEST_FAIL;
}

static int test_zcp_13036_lh(const struct ictx &ctx)
{
	return (strcmp(ctx.codepage, "utf-8") == 0 &&
	        wcsstr(ctx.data, L"k\xF6nnen, \xF6" L"ffnen") != NULL) ?
	        TEST_OK : TEST_FAIL;
}

static int test_zcp_13175(const struct ictx &ctx)
{
	return (strcmp(ctx.codepage, "utf-8") == 0 &&
	        wcsstr(ctx.data, L"extrem \xFC" L"berh\xF6ht") != NULL) ?
	        TEST_OK : TEST_FAIL;
}

static int test_zcp_13337(const struct ictx &ctx)
{
	return (strcmp(ctx.codepage, "utf-8") == 0 &&
	        wcsstr(ctx.data, L"\xA0") != NULL) ?
		TEST_OK : TEST_FAIL;
}

static int test_zcp_13439_nl(const struct ictx &ctx)
{
	if (strcmp(ctx.codepage, "utf-8") != 0 ||
	    wcsstr(ctx.data, L"f\xFCr") == NULL)
		return TEST_FAIL;
	const SPropValue *prop = ctx.find(PR_SUBJECT_W);
	if (prop == NULL)
		return TEST_FAIL;
	const wchar_t *subj = prop->Value.lpszW;
	if (subj == NULL ||
	    wcscmp(subj, L"\xc4\xe4 \xd6\xf6 \xdc\xfc \xdf \x2013 Umlautetest, UMLAUTETEST 2") != 0)
		 return TEST_FAIL;
	return TEST_OK;
}

static int test_zcp_13449_meca(const struct ictx &ctx)
{
	if (strcmp(ctx.codepage, "windows-1252") != 0 ||
	    wcsstr(ctx.data, L"M\xE9" L"canique") == NULL)
		return TEST_FAIL;

	/*
	 * The subject specifies an invalid charset (windows-1252http-equiv…)
	 * and there is no fallback possible, so ASCII is (correctly) chosen,
	 * turning the 'é' (U+00E9) character into something else.
	 */
	const SPropValue *prop = ctx.find(PR_SUBJECT_W);
	if (prop == NULL)
		return TEST_FAIL;
	const wchar_t *subj = prop->Value.lpszW;
	if (wcscmp(subj, L"Orange m?canique") == 0 ||
	    wcscmp(subj, L"Orange mcanique") == 0)
		/*
		 * The question mark as a replacement character is a typical
		 * outcome of many decoders, but I do not think any standard
		 * mandates it. The other way is skipping characters.
		 */
		return TEST_OK;
	return TEST_FAIL;
}

static int test_zcp_13449_na(const struct ictx &ctx)
{
	if (strcmp(ctx.codepage, "us-ascii") != 0)
		return TEST_FAIL;
	/* All non-ASCII is stripped, and the '!' is the leftover. */
	if (wcscmp(ctx.data, L"!") != 0)
		return TEST_FAIL;
	const SPropValue *prop = ctx.find(PR_SUBJECT_W);
	if (prop == NULL)
		return TEST_FAIL;
	const wchar_t *subj = prop->Value.lpszW;
	/*
	 * May need rework depending on how unreadable characters
	 * are transformed (decoder dependent).
	 */
	if (wcscmp(subj, L"N??t ASCII??????") != 0)
		return TEST_FAIL;
	return TEST_OK;
}

static int test_zcp_13473(const struct ictx &ctx)
{
	return (strcmp(ctx.codepage, "utf-8") == 0) ? TEST_OK : TEST_FAIL;
}

int main(int argc, const char **argv)
{
#define TMDIR SOURCEDIR "/testmails/"
	size_t err = 0;
	HRESULT hr = MAPIInitialize(NULL);

	if (hr != hrSuccess) {
		fprintf(stderr, "MAPIInitialize: %s\n",
		        GetMAPIErrorMessage(hr));
		return EXIT_FAILURE;
	}

	err += chkfile(TMDIR "vmime-cte.eml", test_vmime_cte);
	err += chkfile(TMDIR "mime_charset_01.eml", test_mimecset01);
	err += chkfile(TMDIR "mime_charset_02.eml", test_mimecset01);
	err += chkfile(TMDIR "mime_charset_03.eml", test_mimecset03);
	err += chkfile(TMDIR "zcp-11699-ub.eml", test_zcp_11699);
	err += chkfile(TMDIR "zcp-11699-utf8.eml", test_zcp_11699);
	err += chkfile(TMDIR "zcp-11699-p.eml", test_zcp_11699);
	err += chkfile(TMDIR "zcp-11713.eml", test_zcp_11713);
	err += chkfile(TMDIR "zcp-12930.eml", test_zcp_12930);
	err += chkfile(TMDIR "zcp-13036-6906a338.eml", test_zcp_13036_69);
	err += chkfile(TMDIR "zcp-13036-0db504a2.eml", test_zcp_13036_0d);
	err += chkfile(TMDIR "zcp-13036-lh.eml", test_zcp_13036_lh);
	err += chkfile(TMDIR "zcp-13175.eml", test_zcp_13175);
	err += chkfile(TMDIR "zcp-13337.eml", test_zcp_13337);
	err += chkfile(TMDIR "zcp-13439-nl.eml", test_zcp_13439_nl);
	err += chkfile(TMDIR "zcp-13449-meca.eml", test_zcp_13449_meca);
	err += chkfile(TMDIR "zcp-13449-na.eml", test_zcp_13449_na);
	err += chkfile(TMDIR "zcp-13473.eml", test_zcp_13473);
	fprintf(stderr, (err == 0) ? "Overall success\n" : "Overall FAILURE\n");
	MAPIUninitialize();
	return (err == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
#undef TMDIR
}
