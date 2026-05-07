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

#include <algorithm>
#include <cctype>
#include <codecvt>
#include <locale>
#include <strings.h> // For strcasecmp

#include "Windows.h"

#include "StringUtils.h"

/****************************************************************************/

static void TrimLeft(std::string& str)
{
	auto pos = std::find_if(str.begin(), str.end(), [](int ch) {
		return !std::isspace(ch);
	});

	str.erase(str.begin(), pos);
}

/****************************************************************************/

static void TrimRight(std::string& str)
{
	auto pos = std::find_if(str.rbegin(), str.rend(), [](int ch) {
		return !std::isspace(ch);
	});

	str.erase(pos.base(), str.end());
}

/****************************************************************************/

void Trim(std::string& str)
{
	TrimLeft(str);
	TrimRight(str);
}

/****************************************************************************/

void ParseLine(const std::string& Line, std::vector<std::string>& Tokens)
{
	int i = 0;

	while (Line[i] != '\0')
	{
		std::string Token;

		while (Line[i] != '\0' && isspace(Line[i]))
		{
			i++;
		}

		while (Line[i] != '\0' && !isspace(Line[i]))
		{
			Token += Line[i++];
		}

		if (!Token.empty())
		{
			Tokens.push_back(Token);
		}
	}
}

/****************************************************************************/

const char* ParseToken(const char* Str, std::string& Token)
{
	Token.clear();

	while (*Str != '\0' && isspace(*Str))
	{
		Str++;
	}

	while (*Str != '\0' && !isspace(*Str))
	{
		Token += *Str;
		Str++;
	}

	while (*Str != '\0' && isspace(*Str))
	{
		Str++;
	}

	return Str;
}

/****************************************************************************/

bool ParseNumber(const std::string& str, int* pValue)
{
	try
	{
		std::size_t Pos = 0;

		*pValue = std::stoi(str, &Pos);

		if (Pos != str.size())
		{
			return false;
		}
	}
	catch (std::exception&)
	{
		return false;
	}

	return true;
}

/****************************************************************************/

int ParseNumber(const char* Name, const std::string& str, int Min, int Max)
{
	int Value = 0;

	if (!ParseNumber(str, &Value))
	{
		throw std::invalid_argument(Name);
	}

	if (Value < Min || Value > Max)
	{
		throw std::out_of_range(Name);
	}

	return Value;
}

/****************************************************************************/

bool ParseHexNumber(const std::string& str, unsigned long* pValue)
{
	try
	{
		std::size_t Pos = 0;

		*pValue = std::stoul(str, &Pos, 16);

		if (Pos != str.size())
		{
			return false;
		}
	}
	catch (std::exception&)
	{
		return false;
	}

	return true;
}

/****************************************************************************/

bool ParseHexNumber(const std::string& str, int* pValue)
{
	try
	{
		std::size_t Pos = 0;

		*pValue = std::stoi(str, &Pos, 16);

		if (Pos != str.size())
		{
			return false;
		}
	}
	catch (std::exception&)
	{
		return false;
	}

	return true;
}

/****************************************************************************/

char ToHexDigit(int Value)
{
	static const char HexDigit[16] =
	{
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
	};

	return HexDigit[Value];
}

/****************************************************************************/

std::string BytesToString(const unsigned char* pData, int Length)
{
	std::string str;

	for (int i = 0; i < Length; i++)
	{
		char sz[10];
		sprintf(sz, " %02X", pData[i]);

		str += sz;
	}

	return str;
}

/****************************************************************************/

bool StringEndsWith(const std::string& str, const std::string& suffix)
{
	return str.size() >= suffix.size() &&
		str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/****************************************************************************/

std::string WStr2Str(const std::wstring& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> Converter;

	return Converter.to_bytes(str);
}

/****************************************************************************/

std::wstring Str2WStr(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> Converter;

	return Converter.from_bytes(str);
}

/****************************************************************************/

int StrCaseCmp(const char *str1, const char *str2)
{
	#ifdef WIN32

	return _stricmp(str1, str2);

	#else

	return strcasecmp(str1, str2);

	#endif
}

/****************************************************************************/

// Returns a pointer to the end of the string.

char *StrCopy(char *pDest, const char *pSrc)
{
	while (*pSrc != '\0')
	{
		*pDest++ = *pSrc++;
	}

	*pDest = '\0';

	return pDest;
}

/****************************************************************************/

std::string GuidToString(const GUID& Guid)
{
	char str[100];

	sprintf(str, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	        Guid.Data1, Guid.Data2, Guid.Data3,
	        Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
	        Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);

	return str;
}

/****************************************************************************/
