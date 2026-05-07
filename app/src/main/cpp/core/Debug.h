/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2004  Mike Wyatt
Copyright (C) 2004  Rob O'Donnell
Copyright (C) 2009  Steve Pick

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

//
// BeebEm debugger
//

#ifndef DEBUG_HEADER
#define DEBUG_HEADER

#include <stdarg.h>
#include <string>

#include "Via.h"
#include "Windows.h"

enum class DebugType
{
	None,
	Video,
	SysVIA,
	UserVIA,
	Tube,
	Serial,
	Econet,
	RemoteServer,
	Teletext,
	CMOS,
	BRK,
	Manual,
	Breakpoint,
	Last
};

//*******************************************************************
// Data structs

struct Label
{
	std::string name;
	int addr;

	Label() : addr(0)
	{
	}

	Label(const std::string& n, int a) : name(n), addr(a)
	{
	}
};

struct Breakpoint
{
	int start = -1;
	int end = -1;
	std::string name;
};

struct Watch
{
	int start = -1;
	char type = 'w';
	int value = -1;
	bool host = true;
	std::string name;
};

struct InstInfo
{
	const char* opcode;
	int bytes;
	int flag;
};

struct AddrInfo
{
	int start = -1;
	int end = -1;
	std::string desc;
};

struct DebugCmd
{
	const char *name;
	bool (*handler)(const char* arguments);
	const char *argdesc;
	const char *help;
};

bool DebugDisassembler(int Addr,
                       int PrevAddr,
                       int Accumulator,
                       int XReg,
                       int YReg,
                       unsigned char PSR,
                       unsigned char StackReg,
                       bool Host);
int DebugDisassembleInstruction(int Addr, bool Host, char *pszOutput);
int DebugDisassembleInstructionWithCPUStatus(int Addr,
                                             bool Host,
                                             int Accumulator,
                                             int XReg,
                                             int YReg,
                                             unsigned char StackReg,
                                             unsigned char PSR,
                                             char *pszOutput);

void DebugOpenDialog(HINSTANCE hinst, HWND hwndMain);
void DebugCloseDialog();

void DebugDisplayTrace(DebugType Type, bool Host, const char *Info);
void DebugDisplayTraceF(DebugType Type, bool Host, const char *Format, ...);
void DebugDisplayTraceV(DebugType Type, bool Host, const char *Format, va_list Args);

void DebugDisplayInfo(const char *info);
void DebugDisplayInfoF(const char *format, ...);

void DebugRunScript(const char *filename);
bool DebugLoadLabels(const char *filename, bool host);

unsigned char DebugReadMem(int addr, bool host);
void DebugBreakExecution(DebugType Type);

void DebugInitMemoryMaps();
bool DebugLoadMemoryMap(const char* filename, int bank);

extern bool DebugEnabled;
extern HWND hwndDebug;

#endif
