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

#ifndef PLATFORM_LINUX_H
#define PLATFORM_LINUX_H

/*
 * Functions and (type)definitions that are needed for the Linux platform
 *
 */
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <string.h>		/* memset, etc.. */
#include <ctype.h>		/* 64bit int definition */
#include <dlfcn.h>
#include <stddef.h>
#include <libgen.h>
#include <byteswap.h>
#include <endian.h>
#include <errno.h>
#include <locale.h>

#include <string>

#ifdef HAVE_UNISTD_H
  #include <unistd.h>
#endif

#ifndef HAVE_UUID_CREATE
  #include <uuid/uuid.h>
#else
  #include <uuid.h>
#endif

#ifndef UNUSED_VAR
#define UNUSED_VAR __attribute__((unused))
#endif

#include <assert.h>
/* windows compatible asserts ? */
#define ASSERT assert
#define _ASSERT assert

#ifdef _DEBUG
  #error _DEBUG is defined in linux, which should not be used!
#endif

#define _vsnprintf vsnprintf
#define _snprintf snprintf
#ifdef HAVE_VSNPRINTF_L
#define _vsnprintf_l vsnprintf_l
#else
inline int _vsnprintf_l(char *str, size_t size, const char *format, locale_t locale, va_list ap) {
	locale_t prev = uselocale(locale);
	int ret = _vsnprintf(str, size, format, ap);
	uselocale(prev);
	return ret;
}
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/times.h>

/* compiler definitions */
#define __stdcall
#define __cdecl
#define FAR

#define STDAPI_(__type) __type __stdcall

/* Defines for some windows function that have a different name in Linux */
#define _snprintf snprintf
#define strnicmp strncasecmp
#define stricmp strcasecmp

#define _atoi64 atoll

#define CreateDir(dir, mode) mkdir(dir, mode)

/* base types */
typedef void			VOID;
typedef unsigned char	BYTE;
typedef unsigned char	UCHAR;
typedef signed char		CHAR;
typedef BYTE*			LPBYTE;
typedef unsigned short	WORD;
typedef WORD*			LPWORD;
typedef unsigned short	USHORT;
typedef USHORT*			LPUSHORT;
typedef unsigned int	DWORD;
typedef DWORD*			LPDWORD;
typedef unsigned int	UINT;
typedef UINT*			LPUINT;
typedef unsigned int	ULONG;
typedef ULONG*			LPULONG;
typedef long long		LONGLONG;
typedef LONGLONG*		LPLONGLONG;
typedef unsigned long long	ULONGLONG;
typedef ULONGLONG*		LPULONGLONG;

typedef int	LONG;
typedef int	BOOL;

typedef void* LPVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef const char* LPCSTR;

typedef void* HGLOBAL;

typedef __int64_t __int64;

#ifndef FALSE
  #define FALSE	0
#endif
#ifndef TRUE
  #define TRUE	1
#endif

/* This is a workaround for warnings in offsetof from stddef.h */
#define offsetof1(TYPE, MEMBER) ((size_t) (&((TYPE *)1)->MEMBER)-1)

/* find parent class */
#define METHOD_PROLOGUE_(theClass, localClass) \
	UNUSED_VAR theClass* pThis = \
		((theClass*)((BYTE*)this - offsetof1(theClass, m_x##localClass)));

/* GUID defines */
typedef struct __attribute__((__packed__)) _s_GUID {
    DWORD	Data1;
    WORD	Data2;
    WORD	Data3;
    BYTE	Data4[8];
} GUID;
typedef GUID*  LPGUID;
typedef const GUID *LPCGUID;

typedef GUID   IID;
typedef IID*   LPIID;

typedef GUID   CLSID;
typedef CLSID* LPCLSID;

typedef GUID   FMTID;
typedef FMTID* LPFMTID;

#define REFGUID  const GUID &
#define REFIID   const IID &
#define REFCLSID const CLSID &
#define REFFMTID const FMTID &

typedef GUID  UUID;		// needed? existing?


/* See initguid.h for the real defines */
#ifndef INITGUID
/* # ifdef __cplusplus */
/* #   define EXTERN_C extern "C" */
/* # else */
/* #   define EXTERN_C extern */
/* # endif */
  #define GUID_EXT extern "C"
  #define DEFINE_GUID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) GUID_EXT const GUID n
#endif

#define DEFINE_OLEGUID(n,l,w1,w2) DEFINE_GUID(n,l,w1,w2,0xC0,0,0,0,0,0,0,0x46)


//
//  Code Page Default Values.
//
#define CP_ACP                    0           // default to ANSI code page
#define CP_OEMCP                  1           // default to OEM  code page
#define CP_MACCP                  2           // default to MAC  code page
#define CP_THREAD_ACP             3           // current thread's ANSI code page
#define CP_SYMBOL                 42          // SYMBOL translations

#define CP_UTF7                   65000       // UTF-7 translation
#define CP_UTF8                   65001       // UTF-8 translation

//
//  MBCS and Unicode Translation Flags.
//
#define MB_PRECOMPOSED            0x00000001  // use precomposed chars
#define MB_COMPOSITE              0x00000002  // use composite chars
#define MB_USEGLYPHCHARS          0x00000004  // use glyph chars, not ctrl chars
#define MB_ERR_INVALID_CHARS      0x00000008  // error for invalid chars

//
// IStream flags
//

#define STGM_DIRECT             0x00000000L
#define STGM_TRANSACTED         0x00010000L
#define STGM_SIMPLE             0x08000000L

#define STGM_READ               0x00000000L
#define STGM_WRITE              0x00000001L
#define STGM_READWRITE          0x00000002L

#define STGM_SHARE_DENY_NONE    0x00000040L
#define STGM_SHARE_DENY_READ    0x00000030L
#define STGM_SHARE_DENY_WRITE   0x00000020L
#define STGM_SHARE_EXCLUSIVE    0x00000010L

#define STGM_PRIORITY           0x00040000L
#define STGM_DELETEONRELEASE    0x04000000L
#define STGM_NOSCRATCH          0x00100000L

#define STGM_CREATE             0x00001000L
#define STGM_CONVERT            0x00020000L
#define STGM_FAILIFTHERE        0x00000000L

#define STGM_NOSNAPSHOT         0x00200000L
#define STGM_DIRECT_SWMR        0x00400000L

/* correct? */
typedef unsigned int	HINSTANCE;
typedef unsigned int	HANDLE;
typedef unsigned int	HWND;
typedef int				HRESULT;
typedef int				SCODE;

#ifndef MAX_PATH
  #define MAX_PATH	255
#endif

#define SEVERITY_SUCCESS	0
#define SEVERITY_ERROR		1
#define IS_ERROR(scode)	((unsigned int)(scode) >> 31 == SEVERITY_ERROR)
#define FAILED(scode)	((unsigned int)(scode) >> 31)
#define SUCCEEDED(scode) (!FAILED(scode))

/* from winerror.h, same as MAKE_MAPI_SCODE */
#define MAKE_SCODE(sev,fac,code) \
    ((SCODE) (((ULONG)(sev)<<31) | ((ULONG)(fac)<<16) | ((ULONG)(code))) )

/* success codes */
#define S_OK		0
#define S_FALSE		1

/* predefined hresult errors */
#define E_UNEXPECTED ((HRESULT)0x8000FFFF)
#define E_NOTIMPL ((HRESULT)0x80004001)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_POINTER ((HRESULT)0x80004003)
#define E_HANDLE ((HRESULT)0x80070006)
#define E_ABORT ((HRESULT)0x80004004)
#define E_FAIL ((HRESULT)0x80004005)
#define E_ACCESSDENIED ((HRESULT)0x80070005)

/* winerror.h definitions */
#define _HRESULT_TYPEDEF_(_sc) ((HRESULT)_sc)
#define STG_E_INVALIDFUNCTION            _HRESULT_TYPEDEF_(0x80030001L)


/*
 Values are 32 bit values layed out as follows:

  3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 +---+-+-+-----------------------+-------------------------------+
 |Sev|C|R|     Facility          |               Code            |
 +---+-+-+-----------------------+-------------------------------+

 where

     Sev - is the severity code

         00 - Success
         01 - Informational
         10 - Warning
         11 - Error

     C - is the Customer code flag

     R - is a reserved bit

     Facility - is the facility code

     Code - is the facility's status code
*/
#define FACILITY_NULL		0x0000
#define FACILITY_RPC		0x0001
#define FACILITY_DISPATCH	0x0002
#define FACILITY_STORAGE	0x0003
#define FACILITY_ITF		0x0004 /* only used in mapi programs */
#define FACILITY_WIN32		0x0007
#define FACILITY_WINDOWS	0x0008
#define FACILITY_SSPI		0x0009
#define FACILITY_CONTROL	0x000A

/* mapi.h (and more) types */
typedef unsigned int	FLAGS;

typedef unsigned int	LHANDLE;
typedef unsigned int*	LPLHANDLE;


/* mapidefs.h (and more) types */
typedef wchar_t 	WCHAR;

#ifdef UNICODE
  typedef WCHAR		TCHAR;
#define _T(x) L##x
#else
  typedef char		TCHAR;
#define _T(x) x
#endif
typedef unsigned char	TBYTE;

typedef WCHAR*			LPWSTR;
typedef const WCHAR*	LPCWSTR;
typedef TCHAR*			LPTSTR;
typedef const TCHAR*	LPCTSTR;
typedef WCHAR			OLECHAR;

typedef struct _s_FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *LPFILETIME;
#define NANOSECS_BETWEEN_EPOCHS 116444736000000000LL

/* made up .. seems correct */
typedef union _LARGE_INTEGER {
#ifdef __GNUC__
  __extension__ struct {
#else
  struct {
#endif
    DWORD LowPart;
    LONG HighPart;
  };
  struct {
    DWORD LowPart;
    LONG HighPart;
  } u;
  LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
#ifdef __GNUC__
  __extension__ struct {
#else
  struct {
#endif
    DWORD LowPart;
    DWORD HighPart;
  };
  struct {
    DWORD LowPart;
    DWORD HighPart;
  } u;
  ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

/* #define lpszA	char* */
/* #define lppszA	char** */
/* #define MVszA	char** */

/* #define lpszW	WCHAR* */
/* #define lppszW	WCHAR** */
/* #define MVszW	WCHAR** */

// [MS-DTYP].pdf
typedef struct SYSTEMTIME {
	WORD wYear;
	WORD wMonth;
	WORD wDayOfWeek;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME;


/* IUnknown Interface */
class IUnknown {
public:
    virtual ~IUnknown() {};

    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT QueryInterface(REFIID refiid, void **lpvoid) = 0;
};
typedef IUnknown* LPUNKNOWN;

/* IStream Interface */
class ISequentialStream : public IUnknown {
public:
    virtual HRESULT Read(void *pv, ULONG cb, ULONG *pcbRead) = 0;
    virtual HRESULT Write(const void *pv, ULONG cb, ULONG *pcbWritten) = 0;
};

typedef struct tagSTATSTG {
    LPSTR pwcsName;		// was LPOLESTR .. wtf is that?
    DWORD type;
    ULARGE_INTEGER cbSize;
    FILETIME mtime;
    FILETIME ctime;
    FILETIME atime;
    DWORD grfMode;
    DWORD grfLocksSupported;
    CLSID clsid;
    DWORD grfStateBits;
    DWORD reserved;
} STATSTG;

typedef enum tagSTGTY {
    STGTY_STORAGE	= 1,
    STGTY_STREAM	= 2,
    STGTY_LOCKBYTES	= 3,
    STGTY_PROPERTY	= 4
} STGTY;

typedef enum tagSTREAM_SEEK {
    STREAM_SEEK_SET	= 0,
    STREAM_SEEK_CUR	= 1,
    STREAM_SEEK_END	= 2
} STREAM_SEEK;

typedef enum tagLOCKTYPE {
    LOCK_WRITE		= 1,
    LOCK_EXCLUSIVE	= 2,
    LOCK_ONLYONCE	= 4
} LOCKTYPE;

typedef enum tagSTATFLAG {
    STATFLAG_DEFAULT	= 0,
    STATFLAG_NONAME	= 1,
    STATFLAG_NOOPEN	= 2
} STATFLAG;

class IEnumSTATSTG : public IUnknown {
public:
	virtual HRESULT Next(ULONG celt, STATSTG *rgelt, ULONG *pceltFetched) = 0;
	virtual HRESULT Skip(ULONG celt) = 0;
	virtual HRESULT Reset(void) = 0;
	virtual HRESULT Clone(IEnumSTATSTG **ppenum) = 0;
};
#if !defined(INITGUID) || defined(USES_IID_IEnumSTATSTG)
  DEFINE_OLEGUID(IID_IEnumSTATSTG,0x0D,0,0);
#endif

class IStream : public ISequentialStream {
public:
    virtual HRESULT Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition) = 0;
    virtual HRESULT SetSize(ULARGE_INTEGER libNewSize) = 0;
    virtual HRESULT CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten) = 0;
    virtual HRESULT Commit(DWORD grfCommitFlags) = 0;
    virtual HRESULT Revert(void) = 0;
    virtual HRESULT LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) = 0;
    virtual HRESULT UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) = 0;
    virtual HRESULT Stat(STATSTG *pstatstg, DWORD grfStatFlag) = 0;
    virtual HRESULT Clone(IStream **ppstm) = 0;
};
typedef IStream* LPSTREAM;
extern "C" const IID IID_IStream;		/* and the contents are .... somewhere else */
extern "C" const IID IID_ISequentialStream;
extern "C" const IID IID_IStorage;

class IMalloc : public IUnknown {
public:
    virtual void* Alloc(ULONG cb) = 0;
    virtual void* Realloc(void* pv, ULONG cb) = 0;
    virtual void Free(void* pv) = 0;
    virtual ULONG GetSize(void* pv) = 0;
    virtual int DidAlloc(void *pv) = 0;
    virtual void HeapMinimize(void) = 0;
};
typedef IMalloc* LPMALLOC;

// typedef struct tagRemSNB {
// unsigned long ulCntStr;
//     unsigned long ulCntChar;
//     //OLECHAR rgString[ 1 ];
//     } RemSNB;
// typedef RemSNB* wireSNB;
typedef OLECHAR** SNB;


class IStorage : public IUnknown {
public:
	virtual HRESULT CreateStream(const OLECHAR *pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStream **ppstm) = 0;

	virtual HRESULT OpenStream(const OLECHAR *pwcsName, void *reserved1, DWORD grfMode, DWORD reserved2, IStream **ppstm) = 0;
	virtual HRESULT CreateStorage(const OLECHAR *pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStorage **ppstg) = 0;
	virtual HRESULT OpenStorage(const OLECHAR *pwcsName, IStorage *pstgPriority, DWORD grfMode, SNB snbExclude, DWORD reserved,
								IStorage **ppstg) = 0;
	virtual HRESULT CopyTo(DWORD ciidExclude, const IID *rgiidExclude, SNB snbExclude, IStorage *pstgDest) = 0;
	virtual HRESULT MoveElementTo(const OLECHAR *pwcsName, IStorage *pstgDest, const OLECHAR *pwcsNewName, DWORD grfFlags) = 0;
	virtual HRESULT Commit(DWORD grfCommitFlags) = 0; 
	virtual HRESULT Revert(void) = 0;
	virtual HRESULT EnumElements(DWORD reserved1, void *reserved2, DWORD reserved3, IEnumSTATSTG **ppenum) = 0; 
	virtual HRESULT DestroyElement(const OLECHAR *pwcsName) = 0;
	virtual HRESULT RenameElement(const OLECHAR *pwcsOldName, const OLECHAR *pwcsNewName) = 0;
	virtual HRESULT SetElementTimes(const OLECHAR *pwcsName, const FILETIME *pctime, const FILETIME *patime, const FILETIME *pmtime) = 0;
	virtual HRESULT SetClass(REFCLSID clsid) = 0;
	virtual HRESULT SetStateBits(DWORD grfStateBits, DWORD grfMask) = 0;
	virtual HRESULT Stat(STATSTG *pstatstg, DWORD grfStatFlag) = 0;
};
typedef IStorage* LPSTORAGE;


/*
  extra typedefs used in following interfaces
  since these are not used, we can define them as void, instead of the large struct it is in WIN32
*/
typedef DWORD LCID;
typedef long DISPID;
typedef void VARIANT;
typedef void DISPPARAMS;
typedef OLECHAR* BSTR;
class ITypeInfo : public IUnknown {
};
typedef void EXCEPINFO;
typedef void SENS_QOCINFO;
typedef SENS_QOCINFO* LPSENS_QOCINFO;

/* IDispatch Interface */
class IDispatch : public IUnknown {
public:
	virtual HRESULT GetTypeInfoCount(unsigned int FAR* pctinfo) = 0;
	virtual HRESULT GetTypeInfo(unsigned int iTInfo, LCID lcid, ITypeInfo FAR* FAR* ppTInfo) = 0;
	virtual HRESULT GetIDsOfNames(REFIID riid, OLECHAR FAR* FAR* rgszNames, unsigned int cNames, LCID lcid, DISPID FAR* rgDispId) = 0;
	virtual HRESULT Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS FAR* pDispParams, VARIANT FAR* parResult, EXCEPINFO FAR* pExcepInfo, unsigned int FAR* puArgErr) = 0;
};

/* ISensNetwork Interface */
class ISensNetwork : public IDispatch {
public:
	virtual HRESULT ConnectionMade(BSTR bstrConnection,ULONG ulType, LPSENS_QOCINFO lpQOCInfo) = 0;
	virtual HRESULT ConnectionMadeNoQOCInfo(BSTR bstrConnection,ULONG ulType) = 0;
	virtual HRESULT ConnectionLost(BSTR bstrConnection, ULONG ulType) = 0;
	virtual HRESULT DestinationReachable(BSTR bstrDestination, BSTR bstrConnection, ULONG ulType, LPSENS_QOCINFO lpQOCInfo) = 0;
	virtual HRESULT DestinationReachableNoQOCInfo(BSTR bstrDestination, BSTR bstrConnection, ULONG ulType) = 0;
};

class IEventSystem : public IDispatch {
public:
	virtual HRESULT Query(BSTR progID, BSTR queryCriteria, int *errorIndex, IUnknown **ppInterface) = 0;
	virtual HRESULT Store(BSTR ProgID, IUnknown *pInterface) = 0;
	virtual HRESULT Remove(BSTR progID, BSTR queryCriteria, int *errorIndex) = 0;
	virtual HRESULT get_EventObjectChangeEventClassID(BSTR *pbstrEventClassID) = 0;
	virtual HRESULT QueryS(BSTR progID, BSTR queryCriteria, IUnknown **ppInterface) = 0;
	virtual HRESULT RemoveS(BSTR progID, BSTR queryCriteria) = 0;
};

class IEventSubscription : public IDispatch {
};

/* functions */
bool operator!=(GUID, GUID);
bool operator==(REFIID, GUID);
HRESULT CoCreateGuid(LPGUID);

void strupr(char *);

__int64_t Int32x32To64(ULONG, ULONG);

void GetSystemTimeAsFileTime(FILETIME *ft);
DWORD GetTempPath(DWORD inLen, char *lpBuffer);
#define GetTempPathA GetTempPath

#define utf32len wcslen

/* Some wrappers to map Windows unicode functions */
static inline int lstrcmpW(LPCWSTR str1, LPCWSTR str2)
{
	return wcscmp((WCHAR *)str1, (WCHAR *)str2);
}

static inline int lstrlenW(LPCWSTR str)
{
	return wcslen((WCHAR *)str);
}

static inline LPWSTR lstrcpyW(LPWSTR dst, LPCWSTR src)
{
	wcscpy(dst, (WCHAR *)src);
	return dst;
}

#ifdef UNICODE
#define _tcslen	wcslen
#define _tcscpy wcscpy
#define _tcscmp wcscmp
#define _tcsicmp wcscasecmp
#else
#define _tcslen strlen
#define _tcscpy strcpy
#define _tcscmp strcmp
#define _tcsicmp stricmp
#endif

void Sleep(unsigned int usec);

/* because the flags are not used linux, they do not match the windows flags! */
#define GPTR 0
HGLOBAL GlobalAlloc(UINT uFlags, ULONG ulSize);


typedef void * DLIB;
#ifdef __APPLE__
  #define ZARAFA_REAL_DLL_NAME		"libzarafaclient.dylib"
  #define SHARED_OBJECT_EXTENSION	"so"
#else
  #define ZARAFA_REAL_DLL_NAME		"libzarafaclient.so"
  #define SHARED_OBJECT_EXTENSION	"so"
#endif
#define PATH_SEPARATOR				'/'

// timezone compatability
#define _tzset tzset
#define _timezone timezone
#define _daylight daylight
#define _tzname tzname
// unavailable in linux
#define _dstbias 0

time_t GetProcessTime();

#ifndef ntohll
	#if __BYTE_ORDER == __LITTLE_ENDIAN
		#define ntohll(x) __bswap_64(x)
	#else
		#define ntohll(x) (x)
	#endif
#endif

#ifndef htonll
	#if __BYTE_ORDER == __LITTLE_ENDIAN
		#define htonll(x) __bswap_64(x)
	#else
		#define htonll(x) (x)
	#endif
#endif

#define OutputDebugStringA(dstr) fprintf(stderr,"%s",dstr)
#define GetCurrentThreadId() (int)pthread_self()
#define GetTickCount() 0L

#define TICKS_PER_SEC (sysconf(_SC_CLK_TCK))

#if DEBUG_PTHREADS
	#define pthread_mutex_lock(x) my_pthread_mutex_lock(__FILE__, __LINE__, x)
	int my_pthread_mutex_lock(const char *file, unsigned int line, pthread_mutex_t *m);
	
	#define pthread_rwlock_rdlock(x) my_pthread_rwlock_rdlock(__FILE__, __LINE__, x)
	int my_pthread_rwlock_rdlock(const char *file, unsigned int line, pthread_rwlock_t *m);
	
	#define pthread_rwlock_wrlock(x) my_pthread_rwlock_wrlock(__FILE__, __LINE__, x)
	int my_pthread_rwlock_wrlock(const char *file, unsigned int line, pthread_rwlock_t *m);

	std::string dump_pthread_locks();
#endif


#define localemask(_cat) (_cat ## _MASK)
#define createlocale(_cat, _loc) newlocale(_cat ## _MASK, _loc, NULL)
#define createlocale_real(_cat, _loc) newlocale(_cat, _loc, NULL)

/**
 * Tell the compiler that a funtion/method behaves like printf.
 * _fmt is the 1-indexed index to the format argument
 * _va is the 1-indexed index to the first va argument.
 * NOTE: For non-static methods the this pointer has index 1.
 **/
#define __LIKE_PRINTF(_fmt, _va) __attribute__((format(printf, _fmt, _va)))

#endif // PLATFORM_LINUX_H
