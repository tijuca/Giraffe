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
#include "stringutil.h"
#include "charset/convert.h"
#include <string>
#include "ECIConv.h"
#include "ECLogger.h"

#include <mapicode.h> // only for MAPI error codes
#include <mapidefs.h> // only for MAPI error codes

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define BLOCKSIZE	65536

/** 
 * Reads the contents of a file, and writes it to the output file
 * while converting Unix \n enters to DOS \r\n enters.
 * 
 * @todo this function prints errors to screen using perror(), which should be removed
 * @todo this function doesn't return the filepointer in case of an error, but also doesn't unlink the tempfile either.
 *
 * @param[in] fin input file to read strings from
 * @param[out] fout new filepointer to a temporary file
 * 
 * @return MAPI error code
 */
HRESULT HrFileLFtoCRLF(FILE *fin, FILE** fout)
{
	HRESULT hr = hrSuccess;
	char	bufferin[BLOCKSIZE / 2];
	char	bufferout[BLOCKSIZE+1];
	size_t	sizebufferout, readsize;
	FILE*	fTmp = NULL;

	if(fin == NULL || fout == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	fTmp = tmpfile();
	if(fTmp == NULL) {
		perror("Unable to create tmp file");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	while (!feof(fin)) {
		readsize = fread(bufferin, sizeof(char), BLOCKSIZE / 2, fin);
		if (ferror(fin)) {
			perror("Read error");//FIXME: What an error?, what now?
			hr = MAPI_E_CORRUPT_DATA;
			break;
		}

		BufferLFtoCRLF(readsize, bufferin, bufferout, &sizebufferout);

		if (fwrite(bufferout, sizeof(char), sizebufferout, fTmp) != sizebufferout) {
			perror("Write error");//FIXME: What an error?, what now?
			hr = MAPI_E_CORRUPT_DATA;
			break;
		}
	}

	*fout = fTmp;

exit:
	return hr;
}

/** 
 * align to page boundary (4k)
 * 
 * @param size add "padding" to size to make sure it's a multiple 4096 bytes
 * 
 * @return aligned size
 */
static inline int mmapsize(unsigned int size)
{
	return (((size + 1) >> 12) + 1) << 12;
}

/** 
 * Load a file, first by trying to use mmap. If that fails, the whole
 * file is loaded in memory.
 * 
 * @param[in] f file to read
 * @param[out] lppBuffer buffer containing the file contents
 * @param[out] lpSize length of the buffer
 * @param[out] lpImmap boolean denoting if the buffer is mapped or not (used when freeing the buffer)
 * 
 * @return MAPI error code
 */
HRESULT HrMapFileToBuffer(FILE *f, char **lppBuffer, int *lpSize, bool *lpImmap)
{
	HRESULT hr = hrSuccess;
	char *lpBuffer = NULL;
	int offset = 0;
	long ulBufferSize = BLOCKSIZE;
	long ulReadsize;
	struct stat stat;
	int fd = fileno(f);

	*lpImmap = false;

	/* Try mmap first */
	if (fstat(fd, &stat) != 0) {
		perror("Stat failed");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	/* auto-zero-terminate because mmap zeroes bytes after the file */
	lpBuffer = (char *)mmap(0, mmapsize(stat.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
	if (lpBuffer != MAP_FAILED) {
		*lpImmap = true;
		*lppBuffer = lpBuffer;
		*lpSize = stat.st_size;
		goto exit;
	}

	/* mmap failed (probably reading from STDIN as a stream), just read the file into memory, and return that */
	lpBuffer = (char*)malloc(BLOCKSIZE); // will be deleted as soon as possible
	while (!feof(f)) {
		ulReadsize = fread(lpBuffer+offset, sizeof(char), BLOCKSIZE, f);
		if (ferror(f)) {
			perror("Read error");
			break;
		}

		offset += ulReadsize;
		if (offset + BLOCKSIZE > ulBufferSize) {    // Next read could cross buffer boundary, realloc
			char *lpRealloc = (char*)realloc(lpBuffer, offset + BLOCKSIZE);
			if (lpRealloc == NULL) {
				hr = MAPI_E_NOT_ENOUGH_MEMORY;
				goto exit;
			}
			lpBuffer = lpRealloc;
			ulBufferSize += BLOCKSIZE;
		}
	}

	/* Nothing was read */
    if (offset == 0) {
		*lppBuffer = NULL;
		*lpSize = 0;
	} else {
		/* Add terminate character */
		lpBuffer[offset] = 0;

		*lppBuffer = lpBuffer;
		*lpSize = offset;
	}

exit:
	return hr;
}

/** 
 * Free a buffer from HrMapFileToBuffer
 * 
 * @param[in] lpBuffer buffer to free
 * @param[in] ulSize size of the buffer
 * @param[in] bImmap marker if the buffer is mapped or not
 */
HRESULT HrUnmapFileBuffer(char *lpBuffer, int ulSize, bool bImmap)
{
	if (bImmap)
		munmap(lpBuffer, mmapsize(ulSize));
	else
		free(lpBuffer);

	return hrSuccess;
}

/** 
 * Reads a file into an std::string using file mapping if possible.
 *
 * @todo doesn't the std::string undermine the whole idea of mapping?
 * @todo std::string has a length method, so what's with the lpSize parameter?
 * 
 * @param[in] f file to read in memory
 * @param[out] lpstrBuffer string containing the file contents, optionally returned (why?)
 * @param[out] lpSize size of the buffer, optionally returned
 * 
 * @return 
 */
HRESULT HrMapFileToString(FILE *f, std::string *lpstrBuffer, int *lpSize)
{
	HRESULT hr = hrSuccess;
	char *lpBuffer = NULL;
	int ulBufferSize = 0;
	bool immap = false;

	hr = HrMapFileToBuffer(f, &lpBuffer, &ulBufferSize, &immap); // what if message was half read?
	if (hr != hrSuccess || !lpBuffer)
		goto exit;

	if (lpstrBuffer)
		*lpstrBuffer = std::string(lpBuffer, ulBufferSize);
	if (lpSize)
		*lpSize = ulBufferSize;

exit:
	if (lpBuffer)
		HrUnmapFileBuffer(lpBuffer, ulBufferSize, immap);

	return hr;
}

/**
 * Duplicate a file, to a given location
 *
 * @param[in]	lpLogger Pointer to logger, if NULL the errors are sent to stderr
 * @param[in]	lpFile Pointer to the source file
 * @param[in]	strFileName	The new file name
 *
 * @return The result of the duplication of the file
 *
 * @todo on error delete file?
 */
bool DuplicateFile(ECLogger *lpLogger, FILE *lpFile, std::string &strFileName) 
{
	bool bResult = true;
	size_t	ulReadsize = 0;
	FILE *pfNew = NULL;
	char *lpBuffer = NULL;

	// create new file
	pfNew = fopen(strFileName.c_str(), "wb");
	if(pfNew == NULL) {
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to create file, error %d", errno);
		else
			perror("Unable to create file");

		bResult = false;
		goto exit;
	}

	// Set file pointer at the begin.
	rewind(lpFile);

	lpBuffer = (char*)malloc(BLOCKSIZE); 
	if (!lpBuffer) {
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_FATAL, "Duplicate file is out of memory");

		bResult = false;
		goto exit;
	}

	while (!feof(lpFile)) {
		ulReadsize = fread(lpBuffer, sizeof(char), BLOCKSIZE, lpFile);
		if (ferror(lpFile)) {
			if (lpLogger)
				lpLogger->Log(EC_LOGLEVEL_FATAL, "Read error, error %d", errno);
			else
				perror("Read error");

			bResult = false;
			goto exit;
		}
		

		if (fwrite(lpBuffer, sizeof(char), ulReadsize , pfNew) != ulReadsize) {
			if (lpLogger)
				lpLogger->Log(EC_LOGLEVEL_FATAL, "Write error, error %d", errno);
			else
				perror("Write error");

			bResult = false;
			goto exit;
		}
	}

exit:
	if (lpBuffer)
		free(lpBuffer);

	if (pfNew)
		fclose(pfNew);

	return bResult;
}

/**
 * Convert file from UCS2 to UTF8
 *
 * @param[in] lpLogger Pointer to a log object
 * @param[in] strSrcFileName Source filename
 * @param[in] strDstFileName Destination filename
 */
bool ConvertFileFromUCS2ToUTF8(ECLogger *lpLogger, const std::string &strSrcFileName, const std::string &strDstFileName)
{
	bool bResult = false;
	int ulBufferSize = 0;
	FILE *pfSrc = NULL;
	FILE *pfDst = NULL;
	char *lpBuffer = NULL;
	bool bImmap = false;
	std::string strConverted;

	pfSrc = fopen(strSrcFileName.c_str(), "rb");
	if(pfSrc == NULL) {
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open file '%s', error %d", strSrcFileName.c_str(), errno);
		else
			perror("Unable to open file");

		goto exit;
	}

	// create new file
	pfDst = fopen(strDstFileName.c_str(), "wb");
	if(pfDst == NULL) {
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to create file '%s', error %d", strDstFileName.c_str(), errno);
		else
			perror("Unable to create file");

		goto exit;
	}

	if(HrMapFileToBuffer(pfSrc, &lpBuffer, &ulBufferSize, &bImmap))
		goto exit;

	try {
		strConverted = convert_to<std::string>("UTF-8", lpBuffer, ulBufferSize, "UCS-2//IGNORE");
	} catch (const std::runtime_error &) {
		goto exit;
	}
	
	if (fwrite(strConverted.c_str(), sizeof(char), strConverted.size(), pfDst) != strConverted.size()) { 
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to write to file '%s', error %d", strDstFileName.c_str(), errno);
		else
			perror("Write error");

		goto exit;
	}

	bResult = true;

exit:
	if (lpBuffer)
		HrUnmapFileBuffer(lpBuffer, ulBufferSize, bImmap);
	
	if (pfSrc)
		fclose(pfSrc);

	if (pfDst)
		fclose(pfDst);

	return bResult;
}
