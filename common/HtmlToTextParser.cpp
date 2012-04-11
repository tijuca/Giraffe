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

#include "platform.h"
#include "HtmlToTextParser.h"
#include "HtmlEntity.h"
#include <wctype.h>


CHtmlToTextParser::CHtmlToTextParser(void)
{
	tagMap[L"head"] = tagParser(false, &CHtmlToTextParser::parseTagHEAD);
	tagMap[L"/head"] = tagParser(false, &CHtmlToTextParser::parseTagBHEAD);
	tagMap[L"style"] = tagParser(false, &CHtmlToTextParser::parseTagSTYLE);
	tagMap[L"/style"] = tagParser(false, &CHtmlToTextParser::parseTagBSTYLE);
	tagMap[L"script"] = tagParser(false, &CHtmlToTextParser::parseTagSCRIPT);
	tagMap[L"/script"] = tagParser(false, &CHtmlToTextParser::parseTagBSCRIPT);
	tagMap[L"pre"] = tagParser(false, &CHtmlToTextParser::parseTagPRE);
	tagMap[L"/pre"] = tagParser(false, &CHtmlToTextParser::parseTagBPRE);
	tagMap[L"p"] = tagParser(false, &CHtmlToTextParser::parseTagP);
	tagMap[L"/p"] = tagParser(false, &CHtmlToTextParser::parseTagBP);
	tagMap[L"a"] = tagParser(true, &CHtmlToTextParser::parseTagA);
	tagMap[L"/a"] = tagParser(false, &CHtmlToTextParser::parseTagBA);
	tagMap[L"br"] = tagParser(false, &CHtmlToTextParser::parseTagBR);
	tagMap[L"tr"] = tagParser(false, &CHtmlToTextParser::parseTagTR);
	tagMap[L"/tr"] = tagParser(false, &CHtmlToTextParser::parseTagBTR);
	tagMap[L"td"] = tagParser(false, &CHtmlToTextParser::parseTagTDTH);
	tagMap[L"th"] = tagParser(false, &CHtmlToTextParser::parseTagTDTH);
	tagMap[L"img"] = tagParser(true, &CHtmlToTextParser::parseTagIMG);
	tagMap[L"div"] = tagParser(false, &CHtmlToTextParser::parseTagNewLine);
	tagMap[L"/div"] = tagParser(false, &CHtmlToTextParser::parseTagNewLine);
	tagMap[L"hr"] = tagParser(false, &CHtmlToTextParser::parseTagHR);
	tagMap[L"h1"] = tagParser(false, &CHtmlToTextParser::parseTagHeading);
	tagMap[L"h2"] = tagParser(false, &CHtmlToTextParser::parseTagHeading);
	tagMap[L"h3"] = tagParser(false, &CHtmlToTextParser::parseTagHeading);
	tagMap[L"h4"] = tagParser(false, &CHtmlToTextParser::parseTagHeading);
	tagMap[L"h5"] = tagParser(false, &CHtmlToTextParser::parseTagHeading);
	tagMap[L"h6"] = tagParser(false, &CHtmlToTextParser::parseTagHeading);

	tagMap[L"ol"] = tagParser(false, &CHtmlToTextParser::parseTagOL);
	tagMap[L"/ol"] = tagParser(false, &CHtmlToTextParser::parseTagPopList);
	tagMap[L"ul"] = tagParser(false, &CHtmlToTextParser::parseTagUL);
	tagMap[L"/ul"] = tagParser(false, &CHtmlToTextParser::parseTagPopList);
	tagMap[L"li"] = tagParser(false, &CHtmlToTextParser::parseTagLI);
	
	tagMap[L"/dl"] = tagParser(false, &CHtmlToTextParser::parseTagPopList);
	tagMap[L"dt"] = tagParser(false, &CHtmlToTextParser::parseTagDT);
	tagMap[L"dd"] = tagParser(false, &CHtmlToTextParser::parseTagDD);
	tagMap[L"dl"] = tagParser(false, &CHtmlToTextParser::parseTagDL);
	
	// @todo check span
}

CHtmlToTextParser::~CHtmlToTextParser(void)
{
	
}

void CHtmlToTextParser::Init()
{
	fScriptMode = false;
	fHeadMode = false;
	cNewlines = 0;
	fStyleMode = false;
	fTDTHMode = false;
	fPreMode = false;
	fTextMode = false;
	fAddSpace = false;

	strText.clear();
}

bool CHtmlToTextParser::Parse(const WCHAR *lpwHTML)
{
	Init();

	while(*lpwHTML != 0)
	{
		if((*lpwHTML == '\n' || *lpwHTML == '\r' || *lpwHTML == '\t') && !fPreMode) {// ignore tabs and newlines
			if(fTextMode && !fTDTHMode && !fScriptMode && !fHeadMode && !fStyleMode && (*lpwHTML == '\n' || *lpwHTML == '\r'))
				fAddSpace = true;
			else
				fAddSpace = false;

			lpwHTML++;
		} else if(*lpwHTML == '<' && *lpwHTML+1 != ' ') { // The next char can not be a space!
			lpwHTML++;
			parseTag(lpwHTML);
		} else if(*lpwHTML == ' ' && !fPreMode) {
			fTextMode = true;
			addSpace(false);
			lpwHTML++;
		} else {
			if (fTextMode && fAddSpace) {
				addSpace(false);
			}

			fAddSpace = false;
			fTextMode = true;

			// if (skippable and not parsed)
			if (!(fScriptMode || fHeadMode || fStyleMode)) {
				if (parseEntity(lpwHTML))
					continue;
				addChar(*lpwHTML);
			}
			lpwHTML++;
		}
	}

	return true;
}

std::wstring& CHtmlToTextParser::GetText() {
	return strText;
}

void CHtmlToTextParser::addNewLine(bool forceLine) {
	if (strText.empty())
		return;

	if (forceLine || cNewlines == 0)
		strText += L"\r\n";

	cNewlines++;
}

void CHtmlToTextParser::addChar(WCHAR c) {
	if (fScriptMode || fHeadMode || fStyleMode)
		return;

	strText.push_back(c);
	cNewlines = 0;
	fTDTHMode = false;
}

void CHtmlToTextParser::addSpace(bool force) {
	
	if(force || (!strText.empty() && *strText.rbegin() != ' ') )
		addChar(' ');
}

/**
 * @todo validate the entity!!
 */
bool CHtmlToTextParser::parseEntity(const WCHAR* &lpwHTML)
{
	std::wstring entity;

	if(*lpwHTML != '&')
		return false;

	lpwHTML++;

	if (*lpwHTML == '#') {
		int base = 10;

		lpwHTML++;
		if (*lpwHTML == 'x') {
			lpwHTML++;
			base = 16;
		}

		for (int i =0; isxdigit(*lpwHTML) && *lpwHTML != ';' && i < 10; i++) {
			entity += *lpwHTML;
			lpwHTML++;
		}

		strText.push_back(wcstoul(entity.c_str(), NULL, base));
	} else {
		for(int i =0; *lpwHTML != ';' && *lpwHTML != 0 && i < 10; i++) {
			entity += *lpwHTML;
			lpwHTML++;
		}

		WCHAR code = CHtmlEntity::toChar(entity.c_str());
		if (code > 0)
			strText.push_back( code );
	}

	if(*lpwHTML == ';')
		lpwHTML++;

	return true;
}

void CHtmlToTextParser::parseTag(const WCHAR* &lpwHTML)
{
	bool bTagName = true;
	bool bTagEnd = false;
	bool bParseAttrs = false;
	MapParser::iterator iterTag;

	std::wstring tagName;

	while (*lpwHTML != 0 && !bTagEnd) 
	{
		if (bTagName && (*lpwHTML == '!' || *lpwHTML == '-')) {
			
			// HTML comment or doctype detect, ignore all the text
			bool fCommentMode = false;
			lpwHTML++;

			if (*lpwHTML == '-')
				fCommentMode = true;

			while (*lpwHTML != 0) {

				if(fCommentMode && *lpwHTML == '>' && *lpwHTML-1 == '-' && *lpwHTML-2 == '-' ) {
					lpwHTML++;
					return;
				} else if (*lpwHTML == '>') {
					lpwHTML++;
					return;
				}

				lpwHTML++;
			}
		} else if (*lpwHTML == '>') {
			if(!bTagEnd){
				iterTag = tagMap.find(tagName);
				bTagEnd = true;
				bTagName = false;
			}
		} else if (*lpwHTML == '<') {
			return; // Possible broken HTML, ignore data before
		} else {
			if (bTagName) {
				if (*lpwHTML == ' ') {
					bTagName = false;
					iterTag = tagMap.find(tagName);
					if(iterTag != tagMap.end())
						bParseAttrs = iterTag->second.bParseAttrs;
				}else {
					tagName.push_back(towlower(*lpwHTML));
				}
			} else if(bParseAttrs) {
				parseAttributes(lpwHTML);
				break;
			}
		}

		lpwHTML++;
	}

	// Parse tag
	if (!bTagName && iterTag != tagMap.end()) {
		(this->*iterTag->second.parserMethod)();
		fTextMode = false;
	}
}

void CHtmlToTextParser::parseAttributes(const WCHAR* &lpwHTML)
{
	std::wstring attrName;
	std::wstring attrValue;
	bool bAttrName = true;
	bool bAttrValue = false;
	bool bEndTag = false;
	MapAttrs mapAttrs;

	WCHAR firstQuote = 0;

	while(*lpwHTML != 0 && !bEndTag) {
		if(*lpwHTML == '>' && bAttrValue) {
				bAttrValue = false;
				bEndTag = true;
		} else if(*lpwHTML == '>' && bAttrName) {
			lpwHTML++;
			break; // No attributes or broken attribute detect
		} else if(*lpwHTML == '=' && bAttrName) {
			bAttrName = false;
			bAttrValue = true;
		} else if(*lpwHTML == ' ' && bAttrValue && firstQuote == 0) {

			if (!attrValue.empty())
				bAttrValue = false;
			// ignore space
		} else if (bAttrValue) {
			if(*lpwHTML == '\'' || *lpwHTML == '\"') {
				if (firstQuote == 0) {
					firstQuote = *lpwHTML++;
					continue; // Don't add the quote!
				} else {
					if(firstQuote == *lpwHTML) {
						bAttrValue = false;
					}
				}
			}

			if(bAttrValue)
				attrValue.push_back(*lpwHTML);
		} else {
			if (bAttrName) {
				attrName.push_back(towlower(*lpwHTML));
			}
		}

		if(!bAttrName && !bAttrValue) {
			mapAttrs[attrName] = attrValue;

			firstQuote = 0;
			bAttrName = true;
			bAttrValue = false;
			attrValue.clear();
			attrName.clear();
		}

		lpwHTML++;
	}

	stackAttrs.push(mapAttrs);

	return;
}

void CHtmlToTextParser::parseTagP()
{
	if (cNewlines < 2 && !fTDTHMode) {
		addNewLine( false );
		addNewLine( true );
	}
}

void CHtmlToTextParser::parseTagBP() {
	addNewLine( false );
	addNewLine( true );
}


void CHtmlToTextParser::parseTagBR()
{
	addNewLine( true );
}

void CHtmlToTextParser::parseTagTR()
{
	_TableRow t;
	t.bFirstCol = true;

	addNewLine( false );
	stackTableRow.push(t);
}

void CHtmlToTextParser::parseTagBTR()
{
	if(!stackTableRow.empty())
		stackTableRow.pop();
}

void CHtmlToTextParser::parseTagTDTH()
{
	if (!stackTableRow.empty() && stackTableRow.top().bFirstCol == true)
		 stackTableRow.top().bFirstCol = false;
	else
		addChar('\t');

	fTDTHMode = true;
}

void CHtmlToTextParser::parseTagIMG()
{
	if (addURLAttribute(L"src", true)) {
		cNewlines = 0;
		fTDTHMode = false;
	}

	if (!stackAttrs.empty())
		stackAttrs.pop();
}

void CHtmlToTextParser::parseTagA() {
	// nothing todo, only because we want to parse the tag A attributes
}

void CHtmlToTextParser::parseTagBA()
{
	if (addURLAttribute(L"href")) {
		cNewlines = 0;
		fTDTHMode = false;
	}

	if(!stackAttrs.empty())
		stackAttrs.pop();

}

bool CHtmlToTextParser::addURLAttribute(const WCHAR *lpattr, bool bSpaces) {

	MapAttrs::iterator	iter;

	if (stackAttrs.empty())
		return false;

	iter = stackAttrs.top().find(lpattr);
	if (iter != stackAttrs.top().end()) {
		if(wcsncasecmp(iter->second.c_str(), L"http:", 5) == 0 ||
			wcsncasecmp(iter->second.c_str(), L"ftp:", 4) == 0 ||
			wcsncasecmp(iter->second.c_str(), L"mailto:", 7) == 0)
		{
			addSpace(false);

			strText.append(L"<");
			strText.append(iter->second);
			strText.append(L">");

			addSpace(false);
			return true;
		}
	}

	return false;
}

void CHtmlToTextParser::parseTagSCRIPT() {
	fScriptMode = true;
}

void CHtmlToTextParser::parseTagBSCRIPT() {
	fScriptMode = false;
}

void CHtmlToTextParser::parseTagSTYLE() {
	fStyleMode = true;
}

void CHtmlToTextParser::parseTagBSTYLE() {
	fStyleMode = false;
}

void CHtmlToTextParser::parseTagHEAD() {
	fHeadMode = true;
}

void CHtmlToTextParser::parseTagBHEAD() {
	fHeadMode = false;
}

void CHtmlToTextParser::parseTagNewLine() {
	addNewLine( false );
}

void CHtmlToTextParser::parseTagHR() {
	strText.append(L"--------------------------------");
	addNewLine( true );
}
void CHtmlToTextParser::parseTagHeading() {
	addNewLine( false );
	addNewLine( true );
}

void CHtmlToTextParser::parseTagPopList() {
	if (!listInfoStack.empty())
		listInfoStack.pop();
	addNewLine( false );
}

void CHtmlToTextParser::parseTagOL() {
	listInfo.mode = lmOrdered;
	listInfo.count = 1;
	listInfoStack.push(listInfo);
}

void CHtmlToTextParser::parseTagUL() {
	listInfo.mode = lmUnordered;
	listInfo.count = 1;
	listInfoStack.push(listInfo);
}

std::wstring inttostring(unsigned int x) {
	WCHAR buf[33];
	swprintf(buf, 33, L"%u", x);
	return buf;
}

void CHtmlToTextParser::parseTagLI() {
	addNewLine( false );

	if (!listInfoStack.empty()) {
		for (size_t i = 0; i < listInfoStack.size() - 1; ++i)
			strText.append(L"\t");

		if (listInfoStack.top().mode == lmOrdered) {
			strText += inttostring(listInfoStack.top().count++) + L".";
		} else 
			strText.append(L"*");
		
		strText.append(L"\t");
		cNewlines = 0;
		fTDTHMode = false;
	}
}

void CHtmlToTextParser::parseTagDT() {
	addNewLine( false );

	if (!listInfoStack.empty()) {
		for (size_t i = 0; i < listInfoStack.size() - 1; ++i)
			strText.append(L"\t");
	}
}

void CHtmlToTextParser::parseTagDD() {
	addNewLine( false );

	if (!listInfoStack.empty()) {
		for (size_t i = 0; i < listInfoStack.size(); ++i)
			strText.append(L"\t");
	}
}

void CHtmlToTextParser::parseTagDL() {
	listInfo.mode = lmDefinition;
	listInfo.count = 1;
	listInfoStack.push(listInfo);
}

void CHtmlToTextParser::parseTagPRE() {
	fPreMode = true;
    addNewLine( false );
    addNewLine( true );
}

void CHtmlToTextParser::parseTagBPRE() {
	fPreMode = false;
	addNewLine( false );
	addNewLine( true );
}

