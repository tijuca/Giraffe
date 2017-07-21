/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 * Copyright 2016 Kopano and its licensors
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

#ifndef ECRestrictionBuilder_INCLUDED
#define ECRestrictionBuilder_INCLUDED

#include <memory>
#include <utility>
#include <kopano/zcdefs.h>
#include <mapidefs.h>

#include <list>

namespace KC {

/*
 * Caution. Since ECXXRestriction(ECXXRestriction &&) will be defined in
 * classes for the purpose of clone_and_move, that constructor cannot be used
 * to create nested restrictions. The three affected cases are:
 *
 * - ECAndRestriction(ECAndRestriction(expr)) is redundant and equal to
 *   just writing ECAndRestriction(expr).
 * - Same goes for ECOrRestriction.
 * - ECNotRestriction(ECNotRestriction(expr)) is silly to write; just use expr.
 */

class ECRestrictionList;

/**
 * Base class for all other ECxxxRestriction classes.
 * It defines the interface needed to hook the various restrictions together.
 */
class _kc_export ECRestriction {
public:
	enum {
		Full    = 0,
		Cheap	= 1, // Stores the passed LPSPropValue pointer.
		Shallow = 2		// Creates a new SPropValue, but point to the embedded data from the original structure.
	};

	_kc_hidden virtual ~ECRestriction(void) _kc_impdtor;

	/**
	 * Create an LPSRestiction object that represents the restriction on which CreateMAPIRestriction was called.
	 * @param[in]	lppRestriction	Pointer to an LPSRestriction pointer, that will be set to the address
	 * 								of the newly allocated restriction. This memory must be freed with
	 * 								MAPIFreeBuffer.
	 * @param[in]	ulFlags			When set to ECRestriction::Cheap, not all data will be copied to the 
	 * 								new MAPI restriction. This is useful if the ECRestriction will outlive
	 * 								the MAPI restriction.
	 */
	HRESULT CreateMAPIRestriction(LPSRestriction *lppRestriction, ULONG ulFlags) const;
	
	/**
	 * Apply the restriction on a table.
	 * @param[in]	lpTable		The table on which to apply the restriction.
	 */
	HRESULT RestrictTable(IMAPITable *, unsigned int flags = TBL_BATCH) const;

	/**
	 * Use the restriction to perform a FindRow on the passed table.
	 * @param[in]	lpTable		The table on which to perform the FindRow.
	 * @param[in]	BkOrigin	The location to start searching from. Directly passed to FindRow.
	 * @param[in]	ulFlags		Flags controlling search behaviour. Directly passed to FindRow.
	 */
	_kc_export HRESULT FindRowIn(LPMAPITABLE, BOOKMARK origin, ULONG flags) const;

	/**
	 * Populate an SRestriction structure based on the objects state.
	 * @param[in]		lpBase			Base pointer used for allocating additional memory.
	 * @param[in,out]	lpRestriction	Pointer to the SRestriction object that is to be populated.
	 */
	_kc_hidden virtual HRESULT GetMAPIRestriction(LPVOID base, LPSRestriction, ULONG flags = 0) const = 0;
	
	/**
	 * Create a new ECRestriction derived class of the same type of the object on which this
	 * method is invoked.
	 * @return	A copy of the current object.
	 */
	_kc_hidden virtual ECRestriction *Clone(void) const _kc_lvqual = 0;
#ifdef HAVE_MF_QUAL
	_kc_hidden virtual ECRestriction *Clone(void) && = 0;
	_kc_hidden ECRestrictionList operator+(ECRestriction &&) &&;
#endif
	_kc_hidden ECRestrictionList operator+(const ECRestriction &) const;

protected:
	typedef std::shared_ptr<SPropValue> PropPtr;
	typedef std::shared_ptr<ECRestriction> ResPtr;
	typedef std::list<ResPtr>					ResList;

	_kc_hidden ECRestriction(void) = default;
	_kc_hidden static HRESULT CopyProp(SPropValue *src, void *base, ULONG flags, SPropValue **dst);
	_kc_hidden static HRESULT CopyPropArray(ULONG nvals, SPropValue *src, void *base, ULONG flags, SPropValue **dst);
	_kc_hidden static void DummyFree(LPVOID);
};

/**
 * An ECRestrictionList is a list of ECRestriction objects.
 * This class is used to allow a list of restrictions to be passed in the
 * constructors of the ECAndRestriction and the ECOrRestriction classes.
 * It's implicitly created by +-ing multiple ECRestriction objects.
 */
class ECRestrictionList _kc_final {
public:
	ECRestrictionList(const ECRestriction &res1, const ECRestriction &res2) {
		m_list.push_back(ResPtr(res1.Clone()));
		m_list.push_back(ResPtr(res2.Clone()));
	}
	ECRestrictionList(ECRestriction &&o1, ECRestriction &&o2)
	{
		m_list.push_back(ResPtr(std::move(o1).Clone()));
		m_list.push_back(ResPtr(std::move(o2).Clone()));
	}
	
	ECRestrictionList& operator+(const ECRestriction &restriction) {
		m_list.push_back(ResPtr(restriction.Clone()));
		return *this;
	}
	ECRestrictionList &operator+(ECRestriction &&o)
	{
		m_list.push_back(ResPtr(std::move(o).Clone()));
		return *this;
	}

private:
	typedef std::shared_ptr<ECRestriction> ResPtr;
	typedef std::list<ResPtr>					ResList;

	ResList	m_list;

friend class ECAndRestriction;
friend class ECOrRestriction;
};

/**
 * Add two restrictions together and create an ECRestrictionList.
 * @param[in]	restriction		The restriction to add with the current.
 * @return		The ECRestrictionList with the two entries.
 */
inline ECRestrictionList ECRestriction::operator+ (const ECRestriction &other) const {
	return ECRestrictionList(*this, other);
}

#ifdef HAVE_MF_QUAL
inline ECRestrictionList ECRestriction::operator+(ECRestriction &&other) &&
{
	return ECRestrictionList(std::move(*this), std::move(other));
}
#endif

class IRestrictionPush : public ECRestriction {
	public:
	virtual ECRestriction *operator+=(const ECRestriction &) = 0;
	virtual ECRestriction *operator+=(ECRestriction &&) = 0;
};

class _kc_export ECAndRestriction _kc_final : public IRestrictionPush {
public:
	_kc_hidden ECAndRestriction(void) {}
	ECAndRestriction(const ECRestrictionList &list);
	_kc_hidden ECAndRestriction(ECRestrictionList &&o) :
		m_lstRestrictions(std::move(o.m_list))
	{}
	_kc_hidden HRESULT GetMAPIRestriction(LPVOID base, LPSRestriction res, ULONG flags) const _kc_override;
	ECRestriction *Clone(void) const _kc_lvqual _kc_override;
#ifdef HAVE_MF_QUAL
	_kc_hidden ECRestriction *Clone(void) && _kc_override { return new ECAndRestriction(std::move(*this)); }
#endif

	ECRestriction *operator+=(const ECRestriction &restriction)
	{
		m_lstRestrictions.push_back(ResPtr(restriction.Clone()));
		return m_lstRestrictions.rbegin()->get();
	}
	ECRestriction *operator+=(ECRestriction &&o)
	{
		m_lstRestrictions.push_back(ResPtr(std::move(o).Clone()));
		return m_lstRestrictions.rbegin()->get();
	}

	void operator+=(const ECRestrictionList &list);
	void operator+=(ECRestrictionList &&);

private:
	ResList	m_lstRestrictions;
};

class _kc_export ECOrRestriction _kc_final : public IRestrictionPush {
public:
	_kc_hidden ECOrRestriction(void) {}
	ECOrRestriction(const ECRestrictionList &list);
	_kc_hidden ECOrRestriction(ECRestrictionList &&o) :
		m_lstRestrictions(std::move(o.m_list))
	{}
	_kc_hidden HRESULT GetMAPIRestriction(LPVOID base, LPSRestriction r, ULONG flags) const _kc_override;
	ECRestriction *Clone(void) const _kc_lvqual _kc_override;
#ifdef HAVE_MF_QUAL
	ECRestriction *Clone(void) && _kc_override { return new ECOrRestriction(std::move(*this)); }
#endif

	ECRestriction *operator+=(const ECRestriction &restriction)
	{
		m_lstRestrictions.push_back(ResPtr(restriction.Clone()));
		return m_lstRestrictions.rbegin()->get();
	}
	ECRestriction *operator+=(ECRestriction &&o)
	{
		m_lstRestrictions.push_back(ResPtr(std::move(o).Clone()));
		return m_lstRestrictions.rbegin()->get();
	}

	void operator+=(const ECRestrictionList &list);
	void operator+=(ECRestrictionList &&);

private:
	ResList	m_lstRestrictions;
};

class _kc_export ECNotRestriction _kc_final : public IRestrictionPush {
public:
	_kc_hidden ECNotRestriction(const ECRestriction &restriction)
	: m_ptrRestriction(ResPtr(restriction.Clone())) 
	{ }
	_kc_hidden ECNotRestriction(ECRestriction &&o) :
		m_ptrRestriction(std::move(o).Clone())
	{}
	ECNotRestriction(std::nullptr_t) {}

	_kc_hidden HRESULT GetMAPIRestriction(LPVOID base, LPSRestriction r, ULONG flags) const _kc_override;
	ECRestriction *Clone(void) const _kc_lvqual _kc_override;
#ifdef HAVE_MF_QUAL
	ECRestriction *Clone(void) && _kc_override { return new ECNotRestriction(std::move(*this)); }
#endif
	ECRestriction *operator+=(const ECRestriction &r)
	{
		m_ptrRestriction.reset(r.Clone());
		return m_ptrRestriction.get();
	}
	ECRestriction *operator+=(ECRestriction &&r)
	{
		m_ptrRestriction.reset(std::move(r).Clone());
		return m_ptrRestriction.get();
	}

private:
	_kc_hidden ECNotRestriction(ResPtr restriction);

	ResPtr	m_ptrRestriction;
};

class _kc_export ECContentRestriction _kc_final : public ECRestriction {
public:
	ECContentRestriction(ULONG fuzzy_lvl, ULONG tag, const SPropValue *, ULONG flags);
	_kc_hidden HRESULT GetMAPIRestriction(LPVOID base, LPSRestriction r, ULONG flags) const _kc_override;
	ECRestriction *Clone(void) const _kc_lvqual _kc_override;
#ifdef HAVE_MF_QUAL
	_kc_hidden ECRestriction *Clone(void) && _kc_override { return new ECContentRestriction(std::move(*this)); }
#endif

private:
	_kc_hidden ECContentRestriction(ULONG fuzzy_level, ULONG tag, PropPtr prop);

private:
	ULONG	m_ulFuzzyLevel;
	ULONG	m_ulPropTag;
	PropPtr	m_ptrProp;
};

class _kc_export ECBitMaskRestriction _kc_final : public ECRestriction {
public:
	_kc_hidden ECBitMaskRestriction(ULONG relBMR, ULONG ulPropTag, ULONG ulMask)
	: m_relBMR(relBMR)
	, m_ulPropTag(ulPropTag)
	, m_ulMask(ulMask) 
	{ }

	_kc_hidden HRESULT GetMAPIRestriction(LPVOID base, LPSRestriction r, ULONG flags) const _kc_override;
	ECRestriction *Clone(void) const _kc_lvqual _kc_override;
#ifdef HAVE_MF_QUAL
	_kc_hidden ECRestriction *Clone(void) && _kc_override { return new ECBitMaskRestriction(std::move(*this)); }
#endif

private:
	ULONG	m_relBMR;
	ULONG	m_ulPropTag;
	ULONG	m_ulMask;
};

class _kc_export ECPropertyRestriction _kc_final : public ECRestriction {
public:
	ECPropertyRestriction(ULONG relop, ULONG tag, const SPropValue *, ULONG flags);
	_kc_hidden HRESULT GetMAPIRestriction(LPVOID base, LPSRestriction r, ULONG flags) const _kc_override;
	ECRestriction *Clone(void) const _kc_lvqual _kc_override;
#ifdef HAVE_MF_QUAL
	_kc_hidden ECRestriction *Clone(void) && _kc_override { return new ECPropertyRestriction(std::move(*this)); }
#endif

private:
	_kc_hidden ECPropertyRestriction(ULONG relop, ULONG proptag, PropPtr prop);

	ULONG	m_relop;
	ULONG	m_ulPropTag;
	PropPtr	m_ptrProp;
};

class _kc_export ECComparePropsRestriction _kc_final : public ECRestriction {
public:
	_kc_hidden ECComparePropsRestriction(ULONG relop, ULONG ulPropTag1, ULONG ulPropTag2)
	: m_relop(relop)
	, m_ulPropTag1(ulPropTag1)
	, m_ulPropTag2(ulPropTag2)
	{ }

	_kc_hidden HRESULT GetMAPIRestriction(LPVOID base, LPSRestriction r, ULONG flags) const _kc_override;
	ECRestriction *Clone(void) const _kc_lvqual _kc_override;
#ifdef HAVE_MF_QUAL
	_kc_hidden ECRestriction *Clone(void) && _kc_override { return new ECComparePropsRestriction(std::move(*this)); }
#endif

private:
	ULONG	m_relop;
	ULONG	m_ulPropTag1;
	ULONG	m_ulPropTag2;
};

class _kc_export ECExistRestriction _kc_final : public ECRestriction {
public:
	_kc_hidden ECExistRestriction(ULONG ulPropTag)
	: m_ulPropTag(ulPropTag) 
	{ }

	_kc_hidden HRESULT GetMAPIRestriction(LPVOID base, LPSRestriction r, ULONG flags) const _kc_override;
	ECRestriction *Clone(void) const _kc_lvqual _kc_override;
#ifdef HAVE_MF_QUAL
	_kc_hidden ECRestriction *Clone(void) && _kc_override { return new ECExistRestriction(std::move(*this)); }
#endif

private:
	ULONG	m_ulPropTag;
};

/**
 * This is a special class, which encapsulates a raw SRestriction structure to allow
 * prebuild or obtained restriction structures to be used in the ECRestriction model.
 */
class _kc_export ECRawRestriction _kc_final : public ECRestriction {
public:
	ECRawRestriction(const SRestriction *, ULONG flags);
	_kc_hidden HRESULT GetMAPIRestriction(LPVOID base, LPSRestriction r, ULONG flags) const _kc_override;
	ECRestriction *Clone(void) const _kc_lvqual _kc_override;
#ifdef HAVE_MF_QUAL
	_kc_hidden ECRestriction *Clone(void) && _kc_override { return new ECRawRestriction(std::move(*this)); }
#endif

private:
	typedef std::shared_ptr<SRestriction> RawResPtr;
	_kc_hidden ECRawRestriction(RawResPtr restriction);

	RawResPtr	m_ptrRestriction;
};

} /* namespace */

#endif // ndef ECRestrictionBuilder_INCLUDED
