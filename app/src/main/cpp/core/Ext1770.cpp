/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman

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

#include "Ext1770.h"
#include "BeebMem.h"
#include "Disc1770.h"
#include "DriveControlBlock.h"
#include "StringUtils.h"

#include "Hardware/Acorn1770/Acorn.h"
#include "Hardware/OpusDDOS/Opus.h"
#include "Hardware/Watford/Watford.h"

/*--------------------------------------------------------------------------*/

// FDC Board extension DLL variables
char FDCDLL[MAX_PATH] = { 0 };

#ifdef WIN32
static HMODULE hFDCBoard = nullptr;
#endif

static DriveControlBlock ExtBoard =
{
	0, // FDCAddress
	0, // DCAddress
	nullptr, // BoardName
	false // TR00_ActiveHigh
};

typedef void (*GetBoardPropertiesFunc)(struct DriveControlBlock *);
typedef unsigned char (*SetDriveControlFunc)(unsigned char);
typedef unsigned char (*GetDriveControlFunc)(unsigned char);

GetBoardPropertiesFunc PGetBoardProperties;
SetDriveControlFunc PSetDriveControl;
GetDriveControlFunc PGetDriveControl;

/*--------------------------------------------------------------------------*/

void Ext1770Reset()
{
	#ifdef WIN32

	if (hFDCBoard != nullptr)
	{
		FreeLibrary(hFDCBoard);
		hFDCBoard = nullptr;
	}

	#endif

	PGetBoardProperties = nullptr;
	PSetDriveControl = nullptr;
	PGetDriveControl = nullptr;
}

/*--------------------------------------------------------------------------*/

Ext1770Result Ext1770Init(const char *FileName)
{
	#ifdef WIN32

	char PathName[MAX_PATH];
	strcpy(PathName, FileName);
	GetDataPath(mainWin->GetAppPath(), PathName);

	hFDCBoard = LoadLibrary(PathName);

	if (hFDCBoard == nullptr)
	{
		return Ext1770Result::LoadFailed;
	}

	PGetBoardProperties = (GetBoardPropertiesFunc)GetProcAddress(hFDCBoard, "GetBoardProperties");
	PSetDriveControl = (SetDriveControlFunc)GetProcAddress(hFDCBoard, "SetDriveControl");
	PGetDriveControl = (GetDriveControlFunc)GetProcAddress(hFDCBoard, "GetDriveControl");

	if (PGetBoardProperties == nullptr ||
	    PSetDriveControl == nullptr ||
	    PGetDriveControl == nullptr)
	{
		Ext1770Reset();

		return Ext1770Result::InvalidDLL;
	}

	PGetBoardProperties(&ExtBoard);

	EFDCAddr = ExtBoard.FDCAddress;
	EDCAddr = ExtBoard.DCAddress;
	InvertTR00 = ExtBoard.TR00_ActiveHigh;

	return Ext1770Result::Success;

	#else

	if (StrCaseCmp(FileName, "Acorn") == 0)
	{
		PGetBoardProperties = Acorn1770GetBoardProperties;
		PSetDriveControl = Acorn1770SetDriveControl;
		PGetDriveControl = Acorn1770GetDriveControl;
	}
	else if (StrCaseCmp(FileName, "Opus") == 0)
	{
		PGetBoardProperties = OpusGetBoardProperties;
		PSetDriveControl = OpusSetDriveControl;
		PGetDriveControl = OpusGetDriveControl;
	}
	else if (StrCaseCmp(FileName, "Watford") == 0)
	{
		PGetBoardProperties = WatfordGetBoardProperties;
		PSetDriveControl = WatfordSetDriveControl;
		PGetDriveControl = WatfordGetDriveControl;
	}
	else
	{
		Ext1770Reset();
	}

	if (PGetBoardProperties == nullptr ||
	    PSetDriveControl == nullptr ||
	    PGetDriveControl == nullptr)
	{
		Ext1770Reset();

		return Ext1770Result::InvalidDLL;
	}

	PGetBoardProperties(&ExtBoard);

	EFDCAddr = ExtBoard.FDCAddress;
	EDCAddr = ExtBoard.DCAddress;
	InvertTR00 = ExtBoard.TR00_ActiveHigh;

	return Ext1770Result::Success;

	#endif
}

/*--------------------------------------------------------------------------*/

bool HasFDCBoard()
{
	#ifdef WIN32

	return hFDCBoard != nullptr;

	#else

	return PGetBoardProperties != nullptr;

	#endif
}

/*--------------------------------------------------------------------------*/

const char* GetFDCBoardName()
{
	return ExtBoard.BoardName;
}

/*--------------------------------------------------------------------------*/

unsigned char GetDriveControl(unsigned char Value)
{
	return PGetDriveControl(Value);
}

/*--------------------------------------------------------------------------*/

unsigned char SetDriveControl(unsigned char Value)
{
	return PSetDriveControl(Value);
}

/*--------------------------------------------------------------------------*/
