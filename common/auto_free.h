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

#ifndef AUTO_FREE
#define AUTO_FREE

/**
 * Auto Free deallocation template class.
 *
 * Define a deallocation function. The function accept online a function with 
 * one parameter, like void free(void*)
 * 
 * Example usage:
 * @code
 * auto_free<char, auto_free_dealloc<void*, void, free> > auto_free_char;
 *
 * auto_free_char = (char*)malloc(10);
 * @endcode
 */

template<typename _T, typename _R, _R(*_FREE)(_T)>
class auto_free_dealloc {
public:
	static inline _R free(_T d) {
		return _FREE(d);
	}
};

/**
 * Auto free template class.
 *
 * This class will delete the object automatically after leaving the scope.
 * You need to specify a deallocation function.
 *
 * @see auto_free_dealloc
 *
 * @todo add copy function and ref counting
 */
template<typename _T, class dealloc = auto_free_dealloc<_T, void, NULL> >
class auto_free {
	typedef _T value_type;
	typedef _T* pointer;
	typedef const _T* const_pointer;
public:
	auto_free() { data = NULL; }
	auto_free( pointer p ) : data(p) {}
	~auto_free() {
		free();
	}

	// Assign data
	auto_free& operator=(pointer data) {
		free();
		this->data = data;
		return *this;
	}

	pointer* operator&() {
		free();
		return &data;
	}

	pointer operator->() { return data; }
	const_pointer operator->() const { return data; }
	
	pointer operator*() { return data; }
	const_pointer operator*() const { return data; }

	operator pointer() { return data; }
	operator const_pointer() const { return data; }

	operator void*() { return data; }
	operator const void*() const { return data; }

	bool operator!=(_T* other) {
		return (data != other);
	}

	bool operator!() const {
		return data == NULL;
	}

	const bool operator()(_T* other) const {
		return data != other;
	}

	/**
	 * Release pointer
	 *
	 * Set the internal pointer to null without destructing the object.
	 * The auto_free is no longer responsible to destruct the object, which means
	 * the object should be destroyed by the caller.
	 *
	 * @return A pointer to the object. After the call, the internal pointer's value is the null pointer (points to no object).
	 */
	pointer release() {
		pointer ret = data;
		data = NULL;
		return ret;
	}

private: // Disable copying, need ref counting
	auto_free(const auto_free &other);
	auto_free& operator=(const auto_free &other);

	void free() {
		if (data) {
			dealloc::free(data);

			data = NULL;
		}
	}
	pointer data;
};

#endif // #ifndef AUTO_FREE

