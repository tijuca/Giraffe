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

#include <kopano/platform.h>
#include <memory>
#include <new>
#include <Python.h>
#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <kopano/mapiext.h>
#include <mapiguid.h>
#include "PyMapiPlugin.h"
#include <kopano/stringutil.h>
#include "frameobject.h"

#define NEW_SWIG_INTERFACE_POINTER_OBJ(pyswigobj, objpointer, typeobj) {\
	if (objpointer) {\
		pyswigobj.reset(SWIG_NewPointerObj((void *)objpointer, typeobj, SWIG_POINTER_OWN | 0)); \
		PY_HANDLE_ERROR(m_lpLogger, pyswigobj) \
		\
		objpointer->AddRef();\
	} else {\
		pyswigobj.reset(Py_None); \
	    Py_INCREF(Py_None);\
	}\
}

#define BUILD_SWIG_TYPE(pyswigobj, type) {\
	pyswigobj = SWIG_TypeQuery(type); \
	if (!pyswigobj) {\
		assert(false);\
		return S_FALSE;\
	}\
}

/**
 * Handle the python errors
 * 
 * note: The traceback doesn't work very well
 */
static HRESULT PyHandleError(ECLogger *lpLogger, PyObject *pyobj)
{
	HRESULT hr = hrSuccess;

	if (!pyobj) 
	{ 
		PyObject *lpErr = PyErr_Occurred();
		if(lpErr) {
			PyObjectAPtr ptype, pvalue, ptraceback;
			PyErr_Fetch(&~ptype, &~pvalue, &~ptraceback);
			auto traceback = reinterpret_cast<PyTracebackObject *>(ptraceback.get());
			const char *pStrErrorMessage = "Unknown";
			const char *pStrType = "Unknown";

			if (pvalue != nullptr)
				pStrErrorMessage = PyString_AsString(pvalue.get());
			if (ptype != nullptr)
				pStrType = PyString_AsString(ptype.get());

			if (lpLogger)
			{
				lpLogger->Log(EC_LOGLEVEL_ERROR, "  Python type: %s", pStrType);
				lpLogger->Log(EC_LOGLEVEL_ERROR, "  Python error: %s", pStrErrorMessage);
				
				while (traceback && traceback->tb_next != NULL) {
					PyFrameObject *frame = (PyFrameObject *)traceback->tb_frame;
					if (frame) {
						int line = frame->f_lineno;
						const char *filename = PyString_AsString(frame->f_code->co_filename); 
						const char *funcname = PyString_AsString(frame->f_code->co_name); 
						
						lpLogger->Log(EC_LOGLEVEL_ERROR, "  Python trace: %s(%d) %s", filename, line, funcname);
					} else { 
						lpLogger->Log(EC_LOGLEVEL_ERROR, "  Python trace: Unknown");
					}
					
					traceback = traceback->tb_next;
				}
			}

			PyErr_Clear();
		} 
		assert(false); 
		hr = S_FALSE; 
	}

	return hr;
}

#define PY_HANDLE_ERROR(logger, pyobj) { \
	hr = PyHandleError(logger, pyobj);\
	if (hr != hrSuccess) \
		return hr; \
}

#define PY_CALL_METHOD(pluginmanager, functionname, returnmacro, format, ...) {\
	PyObjectAPtr ptrResult;\
	{\
		ptrResult.reset(PyObject_CallMethod(pluginmanager, const_cast<char *>(functionname), const_cast<char *>(format), __VA_ARGS__)); \
		PY_HANDLE_ERROR(m_lpLogger, ptrResult)\
		\
		returnmacro\
	}\
}

/** 
 * Helper macro to parse the python return values which work together 
 * with the macro PY_CALL_METHOD.
 * 
 */
#define PY_PARSE_TUPLE_HELPER(format, ...) {\
	if(!PyArg_ParseTuple(ptrResult, format, __VA_ARGS__)) { \
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "  Wrong return value from the pluginmanager or plugin"); \
		PY_HANDLE_ERROR(m_lpLogger, NULL) \
	} \
}

PyMapiPlugin::~PyMapiPlugin(void)
{ 
	if (m_lpLogger != nullptr)
		m_lpLogger->Release();
}

/**
 * Initialize the PyMapiPlugin.
 *
 * @param[in]	lpConfig Pointer to the configuration class
 * @param[in]	lpLogger Pointer to the logger
 * @param[in]	lpPluginManagerClassName The class name of the plugin handler
 *
 * @return Standard mapi errorcodes
 */
HRESULT PyMapiPlugin::Init(ECLogger *lpLogger, PyObject *lpModMapiPlugin, const char* lpPluginManagerClassName, const char *lpPluginPath)
{
	HRESULT			hr = S_OK;
	PyObjectAPtr	ptrPyLogger;
	PyObjectAPtr	ptrClass;
	PyObjectAPtr	ptrArgs;
	std::string		strEnvPython;

	if (!lpModMapiPlugin)
		return S_OK;
	m_lpLogger = lpLogger;
	if (m_lpLogger)
		m_lpLogger->AddRef();

	// Init MAPI-swig types
	BUILD_SWIG_TYPE(type_p_IMessage, "_p_IMessage");
	BUILD_SWIG_TYPE(type_p_IMAPISession, "_p_IMAPISession");
	BUILD_SWIG_TYPE(type_p_IMsgStore, "_p_IMsgStore");
	BUILD_SWIG_TYPE(type_p_IAddrBook, "_p_IAddrBook");
	BUILD_SWIG_TYPE(type_p_IMAPIFolder, "_p_IMAPIFolder");
	BUILD_SWIG_TYPE(type_p_ECLogger, "_p_ECLogger");
	BUILD_SWIG_TYPE(type_p_IExchangeModifyTable, "_p_IExchangeModifyTable");

	// Init logger swig object
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyLogger, m_lpLogger, type_p_ECLogger);

	// Init plugin class	
	ptrClass.reset(PyObject_GetAttrString(lpModMapiPlugin, /*char* */lpPluginManagerClassName));
	PY_HANDLE_ERROR(m_lpLogger, ptrClass);
	ptrArgs.reset(Py_BuildValue("(sO)", lpPluginPath, ptrPyLogger.get()));
	PY_HANDLE_ERROR(m_lpLogger, ptrArgs);
	m_ptrMapiPluginManager.reset(PyObject_CallObject(ptrClass, ptrArgs));
	PY_HANDLE_ERROR(m_lpLogger, m_ptrMapiPluginManager);
	return hr;
}

/**
 * Plugin python call between MAPI and python.
 *
 * @param[in]	lpFunctionName	Python function name to call in the plugin framework. 
 * 								 The function must be exist the lpPluginManagerClassName defined in the init function.
 * @param[in] lpMapiSession		Pointer to a mapi session. Not NULL.
 * @param[in] lpAdrBook			Pointer to a mapi Addressbook. Not NULL.
 * @param[in] lpMsgStore		Pointer to a mapi mailbox. Can be NULL.
 * @param[in] lpInbox
 * @param[in] lpMessage			Pointer to a mapi message.
 *
 * @return Default mapi error codes
 *
 * @todo something with exit codes
 */
HRESULT PyMapiPlugin::MessageProcessing(const char *lpFunctionName, IMAPISession *lpMapiSession, IAddrBook *lpAdrBook, IMsgStore *lpMsgStore, IMAPIFolder *lpInbox, IMessage *lpMessage, ULONG *lpulResult)
{
	HRESULT hr = hrSuccess;
	PyObjectAPtr ptrPySession, ptrPyAddrBook, ptrPyStore, ptrPyMessage, ptrPyFolderInbox;

	if (!m_ptrMapiPluginManager)
		return hrSuccess;
	if (!lpFunctionName || !lpMapiSession || !lpAdrBook)
		return MAPI_E_INVALID_PARAMETER; 
	if (!m_ptrMapiPluginManager)
		return MAPI_E_CALL_FAILED;

	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPySession, lpMapiSession, type_p_IMAPISession)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyAddrBook, lpAdrBook, type_p_IAddrBook)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyStore, lpMsgStore, type_p_IMsgStore)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyFolderInbox, lpInbox, type_p_IMAPIFolder)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyMessage, lpMessage, type_p_IMessage)

	// Call the python function and get the (hr) return code back
	PY_CALL_METHOD(m_ptrMapiPluginManager, const_cast<char *>(lpFunctionName),
		PY_PARSE_TUPLE_HELPER("I", lpulResult), "OOOOO",
		ptrPySession.get(), ptrPyAddrBook.get(), ptrPyStore.get(),
		ptrPyFolderInbox.get(), ptrPyMessage.get());
	return hr;
}

/**
 * Hook for change the rules.
 *
 * @param[in] lpFunctionName	Python function name to hook the rules in the plugin framework. 
 * 								 The function must be exist the lpPluginManagerClassName defined in the init function.
 * @param[in] lpEMTRules		Pointer to a mapi IExchangeModifyTable object
 */
HRESULT PyMapiPlugin::RulesProcessing(const char *lpFunctionName, IMAPISession *lpMapiSession, IAddrBook *lpAdrBook, IMsgStore *lpMsgStore, IExchangeModifyTable *lpEMTRules, ULONG *lpulResult)
{
	HRESULT hr = hrSuccess;
    PyObjectAPtr  ptrPySession, ptrPyAddrBook, ptrPyStore, ptrEMTIn;

	if (!m_ptrMapiPluginManager)
		return hrSuccess;
	if (!lpFunctionName || !lpMapiSession || !lpAdrBook || !lpEMTRules)
		return MAPI_E_INVALID_PARAMETER;
	if (!m_ptrMapiPluginManager)
		return MAPI_E_CALL_FAILED;
	
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPySession, lpMapiSession, type_p_IMAPISession)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyAddrBook, lpAdrBook, type_p_IAddrBook)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyStore, lpMsgStore, type_p_IMsgStore)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrEMTIn, lpEMTRules, type_p_IExchangeModifyTable)

	PY_CALL_METHOD(m_ptrMapiPluginManager, const_cast<char *>(lpFunctionName),
		PY_PARSE_TUPLE_HELPER("I", lpulResult), "OOOO",
		ptrPySession.get(), ptrPyAddrBook.get(), ptrPyStore.get(),
		ptrEMTIn.get());
	return hr;
}

HRESULT PyMapiPlugin::RequestCallExecution(const char *lpFunctionName, IMAPISession *lpMapiSession, IAddrBook *lpAdrBook, IMsgStore *lpMsgStore,  IMAPIFolder *lpFolder, IMessage *lpMessage, ULONG *lpulDoCallexe, ULONG *lpulResult)
{
	HRESULT hr = hrSuccess;
	PyObjectAPtr  ptrPySession, ptrPyAddrBook, ptrPyStore, ptrFolder, ptrMessage;

	if (!m_ptrMapiPluginManager)
		return hrSuccess;
	if (!lpFunctionName || !lpMapiSession || !lpAdrBook || !lpulDoCallexe)
		return MAPI_E_INVALID_PARAMETER;
	if (!m_ptrMapiPluginManager)
		return MAPI_E_CALL_FAILED;

	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPySession, lpMapiSession, type_p_IMAPISession)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyAddrBook, lpAdrBook, type_p_IAddrBook)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrPyStore, lpMsgStore, type_p_IMsgStore)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrFolder, lpFolder, type_p_IMAPIFolder)
	NEW_SWIG_INTERFACE_POINTER_OBJ(ptrMessage, lpMessage, type_p_IMessage)

	PY_CALL_METHOD(m_ptrMapiPluginManager, const_cast<char *>(lpFunctionName),
		PY_PARSE_TUPLE_HELPER("I|I", lpulResult, lpulDoCallexe), "OOOOO",
		ptrPySession.get(), ptrPyAddrBook.get(), ptrPyStore.get(),
		ptrFolder.get(), ptrMessage.get());
	return hr;
}

PyMapiPluginFactory::~PyMapiPluginFactory()
{
	if (m_ptrModMapiPlugin != NULL) {
		m_ptrModMapiPlugin = NULL;
		Py_Finalize();
	}
	if (m_lpLogger != nullptr)
		m_lpLogger->Release();
}

HRESULT PyMapiPluginFactory::Init(ECConfig* lpConfig, ECLogger *lpLogger)
{
	HRESULT			hr = S_OK;
	std::string		strEnvPython;
	char			*lpEnvPython = NULL;
	PyObjectAPtr	ptrName;
	PyObjectAPtr	ptrModule;

	m_lpLogger = lpLogger;
	if (m_lpLogger)
		m_lpLogger->AddRef();

	m_bEnablePlugin = parseBool(lpConfig->GetSetting("plugin_enabled", NULL, "no"));
	if (!m_bEnablePlugin)
		return S_OK;

	m_strPluginPath = lpConfig->GetSetting("plugin_path");

	strEnvPython = lpConfig->GetSetting("plugin_manager_path");

	lpEnvPython = getenv("PYTHONPATH");
	if (lpEnvPython) 
		strEnvPython += std::string(":") + lpEnvPython;

	setenv("PYTHONPATH", strEnvPython.c_str(), 1);

	lpLogger->Log(EC_LOGLEVEL_DEBUG, "PYTHONPATH = %s", strEnvPython.c_str());

	Py_Initialize();
	ptrModule.reset(PyImport_ImportModule("MAPI"));
	PY_HANDLE_ERROR(m_lpLogger, ptrModule);

	// Import python plugin framework
	// @todo error unable to find file xxx
	ptrName.reset(PyString_FromString("mapiplugin"));
	m_ptrModMapiPlugin.reset(PyImport_Import(ptrName));
	PY_HANDLE_ERROR(m_lpLogger, m_ptrModMapiPlugin);
	return hr;
}

HRESULT PyMapiPluginFactory::CreatePlugin(const char* lpPluginManagerClassName, PyMapiPlugin **lppPlugin)
{
	HRESULT hr = S_OK;
	std::unique_ptr<PyMapiPlugin> lpPlugin(new(std::nothrow) PyMapiPlugin);
	if (lpPlugin == nullptr)
		return MAPI_E_NOT_ENOUGH_MEMORY;
	hr = lpPlugin->Init(m_lpLogger, m_ptrModMapiPlugin, lpPluginManagerClassName, m_strPluginPath.c_str());
	if (hr != S_OK)
		return hr;
	*lppPlugin = lpPlugin.release();
	return S_OK;
}
