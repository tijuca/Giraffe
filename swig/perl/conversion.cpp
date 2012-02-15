/*
 * Copyright 2005 - 2009  Zarafa B.V.
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

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#undef Stat
#undef LOCK_EXCLUSIVE
#undef LOCK_WRITE

#ifdef eof
	#undef eof
#endif
#ifdef bool
	#undef bool
#endif
#ifdef write
	#undef write
#endif
#ifdef read
	#undef read
#endif
#ifdef abort
	#undef abort
#endif

#include "platform.h"
#include "mapi.h"
#include "mapix.h"
#include "mapiutil.h"
#include "ECDefs.h"

#include "conversion.h"

#include <charset/convert.h>
#include <charset/utf8string.h>

void hv_store_simple(HV *hv, const char *key, SV *val)
{
    int hash = 0;
    PERL_HASH(hash, key, strlen(key));
    hv_store(hv, key, strlen(key), val, hash);
}

SV **hv_fetch_simple(HV *hv, const char *key)
{
    return hv_fetch(hv, key, strlen(key), 0);
}

SV *filetime2sv(FILETIME ft)
{
    AV *filetime = newAV();
    
    av_push(filetime, newSVuv(ft.dwLowDateTime));
    av_push(filetime, newSVuv(ft.dwHighDateTime));
    
    return newRV_noinc((SV*)filetime);
}

int sv2filetime(SV *sv, FILETIME *ft) {
    if(!SvROK(sv) || SvTYPE(SvRV(sv)) != SVt_PVAV)
        return 1;
        
    AV *av = (AV *)SvRV(sv);
    SV **psv;
    
    if(av_len(av) != 1)
        return 1;
        
    psv = av_fetch(av, 0, 0);
    if(!psv)
        return 1;
    ft->dwLowDateTime = SvUV(*psv);

    psv = av_fetch(av, 1, 0);
    if(!psv)
        return 1;
    ft->dwHighDateTime = SvUV(*psv);
    
    return 0;
}

HV *HV_from_LPSPropValue(LPSPropValue lpProp)
{
    HV *property = NULL;
    if(!lpProp)
        return (HV *)&PL_sv_undef;

    SV *proptag = newSVuv(lpProp->ulPropTag);
    SV *value = NULL;
    
    switch(PROP_TYPE(lpProp->ulPropTag)) {
        case PT_OBJECT:
        case PT_NULL:
            value = newSVuv(lpProp->Value.x);
            break;
        case PT_SHORT:
            value = newSVuv(lpProp->Value.i);
            break;
        case PT_LONG:
            value = newSVuv(lpProp->Value.ul);
            break;
        case PT_FLOAT:
            value = newSVnv(lpProp->Value.flt);
            break;
        case PT_DOUBLE:
            value = newSVnv(lpProp->Value.dbl);
            break;
        case PT_CURRENCY:
            value = newSVnv(lpProp->Value.cur.int64);
            break;
        case PT_APPTIME:
            value = newSVnv(lpProp->Value.at);
            break;
        case PT_SYSTIME:
            value = filetime2sv(lpProp->Value.ft);
            break;
        case PT_STRING8:
            value = newSVpv(lpProp->Value.lpszA, strlen(lpProp->Value.lpszA));
            break;
        case PT_BINARY:
            value = newSVpv((char *)lpProp->Value.bin.lpb, lpProp->Value.bin.cb);
            break;
        case PT_UNICODE: {
			utf8string strUTF8 = convert_to<utf8string>(lpProp->Value.lpszW);
            value = newSVpv((char *)strUTF8.c_str(), strUTF8.size());
            SvUTF8_on(value);
            break;
        }
        case PT_CLSID:
            value = newSVpv((char *)lpProp->Value.lpguid, sizeof(GUID));
            break;
        case PT_I8:
            value = newSVnv(lpProp->Value.li.QuadPart);
            break;
		case PT_BOOLEAN:
			value = newSVuv(lpProp->Value.b == TRUE ? 1 : 0);
			break;
#define MV_CASE(mapiname, perltype, sub) \
            { \
                AV *mv = newAV(); \
                for(unsigned int i=0;i<lpProp->Value.MV##mapiname.cValues;i++) { \
                    av_push(mv, newSV##perltype(sub(lpProp->Value.MV##mapiname.lp##mapiname[i]))); \
                } \
                value = newRV_noinc((SV*)mv); \
            }
#define BASE(x) x
#define INT64(x) x.int64
#define QUADPART(x) x.QuadPart
        case PT_MV_I2: 
            MV_CASE(i, uv, BASE)
            break;
        case PT_MV_LONG:
            MV_CASE(l, uv, BASE)
            break;
        case PT_MV_R4:
            MV_CASE(flt, nv, BASE)
            break;
        case PT_MV_DOUBLE:
            MV_CASE(dbl, nv, BASE)
            break;
        case PT_MV_CURRENCY:
            MV_CASE(cur, nv, INT64)
            break;
        case PT_MV_APPTIME:
            MV_CASE(at, nv, BASE)
            break;
        case PT_MV_SYSTIME:
            { 
                AV *mv = newAV(); 
                for(unsigned int i=0;i<lpProp->Value.MVft.cValues;i++) { 
                    av_push(mv, filetime2sv(lpProp->Value.MVft.lpft[i])); 
                } 
                value = newRV_noinc((SV*)mv);
            }
            break;
        case PT_MV_BINARY:
            { 
                AV *mv = newAV(); 
                for(unsigned int i=0;i<lpProp->Value.MVbin.cValues;i++) { 
                	if(lpProp->Value.MVbin.lpbin[i].cb == 0)
                		av_push(mv, newSVpvn("", 0));
					else
	                    av_push(mv, newSVpvn((char *)lpProp->Value.MVbin.lpbin[i].lpb, lpProp->Value.MVbin.lpbin[i].cb)); 
                } 
                value = newRV_noinc((SV*)mv);
            }
            break;
        case PT_MV_STRING8:
            { 
                AV *mv = newAV(); 
                for(unsigned int i=0;i<lpProp->Value.MVszA.cValues;i++) { 
                    av_push(mv, newSVpv(lpProp->Value.MVszA.lppszA[i], strlen(lpProp->Value.MVszA.lppszA[i]))); 
                } 
                value = newRV_noinc((SV*)mv);
            }
            break;
        case PT_MV_UNICODE:
            { 
                convert_context converter;                
                AV *mv = newAV(); 
                
                for(unsigned int i=0;i<lpProp->Value.MVszW.cValues;i++) { 
					utf8string strUTF8 = converter.convert_to<utf8string>(lpProp->Value.MVszW.lppszW[i]);
                    
                    SV *newsv = newSVpv(strUTF8.c_str(), strUTF8.size());
                    SvUTF8_on(newsv);
                    av_push(mv, newsv); 
                } 
                value = newRV_noinc((SV*)mv);
            }
            break;
        case PT_MV_CLSID:
            { 
                AV *mv = newAV(); 
                for(unsigned int i=0;i<lpProp->Value.MVguid.cValues;i++) { 
                    av_push(mv, newSVpv((char *)&lpProp->Value.MVguid.lpguid[i], sizeof(GUID))); 
                } 
                value = newRV_noinc((SV*)mv);
            }
            break;
        case PT_MV_I8:
            MV_CASE(li, nv, QUADPART);
            break;
        case PT_ERROR:
            value = newSVuv(lpProp->Value.err);
            break;
    }
    
    if(value) {
        property = newHV();
        hv_store_simple(property, "ulPropTag", proptag);
        hv_store_simple(property, "Value", value);
    }
    
    return property;
}

AV *AV_from_LPSPropValue(LPSPropValue lpProps, ULONG cValues)
{
    AV *properties;
    
    if(!lpProps)
        return (AV*)&PL_sv_undef;
    
    properties = newAV();
    
    for(unsigned int i=0; i <cValues; i++) {
        HV *property = (HV *)HV_from_LPSPropValue(&lpProps[i]);
        
        if(property)
            av_push(properties, newRV_noinc((SV *)property));
    }
    
    return properties;
}

AV *AV_from_LPSRowSet(LPSRowSet lpRowSet)
{
    AV *rows;
    
    if(!lpRowSet)
        return (AV *)&PL_sv_undef;
    
    rows = newAV();
    
    for(unsigned int i=0; i <lpRowSet->cRows; i++) {
        AV *properties = (AV *)AV_from_LPSPropValue(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues);
    
        if(properties)
            av_push(rows, newRV_noinc((SV *)properties));
    }
  
    return rows;
}

LPSRowSet AV_to_LPSRowSet(AV *av)
{
	return NULL; // FIXME
}

LPFlagList AV_to_LPFlagList(AV *av)
{
	return NULL; // FIXME
}

// Returns 0 on success
int HV_to_SPropValue(SPropValue *lpProp, HV *hv, void *lpBase)
{
    SV **entry;
    unsigned int ulPropTag = 0;
    STRLEN len;
    char *data;
    
    entry = hv_fetch_simple(hv, "ulPropTag");
    if(!entry)
        return 0;
        
    ulPropTag = SvUV(*entry);
    
    entry = hv_fetch_simple(hv, "Value");
    if(!entry)
        return 0;
        
    switch(PROP_TYPE(ulPropTag)) {
        case PT_NULL:
            lpProp->Value.x = 0;
            break;
        case PT_SHORT:
            lpProp->Value.i = SvUV(*entry);
            break;
        case PT_LONG:
            lpProp->Value.ul = SvUV(*entry);
            break;
        case PT_FLOAT:
            lpProp->Value.flt = SvNV(*entry);
            break;
        case PT_DOUBLE:
            lpProp->Value.dbl = SvNV(*entry);
            break;
        case PT_CURRENCY:
            lpProp->Value.cur.int64 = SvNV(*entry);
            break;
        case PT_APPTIME:
            lpProp->Value.at = SvNV(*entry);
            break;
        case PT_SYSTIME:
            if(sv2filetime(*entry, &lpProp->Value.ft) != 0)
                return 1;
            break;
		case PT_BINARY:
			lpProp->Value.bin.lpb = (BYTE*)SvPV(*entry, len);
			lpProp->Value.bin.cb = len;
			break;
        case PT_STRING8:
            // Note we do not copy data because we SPropValue will only be used in functions before returning back to perl. This
            // means that this pointer will be valid for subsequent calls.
            lpProp->Value.lpszA = SvPV(*entry, len);
            break;
        case PT_UNICODE:
        {
			const char *lpsz = SvPV(*entry, len);
            std::wstring strWCHAR = convert_to<std::wstring>(lpsz, len, "UTF-8//TRANSLIT");
            MAPIAllocateMore(sizeof(std::wstring::value_type) * (strWCHAR.size() + 1), lpBase, (void **)&lpProp->Value.lpszW);
            wcscpy(lpProp->Value.lpszW, strWCHAR.c_str());
            break;
        }
        case PT_CLSID:
            data = SvPV(*entry, len);
            if(len != sizeof(GUID))
                return 1;
                
            MAPIAllocateMore(len, lpBase, (void **)&lpProp->Value.lpguid);
            memcpy(lpProp->Value.lpguid, data, sizeof(GUID));
            break;
        case PT_LONGLONG:
            lpProp->Value.li.QuadPart = SvNV(*entry);
            break;
		case PT_BOOLEAN:
			lpProp->Value.b = (SvUV(*entry) != 0 ? TRUE : FALSE);
			break;
		case PT_MV_BINARY:
		{
			AV *av = (AV*)SvRV(*entry);
			
			MAPIAllocateMore((av_len(av)+1) * sizeof(SBinaryArray), lpBase, (void **)&lpProp->Value.MVbin.lpbin);
			
			for(int i=0;i<av_len(av)+1;i++) {
				SV **sub = av_fetch(av, i, 0);
				
				data = SvPV(*sub, len);
				lpProp->Value.MVbin.lpbin[i].lpb = (BYTE *)data;
				lpProp->Value.MVbin.lpbin[i].cb = len;
			}
			lpProp->Value.MVbin.cValues = av_len(av)+1;
			
			break;
		}
        default:
            return 1;
    }
    
    lpProp->ulPropTag = ulPropTag;
    
    return 0;
}

LPSPropValue HV_to_LPSPropValue(HV *hv, void *lpBase)
{
    LPSPropValue lpProp = NULL;
    
    MAPIAllocateMore(sizeof(SPropValue), lpBase, (void **)&lpProp);
    
    if(HV_to_SPropValue(lpProp, hv, lpBase) != 0) {
        return NULL;
    }
    
    return lpProp;
}

LPSPropValue AV_to_LPSPropValue(AV *av, STRLEN *lpcValues, void *lpBase)
{
    LPSPropValue lpProps = NULL;
    ULONG cValues = 0;
    SV **entry;
    
    if(!av) {
        *lpcValues = 0;
        return NULL;
    }
    
    if(lpBase)
        MAPIAllocateMore(sizeof(SPropValue) * (av_len(av)+1), lpBase, (void **)&lpProps);
    else
        MAPIAllocateBuffer(sizeof(SPropValue) * (av_len(av)+1), (void **)&lpProps);
        
    memset(lpProps, 0, sizeof(SPropValue) * (av_len(av)+1));
    
    for(int i=0; i<av_len(av)+1; i++) {
        entry = av_fetch(av, i, 0);
        
        if(entry == NULL)
            continue;
            
        if(!SvROK(*entry)) {
            croak("entry %d in SPropValue array is not a reference", i);
            continue;
        }
        
        if(SvTYPE(SvRV(*entry)) != SVt_PVHV) {
            croak("entry %d in SPropValue array is not a HASHREF: %d", i, SvTYPE(*entry));
            continue;
        }
        
        if(HV_to_SPropValue(&lpProps[cValues], (HV *)SvRV(*entry), lpProps) != 0)
            continue;
            
        cValues++;
    }
    
    *lpcValues = cValues;

    return lpProps;
}

LPENTRYLIST	AV_to_LPENTRYLIST(AV *av)
{
    ULONG cValues = 0;
    LPENTRYLIST lpEntryList = NULL;

    if(!av)
        return NULL;
    
	MAPIAllocateBuffer(sizeof *lpEntryList, (void**)&lpEntryList);
	MAPIAllocateMore((av_len(av) + 1) * sizeof *lpEntryList->lpbin, lpEntryList, (void**)&lpEntryList->lpbin);
    
    for(int i=0; i < av_len(av)+1;i++) {
        SV **sv = av_fetch(av, i, 0);
        
        if(sv) {
            STRLEN len = 0;
			char *cstr = SvPV(*sv, len); 

			lpEntryList->lpbin[cValues].cb = len;
			MAPIAllocateMore(len, lpEntryList, (void**)&lpEntryList->lpbin[cValues].lpb);
			memcpy(lpEntryList->lpbin[cValues].lpb, cstr, len);

            cValues++;
        }
    }

    lpEntryList->cValues = cValues;

    return lpEntryList;
}

LPSPropTagArray AV_to_LPSPropTagArray(AV *av)
{
    ULONG cValues = 0;
    LPSPropTagArray lpPropTags = NULL;

    if(!av)
        return NULL;
    
    MAPIAllocateBuffer(CbNewSPropTagArray(av_len(av)+1), (void **) &lpPropTags);
    memset(lpPropTags, 0, CbNewSPropTagArray(av_len(av)+1));
    
    for(int i=0; i < av_len(av)+1;i++) {
        SV **sv = av_fetch(av, i, 0);
        
        if(sv) {
            lpPropTags->aulPropTag[cValues] = SvUV(*sv);
            cValues++;
        }
    }

    lpPropTags->cValues = cValues;

    return lpPropTags;
}

AV *AV_from_LPSPropTagArray(LPSPropTagArray lpPropTagArray)
{
    AV *av;
    
    if(lpPropTagArray == NULL)
        return (AV *)&PL_sv_undef;

	av = newAV();
        
    for(unsigned int i=0; i<lpPropTagArray->cValues; i++) {
        av_push(av, newSVuv(lpPropTagArray->aulPropTag[i]));
    }
    
    return av;
}

AV *AV_from_LPSPropProblemArray(LPSPropProblemArray lpProblemArray)
{
    AV *av;
    if(lpProblemArray == NULL)
        return (AV *)&PL_sv_undef;
        
    av = newAV();
    
    for(unsigned int i=0; i < lpProblemArray->cProblem; i++) {
        HV *hv = newHV();
        
        hv_store_simple(hv, "ulIndex", newSVuv(lpProblemArray->aProblem[i].ulIndex));
        hv_store_simple(hv, "ulPropTag", newSVuv(lpProblemArray->aProblem[i].ulPropTag));
        hv_store_simple(hv, "scode", newSVuv(lpProblemArray->aProblem[i].scode));
        
        av_push(av, newRV_noinc((SV *)hv));
    }
    
    return av;
}

AV *AV_from_LPNOTIFICATION(LPNOTIFICATION lpNotif, ULONG cNotif)
{
    AV *av;
    
    if(lpNotif == NULL)
        return (AV *)&PL_sv_undef;
        
    av = newAV();
    
    for(unsigned int i=0; i < cNotif; i++) {
        HV *hv = newHV();

        switch(lpNotif[i].ulEventType) {
            case fnevCriticalError:
                if(lpNotif[i].info.err.lpEntryID)
                    hv_store_simple(hv, "lpEntryID", 	newSVpv((char *)lpNotif[i].info.err.lpEntryID, lpNotif[i].info.err.cbEntryID));
                    
                hv_store_simple(hv, "scode", newSVuv(lpNotif[i].info.err.scode));
                hv_store_simple(hv, "ulFlags", newSVuv(lpNotif[i].info.err.ulFlags));
                hv_store_simple(hv, "lpMAPIError", newRV_noinc((SV *)HV_from_LPMAPIERROR(lpNotif[i].info.err.lpMAPIError)));
                break;
            case fnevNewMail:
                if(lpNotif[i].info.newmail.lpEntryID)
                    hv_store_simple(hv, "lpEntryID", 	newSVpv((char *)lpNotif[i].info.newmail.lpEntryID, lpNotif[i].info.newmail.cbEntryID));
                if(lpNotif[i].info.newmail.lpParentID)
                    hv_store_simple(hv, "lpParentID",	newSVpv((char *)lpNotif[i].info.newmail.lpParentID, lpNotif[i].info.newmail.cbParentID));
                    
                hv_store_simple(hv, "ulFlags", newSVuv(lpNotif[i].info.newmail.ulFlags));
                
                if(lpNotif[i].info.newmail.lpszMessageClass)
                    hv_store_simple(hv, "lpszMessageClass", newSVpv((char*)lpNotif[i].info.newmail.lpszMessageClass, strlen((char*)lpNotif[i].info.newmail.lpszMessageClass)));
                    
                hv_store_simple(hv, "ulMessageFlags", newSVuv(lpNotif[i].info.newmail.ulMessageFlags));
                
                break;
            case fnevObjectCreated:
            case fnevObjectDeleted:
            case fnevObjectModified:
            case fnevObjectMoved:
            case fnevObjectCopied:
            case fnevSearchComplete:
                if(lpNotif[i].info.obj.lpEntryID)
                    hv_store_simple(hv, "lpEntryID", 	newSVpv((char *)lpNotif[i].info.obj.lpEntryID, lpNotif[i].info.obj.cbEntryID));
                hv_store_simple(hv, "ulObjType",	newSVuv(lpNotif[i].info.obj.ulObjType));
                if(lpNotif[i].info.obj.lpParentID)
                    hv_store_simple(hv, "lpParentID",	newSVpv((char *)lpNotif[i].info.obj.lpParentID, lpNotif[i].info.obj.cbParentID));
                if(lpNotif[i].info.obj.lpOldID)
                    hv_store_simple(hv, "lpOldID",		newSVpv((char *)lpNotif[i].info.obj.lpOldID, lpNotif[i].info.obj.cbOldID));
                if(lpNotif[i].info.obj.lpOldParentID)
                    hv_store_simple(hv, "lpOldParentID",newSVpv((char *)lpNotif[i].info.obj.lpOldParentID, lpNotif[i].info.obj.cbOldParentID));
                if(lpNotif[i].info.obj.lpPropTagArray)
                    hv_store_simple(hv, "lpPropTagArray", newRV_noinc((SV *)AV_from_LPSPropTagArray(lpNotif[i].info.obj.lpPropTagArray)));              
                break;
            case fnevTableModified:
                hv_store_simple(hv, "ulTableEvent", newSVuv(lpNotif[i].info.tab.ulTableEvent));
                hv_store_simple(hv, "hResult", 		newSVuv(lpNotif[i].info.tab.hResult));
                hv_store_simple(hv, "propIndex",	newRV_noinc((SV*)HV_from_LPSPropValue(&lpNotif[i].info.tab.propIndex)));
                hv_store_simple(hv, "propPrior",	newRV_noinc((SV*)HV_from_LPSPropValue(&lpNotif[i].info.tab.propPrior)));
                hv_store_simple(hv, "row", 			newRV_noinc((SV*)AV_from_LPSPropValue(lpNotif[i].info.tab.row.lpProps, lpNotif[i].info.tab.row.cValues)));
                    
                break;
            case fnevStatusObjectModified:
                if(lpNotif[i].info.statobj.lpEntryID)
                    hv_store_simple(hv, "lpEntryID", 	newSVpv((char *)lpNotif[i].info.statobj.lpEntryID, lpNotif[i].info.statobj.cbEntryID));
                if(lpNotif[i].info.statobj.lpPropVals)
                    hv_store_simple(hv, "lpPropVals", 	newRV_noinc((SV*)AV_from_LPSPropValue(lpNotif[i].info.statobj.lpPropVals, lpNotif[i].info.statobj.cValues)));
                break;
        }
        
        hv_store_simple(hv, "ulEventType", newSVuv(lpNotif[i].ulEventType));
        
        av_push(av, newRV_noinc((SV *)hv));
    }
    
    return av;
}

int HV_to_SSortOrder(void *lpBase, SSortOrder *lpSortOrder, HV *hv)
{
    SV **sv;

    memset(lpSortOrder, 0, sizeof(*lpSortOrder));
    
    sv = hv_fetch_simple(hv, "ulPropTag");
    if(sv)
        lpSortOrder->ulPropTag = SvUV(*sv);
    else return 1;
    
    sv = hv_fetch_simple(hv, "ulOrder");
    if(sv)
        lpSortOrder->ulOrder = SvUV(*sv);
        
    return 0;
}

int AV_to_LPSSortOrder(AV *av, LPSSortOrder lpSortOrder, ULONG *lpcSorts, void *lpBase)
{
    ULONG cValues= 0;
    
    for(int i=0; i<av_len(av) + 1; i++) {
        SV **sv = av_fetch(av, i, 0);
        
        if(sv && SvTYPE(*sv) == SVt_RV && SvTYPE(SvRV(*sv)) == SVt_PVHV)
            if(HV_to_SSortOrder(lpBase, &lpSortOrder[cValues], (HV*)SvRV(*sv)) == 0)
                cValues++;
    }
    
    *lpcSorts = cValues;

    return 0;
}

LPSSortOrderSet HV_to_LPSSortOrderSet(HV *hv)
{
    LPSSortOrderSet lpSortOrderSet = NULL;
    unsigned int cSort = 0;
    SV **sv;

    sv = hv_fetch_simple(hv, "aSort");
    if(sv && SvOK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV)
        cSort = av_len((AV*)SvRV(*sv))+1;

    MAPIAllocateBuffer(CbNewSSortOrderSet(cSort), (void**) &lpSortOrderSet);
    memset(lpSortOrderSet, 0, CbNewSSortOrderSet(cSort));
    
    sv = hv_fetch_simple(hv, "cCategories");
    if(sv && SvOK(*sv))
        lpSortOrderSet->cCategories = SvUV(*sv);
    
    sv = hv_fetch_simple(hv, "cExpanded");
    if(sv && SvOK(*sv))
        lpSortOrderSet->cExpanded = SvUV(*sv);

    sv = hv_fetch_simple(hv, "aSort");
    if(sv && SvOK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV)
        AV_to_LPSSortOrder((AV*)SvRV(*sv), lpSortOrderSet->aSort, &lpSortOrderSet->cSorts, lpSortOrderSet);
        
    
    return lpSortOrderSet;
}

LPSRestriction AV_to_LPSRestriction(AV *av, ULONG *lpulCount, void *lpBase)
{
    LPSRestriction lpRes = NULL;
    SV **sv;
    
    if(lpBase)
        MAPIAllocateMore(sizeof(SRestriction) * (av_len(av) + 1), lpBase, (void **)&lpRes);
    else 
        MAPIAllocateBuffer(sizeof(SRestriction) * (av_len(av) + 1), (void **)&lpRes);
     
    memset(lpRes, 0, sizeof(SRestriction) * (av_len(av) + 1));
        
    for(int i =0 ; i< av_len(av)+1;i++) {
        sv = av_fetch(av, i, 0);
        
        if(sv == NULL || !SvOK(*sv) || SvTYPE(SvRV(*sv)) != SVt_PVHV) {
            croak("Item at entry %d is not HASHREF", i);
            if(lpBase == NULL)
                MAPIFreeBuffer(lpRes);
            return NULL;
        }    
        
        if(HV_to_LPSRestriction((HV*)SvRV(*sv), &lpRes[i], lpBase == NULL ? lpRes : lpBase) != 0) {
            if(lpBase == NULL)
                MAPIFreeBuffer(lpRes);
            return NULL;
        }
    }
    
    *lpulCount = av_len(av)+1;
    
    return lpRes;
}

LPSRestriction HV_to_LPSRestriction(HV *hv, void *lpBase)
{
    LPSRestriction lpsRestrict = NULL;

    if(lpBase)
        MAPIAllocateMore(sizeof(SRestriction), lpBase, (void **) &lpsRestrict);
    else
        MAPIAllocateBuffer(sizeof(SRestriction), (void **) &lpsRestrict);

    memset(lpsRestrict, 0, sizeof(SRestriction));

    if(HV_to_LPSRestriction(hv, lpsRestrict, lpBase == NULL ? lpsRestrict : lpBase) == 0)
        return lpsRestrict;
    else {
        if(lpBase == NULL)
            MAPIFreeBuffer(lpsRestrict);
        return NULL;    
    }
}

int HV_to_LPSRestriction(HV *hv, LPSRestriction lpsRestrict, void *lpBase)
{
    SV **sv;
    STRLEN len;
    
    if(!hv)
        return NULL;
    
    sv = hv_fetch_simple(hv, "rt");

    if(sv && SvOK(*sv))
        lpsRestrict->rt = SvUV(*sv);
    else {
        croak("No 'rt' in restriction");
        return NULL;
    }
    
    switch(lpsRestrict->rt) {
        case RES_COMPAREPROPS:
#define SIMPLEPROP(resSub, propname, type) \
            sv = hv_fetch_simple(hv, #propname); \
            if(sv && SvOK(*sv)) \
                lpsRestrict->res.resSub.propname = type(*sv); \
            else { \
                croak("No '" #propname "' in restriction"); \
                return 1; \
            }

            SIMPLEPROP(resCompareProps, relop, SvUV);
            SIMPLEPROP(resCompareProps, ulPropTag1, SvUV);
            SIMPLEPROP(resCompareProps, ulPropTag2, SvUV);
            break;    
        case RES_OR: // a bit of a hack because we know that resOr == resAnd, only 'rt' differs
        case RES_AND:
            sv = hv_fetch_simple(hv, "lpRes");
            if(sv && SvOK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV)
                lpsRestrict->res.resAnd.lpRes = AV_to_LPSRestriction((AV *)SvRV(*sv), &lpsRestrict->res.resAnd.cRes, lpBase);
            else {
                croak("Missing or bad 'lpRes' in restriction");
                return 1;
            }
            break;
        case RES_NOT:
            sv = hv_fetch_simple(hv, "lpRes");
            if(sv && SvOK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVHV)
                lpsRestrict->res.resNot.lpRes = HV_to_LPSRestriction((HV *)SvRV(*sv), lpBase);
            else {
                croak("Missing or bad 'lpRes' in restriction");
                return 1;
            }
            break;
        case RES_CONTENT:
            SIMPLEPROP(resContent, ulFuzzyLevel, SvUV);
            SIMPLEPROP(resContent, ulPropTag, SvUV);

            sv = hv_fetch_simple(hv, "lpProp");
            if(sv && SvOK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVHV)
                lpsRestrict->res.resContent.lpProp = HV_to_LPSPropValue((HV *)SvRV(*sv), lpBase);
            else {
                croak("Missing or bad 'lpProp' in restriction");
                return 1;
            }
            break;
        case RES_PROPERTY:
            SIMPLEPROP(resProperty, relop, SvUV);
            SIMPLEPROP(resProperty, ulPropTag, SvUV);

            sv = hv_fetch_simple(hv, "lpProp");
            if(sv && SvOK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVHV)
                lpsRestrict->res.resProperty.lpProp = HV_to_LPSPropValue((HV *)SvRV(*sv), lpBase);
            else {
                croak("Missing or bad 'lpProp' in restriction");
                return 1;
            }
            break;
                        
        case RES_BITMASK:
            SIMPLEPROP(resBitMask, relBMR, SvUV);
            SIMPLEPROP(resBitMask, ulPropTag, SvUV);
            SIMPLEPROP(resBitMask, ulMask, SvUV);
            break;
            
        case RES_SIZE:
            SIMPLEPROP(resSize, relop, SvUV);
            SIMPLEPROP(resSize, ulPropTag, SvUV);
            SIMPLEPROP(resSize, cb, SvUV);
            break;
            
        case RES_EXIST:
            SIMPLEPROP(resExist, ulPropTag, SvUV);
            break;
            
        case RES_SUBRESTRICTION:
            SIMPLEPROP(resSub, ulSubObject, SvUV);
            
            sv = hv_fetch_simple(hv, "lpRes");
            if(sv && SvOK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVHV)
                lpsRestrict->res.resSub.lpRes = HV_to_LPSRestriction((HV *)SvRV(*sv), lpBase);
            else {
                croak("Missing or bad 'lpRes' in restriction");
                return 1;
            }
            break;
            
        case RES_COMMENT:
            sv = hv_fetch_simple(hv, "lpRes");
            if(sv && SvOK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVHV)
                lpsRestrict->res.resComment.lpRes = HV_to_LPSRestriction((HV *)SvRV(*sv), lpBase);
            else {
                croak("Missing or bad 'lpRes' in restriction");
                return 1;
            }

            sv = hv_fetch_simple(hv, "lpProp");
            if(sv && SvOK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV) {
                lpsRestrict->res.resComment.lpProp = AV_to_LPSPropValue((AV *)SvRV(*sv), &len, lpBase);
                lpsRestrict->res.resComment.cValues = len;
            } else {
                croak("Missing or bad 'lpProp' in restriction");
                return 1;
            }

            break;
    }
            
    return 0;
}

HV *HV_from_LPMAPINAMEID(LPMAPINAMEID lpName)
{
    HV *hv;
    SV *name;
    SV *guid;
    
    if(lpName == NULL)
        return (HV *)&PL_sv_undef;
        
    hv = newHV();
    
    if(lpName->ulKind == MNID_ID) {
        name = newSViv(lpName->Kind.lID);
    } else {
        utf8string strUTF8 = convert_to<utf8string>(lpName->Kind.lpwstrName);
        name = newSVpv(strUTF8.c_str(), strUTF8.size());
        SvUTF8_on(name);
    }
    
    guid = newSVpv((const char *)lpName->lpguid, sizeof(GUID));
    
    hv_store_simple(hv, "name", name);
    hv_store_simple(hv, "guid", guid);
    
    return hv;
            
}

AV *AV_from_p_LPMAPINAMEID(LPMAPINAMEID *lppMAPINameId, ULONG cNames)
{
    AV *av;
    
    if(lppMAPINameId == NULL)
        return (AV *)&PL_sv_undef;
        
    av = newAV();
    
    for(unsigned int i = 0; i < cNames; i++) {
        HV *nameid = (HV *)HV_from_LPMAPINAMEID(lppMAPINameId[i]);
        
        if(nameid) {
            av_push(av, newRV_noinc((SV *)nameid));
        }
    }
    
    return av;
}

void HV_to_LPMAPINAMEID(HV *elem, LPMAPINAMEID *lppName, void *lpBase)
{
    LPMAPINAMEID lpName = NULL;
	const char *lpsz;
	STRLEN len;
	SV **kind;
	SV **id;
	SV **guid;
	ULONG ulKind = 0;

    MAPIAllocateMore(sizeof(MAPINAMEID), lpBase, (void **)&lpName);
    memset(lpName, 0, sizeof(MAPINAMEID));
    
    kind = hv_fetch_simple(elem, "kind");
    id = hv_fetch_simple(elem, "id");
    guid = hv_fetch_simple(elem, "guid");
    
    if(!guid || !id)
		croak("Missing id or guid on MAPINAMEID object");

    if(!kind) {
        // Detect kind from type of 'id' parameter by first trying to use it as an int, then as string
		if (SvIOK(*id))
			ulKind = MNID_ID;
		else
			ulKind = MNID_STRING;
    } else {
		ulKind = SvUV(*kind);
    }
    
    lpName->ulKind = ulKind;
    if(ulKind == MNID_ID) {
        lpName->Kind.lID = SvUV(*id);
    } else {
		const char *lpszFrom = "";
		if (SvUTF8(*id))
			lpszFrom = "UTF-8";

		lpsz = SvPV(*id, len);
		// Convert to MAPI's WCHAR
		std::wstring strData = convert_to<std::wstring>(CHARSET_WCHAR, lpsz, len, lpszFrom);
                            	
		// 0-terminate
		strData.append(sizeof(WCHAR), 0);
        MAPIAllocateMore(strData.size(), lpBase, (void **)&lpName->Kind.lpwstrName);
        memcpy(lpName->Kind.lpwstrName, strData.c_str(), strData.size());
    }
    
	lpName->lpguid = (LPGUID)SvPV(*guid, len);	// Lazy copy
    if(len != sizeof(GUID))
        croak("GUID parameter of MAPINAMEID must be exactly %d bytes", sizeof(GUID));

    *lppName = lpName;    
}

LPMAPINAMEID *	AV_to_p_LPMAPINAMEID(AV *av, STRLEN *lpcNames)
{
    ULONG cValues = 0;
	LPMAPINAMEID *lpNames = NULL;

	cValues = av_len(av) + 1;

    MAPIAllocateBuffer(sizeof(LPMAPINAMEID) * cValues, (void **)&lpNames);
    memset(lpNames, 0, sizeof(LPMAPINAMEID) * cValues);

	for (ULONG i = 0; i < cValues; ++i) {
		SV **entry = av_fetch(av, i, 0);
        
        if (entry == NULL)
            continue;
            
        if (!SvROK(*entry))
            croak("entry %d in MAPINAMEID array is not a reference", i);
        
        if (SvTYPE(SvRV(*entry)) != SVt_PVHV)
            croak("entry %d in MAPINAMEID array is not a HASHREF: %d", i, SvTYPE(*entry));
        
		HV_to_LPMAPINAMEID((HV*)SvRV(*entry), &lpNames[i], lpNames);
	}

	*lpcNames = cValues;

	return lpNames;
}

HV* HV_from_LPMAPIERROR(LPMAPIERROR lpMAPIError)
{
    if (!lpMAPIError)
        return (HV *)&PL_sv_undef;

	SV *version = newSViv(lpMAPIError->ulVersion);
	SV *error = newSVpv((char*)lpMAPIError->lpszError, 0);
	SV *component = newSVpv((char*)lpMAPIError->lpszComponent, 0);
	SV *lowLevelError = newSViv(lpMAPIError->ulLowLevelError);
	SV *context = newSViv(lpMAPIError->ulContext);

	HV *result = newHV();
	hv_store_simple(result, "ulVersion", version);
	hv_store_simple(result, "lpszError", error);
	hv_store_simple(result, "lpszComponent", component);
	hv_store_simple(result, "ulLowLevelError", lowLevelError);
	hv_store_simple(result, "ulContext", context);

	return result;
}

// Unsupported as of yet
HV *HV_from_LPSSortOrderSet(LPSSortOrderSet lpSortOrderSet)
{
	return (HV *)&PL_sv_undef;
}

AV *			AV_from_LPADRLIST(LPADRLIST lpAdrList)
{
	return AV_from_LPSRowSet((LPSRowSet)lpAdrList);
}

LPADRLIST		AV_to_LPADRLIST(AV *av)
{
	return (LPADRLIST)AV_to_LPSRowSet(av);
}

LPCIID		AV_to_LPCIID(AV *av, STRLEN *cIID)
{
	*cIID = 0;
	return NULL;
}

LPECUSER		HV_to_LPECUSER(HV *)
{
	return NULL;
}

HV * HV_from_LPECUSER(LPECUSER lpUser)
{
	return NULL;
}

AV * AV_from_LPECUSER(LPECUSER lpUsers, ULONG cElements)
{
	return NULL;
}

LPECGROUP		HV_to_LPECGROUP(HV *)
{
	return NULL;
}

HV * HV_from_LPECGROUP(LPECGROUP lpGroup)
{
	return NULL;
}

AV * AV_from_LPECGROUP(LPECGROUP lpGroups, ULONG cElements)
{
	return NULL;
}

LPECCOMPANY		HV_to_LPECCOMPANY(HV *)
{
	return NULL;
}

HV * HV_from_LPECCOMPANY(LPECCOMPANY lpCompany)
{
	return NULL;
}

AV * AV_from_LPECCOMPANY(LPECCOMPANY lpCompanies, ULONG cElements)
{
	return NULL;
}

LPROWLIST AV_to_LPROWLIST(AV *)
{
	return NULL;
}

AV * AV_from_LPENTRYLIST(LPENTRYLIST)
{
	return NULL;
}

HV * HV_from_LPSRestriction(LPSRestriction)
{
	return NULL;
}



LPECQUOTA HV_to_LPECQUOTA(HV *)
{
	return NULL;
}

HV * HV_from_LPECQUOTA(LPECQUOTA)
{
	return NULL;
}

LPNOTIFICATION HV_to_LPNOTIFICATION(HV *)
{
	return NULL;
}

LPECSVRNAMELIST AV_to_LPECSVRNAMELIST(AV *)
{
	return NULL;
}

AV * AV_from_LPECSERVERLIST(LPECSERVERLIST)
{
	return NULL;
}

HV * HV_from_LPECQUOTASTATUS(LPECQUOTASTATUS)
{
	return NULL;
}

HV * HV_from_LPECUSERCLIENTUPDATESTATUS(LPECUSERCLIENTUPDATESTATUS)
{
	return NULL;
}
