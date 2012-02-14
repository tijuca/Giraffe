// Generic typemaps
%apply unsigned int {ULONG};
%apply int {LONG};
%apply unsigned int {DWORD, HRESULT};
%apply unsigned int *OUTPUT {ULONG *, ULONG *lpulSteps, ULONG *lpulProgress};
%apply unsigned int *INOUT {ULONG *INOUT};
%apply unsigned int *OUTPUT {LONG *};
%apply bool {BOOL};
#ifdef UNICODE
%apply wchar_t * {LPTSTR};
#else
%apply char * {LPTSTR};
#endif

%include "cstring.i"
%include "cwstring.i"
%cstring_input_binary(const char *pv, ULONG cb);
%cstring_output_allocate_size(char **lpOutput, ULONG *ulRead, MAPIFreeBuffer(*$1));

/////////////////////////////////
// HRESULT
/////////////////////////////////
%include "exception.i"
%typemap(out) HRESULT (char ex[64])
{
  if(FAILED($1)) {
    snprintf(ex,sizeof(ex),"failed with HRESULT 0x%08X", $1);
	SWIG_exception(SWIG_RuntimeError, ex);
  }
}

/////////////////////////////////
// LARGE_INTEGER / ULARGE_INTEGER
/////////////////////////////////

%typemap(in,fragment=SWIG_AsVal_frag(unsigned long long)) ULARGE_INTEGER
{
  unsigned long long l = 0;
  SWIG_AsVal(unsigned long long)($input, &l);  

  $1.QuadPart = l;
}

%typemap(in,numinputs=0) ULARGE_INTEGER * (ULARGE_INTEGER u)
{
  $1 = &u;
}

%typemap(argout,fragment=SWIG_From_frag(unsigned long long)) ULARGE_INTEGER *
{
  %append_output(SWIG_From(unsigned long long)($1->QuadPart));
}

%typemap(in,fragment=SWIG_AsVal_frag(long long)) LARGE_INTEGER
{
  long long l = 0;
  SWIG_AsVal(long long)($input, &l);  

  $1.QuadPart = l;
}

%typemap(in,numinputs=0) LARGE_INTEGER * (LARGE_INTEGER u)
{
  $1 = &u;
}

%typemap(argout,fragment=SWIG_From_frag(unsigned long long)) ULARGE_INTEGER *
{
 %append_output(SWIG_From(unsigned long long)($1->QuadPart));
}


//////////////////
// ULONG+LPENTRYID
//////////////////

// Input
%typemap(in)				(ULONG cbEntryID, LPENTRYID lpEntryID) (int res, char *buf = 0, size_t size, int alloc = 0)
{
  res = SWIG_AsCharPtrAndSize($input, &buf, &size, &alloc);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res,"$type",$symname, $argnum);
  }
  if(buf == NULL) {
	$1 = 0;
	$2 = NULL;
  } else {
	$1 = %numeric_cast(size - 1, $1_ltype);
 	$2 = %reinterpret_cast(buf, $2_ltype);
  }
}
%typemap(freearg) (ULONG cbEntryID, LPENTRYID lpEntryID) {
  if (alloc$argnum == SWIG_NEWOBJ) %delete_array(buf$argnum);
}
%apply (ULONG cbEntryID, LPENTRYID lpEntryID) {(ULONG cFolderKeySize, BYTE *lpFolderSourceKey), (ULONG cMessageKeySize, BYTE *lpMessageSourceKey), (ULONG cbInstanceKey, BYTE *pbInstanceKey)};
%apply (ULONG cbEntryID, LPENTRYID lpEntryID) {(ULONG cbEntryID1, LPENTRYID lpEntryID1), (ULONG cbEntryID2, LPENTRYID lpEntryID2), 
(ULONG cbEIDContainer, LPENTRYID lpEIDContainer), (ULONG cbEIDNewEntryTpl, LPENTRYID lpEIDNewEntryTpl), (ULONG cbUserEntryID, LPENTRYID lpUserEntryID) };

// Output
%typemap(in,numinputs=0) (ULONG *OUTPUT, LPENTRYID *OUTPUT) (ULONG cbEntryID = 0, LPENTRYID lpEntryID = NULL) {
  $1 = &cbEntryID; $2 = &lpEntryID;
}
%typemap(argout,fragment="SWIG_FromCharPtrAndSize") (ULONG *OUTPUT, LPENTRYID *OUTPUT)
{
  if (*$2) {
    %append_output(SWIG_FromCharPtrAndSize((const char *)*$2,*$1));
  }
}
%typemap(freearg) (ULONG *OUTPUT, LPENTRYID *OUTPUT) {
	if(*$2)
		MAPIFreeBuffer(*$2);
}
%apply (ULONG *OUTPUT, LPENTRYID *OUTPUT) {(ULONG* lpcbStoreId, LPENTRYID* lppStoreId), (ULONG* lpcbRootId, LPENTRYID *lppRootId)};

// Optional In & Output
%typemap(in) (ULONG *OPTINOUT, LPENTRYID *OPTINOUT) (int res, char *buf = 0, size_t size, int alloc = 0, ULONG cbEntryID = 0, LPENTRYID lpEntryID = NULL, LPENTRYID lpOrig = NULL) {
  $1 = &cbEntryID; $2 = &lpEntryID;

  res = SWIG_AsCharPtrAndSize($input, &buf, &size, &alloc);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res,"$type",$symname, $argnum);
  }
  if(buf == NULL) {
    *$1 = 0;
    *$2 = NULL;
  } else {
    *$1 = %numeric_cast(size - 1, $*1_ltype);
    *$2 = %reinterpret_cast(buf, $*2_ltype);
  }
  lpOrig = *$2;
}
%typemap(argout,fragment="SWIG_FromCharPtrAndSize") (ULONG *OPTINOUT, LPENTRYID *OPTINOUT)
{
  if (*$2) {
    %append_output(SWIG_FromCharPtrAndSize((const char *)*$2,*$1));
  }
}
%typemap(freearg) (ULONG *OPTINOUT, LPENTRYID *OPTINOUT) {
	if(!lpOrig$argnum && $2 && *$2)
		MAPIFreeBuffer(*$2);
}
%apply (ULONG *OPTINOUT, LPENTRYID *OPTINOUT) {(ULONG* lpcbStoreId_oio, LPENTRYID* lppStoreId_oio), (ULONG* lpcbRootId_oio, LPENTRYID *lppRootId_oio)};

////////////////////////
// IID + LPUNKOWN
////////////////////////

// Output
%typemap(in,numinputs=0) LPUNKNOWN *OUTPUT_USE_IID (LPUNKNOWN temp) {
  $1 = ($1_type)&temp;
}
%typemap(argout) LPUNKNOWN *OUTPUT_USE_IID
{
 %append_output(SWIG_NewPointerObj((void*)*($1), TypeFromIID(*__lpiid), SWIG_SHADOW | SWIG_OWNER));
}
// Also apply to void ** in QueryInterface()
%apply LPUNKNOWN *OUTPUT_USE_IID {void **OUTPUT_USE_IID};

//////////////////////////
// LPMAPIUID/LPCIID/LPGUID
//////////////////////////

// Input
%typemap(in,fragment="SWIG_AsCharPtrAndSize")	LPMAPIUID (int res, char *buf, size_t size, int alloc),
				LPCIID (int res, char *buf, size_t size, int alloc),
				LPGUID (int res, char *buf, size_t size, int alloc)
{
  alloc = SWIG_OLDOBJ;
  res = SWIG_AsCharPtrAndSize($input, &buf, &size, &alloc);
  if (!SWIG_IsOK(res) || (size != 0 && (size-1) != sizeof(MAPIUID))) { // size-1 because we get \0 terminated string
    %argument_fail(res,"$type",$symname, $argnum);
  }
  $1 = %reinterpret_cast(buf, $1_ltype);
}
%typemap(in,fragment="SWIG_AsCharPtrAndSize")	const IID& (int res, char *buf, size_t size, int alloc)
{
  alloc = SWIG_OLDOBJ;
  res = SWIG_AsCharPtrAndSize($input, &buf, &size, &alloc);
  if (!SWIG_IsOK(res) || (size != 0 && (size-1) != sizeof(MAPIUID))) { // size-1 because we get \0 terminated string
    %argument_fail(res,"$type",$symname, $argnum);
  }
  $1 = %reinterpret_cast(buf, $1_ltype);
}
%typemap(freearg,noblock=1,match="in") LPMAPIUID, LPCIID, const IID& {
  if (alloc$argnum == SWIG_NEWOBJ) %delete_array(buf$argnum);
}
// Used for LPUNKNOWN *
%typemap(arginit,noblock=1,fragment="SWIG_AsCharPtrAndSize") LPCIID USE_IID_FOR_OUTPUT
{
  LPCIID &__lpiid = $1;
}
%typemap(arginit,noblock=1,fragment="SWIG_AsCharPtrAndSize") const IID& USE_IID_FOR_OUTPUT
{
  LPIID &__lpiid = $1;
}

%typemap(in, numinputs=0) LPMAPIUID OUTPUT (MAPIUID tmpUid)
{
	$1 = &tmpUid;
}

%typemap(argout) LPMAPIUID OUTPUT
{
	%append_output(SWIG_FromCharPtrAndSize((const char *)$1,sizeof(MAPIUID)));
}

///////////////////////
// ULONG ulFlags
///////////////////////
%typemap(arginit,noblock=1) ULONG ulFlags
{
  ULONG ulFlags = 0;
}

%typemap(in) ULONG ulFlags (unsigned int fl, int ecode)
{
	ecode = SWIG_AsVal(unsigned int)($input, &fl);
	if (!SWIG_IsOK(ecode)) {
		%argument_fail(ecode,"$type",$symname, $argnum);
	} 
	$1 = fl;
	ulFlags = fl;
}

///////////////////////
// LPTSTR
///////////////////////

// Output
%typemap(in,numinputs=0) (LPTSTR *OUTPUT) (LPTSTR lpStr = NULL) {
  $1 = &lpStr;
}
%typemap(argout,fragment="SWIG_FromCharPtr,SWIG_FromWCharPtr") LPTSTR *OUTPUT {
	if (ulFlags & MAPI_UNICODE) {
		%append_output(SWIG_FromWCharPtr(*$1));
	} else {
		%append_output(SWIG_FromCharPtr((char*)*$1));
	}
}
%typemap(freearg) LPTSTR *OUTPUT {
  if(*$1)
	MAPIFreeBuffer(*$1);
}

//////////////////////
// char** allocated with mapi
/////////////////////
%typemap(in,numinputs=0) (char** OUTMAPICHAR) (char* lpStr = NULL) {
  $1 = &lpStr;
}
%typemap(argout,fragment="SWIG_FromCharPtr") char** OUTMAPICHAR {
    %append_output(SWIG_FromCharPtr((char*)*$1));
}
%typemap(freearg) char** OUTMAPICHAR {
  if(*$1)
    MAPIFreeBuffer(*$1);
}

//////////////////////
// ULONG+IUnknown **
//////////////////////

// Output
%typemap(in,numinputs=0) (ULONG *OUTPUT, IUnknown **OUTPUT) (ULONG ulType, IUnknown *lpUnk)
{
	ulType = NULL;
	lpUnk = NULL;

	$1 = &ulType;
	$2 = &lpUnk;
}

%typemap(argout) (ULONG *OUTPUT, IUnknown **OUTPUT)
{
  switch(*($1)) {
    case MAPI_FOLDER:
		%append_output(SWIG_NewPointerObj((void*)*($2), SWIGTYPE_p_IMAPIFolder, SWIG_SHADOW | SWIG_OWNER)); break;
	case MAPI_MESSAGE:
		%append_output(SWIG_NewPointerObj((void*)*($2), SWIGTYPE_p_IMessage, SWIG_SHADOW | SWIG_OWNER)); break;
	case MAPI_MAILUSER:
		%append_output(SWIG_NewPointerObj((void*)*($2), SWIGTYPE_p_IMailUser, SWIG_SHADOW | SWIG_OWNER)); break;
	case MAPI_DISTLIST:
		%append_output(SWIG_NewPointerObj((void*)*($2), SWIGTYPE_p_IDistList, SWIG_SHADOW | SWIG_OWNER)); break;
	case MAPI_ABCONT:
		%append_output(SWIG_NewPointerObj((void*)*($2), SWIGTYPE_p_IABContainer, SWIG_SHADOW | SWIG_OWNER)); break;
    default:
		break;
  }
}

/////////////////////////////////
// unsigned char *, unsigned int
/////////////////////////////////
%typemap(in,numinputs=1) (unsigned char *, unsigned int) (int res, char *buf = 0, size_t size, int alloc = 0)
{
	res = SWIG_AsCharPtrAndSize($input, &buf, &size, &alloc);
	if (!SWIG_IsOK(res)) {
		%argument_fail(res,"$type",$symname, $argnum);
	}
	if(buf == NULL) {
		$1 = NULL;
		$2 = 0;
	} else {
		$1 = %reinterpret_cast(buf, $1_ltype);
		$2 = %numeric_cast(size - 1, $2_ltype);
	}
}

/////////////////////////////////////////////
// MAPISTRUCT (MAPI data struct)
/////////////////////////////////////////////

// Input
%typemap(arginit) MAPISTRUCT
	"$1 = NULL;"
%typemap(freearg)	MAPISTRUCT
{
	if($1)
		MAPIFreeBuffer($1);
}

/////////////////////////////////////////////
// MAPISTRUCT_W_FLAGS (MAPI data struct with LPTSTR members)
/////////////////////////////////////////////

// Input
%typemap(arginit) MAPISTRUCT_W_FLAGS
	"$1 = NULL;"
%typemap(freearg)	MAPISTRUCT_W_FLAGS
{
	if($1)
		MAPIFreeBuffer($1);
}

//////////////////////////////////////////////
// MAPILIST (MAPI list of data structs)
//////////////////////////////////////////////

// Input
%typemap(arginit) MAPILIST
	"$1 = NULL;"
%typemap(freearg)	MAPILIST
{
	if($1)
		MAPIFreeBuffer($1);
}

// Output
%typemap(in,numinputs=0)	MAPILIST * ($basetype temp), MAPISTRUCT * ($basetype temp)
	"temp = NULL; $1 = &temp;";
%typemap(freearg) 	MAPILIST *, MAPISTRUCT *
{
	if(*$1)
		MAPIFreeBuffer(*$1);
}

%typemap(freearg) 	MAPILIST *INPUT
{
	if(*$1)
		MAPIFreeBuffer(*$1);
}


//////////////////////////////////////////////
// MAPICLASS (Class instances of MAPI objects)
//////////////////////////////////////////////

// Output
%typemap(in,numinputs=0) 	MAPICLASS *($basetype *temp)
	"temp = NULL; $1 = &temp;";
%typemap(argout)	MAPICLASS *
{
  %append_output(SWIG_NewPointerObj((void*)*($1), $*1_descriptor, SWIG_SHADOW | SWIG_OWNER));
}

/////////////////////////////////////////////
// MAPIARRAY (List of objects)
/////////////////////////////////////////////

// Output
%typemap(in,numinputs=0)	(ULONG *,MAPIARRAY *)(ULONG c, $*2_type lp)
	"lp = NULL; $2 = &lp; c = 0; $1 = &c;";
%typemap(freearg) (ULONG *, MAPIARRAY *)
{
	if(*$2)
		MAPIFreeBuffer(*$2);
}

%typemap(arginit) (ULONG, MAPIARRAY)
{
	$1 = 0;
	$2 = NULL;
}

%typemap(freearg) (ULONG, MAPIARRAY)
{
	if($2)
		MAPIFreeBuffer((void *)$2);
}

///////////////////////////////////
// ECLogger director
///////////////////////////////////
#if SWIGPYTHON
#ifndef WIN32

%typemap(in) ECLogger * (int res, ECSimpleLogger *sl, ECLoggerProxy *proxy)
{
	res = SWIG_ConvertPtr($input, (void **)&sl, SWIGTYPE_p_ECSimpleLogger, 0 | 0);
	if(!SWIG_IsOK(res))
		%argument_fail(res,"ECSimpleLogger",$symname, $argnum);

	ECLoggerProxy::Create(EC_LOGLEVEL_DEBUG, sl, &proxy);
	$1 = proxy;
}

%typemap(freearg) ECLogger *
{
	$1->Release();
}

#endif
#endif

// Pull in the language-specific typemap
#if SWIGPERL
%include "perl/typemap_perl.i"
#elif SWIGRUBY
%include "ruby/typemap_ruby.i"
#elif SWIGPYTHON
%include "python/typemap_python.i"
#endif

/////////////////////////////////////////////////
// Mapping of types to correct MAPI* handler type
/////////////////////////////////////////////////

// Input
%apply (ULONG, MAPIARRAY) {(ULONG cValues, LPSPropValue lpProps), (ULONG cPropNames, LPMAPINAMEID* lppPropNames), (ULONG cInterfaces, LPCIID lpInterfaces), ( ULONG cValuesConversion, LPSPropValue lpPropArrayConversion) };
%apply MAPILIST {LPSPropTagArray, LPENTRYLIST, LPADRLIST, LPFlagList, LPROWLIST};
%apply MAPILIST *INPUT {LPSPropTagArray *};
%apply MAPISTRUCT {LPSRestriction, LPSSortOrderSet, LPSPropValue, LPNOTIFICATION};

// Output
%apply (ULONG *, MAPIARRAY *) {(ULONG *OUTPUT, LPSPropValue *OUTPUT), (ULONG *OUTPUT, LPNOTIFICATION *OUTPUT), (ULONG *OUTPUT, LPMAPINAMEID **OUTPUT)};
%apply MAPILIST * {LPADRLIST *OUTPUT, LPSRowSet *OUTPUT, LPSPropProblemArray *OUTPUT, LPSPropTagArray *OUTPUT, LPENTRYLIST *OUTPUT};
%apply MAPISTRUCT * {LPMAPIERROR *OUTPUT, LPSSortOrderSet *OUTPUT, LPSRestriction *OUTPUT};

// Input/Output
%apply MAPILIST INOUT {LPADRLIST INOUT, LPFlagList INOUT };

// Classes
%apply MAPICLASS *{IMAPISession **, IProfAdmin **, IMsgServiceAdmin **, IMAPITable **, IMsgStore **, IMAPIFolder **, IMAPITable **, IStream **, IMessage **, IAttach **, IAddrBook **, IProviderAdmin **, IProfSect **, IUnknown **}

// Specialization for LPSRowSet and LPADRLIST
%typemap(freearg) LPSRowSet *OUTPUT, LPADRLIST *OUTPUT
{
	FreeProws((LPSRowSet)*$1);
}

%typemap(freearg) LPADRLIST INOUT, LPSRowSet INOUT, LPSRowSet INPUT, LPADRLIST INPUT
{
    FreeProws((LPSRowSet)$1);
}

