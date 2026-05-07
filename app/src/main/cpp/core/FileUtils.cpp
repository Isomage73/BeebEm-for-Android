/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2023 Chris Needham

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; if not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

#include "Windows.h"

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "FileUtils.h"
#include "StringUtils.h"

/****************************************************************************/

#ifdef WIN32
const char DIR_SEPARATOR = '\\';
#else
const char DIR_SEPARATOR = '/';
#endif

/****************************************************************************/

bool FileExists(const char* PathName)
{
	#ifdef WIN32

	DWORD dwAttrib = GetFileAttributes(PathName);

	return dwAttrib != INVALID_FILE_ATTRIBUTES &&
	       !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);

	#else

	struct stat st;

	if (stat(PathName, &st) == -1)
	{
		return false;
	}

	return S_ISREG(st.st_mode);

	#endif
}

/****************************************************************************/

bool FolderExists(const char* PathName)
{
	#ifdef WIN32

	DWORD dwAttrib = GetFileAttributes(PathName);

	return dwAttrib != INVALID_FILE_ATTRIBUTES &&
	       (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) != 0;

	#else

	struct stat st;

	if (stat(PathName, &st) == -1)
	{
		return false;
	}

	return S_ISDIR(st.st_mode);

	#endif
}

/****************************************************************************/

std::string AppendPath(const std::string& BasePath, const std::string& Path)
{
	std::string PathName(BasePath);

	if (!PathName.empty())
	{
		char LastChar = PathName[PathName.size() - 1];

		if (LastChar != '\\' && LastChar != '/')
		{
			PathName.append(1, DIR_SEPARATOR);
		}
	}

	PathName += Path;

	return PathName;
}

/****************************************************************************/

bool HasFileExt(const char* FileName, const char* Ext)
{
	const size_t ExtLen = strlen(Ext);
	const size_t FileNameLen = strlen(FileName);

	return FileNameLen >= ExtLen &&
	       StrCaseCmp(FileName + FileNameLen - ExtLen, Ext) == 0;
}

/****************************************************************************/

std::string ReplaceFileExt(const std::string& FileName, const char* Ext)
{
	size_t index = FileName.find_last_of(".");

	if (index == std::string::npos)
	{
		return FileName;
	}

	std::string NewFileName(FileName, 0, index);

	NewFileName += Ext;

	return NewFileName;
}

/****************************************************************************/

void GetPathFromFileName(const char* FileName, char* Path, size_t Size)
{
	Path[0] = '\0';

	const char* Pos = strrchr(FileName, DIR_SEPARATOR);

	if (Pos != nullptr)
	{
		size_t PathLength = Pos - FileName;

		if (PathLength < Size)
		{
			strncpy(Path, FileName, PathLength);
			Path[PathLength] = '\0';
		}
	}
}

/****************************************************************************/

const char* GetFileNameFromPath(const char* PathName)
{
	const char* FileName = strrchr(PathName, '\\');

	if (FileName == nullptr)
	{
		FileName = strrchr(PathName, '/');
	}

	if (FileName == nullptr)
	{
		FileName = PathName;
	}
	else
	{
		FileName++;
	}

	return FileName;
}

/****************************************************************************/

void MakeFileName(char* Path, size_t /* Size */, const char* DirName, const char* FileName, ...)
{
	va_list args;
	va_start(args, FileName);

	strcpy(Path, DirName);

	size_t Len = strlen(Path);

	if (Path[Len] != DIR_SEPARATOR)
	{
		Path[Len++] = DIR_SEPARATOR;
	}

	vsprintf(&Path[Len], FileName, args);

	va_end(args);
}

/****************************************************************************/

void MakePreferredPath(char* PathName)
{
	for (size_t i = 0; i < strlen(PathName); ++i)
	{
		if (PathName[i] == '\\' || PathName[i] == '/')
		{
			PathName[i] = DIR_SEPARATOR;
		}
	}
}

/****************************************************************************/

void AppendPath(char* pszPath, const char* pszPathToAppend)
{
	size_t Len = strlen(pszPath);

	if (Len > 0)
	{
		if (pszPath[Len - 1] != DIR_SEPARATOR)
		{
			pszPath[Len] = DIR_SEPARATOR;
			pszPath[++Len] = '\0';
		}
	}

	if (Len > 0 && pszPathToAppend[0] == DIR_SEPARATOR)
	{
		strcpy(&pszPath[Len], pszPathToAppend + 1);
	}
	else
	{
		strcpy(&pszPath[Len], pszPathToAppend);
	}

	MakePreferredPath(pszPath);
}

/****************************************************************************/

bool IsRelativePath(const char* pszPath)
{
	#ifdef WIN32

	return !!PathIsRelative(pszPath);

	#else

	return pszPath[0] != '/';

	#endif
}

/****************************************************************************/

bool CopyFile(const char *pszSourcePath, const char *pszDestPath)
{
	FILE *src_f, *dst_f;
	int c, failed=0;

	if (pszSourcePath == NULL)
	{
		return false;
	}

	if (pszDestPath == NULL)
	{
		return false;
	}

	// Open the source file for reading, quit on fail.
	if ((src_f = fopen(pszSourcePath, "r")) == NULL)
	{
		return false;
	}

	// Open and truncate the dest file for writing,
	// close source file and quit on fail.
	if ((dst_f = fopen(pszDestPath, "w")) == NULL)
	{
		fclose(src_f);
		return FALSE;
	}

	// Copy data from the source file to the dest file.
	// (this is not fast!)
	while ((c = fgetc(src_f)) != EOF)
	{
		if (fputc(c, dst_f) == EOF)
		{
			failed = 1;
			break;
		}
	}

	// Close files
	fclose(dst_f);
	fclose(src_f);

	return !failed;
}

/****************************************************************************/

std::string GetSystemError()
{
	char str[256];

	#ifdef WIN32

	strerror_s(str, 256, errno);

	#else

	strerror_r(errno, str, 256);

	#endif

	return str;
}

/****************************************************************************/
