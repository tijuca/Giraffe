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
#include "ConsoleTable.h"
#include <iostream>
using namespace std;

/** 
 * Creates a static string table in set sizes
 * 
 * @param[in] rows number of rows in the table (if exceeding, table will be printed and cleared)
 * @param[in] columns exact number of columns
 */
ConsoleTable::ConsoleTable(size_t rows, size_t columns) : m_iRows(rows), m_iColumns(columns)
{
	m_nRow = 0;
	m_vTable.resize(rows);
	for (size_t i = 0; i < rows; i++) m_vTable[i].resize(columns);
	m_vMaxLengths.resize(m_iColumns);
	m_vHeader.resize(m_iColumns);
	bHaveHeader = false;
}

ConsoleTable::~ConsoleTable()
{
}

/**
 * Removes all contents from the current table
 */
void ConsoleTable::Clear()
{
	// remove all data by using resize to 0 and back to original size
	m_vTable.resize(0);
	m_vTable.resize(m_iRows);
	for (size_t i = 0; i < m_iRows; i++) m_vTable[i].resize(m_iColumns);
	m_vMaxLengths.clear();
	m_vHeader.clear();
	m_nRow = 0;
	bHaveHeader = false;
}

/** 
 * Removes all contents from the current table and makes the table size in the new given size
 * 
 * @param[in] rows guessed number of rows in the table
 * @param[in] columns exact number of columns
 */
void ConsoleTable::Resize(size_t rows, size_t columns)
{
	m_nRow = 0;
	bHaveHeader = false;

	m_iRows = rows;
	m_iColumns = columns;

	m_vTable.resize(rows);
	for (size_t i = 0; i < rows; i++) m_vTable[i].resize(columns);
	m_vMaxLengths.resize(m_iColumns);
	m_vHeader.resize(m_iColumns);
}

/** 
 * Sets the header name for a column. This is optional, as not all tables have headers.
 * 
 * @param[in] col column numer to set header name for
 * @param[in] entry name of the header
 * @retval		true on success, false if offsets are out of range
 */
bool ConsoleTable::SetHeader(size_t col, const string& entry)
{
	size_t len;

	if (col >= m_iColumns)
		return false;

	m_vHeader[col] = m_converter.convert_to<wstring>(CHARSET_WCHAR, entry, entry.length(), CHARSET_CHAR);
	len = entry.length();
	if (len > m_vMaxLengths[col])
		m_vMaxLengths[col] = len;

	bHaveHeader = true;

	return true;
}

/**
 * Adds entry at column in the table at the given column. Row
 * increments automatically after setting last column of table.
 *
 * @param[in]	col		column offset of the table, starting at 0
 * @param[in]	entry	utf-8 string to set in the table in current terminal charset
 * @retval		true on success, false if offsets are out of range
 */
bool ConsoleTable::AddColumn(size_t col, const string& entry)
{
	if (col >= m_iColumns)
		return false;

	if (m_nRow >= m_iRows) {
		PrintTable();
		Clear();
	}

	SetColumn(m_nRow, col, entry);

	return true;
}

/**
 * Sets entry in the table at the given offsets
 *
 * @param[in]	row		row offset of the table, starting at 0
 * @param[in]	col		column offset of the table, starting at 0
 * @param[in]	entry	utf-8 string to set in the table in current terminal charset
 * @retval		true on success, false if offsets are out of range
 */
bool ConsoleTable::SetColumn(size_t row, size_t col, const string& entry)
{
	size_t len;

	if (col >= m_iColumns || row >= m_iRows)
		return false;

	// we want to count number of printable characters, which is not possible using UTF-8
	m_vTable[row][col] = m_converter.convert_to<wstring>(CHARSET_WCHAR, entry, entry.length(), CHARSET_CHAR);
	len = m_vTable[row][col].length();
	if (len > m_vMaxLengths[col])
		m_vMaxLengths[col] = len;

	m_nRow = row;
	if (col+1 == m_iColumns)
		m_nRow++;

	return true;
}

/** 
 * Prints one row of a table
 * 
 * @param[in] vRow reference to the row wanted on the screen
 */
void ConsoleTable::PrintRow(const vector<wstring>& vRow)
{
	vector<wstring>::const_iterator iCol;
	size_t nCol;
	size_t longest, ntabs;

	cout << '\t';
	for (nCol = 0, iCol = vRow.begin(); iCol != vRow.end(); iCol++, nCol++) {
		// cout can't print wstring, and wcout is not allowed to mix with cout.
		printf("%ls\t", iCol->c_str());
		if (nCol+1 < m_iColumns) {
			longest = ((m_vMaxLengths[nCol] /8) +1);
			ntabs = longest - ((iCol->length() /8) +1);
			cout << string(ntabs, '\t');
		}
	}
	cout << endl;
}

/**
 * Prints the table on screen
 */
void ConsoleTable::PrintTable()
{
	size_t nRow, nCol;
	size_t total;

	if (bHaveHeader) {
		PrintRow(m_vHeader);
		total = 0;
		for (nCol = 0; nCol < m_iColumns; nCol++)
			total += m_vMaxLengths[nCol];
		total += (m_iColumns -1) * 8;
		cout << "\t" << string(total, '-') << endl;
	}

	for (nRow = 0; nRow < m_nRow; nRow++) {
		PrintRow(m_vTable[nRow]);
	}
}
