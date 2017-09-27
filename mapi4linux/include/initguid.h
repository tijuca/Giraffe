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

#ifndef __M4L_INITGUID_H_
#define __M4L_INITGUID_H_

/* Overwrite DEFINE_GUID to really create the guid data, not just declare. */
#include <kopano/platform.h>

#define INITGUID
#undef DEFINE_GUID
#define DEFINE_GUID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
	GUID_EXT _kc_export constexpr const GUID n = \
		{cpu_to_le32(l), cpu_to_le16(w1), cpu_to_le16(w2), \
		{b1, b2, b3, b4, b5, b6, b7, b8}}

#endif
