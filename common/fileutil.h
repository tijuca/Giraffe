/*
 * Copyright 2005 - 2016 Zarafa and its licensors
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

#ifndef _FILEUTIL_H
#define _FILEUTIL_H

#include <kopano/zcdefs.h>
#include <string>
#include <cstdio>

namespace KC {

class file_deleter {
	public:
	void operator()(FILE *f) { fclose(f); }
};

extern _kc_export HRESULT HrFileLFtoCRLF(FILE *fin, FILE **fout);
extern _kc_export HRESULT HrMapFileToString(FILE *f, std::string *buf, int *size = nullptr);
extern _kc_export bool DuplicateFile(FILE *, std::string &newname);

} /* namespace */

#endif
