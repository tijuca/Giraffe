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

#ifndef INPUT_STREAM_MAPI_ADAPTER_H
#define INPUT_STREAM_MAPI_ADAPTER_H

#include <kopano/zcdefs.h>
#include <mapidefs.h>
#include <sys/types.h>
#include <kopano/memory.hpp>
#include <vmime/utility/inputStream.hpp>
#include <vmime/utility/outputStream.hpp>

namespace KC {

class inputStreamMAPIAdapter _kc_final : public vmime::utility::inputStream {
public:
	inputStreamMAPIAdapter(IStream *lpStream);
	virtual size_t read(vmime::byte_t *, size_t) _kc_override;
	virtual size_t skip(size_t) _kc_override;
	virtual void reset(void) _kc_override;
	virtual bool eof(void) const _kc_override { return this->ateof; }

private:
	bool ateof = false;
	object_ptr<IStream> lpStream;
};

class outputStreamMAPIAdapter _kc_final : public vmime::utility::outputStream {
	public:
	outputStreamMAPIAdapter(IStream *);
	virtual void writeImpl(const unsigned char *, const size_t) _kc_override;
	virtual void flush(void) _kc_override;

	private:
	object_ptr<IStream> lpStream;
};

} /* namespace */

#endif
