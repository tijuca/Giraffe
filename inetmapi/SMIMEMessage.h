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

#ifndef SMIMEMESSAGE_H
#define SMIMEMESSAGE_H

#include <vmime/message.hpp>
#include <vmime/utility/stream.hpp>

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
 * use that body (including some headers!) to generate the RFC822 message. All other methods are inherited
 * directly from vmime::message.
 *
 * Note that any other body data set will be override by the SMIMEBody.
 *
 */
class SMIMEMessage : public vmime::message {
public:
    SMIMEMessage();

	void generate(vmime::utility::outputStream& os, const std::string::size_type maxLineLength = vmime::options::getInstance()->message.maxLineLength(), const std::string::size_type curLinePos = 0, std::string::size_type* newLinePos = NULL) const;

    void setSMIMEBody(std::string &body);    
    
private:
    std::string m_body;
};

#endif
