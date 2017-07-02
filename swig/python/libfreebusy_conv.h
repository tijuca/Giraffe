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

#ifndef LIBFREEBUSY_CONV_H
#define LIBFREEBUSY_CONV_H

#include "freebusy.h"
#include <Python.h>

void InitFreebusy();
LPFBUser List_to_p_FBUser(PyObject *, ULONG *);
LPFBBlock_1 List_to_p_FBBlock_1(PyObject *, ULONG *);
PyObject* List_from_FBBlock_1(LPFBBlock_1, LONG *);
#endif
