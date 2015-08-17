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

#ifndef _PYMAPIPLUGIN_H
#define _PYMAPIPLUGIN_H

#include <Python.h>
#include "ECLogger.h"
#include "ECConfig.h"
#include "PythonSWIGRuntime.h"
#include <edkmdb.h>

#include <auto_free.h>

inline void my_DECREF(PyObject *obj) {
	Py_DECREF(obj);
}

//@fixme wrong name, autofree should be auto_decref
typedef auto_free<PyObject, auto_free_dealloc<PyObject*, void, my_DECREF> >PyObjectAPtr;

#define MAKE_CUSTOM_SCODE(sev,fac,code) \
				(((unsigned int)(sev)<<31) | ((unsigned int)(1)<<29) | ((unsigned int)(fac)<<16) | ((unsigned int)(code)))

#define MAPI_E_MP_STOP		MAKE_CUSTOM_SCODE(1, FACILITY_ITF, 0x1)

#define MP_CONTINUE			0	// Continue with the next hook
#define MP_FAILED			1	// Whole process failed
#define MP_STOP_SUCCESS		2	// Stop with the message processing go to the next recipient. Recpient return code OK
#define MP_STOP_FAILED		3	// Stop with the message processing go to the next recipient. Recpient return code failed
#define MP_EXIT				4	// Exit the all the hook calls and go futher with the mail process.
#define MP_RETRY_LATER		5	// Stop Process and retry later


class PyMapiPlugin
{
public:
	PyMapiPlugin(void);
	virtual ~PyMapiPlugin(void);

	HRESULT Init(ECLogger *lpLogger, PyObject *lpModMapiPlugin, const char* lpPluginManagerClassName, const char *lpPluginPath);
	HRESULT MessageProcessing(const char *lpFunctionName, IMAPISession *lpMapiSession, IAddrBook *lpAdrBook, IMsgStore *lpMsgStore, IMAPIFolder *lpInbox, IMessage *lpMessage, ULONG *lpulResult);
	HRESULT RulesProcessing(const char *lpFunctionName, IMAPISession *lpMapiSession, IAddrBook *lpAdrBook, IMsgStore *lpMsgStore, IExchangeModifyTable *lpEMTRules, ULONG *lpulResult);
	HRESULT RequestCallExecution(const char *lpFunctionName, IMAPISession *lpMapiSession, IAddrBook *lpAdrBook, IMsgStore *lpMsgStore,  IMAPIFolder *lpFolder, IMessage *lpMessage, ULONG *lpulDoCallexe, ULONG *lpulResult);

    swig_type_info *type_p_ECLogger;
	swig_type_info *type_p_IAddrBook;
	swig_type_info *type_p_IMAPIFolder;
	swig_type_info *type_p_IMAPISession;
	swig_type_info *type_p_IMsgStore;
	swig_type_info *type_p_IMessage;
	swig_type_info *type_p_IExchangeModifyTable;

private:
	PyObjectAPtr m_ptrMapiPluginManager;
	ECLogger *m_lpLogger;

private:
	// Inhibit (accidental) copying
	PyMapiPlugin(const PyMapiPlugin &);
	PyMapiPlugin& operator=(const PyMapiPlugin &);
};



class PyMapiPluginFactory
{
public:
	PyMapiPluginFactory();
	HRESULT Init(ECConfig* lpConfig, ECLogger *lpLogger);
	~PyMapiPluginFactory();

	HRESULT CreatePlugin(const char* lpPluginManagerClassName, PyMapiPlugin **lppPlugin);

private:
	PyObjectAPtr m_ptrModMapiPlugin;
	bool m_bEnablePlugin;
	std::string m_strPluginPath;
	ECLogger *m_lpLogger;

private:
	// Inhibit (accidental) copying
	PyMapiPluginFactory(const PyMapiPluginFactory &);
	PyMapiPluginFactory& operator=(const PyMapiPluginFactory &);
};



inline void my_delete(PyMapiPlugin *obj) { delete obj; }

typedef auto_free<PyMapiPlugin, auto_free_dealloc<PyMapiPlugin*, void, my_delete> >PyMapiPluginAPtr;


#endif
