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

#ifndef convert_INCLUDED
#define convert_INCLUDED

#include <map>
#include <set>
#include <list>
#include <string>
#include <stdexcept>
#include <typeinfo>

#include <iconv.h>
#include "traits.h"

/**
 * @brief	Exception class
 */
class convert_exception : public std::runtime_error
{
public:
	enum exception_type {
		eUnknownCharset,
		eIllegalSequence
	};
	convert_exception(enum exception_type type, const std::string &message);
	
	enum exception_type type() const {
		return m_type;
	}
	
private:
	enum exception_type m_type;
};

/**
 * @brief	Unknown charset
 */
class unknown_charset_exception : public convert_exception
{
public:
	unknown_charset_exception(const std::string &message);
};

/**
 * @brief	Illegal sequence
 */
class illegal_sequence_exception : public convert_exception
{
public:
	illegal_sequence_exception(const std::string &message);
};



namespace details {

	/** 
	 * @brief	Performs the generic iconv processing.
	 */
	class iconv_context_base
	{
	public:
		/**
		 * @brief Destructor.
		 */
		virtual ~iconv_context_base();
		
	protected:
		/**
		 * @brief Constructor.
		 *
		 * @param[in]  tocode		The destination charset.
		 * @param[out] fromcode		The source charset.
		 */
		iconv_context_base(const char* tocode, const char* fromcode);
		
		/**
		 * @brief Performs the actual conversion.
		 *
		 * Performs the conversion and stores the result in the output string
		 * by calling append, which must be overridden by a derived class.
		 * @param[in] lpFrom	Pointer to the source data.
		 * @param[in] cbFrom	Size of the source data in bytes.
		 */
		void doconvert(const char *lpFrom, size_t cbFrom);
		
	private:
		/**
		 * @brief Appends converted data to the result.
		 *
		 * @param[in] lpBuf		Pointer to the data to be appended.
		 * @param[in] cbBuf		Size of the data to be appended in bytes.
		 */
		virtual void append(const char *lpBuf, size_t cbBuf) throw() = 0;
		
	private:
		iconv_t	m_cd;
		bool m_bForce;
		bool m_bHTML;
		
	private:	// Disallow copying
		iconv_context_base(const iconv_context_base &);
		iconv_context_base &operator=(const iconv_context_base &);
	};


	/**
	 * @brief	Default converter from one charset to another with string types.
	 */
	template <typename _To_Type, typename _From_Type>
	class iconv_context : public iconv_context_base
	{
	public:
		/**
		 * @brief Contructor.
		 *
		 * Constructs a iconv_context_base with the right tocode and fromcode based
		 * on the _To_Type and _From_Type template parameters.
		 */
		iconv_context()
			: iconv_context_base(iconv_charset<_To_Type>::name(), iconv_charset<_From_Type>::name()) 
		{}
		
		/**
		 * @brief Contructor.
		 *
		 * Constructs a iconv_context_base with the tocode based on the _To_Type 
		 * and the passed fromcode.
		 */
		iconv_context(const char *fromcode)
			: iconv_context_base(iconv_charset<_To_Type>::name(), fromcode) 
		{}
		
		/**
		 * @brief Contructor.
		 *
		 * Constructs a iconv_context_base with the tocode based on the _To_Type 
		 * and the passed fromcode.
		 */
		iconv_context(const char *tocode, const char *fromcode)
			: iconv_context_base(tocode, fromcode) 
		{}

		/**
		 * @brief Performs the conversion.
		 *
		 * The actual conversion in delegated to iconv_context_base.
		 * @param[in] lpRaw		Raw pointer to the data to be converted.
		 * @param[in] cbRaw		The size in bytes of the data to be converted.
		 * @return				The converted string.
		 */
		_To_Type convert(const char *lpRaw, size_t cbRaw) {
			m_to.clear();
			doconvert(lpRaw, cbRaw);
			return m_to;
		}
		
		/**
		 * @brief Performs the conversion.
		 *
		 * The actual conversion in delegated to iconv_context_base.
		 * @param[in] _from		The string to be converted.
		 * @return				The converted string.
		 */
		_To_Type convert(const _From_Type &_from) {
			return convert(iconv_charset<_From_Type>::rawptr(_from), iconv_charset<_From_Type>::rawsize(_from));
		}
		
	private:
		void append(const char *lpBuf, size_t cbBuf) throw() {
			m_to.append(reinterpret_cast<typename _To_Type::const_pointer>(lpBuf), cbBuf / sizeof(typename _To_Type::value_type));
		}

	private:
		_To_Type	m_to;
	};



	/**
	 * @brief	Helper class for converting from one charset to another.
	 *
	 * The converter_helper class detects when the to and from charsets are identical. In
	 * that case the string is merely copied.
	 */
	template <typename _Type>
	class convert_helper {
	public:
		/**
		 * @brief Converts a string to a string with the same charset.
		 *
		 * Effectively this method does nothing but a copy.
		 * @param[in] _from		The string to be converted.
		 * @return				The converted string.
		 */
		static _Type convert(const _Type &_from) throw() { return _from; }

		/**
		 * @brief Converts a string to a string with a different charset.
		 *
		 * The string with a charset linked to _Other_Type is converted to a
		 * string with a charset linked to _Type.
		 * @param[in] _from		The string to be converted.
		 * @return				The converted string.
		 */
		template <typename _Other_Type>
		static _Type convert(const _Other_Type &_from) {
			details::iconv_context<_Type, _Other_Type> context;
			return context.convert(_from);
		}
	};

} // namespace details



/**
 * @brief	Converts a string to a string wirh a different charset.
 *
 * This is the function to call when a one of conversion from one charset to 
 * another is required. The to- and from charsets are implicitly determined by
 * on one side the passed _To_Type and on the other side the _from argument. 
 * @tparam	  _To_Type		The type of the destination string.
 * @param[in] _from			The string that is to be converted to another charset.
 * @return					The converted string.
 *
 * @note	Since this method needs to create an iconv object internally
 *			it is better to use a convert_context when multiple conversions
 *			need to be performed.
 */
template <typename _To_Type, typename _From_Type>
inline _To_Type convert_to(const _From_Type &_from)
{
	return details::convert_helper<_To_Type>::convert(_from);
}

/**
 * @brief	Converts a string to a string with a different charset.
 *
 * This is the function to call when a one of conversion from one charset to 
 * another is required. The to charset is implicitly determined by
 * the passed _To_Type. The from charset is determined by fromcode.
 * @tparam	  _To_Type		The type of the destination string.
 * @param[in] _from			The string that is to be converted to another charset.
 * @param[in] cbBytes		The size in bytes of the string to convert.
 * @param[in] fromcode		The source charset.
 * @return					The converted string.
 *
 * @note	Since this method needs to create an iconv object internally
 *			it is better to use a convert_context when multiple conversions
 *			need to be performed.
 */
template <typename _To_Type, typename _From_Type>
inline _To_Type convert_to(const _From_Type &_from, size_t cbBytes, const char *fromcode)
{
	details::iconv_context<_To_Type, _From_Type> context(fromcode);
	return context.convert(iconv_charset<_From_Type>::rawptr(_from), cbBytes);
}

/**
 * @brief	Converts a string to a string with a different charset.
 *
 * This is the function to call when a one of conversion from one charset to 
 * another is required. The to charset is determined by tocode.
 * The from charset is determined by fromcode.
 * @param[in] tocode		The destination charset.
 * @param[in] _from			The string that is to be converted to another charset.
 * @param[in] cbBytes		The size in bytes of the string to convert.
 * @param[in] fromcode		The source charset.
 * @return					The converted string.
 *
 * @note	Since this method needs to create an iconv object internally
 *			it is better to use a convert_context when multiple conversions
 *			need to be performed.
 */
template <typename _To_Type, typename _From_Type>
inline _To_Type convert_to(const char *tocode, const _From_Type &_from, size_t cbBytes, const char *fromcode)
{
	details::iconv_context<_To_Type, _From_Type> context(tocode, fromcode);
	return context.convert(iconv_charset<_From_Type>::rawptr(_from), cbBytes);
}


/**
 * @brief	Allows multiple conversions within the same context.
 * 
 * The convert_context class is used to perform multiple conversions within the
 * same context. This basically means that the details::iconv_context classes can
 * be reused, removing the need to recreate them for each conversion.
 */
class convert_context
{
public:
	/**
	 * @brief Constructor.
	 */
	convert_context() throw();
	
	/**
	 * @brief Destructor.
	 */
	~convert_context();

	/**
	 * @brief	Converts a string to a string wirh a different charset.
	 *
	 * The to- and from charsets are implicitly determined by on one side the 
	 * passed _To_Type and on the other side the _from argument. 
	 * @tparam	  _To_Type		The type of the destination string.
	 * @param[in] _from			The string that is to be converted to another charset.
	 * @return					The converted string.
	 */
	template <typename _To_Type, typename _From_Type>
	_To_Type convert_to(const _From_Type &_from) {
		return helper<_To_Type>(*this).convert(_from);		
	}
	
	/**
	 * @brief	Converts a string to a string wirh a different charset.
	 *
	 * The to charset is implicitly determined by the passed _To_Type.
	 * The from charset is passed in fromcode. 
	 * @tparam	  _To_Type		The type of the destination string.
	 * @param[in] _from			The string that is to be converted to another charset.
	 * @param[in] cbBytes		The size in bytes of the string to convert.
	 * @param[in] fromcode		The source charset.
	 * @return					The converted string.
	 */
	template <typename _To_Type, typename _From_Type>
	_To_Type convert_to(const _From_Type &_from, size_t cbBytes, const char *fromcode) {
		return helper<_To_Type>(*this).convert(_from, cbBytes, fromcode);
	}
	
	/**
	 * @brief	Converts a string to a string wirh a different charset.
	 *
	 * The to charset is passed in tocode.
	 * The from charset is passed in fromcode. 
	 * @param[in] tocode		the destination charset.
	 * @param[in] _from			The string that is to be converted to another charset.
	 * @param[in] cbBytes		The size in bytes of the string to convert.
	 * @param[in] fromcode		The source charset.
	 * @return					The converted string.
	 */
	template <typename _To_Type, typename _From_Type>
	_To_Type convert_to(const char *tocode, const _From_Type &_from, size_t cbBytes, const char *fromcode) {
		return helper<_To_Type>(*this).convert(tocode, _from, cbBytes, fromcode);
	}
	
private:
	/**
	 * @brief	Helper class for converting from one charset to another.
	 *
	 * The convert_context::helper class detects when the to and from charsets are
	 * identical. In that case the string is merely copied.
	 */
	template <typename _Type>
	class helper {
	public:
		/**
		 * @brief Constructor.
		 */
		helper(convert_context &context) throw()
			: m_context(context) 
		{}

		/**
		 * @brief Converts a string to a string with the same charset.
		 *
		 * Effectively this method does nothing but a copy.
		 * @param[in] _from		The string to be converted.
		 * @return				The converted string.
		 */
		_Type convert(const _Type &_from) throw() {
			return _from;
		}

		/**
		 * @brief Converts a string to a string with a different charset.
		 *
		 * The string with a charset linked to _Other_Type is converted to a
		 * string with a charset linked to _Type. The actual conversion is
		 * delegated to a iconv_context obtained through get_context().
		 * @param[in] _from		The string to be converted.
		 * @return				The converted string.
		 */
		template <typename _Other_Type>
		_Type convert(const _Other_Type &_from) {
			return m_context.get_context<_Type, _Other_Type>()->convert(_from);
		}
		
		/**
		 * @brief Converts a string to a string with a different charset.
		 *
		 * The string with a charset specified with fromcode is converted to a
		 * string with a charset linked to _Type. The actual conversion is
		 * delegated to a iconv_context obtained through get_context().
		 * @param[in] _from		The string to be converted.
		 * @param[in] cbBytes	The size in bytes of the string to convert.
		 * @param[in] fromcode	The source charset.
		 * @return				The converted string.
		 */
		template <typename _Other_Type>
		_Type convert(const _Other_Type &_from, size_t cbBytes, const char *fromcode) {
			return m_context.get_context<_Type, _Other_Type>(fromcode)->convert(iconv_charset<_Other_Type>::rawptr(_from), cbBytes);
		}
		
		/**
		 * @brief Converts a string to a string with a different charset.
		 *
		 * The string with a charset specified with fromcode is converted to a
		 * string with a charset specified with tocode. The actual conversion is
		 * delegated to a iconv_context obtained through get_context().
		 * @param[in] tocode	The destination charset.
		 * @param[in] _from		The string to be converted.
		 * @param[in] cbBytes	The size in bytes of the string to convert.
		 * @param[in] fromcode	The source charset.
		 * @return				The converted string.
		 */
		template <typename _Other_Type>
		_Type convert(const char *tocode, const _Other_Type &_from, size_t cbBytes, const char *fromcode) {
			return m_context.get_context<_Type, _Other_Type>(tocode, fromcode)->convert(iconv_charset<_Other_Type>::rawptr(_from), cbBytes);
		}
		
	private:
		convert_context	&m_context;
	};
	
	/**
	 * @brief	Helper class for converting from one charset to another.
	 *
	 * This specialization is used to convert to pointer types. In that case the
	 * result needs to be stores to guarantee storage of the data. Without this
	 * the caller will end up with a pointer to non-existing data.
	 */
	template <typename _Type>
	class helper<_Type*> {
	public:
		typedef std::basic_string<_Type> string_type;
	
		/**
		 * @brief Constructor.
		 */
		helper(convert_context &context) throw()
			: m_context(context) 
			, m_helper(context)
		{}

		/**
		 * @brief Converts a string to a string with a different charset.
		 *
		 * The string with a charset linked to _Other_Type is converted to a
		 * string with a charset linked to _Type. The actual conversion is
		 * delegated to a iconv_context obtained through get_context().
		 * @param[in] _from		The string to be converted.
		 * @return				The converted string.
		 */
		template <typename _Other_Type>
		_Type* convert(const _Other_Type &_from) {
			string_type s = m_helper.convert(_from);
			return m_context.persist_string(s);
		}
		
		/**
		 * @brief Converts a string to a string with a different charset.
		 *
		 * The string with a charset specified with fromcode is converted to a
		 * string with a charset linked to _Type. The actual conversion is
		 * delegated to a iconv_context obtained through get_context().
		 * @param[in] _from		The string to be converted.
		 * @param[in] cbBytes	The size in bytes of the string to convert.
		 * @param[in] fromcode	The source charset.
		 * @return				The converted string.
		 */
		template <typename _Other_Type>
		_Type* convert(const _Other_Type &_from, size_t cbBytes, const char *fromcode) {
			string_type s = m_helper.convert(_from, cbBytes, fromcode);
			return m_context.persist_string(s);
		}
		
		/**
		 * @brief Converts a string to a string with a different charset.
		 *
		 * The string with a charset specified with fromcode is converted to a
		 * string with a charset specified with tocode. The actual conversion is
		 * delegated to a iconv_context obtained through get_context().
		 * @param[in] tocode	The destination charset.
		 * @param[in] _from		The string to be converted.
		 * @param[in] cbBytes	The size in bytes of the string to convert.
		 * @param[in] fromcode	The source charset.
		 * @return				The converted string.
		 */
		template <typename _Other_Type>
		_Type* convert(const char *tocode, const _Other_Type &_from, size_t cbBytes, const char *fromcode) {
			string_type s = m_helper.convert(tocode, _from, cbBytes, fromcode);
			return m_context.persist_string(s);
		}
		
	private:
		convert_context	&m_context;
		helper<string_type> m_helper;
	};
	
	/**
	 * @brief Key for the context_map;
	 */
	typedef struct _context_key {
		const char *totype;
		const char *tocode;
		const char *fromtype;
		const char *fromcode;
	} context_key;

	/** Create a context_key based on the to- and from types and optionaly the to- and from codes.
	 *
	 * @tparam	_To_Type
	 *			The destination type.
	 * @tparam	_From_Type
	 *			The source type.
	 * @param[in]	tocode
	 *			The destination encoding. NULL for autodetect (based on _To_Type).
	 * @param[in]	fromcode
	 *			The source encoding. NULL for autodetect (based on_From_Type).
	 *
	 * @return	The new context_key
	 */
	template <typename _To_Type, typename _From_Type>
	context_key create_key(const char *tocode, const char *fromcode) {
		context_key key = {
			typeid(_To_Type).name(), 
			(tocode ? tocode : iconv_charset<_To_Type>::name()), 
			typeid(_From_Type).name(),
			(fromcode ? fromcode : iconv_charset<_From_Type>::name())
		};
		return key;
	}

	/**
	 * @brief Sort predicate for the context_map;
	 */
	class context_predicate {
	public:
		bool operator()(const context_key &lhs, const context_key &rhs) const throw() {
			int r = strcmp(lhs.fromtype, rhs.fromtype);
			if (r != 0)
				return (r < 0);

			r = strcmp(lhs.totype, rhs.totype);
			if (r != 0)
				return (r < 0);

			r = strcmp(lhs.fromcode, rhs.fromcode);
			if (r != 0)
				return (r < 0);

			return (strcmp(lhs.tocode, rhs.tocode) < 0);
		}
	};
	
	/**
	 * @brief Map containing contexts that can be reused.
	 */
	typedef std::map<context_key, details::iconv_context_base*, context_predicate>	context_map;

	/**
	 * @brief Set containing dynamic allocated from- and to codes.
	 */
	typedef std::set<const char*> code_set;
	
private:
	/**
	 * @brief Obtains an iconv_context object.
	 *
	 * The correct iconv_context is based on _To_Type and _From_Type and is
	 * obtained from the context_map. If the correct iconv_context is not found a new
	 * one is created and stored in the context_map;
	 * @tparam	_To_Type	The type of the destination string.
	 * @tparam	_From_Type	The type of the source string.
	 * @return				A pointer to a iconv_context.
	 */
	template <typename _To_Type, typename _From_Type>
	details::iconv_context<_To_Type, _From_Type> *get_context() {
		context_key key(create_key<_To_Type, _From_Type>(NULL, NULL));
		context_map::const_iterator iContext = m_contexts.find(key);
		if (iContext == m_contexts.end()) {
			details::iconv_context_base *lpContext = new details::iconv_context<_To_Type, _From_Type>();
			iContext = m_contexts.insert(context_map::value_type(key, lpContext)).first;
		}
		return dynamic_cast<details::iconv_context<_To_Type, _From_Type>*>(iContext->second);
	}
	
	/**
	 * @brief Obtains an iconv_context object.
	 *
	 * The correct iconv_context is based on _To_Type and fromcode and is
	 * obtained from the context_map. If the correct iconv_context is not found a new
	 * one is created and stored in the context_map;
	 * @tparam		_To_Type	The type of the destination string.
	 * @param[in]	fromcode	The source charset.
	 * @return					A pointer to a iconv_context.
	 */
	template <typename _To_Type, typename _From_Type>
	details::iconv_context<_To_Type, _From_Type> *get_context(const char *fromcode) {
		context_key key(create_key<_To_Type, _From_Type>(NULL, fromcode));
		context_map::const_iterator iContext = m_contexts.find(key);
		if (iContext == m_contexts.end()) {
			details::iconv_context_base *lpContext = new details::iconv_context<_To_Type, _From_Type>(fromcode);
			
			// Before we store it, we need to copy the fromcode as we don't know what the
			// lifetime will be.
			persist_code(key, pfFromCode);
			
			iContext = m_contexts.insert(context_map::value_type(key, lpContext)).first;
		}
		return dynamic_cast<details::iconv_context<_To_Type, _From_Type>*>(iContext->second);
	}

	/**
	 * @brief Obtains an iconv_context object.
	 *
	 * The correct iconv_context is based on tocode and fromcode and is
	 * obtained from the context_map. If the correct iconv_context is not found a new
	 * one is created and stored in the context_map;
	 * @param[in]	tocode		The destination charset.
	 * @param[in]	fromcode	The source charset.
	 * @return					A pointer to a iconv_context.
	 */
	template <typename _To_Type, typename _From_Type>
	details::iconv_context<_To_Type, _From_Type> *get_context(const char *tocode, const char *fromcode) {
		context_key key(create_key<_To_Type, _From_Type>(tocode, fromcode));
		context_map::const_iterator iContext = m_contexts.find(key);
		if (iContext == m_contexts.end()) {
			details::iconv_context_base *lpContext = new details::iconv_context<_To_Type, _From_Type>(tocode, fromcode);
			
			// Before we store it, we need to copy the fromcode as we don't know what the
			// lifetime will be.
			persist_code(key, pfToCode|pfFromCode);
			
			iContext = m_contexts.insert(context_map::value_type(key, lpContext)).first;
		}
		return dynamic_cast<details::iconv_context<_To_Type, _From_Type>*>(iContext->second);
	}

	/**
	 * @brief Flags that determine which code of a context_key is persisted
	 */
	enum {
		pfToCode = 1,
		pfFromCode = 2
	};

	/**
	 * @brief	Persists the code for the fromcode when it's not certain it won't
	 *			be destroyed while in use.
	 *
	 * @param[in,out]	key		The key for which the second field will be persisted.
	 */
	void persist_code(context_key &key, unsigned flags);
	
	/**
	 * Persist the string so a raw pointer to its content can be used.
	 * 
	 * The pointer that can be used is returned by this function. Using the
	 * pointer to the data of the original string will be a recipe to disaster.
	 * 
	 * @param[in]	string		The string to persist.
	 * @return		The raw pointer that can be used as long as the convert_context exists.
	 */
	char *persist_string(const std::string &strValue);
	
	/**
	 * Persist the string so a raw pointer to its content can be used.
	 * 
	 * The pointer that can be used is returned by this function. Using the
	 * pointer to the data of the original string will be a recipe to disaster.
	 * 
	 * @param[in]	string		The string to persist.
	 * @return		The raw pointer that can be used as long as the convert_context exists.
	 */
	wchar_t *persist_string(const std::wstring &wstrValue);
	
private:
	code_set	m_codes;
	context_map	m_contexts;
	std::list<std::string>	m_lstStrings;
	std::list<std::wstring>	m_lstWstrings;
	
// a convert_context is not supposed to be copyable.
private:
	convert_context(const convert_context &);
	convert_context &operator=(const convert_context &);
};



/** Convert a string to UTF-8.
 *
 * @param[in]	_context
 *					The convert_context used for the conversion
 * @param[in]	_ptr
 *					Pointer to the string containing the data to be converted.
 * @param[in]	_flags
 *					If set to MAPI_UNICODE, the _ptr argument is interpreted as a wide character string. Otherwise
 *					the _ptr argument is interpreted as a single byte string encoded in the current locale.
 *
 * @return	The converted string.
 */
#define TO_UTF8(_context, _ptr, _flags)													\
	((_ptr) ?																			\
		(_context).convert_to<char*>("UTF-8", (_ptr),									\
		((_flags) & MAPI_UNICODE) ? sizeof(wchar_t) * wcslen((wchar_t*)(_ptr)) : strlen((char*)(_ptr)),	\
		((_flags) & MAPI_UNICODE) ? CHARSET_WCHAR : CHARSET_CHAR)						\
	: NULL )

/**
 * Convert a string to UTF-8 with default arguments.
 *
 * This version requeres the convert_context to be named 'converter' and the flags argument 'ulFlags'.
 *
 * @param[in]	_ptr
 *					Pointer to the string containing the data to be converted.
 *
 * @return	The converted string.
 */
#define TO_UTF8_DEF(_ptr)					\
	TO_UTF8(converter, (_ptr), ulFlags)



#ifdef MAPIDEFS_H

namespace details {

	HRESULT HrFromException(const convert_exception &ce);

} // namespace details

/**
 * @brief	Converts a string from one charset to another. Failure is indicated
 *			through the return code instead of an exception.
 *
 * @param[in]  _from	The string to be converted.
 * @param[out] _to		The converted string.
 * @return				HRESULT.
 */
template <typename _To_Type, typename _From_Type>
HRESULT TryConvert(const _From_Type &_from, _To_Type &_to) throw() {
	try {
		_to = convert_to<_To_Type>(_from);
		return hrSuccess;
	} catch (const convert_exception &ce) {
		return details::HrFromException(ce);
	}
}


/**
 * @brief	Converts a string from one charset to another. Failure is indicated
 *			through the return code instead of an exception.
 *
 * @param[in]  _from	The string to be converted.
 * @param[in] cbBytes	The size in bytes of the string to convert.
 * @param[in]  fromcode The source charset.
 * @param[out] _to		The converted string.
 * @return				HRESULT.
 */
template <typename _To_Type, typename _From_Type>
HRESULT TryConvert(const _From_Type &_from, size_t cbBytes, const char *fromcode, _To_Type &_to) throw() {
	try {
		_to = convert_to<_To_Type>(_from, cbBytes, fromcode);
		return hrSuccess;
	} catch (const convert_exception &ce) {
		return details::HrFromException(ce);
	}
}


/**
 * @brief	Converts a string from one charset to another. Failure is indicated
 *			through the return code instead of an exception.
 *
 * @param[in]  context	A convert_context to perform the conversion on.
 * @param[in]  _from	The string to be converted.
 * @param[out] _to		The converted string.
 * @return				HRESULT.
 */
template <typename _To_Type, typename _From_Type>
HRESULT TryConvert(convert_context &context, const _From_Type &_from, _To_Type &_to) throw() {
	try {
		_to = context.convert_to<_To_Type>(_from);
		return hrSuccess;
	} catch (const convert_exception &ce) {
		return details::HrFromException(ce);
	}
}


/**
 * @brief	Converts a string from one charset to another. Failure is indicated
 *			through the return code instead of an exception.
 *
 * @param[in]  context	A convert_context to perform the conversion on.
 * @param[in]  _from	The string to be converted.
 * @param[in] cbBytes	The size in bytes of the string to convert.
 * @param[in]  fromcode The source charset.
 * @param[out] _to		The converted string.
 * @return				HRESULT.
 */
template <typename _To_Type, typename _From_Type>
HRESULT TryConvert(convert_context &context, const _From_Type &_from, size_t cbBytes, const char *fromcode, _To_Type &_to) throw() {
	try {
		_to = context.convert_to<_To_Type>(_from, cbBytes, fromcode);
		return hrSuccess;
	} catch (const convert_exception &ce) {
		return details::HrFromException(ce);
	}
}

#endif // MAPIDEFS_H

#endif // ndef convert_INCLUDED
