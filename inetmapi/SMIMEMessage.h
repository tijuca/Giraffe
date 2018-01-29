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

#ifndef SMIMEMESSAGE_H
#define SMIMEMESSAGE_H

#include <kopano/zcdefs.h>
#include <string>
#include <vmime/message.hpp>
#include <vmime/utility/stream.hpp>
#include <vmime/generationContext.hpp>

namespace KC {

/**
 * We are adding a bit of functionality to vmime::message here for S/MIME support.
 *
 * MAPI provides us with the actual body of a signed S/MIME message that looks like
 *
 * -----------------------
 * Content-Type: xxxx
 *
 * data
 * data
 * data
 * ...
 * -----------------------
 *
 * This class works just like a vmime::message instance, except that when then 'SMIMEBody' is set, it will
 * use that body (including some headers!) to generate the RFC 2822 message. All other methods are inherited
 * directly from vmime::message.
 *
 * Note that any other body data set will be override by the SMIMEBody.
 *
 */
class SMIMEMessage _kc_final : public vmime::message {
public:
	void generateImpl(const vmime::generationContext &, vmime::utility::outputStream &, size_t curLinePos = 0, size_t *newLinePos = NULL) const _kc_override;
	void setSMIMEBody(const char *body) { m_body = body; }
private:
    std::string m_body;
};

} /* namespace */

#endif
