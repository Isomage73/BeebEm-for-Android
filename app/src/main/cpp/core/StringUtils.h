/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2020  Chris Needham

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

#ifndef STRING_UTILS_HEADER
#define STRING_UTILS_HEADER

#include <string>
#include <vector>

void Trim(std::string& str);
void ParseLine(const std::string& Line, std::vector<std::string>& Tokens);
const char* ParseToken(const char* Str, std::string& Token);
bool ParseNumber(const std::string& str, int* pValue);
int ParseNumber(const char* Name, const std::string& str, int Min, int Max);
bool ParseHexNumber(const std::string& str, unsigned long* pValue);
bool ParseHexNumber(const std::string& str, int* pValue);
char ToHexDigit(int Value);
std::string BytesToString(const unsigned char* pData, int Length);
bool StringEndsWith(const std::string& str, const std::string& suffix);
std::string WStr2Str(const std::wstring& str);
std::wstring Str2WStr(const std::string& str);
int StrCaseCmp(const char *str1, const char *str2);
char *StrCopy(char *pDest, const char *pSrc);
std::string GuidToString(const GUID& Guid);

#endif
