/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#pragma once

#include "zcdefs.h"
#include <string>
#include <map>
#include <stack>

class CHtmlToTextParser _final
{
public:
	CHtmlToTextParser(void);
	~CHtmlToTextParser(void);

	bool Parse(const WCHAR *lpwHTML);
	std::wstring& GetText();

protected:
	void Init();

	void parseTag(const WCHAR* &lpwHTML);
	bool parseEntity(const WCHAR* &lpwHTML);
	void parseAttributes(const WCHAR* &lpwHTML);

	void addChar(WCHAR c);
	void addNewLine(bool forceLine);
	bool addURLAttribute(const WCHAR *lpattr, bool bSpaces = false);
	void addSpace(bool force);

	//Parse tags
	void parseTagP();
	void parseTagBP();
	void parseTagBR();
	void parseTagTR();
	void parseTagBTR();
	void parseTagTDTH();
	void parseTagIMG();
	void parseTagA();
	void parseTagBA();
	void parseTagSCRIPT();
	void parseTagBSCRIPT();
	void parseTagSTYLE();
	void parseTagBSTYLE();
	void parseTagHEAD();
	void parseTagBHEAD();
	void parseTagNewLine();
	void parseTagHR();
	void parseTagHeading();
	void parseTagPRE();
	void parseTagBPRE();
	void parseTagOL();
	void parseTagUL();
	void parseTagLI();
	void parseTagPopList();
	void parseTagDL();
	void parseTagDT();
	void parseTagDD();

	std::wstring strText;
	bool fScriptMode;
	bool fHeadMode;
	short cNewlines;
	bool fStyleMode;
	bool fTDTHMode;
	bool fPreMode;
	bool fTextMode;
	bool fAddSpace; 

	typedef void ( CHtmlToTextParser::*ParseMethodType )( void );

	struct tagParser {
		tagParser(){};
		tagParser(bool bParseAttrs, ParseMethodType parserMethod){
			this->bParseAttrs = bParseAttrs;
			this->parserMethod = parserMethod;
		};
		bool bParseAttrs;
		ParseMethodType parserMethod;
	};

	struct _TableRow {
		bool bFirstCol;
	};

	enum eListMode { lmDefinition, lmOrdered, lmUnordered };
	struct ListInfo {
		eListMode mode;
		unsigned count;
	};

	typedef std::map<std::wstring, tagParser>		MapParser;
	typedef std::map<std::wstring, std::wstring>	MapAttrs;

	typedef std::stack<MapAttrs>	StackMapAttrs;
	typedef std::stack<_TableRow>	StackTableRow;
	typedef std::stack<ListInfo>	ListInfoStack;
	
	StackTableRow	stackTableRow;
	MapParser		tagMap;
	StackMapAttrs	stackAttrs;
	ListInfo 		listInfo;
	ListInfoStack	listInfoStack;
};
