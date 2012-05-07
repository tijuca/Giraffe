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

#ifndef IMAPIOFFLINE_INCLUDED
#define IMAPIOFFLINE_INCLUDED

#include <mapix.h>
#include <mapidefs.h>

/* interface IMAPIOfflineNotify, Notify */

#define MAPIOFFLINE_ADVISE_DEFAULT				(ULONG)0 
#define MAPIOFFLINE_UNADVISE_DEFAULT			(ULONG)0 
#define MAPIOFFLINE_ADVISE_TYPE_STATECHANGE		1
/* GetCapabilities */
#define MAPIOFFLINE_CAPABILITY_OFFLINE			0x1 
#define MAPIOFFLINE_CAPABILITY_ONLINE			0x2 

/* SetCurrentState */
#define MAPIOFFLINE_FLAG_BLOCK					0x00002000 
#define MAPIOFFLINE_FLAG_DEFAULT				0x00000000 

  
#define MAPIOFFLINE_STATE_ALL					0x003f037f  
  
/* GetCurrentState and SetCurrentState */
#define MAPIOFFLINE_STATE_OFFLINE_MASK			0x00000003  
#define MAPIOFFLINE_STATE_OFFLINE				0x00000001  
#define MAPIOFFLINE_STATE_ONLINE				0x00000002 


typedef enum 
{ 
     MAPIOFFLINE_CALLBACK_TYPE_NOTIFY = 0
} MAPIOFFLINE_CALLBACK_TYPE;


// The MAPIOFFLINE_NOTIFY_TYPE of a notification identifies if a change in the connection state is going to take place, is taking place, or has completed. 
typedef enum {
	MAPIOFFLINE_NOTIFY_TYPE_STATECHANGE_START = 1, 
	MAPIOFFLINE_NOTIFY_TYPE_STATECHANGE = 2, 
	MAPIOFFLINE_NOTIFY_TYPE_STATECHANGE_DONE = 3 
} MAPIOFFLINE_NOTIFY_TYPE;

typedef struct
{
	ULONG				ulSize;					// The size of MAPIOFFLINE_ADVISEINFO
	ULONG				ulClientToken;			// A token defined by the client about a callback. It is the ulClientToken member of the MAPIOFFLINE_NOTIFY structure passed to IMAPIOfflineNotify::Notify.
	MAPIOFFLINE_CALLBACK_TYPE	CallbackType;	// Type of callback to make.
	IUnknown*			pCallback;				// Interface to use for callback. This is the client's implementation of IMAPIOfflineNotify.
	ULONG				ulAdviseTypes;			//The types of advise, as identified by the condition for advising. The only supported type is MAPIOFFLINE_ADVISE_TYPE_STATECHANGE.
	ULONG				ulStateMask;			// The only supported state is MAPIOFFLINE_STATE_ALL
} MAPIOFFLINE_ADVISEINFO;

typedef struct 
{
	ULONG ulSize;								// Size of the MAPIOFFLINE_NOTIFY structure.		
	MAPIOFFLINE_NOTIFY_TYPE NotifyType;			// Type of notification
	ULONG ulClientToken;						// A token defined by the client in the MAPIOFFLINE_ADVISEINFO structure in IMAPIOfflineMgr::Advise
	union {
		struct
		{
			ULONG ulMask;						// The part of the connection state that has changed.
			ULONG ulStateOld;					// The old connection state
			ULONG ulStateNew;					// The new connection state
		} StateChange;
	} Info;
} MAPIOFFLINE_NOTIFY;


/*
Provides information for an offline object.

Class: IMAPIOffline
Inherits from: IUnknown 
Provided by: Outlook 
Interface identifier: IID_IMAPIOffline

Remarks:
The client must implement this interface and pass a pointer to it as a member in MAPIOFFLINE_ADVISEINFO when setting up callbacks using IMAPIOfflineMgr::Advise. Subsequently, Outlook will be able to use this interface to send notification callbacks to the client.
*/
class IMAPIOffline : public IUnknown
{
public:
	/*
	Sets the current state of an offline object to online or offline.

	Parameters:
		ulFlags 
			[in] Modifies the behavior of this call. The supported values are:
				MAPIOFFLINE_FLAG_BLOCK
					Setting ulFlags to this value will block the SetCurrentState call until the state change is complete. By default the transition takes place asynchronously. When the transition is occuring asynchronously, all SetCurrentState calls will return E_PENDING until the change is complete.
				MAPIOFFLINE_FLAG_DEFAULT
					Sets the current state without blocking.
		ulMask 
			[in] The part of the state to change. The only supported value is MAPIOFFLINE_STATE_OFFLINE_MASK.
		ulState 
			[in] The state to change to. It must be one of these two values:
				MAPIOFFLINE_STATE_ONLINE
				MAPIOFFLINE_STATE_OFFLINE
		pReserved
			This parameter is reserved for Outlook internal use and is not supported. 

	Return Values:
		S_OK
			The state of the offline object has been changed successfully.
		E_PENDING
			This indicates that the state of the offline object is changing asynchronously. This occurs when ulFlags is set
			to MAPIOFFLINE_FLAG_BLOCK in an earlier SetCurrentState call, and any subsequent SetCurrentState call will return 
			this value until the asynchronous state change is complete.
	*/
	virtual HRESULT __stdcall SetCurrentState(ULONG ulFlags, ULONG ulMask, ULONG ulState, void* pReserved) = 0;

	/*
	Gets the conditions for which callbacks are supported by an offline object.
	
	Parameters:
		pulCapablities
			[out] A bitmask of the following capability flags:
				MAPIOFFLINE_CAPABILITY_OFFLINE
					The offline object is capable of providing offline notifications.
				MAPIOFFLINE_CAPABILITY_ONLINE
					The offline object is capable of providing online notifications.
	Return Values:
		Unknown
	Remarks
		Upon opening an offline object using HrOpenOfflineObj, a client can query on IMAPIOfflineMgr to obtain a pointer 
		to an IMAPIOffline interface, and call IMAPIOffline::GetCapabilities to find out the callbacks supported by the object.
		The client can then choose to set up callbacks by using IMAPIOfflineMgr.

		Note that, depending on the mail server for an offline object, an object that supports callbacks for going online does
		not necessarily support callbacks for going offline.

		Also note that, while an offline object may support callbacks for changes other than online/offline, the Offline State
		API supports only online/offline changes, and clients must check for only such capabilities.

	*/
	virtual HRESULT __stdcall GetCapabilities(ULONG *pulCapabilities) = 0;

	/*
	Gets the current online or offline state of an offline object.

	Parameters:
		pulState
			[out] The current online or offline state of an offline object. It must be one of these two values:
				MAPIOFFLINE_STATE_ONLINE
				MAPIOFFLINE_STATE_OFFLINE
	Return Values:
		Unknown
	*/
	virtual HRESULT __stdcall GetCurrentState(ULONG* pulState) = 0;

	virtual HRESULT __stdcall Placeholder1(void* ulTest) = 0;
};

/*
Supports Outlook sending notification callbacks to a client.

Class: IMAPIOfflineNotify
Inherits from: IUnknown 
Provided by: Outlook 
Interface identifier: IID_IMAPIOfflineNotify 

Remarks:
The client must implement this interface and pass a pointer to it as a member in MAPIOFFLINE_ADVISEINFO when setting up callbacks using IMAPIOfflineMgr::Advise. Subsequently, Outlook will be able to use this interface to send notification callbacks to the client.
*/

class IMAPIOfflineNotify : public IUnknown
{
public:
	/*
	Sends notifications to the client about changes in connection state.

	Parameters:
		pNotifyInfo 
			[in] The notification that Outlook sends to the client. The notification indicates the part of the connection 
				state that has changed, the old connection state, and the new connection state.
	Return Values:
		none
	Remarks:
		Outlook uses this method to send notification callbacks to a client. To make this interface available to Outlook, 
		the client must implement this interface and pass a pointer to it as a member in MAPIOFFLINE_ADVISEINFO when 
		setting up callbacks using IMAPIOfflineMgr::Advise. 

		The client also passes to MAPIOFFLINE_ADVISEINFO a client token that Outlook uses in IMAPIOfflineNotify::Notify to 
		identify the client registered for the notification callback.

		In general, Outlook can notify a client of online/offline changes and other connection state changes, but the Offline
		State API supports only notifications for online/offline changes. The client must ignore all other notifications.
	*/
	virtual void __stdcall Notify(const MAPIOFFLINE_NOTIFY *pNotifyInfo) = 0;
};


/*
Supports registering for notification callbacks about connection state changes of a user account.

Class: IMAPIOfflineMgr
Inherits from: IMAPIOffline 
Provided by: Outlook 
Interface identifier: IID_IMAPIOfflineMgr 

Remarks:
	Upon opening an offline object for a user account profile using HrOpenOfflineObj, a client obtains an 
	offline object that supports IMAPIOfflineMgr.

	Because this interface inherits from IUnknown, the client can query this interface (by using IUnknown::QueryInterface)
	to obtain an object that supports IMAPIOffline. The client can then find out about the callback capabilities of the 
	offline object (by calling IMAPIOffline::GetCapabilities), and choose to set up callbacks (by using IMAPIOfflineMgr::Advise).

*/
class IMAPIOfflineMgr : public IMAPIOffline
{
public:
	/*
	Registers a client to receive callbacks on an offline object.

	Parameters:
		ulFlags 
			[in] Flags that modify behavior. Only the value MAPIOFFLINE_ADVISE_DEFAULT is supported.
		pAdviseInfo
			[in] Information about the type of callback, when to receive a callback, a callback interface for the caller, 
				and other details. It also contains a client token that Outlook uses in sending subsequent 
				notification callbacks to the client caller.
		pulAdviseToken
			[out] An advise token returned to the client caller for subsequently canceling callback for the object.
	
	Return Values:
		S_OK 
			The call was successful.
		E_INVALIDARG 
			An invalid parameter has been specified.
		E_NOINTERFACE 
			The callback interface specified in pAdviseInfo is not valid.
	Remarks:
		Upon opening an offline object using HrOpenOfflineObj, a client obtains an offline object that supports IMAPIOfflineMgr. 
		The client can check for the kinds of callbacks supported by the object by using IMAPIOffline::GetCapabilities. 
		The client can determine the type and other details about the callback it wants, and then call IMAPIOfflineMgr::Advise 
		to register to receive such callbacks about the object.
	*/
	virtual  HRESULT __stdcall Advise(ULONG ulFlags, MAPIOFFLINE_ADVISEINFO* pAdviseInfo, ULONG* pulAdviseToken) = 0;

	/*
	Cancels callbacks for an offline object.

	Parameters:
		ulFlags 
			[in] Flags for canceling callback. Only the value MAPIOFFLINE_UNADVISE_DEFAULT is supported.
		ulAdviseToken 
			[in] An advise token that identifies the callback registration that is to be canceled.

	Return Values:
		S_OK 
			The call was successful.

	Remarks:
		Removes the registration for the callback that was associated with ulAdviseToken returned from a prior call to 
		IMAPIOfflineMgr::Advise. Causes the IMAPIOfflineMgr object to release its reference on the IMAPIOfflineNotify 
		object associated with ulAdviseToken.
	*/
	virtual HRESULT __stdcall Unadvise(ULONG ulFlags,ULONG ulAdviseToken) = 0;
	virtual HRESULT __stdcall Placeholder2() = 0;
	virtual HRESULT __stdcall Placeholder3() = 0;
	virtual HRESULT __stdcall Placeholder4() = 0;
	virtual HRESULT __stdcall Placeholder5() = 0;
	virtual HRESULT __stdcall Placeholder6() = 0;
	virtual HRESULT __stdcall Placeholder7() = 0;
	virtual HRESULT __stdcall Placeholder8() = 0;

};

/*
Opens an offline object based on a given profile.

Parameters:
	ulReserved 
		[in] This parameter is not used. It must be 0. 

	pwszProfileNameIn
		[in] The name of the profile that the offline object is for. It must be expressed in Unicode. 
	pGUID 
		[in] Pointer to a GUID which can be used to uniquely identify this object from other offline objects.
			It must be GUID_GlobalState.
	pReserved 
		[in] This parameter is not used. It must be NULL. 
	ppOfflineObj 
		[out] A pointer to the requested offline object. The caller can use this pointer to access the 
			IMAPIOfflineMgr interface to find out the callbacks that this object supports and to set up callbacks for it.

Return Values:
	S_OK 
		The function call is successful.

Remarks:
	This is the first call that a client makes when the client wants to be notified of any connection state changes for 
	a given profile. Upon calling HrOpenOfflineObj, the client obtains an offline object that supports IMAPIOfflineMgr.
	The client can check for the kinds of callbacks supported by the object (by using IMAPIOffline::GetCapabilities),
	and then set up callbacks for it (by using IMAPIOfflineMgr::Advise).

	When using GetProcAddress to look for the address of this function in msmapi32.dll, specify HrOpenOfflineObj@20 as the procedure name.  
*/

#endif //IMAPIOFFLINE_INCLUDED
