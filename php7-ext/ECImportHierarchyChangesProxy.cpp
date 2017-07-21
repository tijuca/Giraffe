/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include "phpconfig.h"

#include <kopano/platform.h>
#include <kopano/ecversion.h>
#include <cstdio>
#include <ctime>
#include <cmath>

using namespace std;

extern "C" {
	// Remove these defines to remove warnings
	#undef PACKAGE_VERSION
	#undef PACKAGE_TARNAME
	#undef PACKAGE_NAME
	#undef PACKAGE_STRING
	#undef PACKAGE_BUGREPORT

	#if !__GNUC_PREREQ(6,0)
	#define zend_isnan(a) std::isnan(a)
	#endif
	
	#include "php.h"
   	#include "php_globals.h"
	#include "ext/standard/info.h"
	#include "ext/standard/php_string.h"
}

// A very, very nice PHP #define that causes link errors in MAPI when you have multiple
// files referencing MAPI....
#undef inline

/***************************************************************
* MAPI Includes
***************************************************************/

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapispi.h>
#include <mapitags.h>
#include <mapicode.h>

#define USES_IID_IMAPIProp
#define USES_IID_IMAPIContainer
#define USES_IID_IMsgStore
#define USES_IID_IMessage
#define USES_IID_IExchangeManageStore

#include <edkguid.h>
#include <edkmdb.h>
#include "ECImportHierarchyChangesProxy.h"
#include "typeconversion.h"

ECImportHierarchyChangesProxy::ECImportHierarchyChangesProxy(const zval *v TSRMLS_DC) :
	m_cRef(1)
{
	ZVAL_OBJ(&m_lpObj, Z_OBJ_P(v));
	Z_ADDREF(m_lpObj);
}

ECImportHierarchyChangesProxy::~ECImportHierarchyChangesProxy() {
	Z_DELREF(m_lpObj);
}

ULONG 	ECImportHierarchyChangesProxy::AddRef() {
	return ++m_cRef;
}

ULONG	ECImportHierarchyChangesProxy::Release() {
	if (--m_cRef == 0) {
        delete this;
        return 0;
    }
        
    return m_cRef;
}

HRESULT ECImportHierarchyChangesProxy::QueryInterface(REFIID iid, void **lpvoid) {
    if(iid == IID_IExchangeImportHierarchyChanges) {
        AddRef();
        *lpvoid = this;
        return hrSuccess;
    }
    return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECImportHierarchyChangesProxy::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError) {
    return MAPI_E_NO_SUPPORT;
}

HRESULT ECImportHierarchyChangesProxy::Config(LPSTREAM lpStream, ULONG ulFlags) {
    zval pvalFuncName;
    zval pvalReturn;
    zval pvalArgs[2];
    
    if(lpStream) {
	Z_LVAL_P(&pvalArgs[0]) = (long)lpStream;
	Z_TYPE_INFO_P(&pvalArgs[0]) = IS_RESOURCE;
    } else {
        ZVAL_NULL(&pvalArgs[0]);
    }
    
    ZVAL_LONG(&pvalArgs[1], ulFlags);
    
    ZVAL_STRING(&pvalFuncName, "Config");
    if (call_user_function(NULL, &m_lpObj, &pvalFuncName, &pvalReturn, 2, pvalArgs TSRMLS_CC) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Config method not present on ImportHierarchyChanges object");
        return MAPI_E_CALL_FAILED;
    }
    
    convert_to_long_ex(&pvalReturn);
    return pvalReturn.value.lval;
}

HRESULT ECImportHierarchyChangesProxy::UpdateState(LPSTREAM lpStream) {
    zval pvalFuncName;
    zval pvalReturn;
    zval pvalArgs[1];
    
    if(lpStream) {
	Z_LVAL_P(&pvalArgs[0]) = (long)lpStream;
	Z_TYPE_INFO_P(&pvalArgs[0]) = IS_RESOURCE;
    } else {
        ZVAL_NULL(&pvalArgs[0]);
    }
    
    ZVAL_STRING(&pvalFuncName, "UpdateState");
    if (call_user_function(NULL, &m_lpObj, &pvalFuncName, &pvalReturn, 1, pvalArgs TSRMLS_CC) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "UpdateState method not present on ImportHierarchyChanges object");
        return MAPI_E_CALL_FAILED;
    }
    
    convert_to_long_ex(&pvalReturn);
    return pvalReturn.value.lval;
}

HRESULT ECImportHierarchyChangesProxy::ImportFolderChange(ULONG cValues, LPSPropValue lpPropArray)  {
    zval pvalFuncName;
    zval pvalReturn;
    zval pvalArgs[1];
    HRESULT hr = PropValueArraytoPHPArray(cValues, lpPropArray, &pvalArgs[0] TSRMLS_CC);
    if(hr != hrSuccess) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to convert MAPI propvalue array to PHP");
        return hr;
    }
    
    ZVAL_STRING(&pvalFuncName, "ImportFolderChange");
    if (call_user_function(NULL, &m_lpObj, &pvalFuncName, &pvalReturn, 1, pvalArgs TSRMLS_CC) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "ImportFolderChange method not present on ImportHierarchyChanges object");
        return MAPI_E_CALL_FAILED;
    }
        
    convert_to_long_ex(&pvalReturn);
    return pvalReturn.value.lval;
}

HRESULT ECImportHierarchyChangesProxy::ImportFolderDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList) {
    zval pvalFuncName;
    zval pvalReturn;
    zval pvalArgs[2];
    
    ZVAL_LONG(&pvalArgs[0], ulFlags);
    SBinaryArraytoPHPArray(lpSourceEntryList, &pvalArgs[1] TSRMLS_CC);

    ZVAL_STRING(&pvalFuncName, "ImportFolderDeletion");
    if (call_user_function(NULL, &m_lpObj, &pvalFuncName, &pvalReturn, 2, pvalArgs TSRMLS_CC) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "ImportFolderDeletion method not present on ImportHierarchyChanges object");
        return MAPI_E_CALL_FAILED;
    }
    
    convert_to_long_ex(&pvalReturn);
    return pvalReturn.value.lval;
}
