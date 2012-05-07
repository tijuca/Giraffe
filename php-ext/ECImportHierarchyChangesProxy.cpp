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

#include "phpconfig.h"

#include "platform.h"
#include "ecversion.h"
#include <stdio.h>
#include <time.h>
#include <math.h>

extern "C" {
	// Remove these defines to remove warnings
	#undef PACKAGE_VERSION
	#undef PACKAGE_TARNAME
	#undef PACKAGE_NAME
	#undef PACKAGE_STRING
	#undef PACKAGE_BUGREPORT
	
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

ECImportHierarchyChangesProxy::ECImportHierarchyChangesProxy(zval *lpObj TSRMLS_DC) {
    this->m_cRef = 1; // Object is in use when created!
    this->m_lpObj = lpObj;
#if ZEND_MODULE_API_NO >= 20071006
    Z_ADDREF_P(m_lpObj);
#else
    ZVAL_ADDREF(m_lpObj);
#endif
#ifdef ZTS
	this->TSRMLS_C = TSRMLS_C;
#endif
}

ECImportHierarchyChangesProxy::~ECImportHierarchyChangesProxy() {
    zval_ptr_dtor(&m_lpObj);
}

ULONG 	ECImportHierarchyChangesProxy::AddRef() {
    m_cRef++;
    
    return m_cRef;
}

ULONG	ECImportHierarchyChangesProxy::Release() {
    m_cRef--;
    
    if(m_cRef == 0) {
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
    HRESULT hr = hrSuccess;
    
    zval *pvalFuncName;
    zval *pvalReturn;
    zval *pvalArgs[2];
    
    MAKE_STD_ZVAL(pvalFuncName);
    MAKE_STD_ZVAL(pvalReturn);
    
    MAKE_STD_ZVAL(pvalArgs[0]);
    MAKE_STD_ZVAL(pvalArgs[1]);

    if(lpStream) {
        ZVAL_RESOURCE(pvalArgs[0], (long)lpStream);
    } else {
        ZVAL_NULL(pvalArgs[0]);
    }
    
    ZVAL_LONG(pvalArgs[1], ulFlags);
    
    ZVAL_STRING(pvalFuncName, "Config" , 1);
    
    if(call_user_function(NULL, &m_lpObj, pvalFuncName, pvalReturn, 2, pvalArgs TSRMLS_CC) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Config method not present on ImportHierarchyChanges object");
        hr = MAPI_E_CALL_FAILED;
        goto exit;
    }
    
    convert_to_long_ex(&pvalReturn);
    
    hr = pvalReturn->value.lval;

exit:
    zval_ptr_dtor(&pvalFuncName);
    zval_ptr_dtor(&pvalReturn);
    zval_ptr_dtor(&pvalArgs[0]);
    zval_ptr_dtor(&pvalArgs[1]);
    
    return hr;
}

HRESULT ECImportHierarchyChangesProxy::UpdateState(LPSTREAM lpStream) {
    HRESULT hr = hrSuccess;
    
    zval *pvalFuncName;
    zval *pvalReturn;
    zval *pvalArgs[1];
    
    MAKE_STD_ZVAL(pvalFuncName);
    MAKE_STD_ZVAL(pvalReturn);
    
    MAKE_STD_ZVAL(pvalArgs[0]);

    if(lpStream) {
        ZVAL_RESOURCE(pvalArgs[0], (long)lpStream);
    } else {
        ZVAL_NULL(pvalArgs[0]);
    }
    
    ZVAL_STRING(pvalFuncName, "UpdateState" , 1);
    
    if(call_user_function(NULL, &m_lpObj, pvalFuncName, pvalReturn, 1, pvalArgs TSRMLS_CC) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "UpdateState method not present on ImportHierarchyChanges object");
        hr = MAPI_E_CALL_FAILED;
        goto exit;
    }
    
    convert_to_long_ex(&pvalReturn);
    
    hr = pvalReturn->value.lval;

exit:
    zval_ptr_dtor(&pvalFuncName);
    zval_ptr_dtor(&pvalReturn);
    zval_ptr_dtor(&pvalArgs[0]);
    
    return hr;
}

HRESULT ECImportHierarchyChangesProxy::ImportFolderChange(ULONG cValues, LPSPropValue lpPropArray)  {
    zval *pvalFuncName;
    zval *pvalReturn;
    zval *pvalArgs[1];
    HRESULT hr = hrSuccess;
    
    MAKE_STD_ZVAL(pvalFuncName);
    MAKE_STD_ZVAL(pvalReturn);

    hr = PropValueArraytoPHPArray(cValues, lpPropArray, &pvalArgs[0] TSRMLS_CC);
    if(hr != hrSuccess) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to convert MAPI propvalue array to PHP");
        goto exit;
    }
    
    ZVAL_STRING(pvalFuncName, "ImportFolderChange", 1);
    
    if(call_user_function(NULL, &m_lpObj, pvalFuncName, pvalReturn, 1, pvalArgs TSRMLS_CC) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "ImportFolderChange method not present on ImportHierarchyChanges object");
        hr = MAPI_E_CALL_FAILED;
        goto exit;
    }
        
    convert_to_long_ex(&pvalReturn);
    
    hr = pvalReturn->value.lval;
    
    if(hr != hrSuccess)
        goto exit;

exit:
    zval_ptr_dtor(&pvalFuncName);
    zval_ptr_dtor(&pvalReturn);
    zval_ptr_dtor(&pvalArgs[0]);
  
    return hr;
}

HRESULT ECImportHierarchyChangesProxy::ImportFolderDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList) {
    HRESULT hr = hrSuccess;
    
    zval *pvalFuncName;
    zval *pvalReturn;
    zval *pvalArgs[2];
    
    MAKE_STD_ZVAL(pvalFuncName);
    MAKE_STD_ZVAL(pvalReturn);
    
    MAKE_STD_ZVAL(pvalArgs[0]);

    ZVAL_LONG(pvalArgs[0], ulFlags);
    SBinaryArraytoPHPArray(lpSourceEntryList, &pvalArgs[1] TSRMLS_CC);

    ZVAL_STRING(pvalFuncName, "ImportFolderDeletion" , 1);
    
    if(call_user_function(NULL, &m_lpObj, pvalFuncName, pvalReturn, 2, pvalArgs TSRMLS_CC) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "ImportFolderDeletion method not present on ImportHierarchyChanges object");
        hr = MAPI_E_CALL_FAILED;
        goto exit;
    }
    
    convert_to_long_ex(&pvalReturn);
    
    hr = pvalReturn->value.lval;

exit:
    zval_ptr_dtor(&pvalFuncName);
    zval_ptr_dtor(&pvalReturn);
    zval_ptr_dtor(&pvalArgs[0]);
    zval_ptr_dtor(&pvalArgs[1]);
    
    return hr;
}

