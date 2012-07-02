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

#include "platform.h"
#include "itercomp.h"

#include <unicode/coll.h>
#include <unicode/tblcoll.h>
#include <unicode/coleitr.h>

#include <memory>



MakeIterator::MakeIterator(const char *lpsz)
: m_us(lpsz)
, m_ucci(m_us.getBuffer(), m_us.length())
{ }

MakeIterator::operator const CharacterIterator &() const {
	return m_ucci;
}



static RuleBasedCollator *createCollator(const Locale &locale, bool ignoreCase)
{
	UErrorCode status = U_ZERO_ERROR;
	Collator *lpCollator(Collator::createInstance(locale, status));
	
	RuleBasedCollator *lpRBCollator = dynamic_cast<RuleBasedCollator*>(lpCollator);
	assert(lpRBCollator != NULL);

	status = U_ZERO_ERROR;
	lpRBCollator->setAttribute(UCOL_STRENGTH, (ignoreCase ? UCOL_SECONDARY : UCOL_TERTIARY), status);

	return lpRBCollator;
}

int ic_compare(const CharacterIterator &i1, const CharacterIterator &i2, const Locale &locale, bool ignoreCase)
{
	std::auto_ptr<RuleBasedCollator> ptrCollator(createCollator(locale, ignoreCase));
		
	std::auto_ptr<CollationElementIterator> cei1(ptrCollator->createCollationElementIterator(i1));
	std::auto_ptr<CollationElementIterator> cei2(ptrCollator->createCollationElementIterator(i2));

	UErrorCode status = U_ZERO_ERROR;
	while (true) {
		int32_t r1 = cei1->next(status);
		int32_t s1 = cei1->strengthOrder(r1);
		
		int32_t r2 = cei2->next(status);
		int32_t s2 = cei2->strengthOrder(r2);

		// @note
		// NULLORDER is returned on error or end of string, but since we wrongly set the length (utf8iter.cpp)
		// we need to check for 0 too, since we correctly step through the characters
		if (s1 != s2) {
				if (r1 == CollationElementIterator::NULLORDER || r1 == 0)
					return -1;
				else if (r2 == CollationElementIterator::NULLORDER || r2 == 0)
					return 1;
				else
					return s1 - s2;
		}
		else if (r1 == CollationElementIterator::NULLORDER || r1 == 0) // Since s1 == s2 here, we must have reach both ends
			return 0;
	}
}

