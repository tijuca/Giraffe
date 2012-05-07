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

#include "CLucene/StdHeader.h"
#include "CLucene/analysis/standard/StandardTokenizerConstants.h"
#include "CLucene/analysis/standard/StandardFilter.h"
#include "CLucene/analysis/standard/StandardTokenizer.h"
#include "CLucene/analysis/Analyzers.h"
#include "ECAnalyzers.h"

#include <iostream>
#include <algorithm>
#include <string.h>
#include "stringutil.h"

EmailFilter::EmailFilter(TokenStream *in, bool deleteTokenStream) : TokenFilter(in, deleteTokenStream) {
	part = 0;
}

EmailFilter::~EmailFilter() {
}

/**
 * Filters e-mail tokens (like user@domain.com) into multiple tokens;
 * - user part (user)
 * - host part (domain.com)
 * - all parts (user domain com)
 * - original token (user@domain.com)
 *
 * @param token Output token
 * @return false if no more token was available
 */
bool EmailFilter::next(lucene::analysis::Token *token) {
	// See if we had any stored tokens
	if(part < parts.size()) {
		token->set(parts[part].c_str(), 0, 0, _T("<EMAIL>"));
		token->setPositionIncrement(0);
		part++;
		return true;
	} else {
		// No more stored token, get a new one
		if(!input->next(token))
			return false;

		// Split EMAIL tokens into the various parts
		if(wcscmp(token->type(), L"<EMAIL>") == 0) {
			// Split into user, domain, com
			parts = tokenize((std::wstring)token->_termText, (std::wstring)L".@");
			// Split into user, domain.com
			std::vector<std::wstring> moreparts = tokenize((std::wstring)token->_termText, (std::wstring)L"@");
			parts.insert(parts.end(), moreparts.begin(), moreparts.end());
			
			// Only add parts once (unique parts)
			std::sort(parts.begin(), parts.end());
			parts.erase(std::unique(parts.begin(), parts.end()), parts.end());
			
			part = 0;
		}
		
		return true;
	}
}

ECAnalyzer::ECAnalyzer()
{
}

ECAnalyzer::~ECAnalyzer()
{
}

/**
 * Generate a tokenStream based on the standard lucene tokenStream, but add our EmailFilter
 *
 * @param fieldName Field that the data will be filtered into
 * @param reader Reader to read the bytestream to tokenize
 * @return A TokenStream outputting the tokens to be indexed
 */
lucene::analysis::TokenStream* ECAnalyzer::tokenStream(const TCHAR* fieldName, lucene::util::Reader* reader) 
{
	lucene::analysis::TokenStream* ret = _CLNEW lucene::analysis::standard::StandardTokenizer(reader);
	ret = _CLNEW lucene::analysis::standard::StandardFilter(ret,true);
	ret = _CLNEW lucene::analysis::LowerCaseFilter(ret,true);
	ret = _CLNEW EmailFilter(ret,true);
	return ret;
}
