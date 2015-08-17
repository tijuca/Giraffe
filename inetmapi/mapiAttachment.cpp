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

#include "mapiAttachment.h"

#include <vmime/contentDispositionField.hpp>
#include <vmime/contentTypeField.hpp>
#include <vmime/messageId.hpp>

mapiAttachment::mapiAttachment(vmime::ref <const vmime::contentHandler> data, const vmime::encoding& enc, const vmime::mediaType& type, const std::string& contentid, const vmime::word filename, const vmime::text& desc, const vmime::word& name) : defaultAttachment(data, enc, type, desc, name)
{
	m_filename = filename;
	m_contentid = contentid;

	m_hasCharset = false;
}

void mapiAttachment::addCharset(vmime::charset ch) {
	m_hasCharset = true;
	m_charset = ch;
}

void mapiAttachment::generatePart(vmime::ref<vmime::bodyPart> part) const
{
	vmime::defaultAttachment::generatePart(part);


	part->getHeader()->ContentDisposition().dynamicCast <vmime::contentDispositionField>()->setFilename(m_filename);

	vmime::ref<vmime::contentTypeField> ctf = part->getHeader()->ContentType().dynamicCast <vmime::contentTypeField>();

	ctf->getParameter("name")->setValue(m_filename);
	if (m_hasCharset)
		ctf->setCharset(vmime::charset(m_charset));

	
	if (m_contentid != "") {
		// Field is created when accessed through ContentId, so don't create if we have no
		// content-id to set
		part->getHeader()->ContentId()->setValue(vmime::messageId("<" + m_contentid + ">"));
	}
}

