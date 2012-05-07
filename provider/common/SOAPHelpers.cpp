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

#include "SOAPHelpers.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void *mime_file_read_open(struct soap *soap, void *handle, const char *id, const char *type, const char *description) 
{
	return handle;
}

void mime_file_read_close(struct soap *soap, void *handle) 
{
	if (handle)
		fclose((FILE*)handle);
}

size_t mime_file_read(struct soap *soap, void *handle, char *buf, size_t len) 
{
	return fread(buf, 1, len, (FILE*)handle); 
}

void *mime_file_write_open(struct soap *soap, void *handle, const char *id, const char *type, const char *description, enum soap_mime_encoding encoding)
{
	char *lpFilename = (char *)handle;

	if (!lpFilename) {
		soap->error = SOAP_EOF;
		soap->errnum = errno;

		return NULL;
	}

	FILE *fHandle = fopen(lpFilename, "wb");

	if (!fHandle) {
		soap->error = SOAP_EOF;
		soap->errnum = errno;
	}

	return (void*)fHandle;
}

void mime_file_write_close(struct soap *soap, void *handle)
{
	fclose((FILE*)handle);
}

int mime_file_write(struct soap *soap, void *handle, const char *buf, size_t len)
{ 
	size_t nwritten;
	while (len)
	{
		nwritten = fwrite(buf, 1, len, (FILE*)handle);
		if (!nwritten)
		{
			soap->errnum = errno;
			return SOAP_EOF;
		}
		len -= nwritten;
		buf += nwritten;
	}
	return SOAP_OK;
}