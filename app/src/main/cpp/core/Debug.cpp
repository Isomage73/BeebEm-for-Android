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
// Mike Wyatt - Nov 2004
// Econet added Rob O'Donnell 2004-12-28.

#include "Windows.h"

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include <algorithm>
#include <deque>
#include <fstream>
#include <string>
#include <vector>

#include "Debug.h"
#include "6502core.h"
#include "Arm.h"
#include "ArmDisassembler.h"
#include "BeebMem.h"
#include "DebugTrace.h"
#include "Econet.h"
#include "FileDialog.h"
#include "Main.h"
#include "Master512CoPro.h"
#include "Resource.h"
#include "Serial.h"
#include "SprowCoPro.h"
#include "StringUtils.h"
#include "SysVia.h"
#include "Tube.h"
#include "UserVia.h"
#include "WindowUtils.h"
#include "Z80mem.h"
#include "Z80.h"

/****************************************************************************/

constexpr int MAX_LINES = 4096;          // Max lines in info window
constexpr int LINES_IN_INFO = 28;        // Visible lines in info window
constexpr int MAX_COMMAND_LEN = 200;     // Max debug command length
constexpr int MAX_BPS = 50;              // Max num of breakpoints/watches
constexpr int MAX_HISTORY = 20;          // Number of commands in the command history.

// Instruction format
constexpr int IMM = 0x20;
constexpr int ABS = 0x40;
constexpr int ACC = 0x80;
constexpr int IMP = 0x100;
constexpr int INX = 0x200;
constexpr int INY = 0x400;
constexpr int ZPX = 0x800;
constexpr int ABX = 0x1000;
constexpr int ABY = 0x2000;
constexpr int REL = 0x4000;
constexpr int IND = 0x8000;
constexpr int ZPY = 0x10000;
constexpr int ZPG = 0x20000;
constexpr int ZPR = 0x40000;
constexpr int ILL = 0x80000;

constexpr int ADRMASK = IMM | ABS | ACC | IMP | INX | INY | ZPX | ABX | ABY | REL | IND | ZPY | ZPG | ZPR | ILL;

bool DebugEnabled = false; // Debug dialog visible

#ifdef WIN32
static DebugType DebugSource = DebugType::None; // Debugging active?
static int LinesDisplayed = 0;  // Lines in info window
static int InstCount = 0;       // Instructions to execute before breaking
static int DumpAddress = 0;     // Next address for memory dump command
static int DisAddress = 0;      // Next address for disassemble command
static int LastBreakAddr = 0;   // Address of last break
static int DebugInfoWidth = 0;  // Width of debug info window

static bool DebugTraceEnable[(int)DebugType::Last];
static bool DebugBreakEnable[(int)DebugType::Last];

static bool BPSOn = true;
static bool StepOver = false;
static int ReturnAddress = 0;
static bool DebugOS = false;
static bool LastAddrInOS = false;
static bool LastAddrInBIOS = false;
static bool DebugROM = false;
static bool LastAddrInROM = false;
static bool DebugHost = true;
static bool DebugParasite = false;
static bool WatchDecimal = false;
static bool WatchRefresh = false;
static bool WatchBigEndian = false;
#endif

#ifdef WIN32
HWND hwndDebug;
static HWND hwndInvisibleOwner;
static HWND hwndInfo;
static HWND hwndBP;
static HWND hwndW;
static HACCEL haccelDebug;
#endif

static std::vector<Label> Labels[2]; // Host and copro
static std::vector<Breakpoint> Breakpoints;
static std::vector<Watch> Watches;

typedef std::vector<AddrInfo> MemoryMap;

static MemoryMap MemoryMaps[17];

static std::deque<std::string> DebugHistory;
static int DebugHistoryIndex = 0;

/****************************************************************************/

#ifdef WIN32

INT_PTR CALLBACK DebugDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

static void DebugParseCommand(const char *command);
static void DebugWriteMem(int addr, bool host, unsigned char data);
static int DebugDisassembleCommand(int Addr, int Count, bool Host);
static void DebugMemoryDump(int addr, int count, bool host);
static void DebugExecuteCommand();
static void DebugToggleRun();

static int DebugFindBreakpoint(const Breakpoint& bp);
static void DebugAddBreakpoint(const Breakpoint& bp);

static int DebugFindWatch(const Watch& w);
static void DebugAddWatch(const Watch& w);
static void DebugUpdateWatches(bool UpdateAll);

static bool DebugLookupAddress(int Addr, bool Host, AddrInfo* addrInfo);
static void DebugHistoryMove(int delta);
static void DebugHistoryAdd(const char* command);
static void DebugSetCommandString(const char* str);

#endif

static void DebugChompString(char* str);

#ifdef WIN32

// Command handlers
static bool DebugCmdBreakContinue(const char* args);
static bool DebugCmdToggleBreak(const char* args);
static bool DebugCmdLabels(const char* args);
static bool DebugCmdHelp(const char* args);
static bool DebugCmdSet(const char* args);
static bool DebugCmdNext(const char* args);
static bool DebugCmdOver(const char* args);
static bool DebugCmdPeek(const char* args);
static bool DebugCmdCode(const char* args);
static bool DebugCmdWatch(const char* args);
static bool DebugCmdState(const char* args);
static bool DebugCmdSave(const char* args);
static bool DebugCmdPoke(const char* args);
static bool DebugCmdGoto(const char* args);
static bool DebugCmdFile(const char* args);
static bool DebugCmdEcho(const char* args);
static bool DebugCmdScript(const char* args);
static bool DebugCmdClear(const char* args);

/****************************************************************************/

// Debugger commands go here. Format is COMMAND, HANDLER, ARGSPEC, HELPSTRING
// Aliases are supported, put these below the command they reference and leave argspec/help
// empty.
static const DebugCmd DebugCmdTable[] = {
	{ "bp",         DebugCmdToggleBreak,   "<start>[-<end>] [<name>]", "Set or clear a breakpoint or break range" },
	{ "b",          DebugCmdToggleBreak,   "", ""}, // Alias of "bp"
	{ "breakpoint", DebugCmdToggleBreak,   "", ""}, // Alias of "bp"
	{ "labels",     DebugCmdLabels,        "<load|show|clear> [p] [<filename>]", "Load labels from file, display known labels, or clear all labels" },
	{ "l",          DebugCmdLabels,        "", ""}, // Alias of "labels"
	{ "help",       DebugCmdHelp,          "[<item>]", "Display help for the specified command or address" },
	{ "?",          DebugCmdHelp,          "", ""}, // Alias of "help"
	{ "q",          DebugCmdHelp,          "", ""}, // Alias of "help"
	{ "break",      DebugCmdBreakContinue, "", "Break or continue" },
	{ ".",          DebugCmdBreakContinue, "",""}, // Alias of "break"
	{ "set",        DebugCmdSet,           "<host|parasite|rom|os|endian|breakpoints|decimal|brk> <on|off>", "Turn various UI checkboxes on or off" },
	{ "next",       DebugCmdNext,          "[<count>]", "Execute the specified number instructions, default 1" },
	{ "n",          DebugCmdNext,          "", ""}, // Alias of "next"
	{ "over",       DebugCmdOver,          "", "Step over JSR (host only)" },
	{ "o",          DebugCmdOver,          "", ""}, // Alias of "over"
	{ "peek",       DebugCmdPeek,          "[p] [<start>] [<count>]", "Dump memory to the console" },
	{ "m",          DebugCmdPeek,          "", ""}, // Alias of "peek"
	{ "code",       DebugCmdCode,          "[p] [<start>] [<count>]", "Disassembles specified range" },
	{ "d",          DebugCmdCode,          "", ""}, // Alias of "code"
	{ "watch",      DebugCmdWatch,         "[p] <addr> <b|w|d> [<name>]", "Set or clear a byte, word, or dword watch at addr" },
	{ "e",          DebugCmdWatch,         "", ""}, // Alias of "watch"
	{ "state",      DebugCmdState,         "<v|u|s|e|n|t|m|r>", "Display state of Video/UserVIA/SysVIA/Serial/Econet/Tube/Memory/ROMs" },
	{ "s",          DebugCmdState,         "", ""}, // Alias of "state"
	{ "save",       DebugCmdSave,          "[<count>] [<filename>]", "Write console lines to file" },
	{ "w",          DebugCmdSave,          "", ""}, // Alias of "save"
	{ "poke",       DebugCmdPoke,          "[p] <start> <byte> [<byte> ...]", "Write bytes to memory" },
	{ "c",          DebugCmdPoke,          "", ""}, // Alias of "poke"
	{ "goto",       DebugCmdGoto,          "[p] <addr>", "Jump to address" },
	{ "g",          DebugCmdGoto,          "", ""}, // Alias of "goto"
	{ "file",       DebugCmdFile,          "<r|w> [p] <addr> [<count>] [<filename>]", "Read/Write memory at address from/to file" },
	{ "f",          DebugCmdFile,          "", ""}, // Alias of "file"
	{ "echo",       DebugCmdEcho,          "<string>", "Write a string to the console" },
	{ "!",          DebugCmdEcho,          "", "" }, // Alias of "echo"
	{ "script",     DebugCmdScript,        "[<filename>]", "Execute a debugger script" },
	{ "clear",      DebugCmdClear,         "", "Clear the console." }
};

/****************************************************************************/

static const InstInfo optable_6502[256] =
{
	{ "BRK",  1, IMP }, // 00
	{ "ORA",  2, INX }, // 01
	{ "KIL",  1, ILL }, // 02
	{ "SLO",  2, INX }, // 03
	{ "NOP",  2, ZPG }, // 04
	{ "ORA",  2, ZPG }, // 05
	{ "ASL",  2, ZPG }, // 06
	{ "SLO",  2, ZPG }, // 07
	{ "PHP",  1, IMP }, // 08
	{ "ORA",  2, IMM }, // 09
	{ "ASL",  1, ACC }, // 0a
	{ "ANC",  2, IMM }, // 0b
	{ "NOP",  3, ABS }, // 0c
	{ "ORA",  3, ABS }, // 0d
	{ "ASL",  3, ABS }, // 0e
	{ "SLO",  3, ABS }, // 0f
	{ "BPL",  2, REL }, // 10
	{ "ORA",  2, INY }, // 11
	{ "KIL",  1, ILL }, // 12
	{ "SLO",  2, INY }, // 13
	{ "NOP",  2, ZPX }, // 14
	{ "ORA",  2, ZPX }, // 15
	{ "ASL",  2, ZPX }, // 16
	{ "SLO",  2, ZPX }, // 17
	{ "CLC",  1, IMP }, // 18
	{ "ORA",  3, ABY }, // 19
	{ "NOP",  1, IMP }, // 1a
	{ "SLO",  3, ABY }, // 1b
	{ "NOP",  3, ABX }, // 1c
	{ "ORA",  3, ABX }, // 1d
	{ "ASL",  3, ABX }, // 1e
	{ "SLO",  3, ABX }, // 1f
	{ "JSR",  3, ABS }, // 20
	{ "AND",  2, INX }, // 21
	{ "KIL",  1, ILL }, // 22
	{ "RLA",  2, INX }, // 23
	{ "BIT",  2, ZPG }, // 24
	{ "AND",  2, ZPG }, // 25
	{ "ROL",  2, ZPG }, // 26
	{ "RLA",  2, ZPG }, // 27
	{ "PLP",  1, IMP }, // 28
	{ "AND",  2, IMM }, // 29
	{ "ROL",  1, ACC }, // 2a
	{ "ANC",  2, IMM }, // 2b
	{ "BIT",  3, ABS }, // 2c
	{ "AND",  3, ABS }, // 2d
	{ "ROL",  3, ABS }, // 2e
	{ "RLA",  3, ABS }, // 2f
	{ "BMI",  2, REL }, // 30
	{ "AND",  2, INY }, // 31
	{ "KIL",  1, ILL }, // 32
	{ "RLA",  2, INY }, // 33
	{ "NOP",  2, ZPX }, // 34
	{ "AND",  2, ZPX }, // 35
	{ "ROL",  2, ZPX }, // 36
	{ "RLA",  2, ZPX }, // 37
	{ "SEC",  1, IMP }, // 38
	{ "AND",  3, ABY }, // 39
	{ "NOP",  1, IMP }, // 3a
	{ "RLA",  3, ABY }, // 3b
	{ "NOP",  3, ABX }, // 3c
	{ "AND",  3, ABX }, // 3d
	{ "ROL",  3, ABX }, // 3e
	{ "RLA",  3, ABX }, // 3f
	{ "RTI",  1, IMP }, // 40
	{ "EOR",  2, INX }, // 41
	{ "KIL",  1, ILL }, // 42
	{ "SRE",  2, INX }, // 43
	{ "NOP",  2, ZPG }, // 44
	{ "EOR",  2, ZPG }, // 45
	{ "LSR",  2, ZPG }, // 46
	{ "SRE",  2, ZPG }, // 47
	{ "PHA",  1, IMP }, // 48
	{ "EOR",  2, IMM }, // 49
	{ "LSR",  1, ACC }, // 4a
	{ "ALR",  2, IMM }, // 4b
	{ "JMP",  3, ABS }, // 4c
	{ "EOR",  3, ABS }, // 4d
	{ "LSR",  3, ABS }, // 4e
	{ "SRE",  3, ABS }, // 4f
	{ "BVC",  2, REL }, // 50
	{ "EOR",  2, INY }, // 51
	{ "KIL",  1, ILL }, // 52
	{ "SRE",  2, INY }, // 53
	{ "NOP",  2, ZPX }, // 54
	{ "EOR",  2, ZPX }, // 55
	{ "LSR",  2, ZPX }, // 56
	{ "SRE",  2, ZPX }, // 57
	{ "CLI",  1, IMP }, // 58
	{ "EOR",  3, ABY }, // 59
	{ "NOP",  1, IMP }, // 5a
	{ "SRE",  3, ABY }, // 5b
	{ "NOP",  3, ABX }, // 5c
	{ "EOR",  3, ABX }, // 5d
	{ "LSR",  3, ABX }, // 5e
	{ "SRE",  3, ABX }, // 5f
	{ "RTS",  1, IMP }, // 60
	{ "ADC",  2, INX }, // 61
	{ "KIL",  1, ILL }, // 62
	{ "RRA",  2, INX }, // 63
	{ "NOP",  2, ZPG }, // 64
	{ "ADC",  2, ZPG }, // 65
	{ "ROR",  2, ZPG }, // 66
	{ "RRA",  2, ZPG }, // 67
	{ "PLA",  1, IMP }, // 68
	{ "ADC",  2, IMM }, // 69
	{ "ROR",  1, ACC }, // 6a
	{ "ARR",  2, IMM }, // 6b
	{ "JMP",  3, IND }, // 6c
	{ "ADC",  3, ABS }, // 6d
	{ "ROR",  3, ABS }, // 6e
	{ "RRA",  3, ABS }, // 6f
	{ "BVS",  2, REL }, // 70
	{ "ADC",  2, INY }, // 71
	{ "KIL",  1, ILL }, // 72
	{ "RRA",  2, INY }, // 73
	{ "NOP",  2, ZPX }, // 74
	{ "ADC",  2, ZPX }, // 75
	{ "ROR",  2, ZPX }, // 76
	{ "RRA",  2, ZPX }, // 77
	{ "SEI",  1, IMP }, // 78
	{ "ADC",  3, ABY }, // 79
	{ "NOP",  1, IMP }, // 7a
	{ "RRA",  3, ABY }, // 7b
	{ "NOP",  3, ABX }, // 7c
	{ "ADC",  3, ABX }, // 7d
	{ "ROR",  3, ABX }, // 7e
	{ "RRA",  3, ABX }, // 7f
	{ "NOP",  2, IMM }, // 80
	{ "STA",  2, INX }, // 81
	{ "NOP",  2, IMM }, // 82
	{ "SAX",  2, INX }, // 83
	{ "STY",  2, ZPG }, // 84
	{ "STA",  2, ZPG }, // 85
	{ "STX",  2, ZPG }, // 86
	{ "SAX",  2, ZPG }, // 87
	{ "DEY",  1, IMP }, // 88
	{ "NOP",  2, IMM }, // 89
	{ "TXA",  1, IMP }, // 8a
	{ "XAA",  2, IMM }, // 8b
	{ "STY",  3, ABS }, // 8c
	{ "STA",  3, ABS }, // 8d
	{ "STX",  3, ABS }, // 8e
	{ "SAX",  3, ABS }, // 8f
	{ "BCC",  2, REL }, // 90
	{ "STA",  2, INY }, // 91
	{ "KIL",  1, ILL }, // 92
	{ "AHX",  2, INY }, // 93
	{ "STY",  2, ZPX }, // 94
	{ "STA",  2, ZPX }, // 95
	{ "STX",  2, ZPY }, // 96
	{ "SAX",  2, ZPY }, // 97
	{ "TYA",  1, IMP }, // 98
	{ "STA",  3, ABY }, // 99
	{ "TXS",  1, IMP }, // 9a
	{ "TAS",  3, ABY }, // 9b
	{ "SHY",  3, ABX }, // 9c
	{ "STA",  3, ABX }, // 9d
	{ "SHX",  3, ABY }, // 9e
	{ "AHX",  3, ABY }, // 9f
	{ "LDY",  2, IMM }, // a0
	{ "LDA",  2, INX }, // a1
	{ "LDX",  2, IMM }, // a2
	{ "LAX",  2, INX }, // a3
	{ "LDY",  2, ZPG }, // a4
	{ "LDA",  2, ZPG }, // a5
	{ "LDX",  2, ZPG }, // a6
	{ "LAX",  2, ZPG }, // a7
	{ "TAY",  1, IMP }, // a8
	{ "LDA",  2, IMM }, // a9
	{ "TAX",  1, IMP }, // aa
	{ "LAX",  2, IMM }, // ab
	{ "LDY",  3, ABS }, // ac
	{ "LDA",  3, ABS }, // ad
	{ "LDX",  3, ABS }, // ae
	{ "LAX",  3, ABS }, // af
	{ "BCS",  2, REL }, // b0
	{ "LDA",  2, INY }, // b1
	{ "KIL",  1, ILL }, // b2
	{ "LAX",  2, INY }, // b3
	{ "LDY",  2, ZPX }, // b4
	{ "LDA",  2, ZPX }, // b5
	{ "LDX",  2, ZPY }, // b6
	{ "LAX",  2, ZPY }, // b7
	{ "CLV",  1, IMP }, // b8
	{ "LDA",  3, ABY }, // b9
	{ "TSX",  1, IMP }, // ba
	{ "LAS",  3, ABY }, // bb
	{ "LDY",  3, ABX }, // bc
	{ "LDA",  3, ABX }, // bd
	{ "LDX",  3, ABY }, // be
	{ "LAX",  3, ABY }, // bf
	{ "CPY",  2, IMM }, // c0
	{ "CMP",  2, INX }, // c1
	{ "NOP",  2, IMM }, // c2
	{ "DCP",  2, INX }, // c3
	{ "CPY",  2, ZPG }, // c4
	{ "CMP",  2, ZPG }, // c5
	{ "DEC",  2, ZPG }, // c6
	{ "DCP",  2, ZPG }, // c7
	{ "INY",  1, IMP }, // c8
	{ "CMP",  2, IMM }, // c9
	{ "DEX",  1, IMP }, // ca
	{ "AXS",  2, IMM }, // cb
	{ "CPY",  3, ABS }, // cc
	{ "CMP",  3, ABS }, // cd
	{ "DEC",  3, ABS }, // ce
	{ "DCP",  3, ABS }, // cf
	{ "BNE",  2, REL }, // d0
	{ "CMP",  2, INY }, // d1
	{ "KIL",  1, ILL }, // d2
	{ "DCP",  2, INY }, // d3
	{ "NOP",  2, ZPX }, // d4
	{ "CMP",  2, ZPX }, // d5
	{ "DEC",  2, ZPX }, // d6
	{ "DCP",  2, ZPX }, // d7
	{ "CLD",  1, IMP }, // d8
	{ "CMP",  3, ABY }, // d9
	{ "NOP",  1, IMP }, // da
	{ "DCP",  3, ABY }, // db
	{ "NOP",  3, ABX }, // dc
	{ "CMP",  3, ABX }, // dd
	{ "DEC",  3, ABX }, // de
	{ "DCP",  3, ABX }, // df
	{ "CPX",  2, IMM }, // e0
	{ "SBC",  2, INX }, // e1
	{ "NOP",  2, IMM }, // e2
	{ "ISC",  2, INX }, // e3
	{ "CPX",  2, ZPG }, // e4
	{ "SBC",  2, ZPG }, // e5
	{ "INC",  2, ZPG }, // e6
	{ "ISC",  2, ZPG }, // e7
	{ "INX",  1, IMP }, // e8
	{ "SBC",  2, IMM }, // e9
	{ "NOP",  1, IMP }, // ea
	{ "SBC",  2, IMM }, // eb
	{ "CPX",  3, ABS }, // ec
	{ "SBC",  3, ABS }, // ed
	{ "INC",  3, ABS }, // ee
	{ "ISC",  3, ABS }, // ef
	{ "BEQ",  2, REL }, // f0
	{ "SBC",  2, INY }, // f1
	{ "KIL",  1, ILL }, // f2
	{ "ISC",  2, INY }, // f3
	{ "NOP",  2, ZPX }, // f4
	{ "SBC",  2, ZPX }, // f5
	{ "INC",  2, ZPX }, // f6
	{ "ISC",  2, ZPX }, // f7
	{ "SED",  1, IMP }, // f8
	{ "SBC",  3, ABY }, // f9
	{ "NOP",  1, IMP }, // fa
	{ "ISC",  3, ABY }, // fb
	{ "NOP",  3, ABX }, // fc
	{ "SBC",  3, ABX }, // fd
	{ "INC",  3, ABX }, // fe
	{ "ISC",  3, ABX }  // ff
};

static const InstInfo optable_65c02[256] =
{
	{ "BRK",  1, IMP }, // 00
	{ "ORA",  2, INX }, // 01
	{ "NOP",  2, IMM }, // 02
	{ "NOP",  1, IMP }, // 03
	{ "TSB",  2, ZPG }, // 04
	{ "ORA",  2, ZPG }, // 05
	{ "ASL",  2, ZPG }, // 06
	{ "RMB0", 2, ZPG }, // 07
	{ "PHP",  1, IMP }, // 08
	{ "ORA",  2, IMM }, // 09
	{ "ASL",  1, ACC }, // 0a
	{ "NOP",  1, IMP }, // 0b
	{ "TSB",  3, ABS }, // 0c
	{ "ORA",  3, ABS }, // 0d
	{ "ASL",  3, ABS }, // 0e
	{ "BBR0", 3, ZPR }, // 0f
	{ "BPL",  2, REL }, // 10
	{ "ORA",  2, INY }, // 11
	{ "ORA",  2, IND }, // 12
	{ "NOP",  1, IMP }, // 13
	{ "TRB",  2, ZPG }, // 14
	{ "ORA",  2, ZPX }, // 15
	{ "ASL",  2, ZPX }, // 16
	{ "RMB1", 2, ZPG }, // 17
	{ "CLC",  1, IMP }, // 18
	{ "ORA",  3, ABY }, // 19
	{ "INC",  1, ACC }, // 1a
	{ "NOP",  1, IMP }, // 1b
	{ "TRB",  3, ABS }, // 1c
	{ "ORA",  3, ABX }, // 1d
	{ "ASL",  3, ABX }, // 1e
	{ "BBR1", 3, ZPR }, // 1f
	{ "JSR",  3, ABS }, // 20
	{ "AND",  2, INX }, // 21
	{ "NOP",  2, IMM }, // 22
	{ "NOP",  1, IMP }, // 23
	{ "BIT",  2, ZPG }, // 24
	{ "AND",  2, ZPG }, // 25
	{ "ROL",  2, ZPG }, // 26
	{ "RMB2", 2, ZPG }, // 27
	{ "PLP",  1, IMP }, // 28
	{ "AND",  2, IMM }, // 29
	{ "ROL",  1, ACC }, // 2a
	{ "NOP",  1, IMP }, // 2b
	{ "BIT",  3, ABS }, // 2c
	{ "AND",  3, ABS }, // 2d
	{ "ROL",  3, ABS }, // 2e
	{ "BBR2", 3, ZPR }, // 2f
	{ "BMI",  2, REL }, // 30
	{ "AND",  2, INY }, // 31
	{ "AND",  2, IND }, // 32
	{ "NOP",  1, IMP }, // 33
	{ "BIT",  2, ZPX }, // 34
	{ "AND",  2, ZPX }, // 35
	{ "ROL",  2, ZPX }, // 36
	{ "RMB3", 2, ZPG }, // 37
	{ "SEC",  1, IMP }, // 38
	{ "AND",  3, ABY }, // 39
	{ "DEC",  1, ACC }, // 3a
	{ "NOP",  1, IMP }, // 3b
	{ "BIT",  3, ABX }, // 3c
	{ "AND",  3, ABX }, // 3d
	{ "ROL",  3, ABX }, // 3e
	{ "BBR3", 3, ZPR }, // 3f
	{ "RTI",  1, IMP }, // 40
	{ "EOR",  2, INX }, // 41
	{ "NOP",  2, IMM }, // 42
	{ "NOP",  1, IMP }, // 43
	{ "NOP",  2, ZPG }, // 44
	{ "EOR",  2, ZPG }, // 45
	{ "LSR",  2, ZPG }, // 46
	{ "RMB4", 2, ZPG }, // 47
	{ "PHA",  1, IMP }, // 48
	{ "EOR",  2, IMM }, // 49
	{ "LSR",  1, ACC }, // 4a
	{ "NOP",  1, IMP }, // 4b
	{ "JMP",  3, ABS }, // 4c
	{ "EOR",  3, ABS }, // 4d
	{ "LSR",  3, ABS }, // 4e
	{ "BBR4", 3, ZPR }, // 4f
	{ "BVC",  2, REL }, // 50
	{ "EOR",  2, INY }, // 51
	{ "EOR",  2, IND }, // 52
	{ "NOP",  1, IMP }, // 53
	{ "NOP",  2, ZPX }, // 54
	{ "EOR",  2, ZPX }, // 55
	{ "LSR",  2, ZPX }, // 56
	{ "RMB5", 2, ZPG }, // 57
	{ "CLI",  1, IMP }, // 58
	{ "EOR",  3, ABY }, // 59
	{ "PHY",  1, IMP }, // 5a
	{ "NOP",  1, IMP }, // 5b
	{ "NOP",  3, ABS }, // 5c
	{ "EOR",  3, ABX }, // 5d
	{ "LSR",  3, ABX }, // 5e
	{ "BBR5", 3, ZPR }, // 5f
	{ "RTS",  1, IMP }, // 60
	{ "ADC",  2, INX }, // 61
	{ "NOP",  2, IMM }, // 62
	{ "NOP",  1, IMP }, // 63
	{ "STZ",  2, ZPG }, // 64
	{ "ADC",  2, ZPG }, // 65
	{ "ROR",  2, ZPG }, // 66
	{ "RMB6", 2, ZPG }, // 67
	{ "PLA",  1, IMP }, // 68
	{ "ADC",  2, IMM }, // 69
	{ "ROR",  1, ACC }, // 6a
	{ "NOP",  1, IMP }, // 6b
	{ "JMP",  3, IND }, // 6c
	{ "ADC",  3, ABS }, // 6d
	{ "ROR",  3, ABS }, // 6e
	{ "BBR6", 3, ZPR }, // 6f
	{ "BVS",  2, REL }, // 70
	{ "ADC",  2, INY }, // 71
	{ "ADC",  2, IND }, // 72
	{ "NOP",  1, IMP }, // 73
	{ "STZ",  2, ZPX }, // 74
	{ "ADC",  2, ZPX }, // 75
	{ "ROR",  2, ZPX }, // 76
	{ "RMB7", 2, ZPG }, // 77
	{ "SEI",  1, IMP }, // 78
	{ "ADC",  3, ABY }, // 79
	{ "PLY",  1, IMP }, // 7a
	{ "NOP",  1, IMP }, // 7b
	{ "JMP",  3, INX }, // 7c
	{ "ADC",  3, ABX }, // 7d
	{ "ROR",  3, ABX }, // 7e
	{ "BBR7", 3, ZPR }, // 7f
	{ "BRA",  2, REL }, // 80
	{ "STA",  2, INX }, // 81
	{ "NOP",  2, IMM }, // 82
	{ "NOP",  1, IMP }, // 83
	{ "STY",  2, ZPG }, // 84
	{ "STA",  2, ZPG }, // 85
	{ "STX",  2, ZPG }, // 86
	{ "SMB0", 2, ZPG }, // 87
	{ "DEY",  1, IMP }, // 88
	{ "BIT",  2, IMM }, // 89
	{ "TXA",  1, IMP }, // 8a
	{ "NOP",  1, IMP }, // 8b
	{ "STY",  3, ABS }, // 8c
	{ "STA",  3, ABS }, // 8d
	{ "STX",  3, ABS }, // 8e
	{ "BBS0", 3, ZPR }, // 8f
	{ "BCC",  2, REL }, // 90
	{ "STA",  2, INY }, // 91
	{ "STA",  2, IND }, // 92
	{ "NOP",  1, IMP }, // 93
	{ "STY",  2, ZPX }, // 94
	{ "STA",  2, ZPX }, // 95
	{ "STX",  2, ZPY }, // 96
	{ "SMB1", 2, ZPG }, // 97
	{ "TYA",  1, IMP }, // 98
	{ "STA",  3, ABY }, // 99
	{ "TXS",  1, IMP }, // 9a
	{ "NOP",  1, IMP }, // 9b
	{ "STZ",  3, ABS }, // 9c
	{ "STA",  3, ABX }, // 9d
	{ "STZ",  3, ABX }, // 9e
	{ "BBS1", 3, ZPR }, // 9f
	{ "LDY",  2, IMM }, // a0
	{ "LDA",  2, INX }, // a1
	{ "LDX",  2, IMM }, // a2
	{ "NOP",  1, IMP }, // a3
	{ "LDY",  2, ZPG }, // a4
	{ "LDA",  2, ZPG }, // a5
	{ "LDX",  2, ZPG }, // a6
	{ "SMB2", 2, ZPG }, // a7
	{ "TAY",  1, IMP }, // a8
	{ "LDA",  2, IMM }, // a9
	{ "TAX",  1, IMP }, // aa
	{ "NOP",  1, IMP }, // ab
	{ "LDY",  3, ABS }, // ac
	{ "LDA",  3, ABS }, // ad
	{ "LDX",  3, ABS }, // ae
	{ "BBS2", 3, ZPR }, // af
	{ "BCS",  2, REL }, // b0
	{ "LDA",  2, INY }, // b1
	{ "LDA",  2, IND }, // b2
	{ "NOP",  1, IMP }, // b3
	{ "LDY",  2, ZPX }, // b4
	{ "LDA",  2, ZPX }, // b5
	{ "LDX",  2, ZPY }, // b6
	{ "SMB3", 2, ZPG }, // b7
	{ "CLV",  1, IMP }, // b8
	{ "LDA",  3, ABY }, // b9
	{ "TSX",  1, IMP }, // ba
	{ "NOP",  1, IMP }, // bb
	{ "LDY",  3, ABX }, // bc
	{ "LDA",  3, ABX }, // bd
	{ "LDX",  3, ABY }, // be
	{ "BBS3", 3, ZPR }, // bf
	{ "CPY",  2, IMM }, // c0
	{ "CMP",  2, INX }, // c1
	{ "NOP",  2, IMM }, // c2
	{ "NOP",  1, IMP }, // c3
	{ "CPY",  2, ZPG }, // c4
	{ "CMP",  2, ZPG }, // c5
	{ "DEC",  2, ZPG }, // c6
	{ "SMB4", 2, ZPG }, // c7
	{ "INY",  1, IMP }, // c8
	{ "CMP",  2, IMM }, // c9
	{ "DEX",  1, IMP }, // ca
	{ "NOP",  1, IMP }, // cb
	{ "CPY",  3, ABS }, // cc
	{ "CMP",  3, ABS }, // cd
	{ "DEC",  3, ABS }, // ce
	{ "BBS4", 3, ZPR }, // cf
	{ "BNE",  2, REL }, // d0
	{ "CMP",  2, INY }, // d1
	{ "CMP",  2, IND }, // d2
	{ "NOP",  1, IMP }, // d3
	{ "NOP",  2, ZPX }, // d4
	{ "CMP",  2, ZPX }, // d5
	{ "DEC",  2, ZPX }, // d6
	{ "SMB5", 2, ZPG }, // d7
	{ "CLD",  1, IMP }, // d8
	{ "CMP",  3, ABY }, // d9
	{ "PHX",  1, IMP }, // da
	{ "NOP",  1, IMP }, // db
	{ "NOP",  3, ABS }, // dc
	{ "CMP",  3, ABX }, // dd
	{ "DEC",  3, ABX }, // de
	{ "BBS5", 3, ZPR }, // df
	{ "CPX",  2, IMM }, // e0
	{ "SBC",  2, INX }, // e1
	{ "NOP",  2, IMM }, // e2
	{ "NOP",  1, IMP }, // e3
	{ "CPX",  2, ZPG }, // e4
	{ "SBC",  2, ZPG }, // e5
	{ "INC",  2, ZPG }, // e6
	{ "SMB6", 2, ZPG }, // e7
	{ "INX",  1, IMP }, // e8
	{ "SBC",  2, IMM }, // e9
	{ "NOP",  1, IMP }, // ea
	{ "NOP",  1, IMP }, // eb
	{ "CPX",  3, ABS }, // ec
	{ "SBC",  3, ABS }, // ed
	{ "INC",  3, ABS }, // ee
	{ "BBS6", 3, ZPR }, // ef
	{ "BEQ",  2, REL }, // f0
	{ "SBC",  2, INY }, // f1
	{ "SBC",  2, IND }, // f2
	{ "NOP",  1, IMP }, // f3
	{ "NOP",  2, ZPX }, // f4
	{ "SBC",  2, ZPX }, // f5
	{ "INC",  2, ZPX }, // f6
	{ "SMB7", 2, ZPG }, // f7
	{ "SED",  1, IMP }, // f8
	{ "SBC",  3, ABY }, // f9
	{ "PLX",  1, IMP }, // fa
	{ "NOP",  1, IMP }, // fb
	{ "NOP",  3, ABS }, // fc
	{ "SBC",  3, ABX }, // fd
	{ "INC",  3, ABX }, // fe
	{ "BBS7", 3, ZPR }  // ff
};

// Same as 65c02 but without BBRx and RMBx instructions

static const InstInfo optable_65sc12[256] =
{
	{ "BRK",  1, IMP }, // 00
	{ "ORA",  2, INX }, // 01
	{ "NOP",  2, IMM }, // 02
	{ "NOP",  1, IMP }, // 03
	{ "TSB",  2, ZPG }, // 04
	{ "ORA",  2, ZPG }, // 05
	{ "ASL",  2, ZPG }, // 06
	{ "NOP",  1, IMP }, // 07
	{ "PHP",  1, IMP }, // 08
	{ "ORA",  2, IMM }, // 09
	{ "ASL",  1, ACC }, // 0a
	{ "NOP",  1, IMP }, // 0b
	{ "TSB",  3, ABS }, // 0c
	{ "ORA",  3, ABS }, // 0d
	{ "ASL",  3, ABS }, // 0e
	{ "NOP",  1, IMP }, // 0f
	{ "BPL",  2, REL }, // 10
	{ "ORA",  2, INY }, // 11
	{ "ORA",  2, IND }, // 12
	{ "NOP",  1, IMP }, // 13
	{ "TRB",  2, ZPG }, // 14
	{ "ORA",  2, ZPX }, // 15
	{ "ASL",  2, ZPX }, // 16
	{ "NOP",  1, IMP }, // 17
	{ "CLC",  1, IMP }, // 18
	{ "ORA",  3, ABY }, // 19
	{ "INC",  1, ACC }, // 1a
	{ "NOP",  1, IMP }, // 1b
	{ "TRB",  3, ABS }, // 1c
	{ "ORA",  3, ABX }, // 1d
	{ "ASL",  3, ABX }, // 1e
	{ "NOP",  1, IMP }, // 1f
	{ "JSR",  3, ABS }, // 20
	{ "AND",  2, INX }, // 21
	{ "NOP",  2, IMM }, // 22
	{ "NOP",  1, IMP }, // 23
	{ "BIT",  2, ZPG }, // 24
	{ "AND",  2, ZPG }, // 25
	{ "ROL",  2, ZPG }, // 26
	{ "NOP",  1, IMP }, // 27
	{ "PLP",  1, IMP }, // 28
	{ "AND",  2, IMM }, // 29
	{ "ROL",  1, ACC }, // 2a
	{ "NOP",  1, IMP }, // 2b
	{ "BIT",  3, ABS }, // 2c
	{ "AND",  3, ABS }, // 2d
	{ "ROL",  3, ABS }, // 2e
	{ "NOP",  1, IMP }, // 2f
	{ "BMI",  2, REL }, // 30
	{ "AND",  2, INY }, // 31
	{ "AND",  2, IND }, // 32
	{ "NOP",  1, IMP }, // 33
	{ "BIT",  2, ZPX }, // 34
	{ "AND",  2, ZPX }, // 35
	{ "ROL",  2, ZPX }, // 36
	{ "NOP",  1, IMP }, // 37
	{ "SEC",  1, IMP }, // 38
	{ "AND",  3, ABY }, // 39
	{ "DEC",  1, ACC }, // 3a
	{ "NOP",  1, IMP }, // 3b
	{ "BIT",  3, ABX }, // 3c
	{ "AND",  3, ABX }, // 3d
	{ "ROL",  3, ABX }, // 3e
	{ "NOP",  1, IMP }, // 3f
	{ "RTI",  1, IMP }, // 40
	{ "EOR",  2, INX }, // 41
	{ "NOP",  2, IMM }, // 42
	{ "NOP",  1, IMP }, // 43
	{ "NOP",  2, ZPG }, // 44
	{ "EOR",  2, ZPG }, // 45
	{ "LSR",  2, ZPG }, // 46
	{ "NOP",  1, IMP }, // 47
	{ "PHA",  1, IMP }, // 48
	{ "EOR",  2, IMM }, // 49
	{ "LSR",  1, ACC }, // 4a
	{ "NOP",  1, IMP }, // 4b
	{ "JMP",  3, ABS }, // 4c
	{ "EOR",  3, ABS }, // 4d
	{ "LSR",  3, ABS }, // 4e
	{ "NOP",  1, IMP }, // 4f
	{ "BVC",  2, REL }, // 50
	{ "EOR",  2, INY }, // 51
	{ "EOR",  2, IND }, // 52
	{ "NOP",  1, IMP }, // 53
	{ "NOP",  2, ZPX }, // 54
	{ "EOR",  2, ZPX }, // 55
	{ "LSR",  2, ZPX }, // 56
	{ "NOP",  1, IMP }, // 57
	{ "CLI",  1, IMP }, // 58
	{ "EOR",  3, ABY }, // 59
	{ "PHY",  1, IMP }, // 5a
	{ "NOP",  1, IMP }, // 5b
	{ "NOP",  3, ABS }, // 5c
	{ "EOR",  3, ABX }, // 5d
	{ "LSR",  3, ABX }, // 5e
	{ "NOP",  1, IMP }, // 5f
	{ "RTS",  1, IMP }, // 60
	{ "ADC",  2, INX }, // 61
	{ "NOP",  2, IMM }, // 62
	{ "NOP",  1, IMP }, // 63
	{ "STZ",  2, ZPG }, // 64
	{ "ADC",  2, ZPG }, // 65
	{ "ROR",  2, ZPG }, // 66
	{ "NOP",  1, IMP }, // 67
	{ "PLA",  1, IMP }, // 68
	{ "ADC",  2, IMM }, // 69
	{ "ROR",  1, ACC }, // 6a
	{ "NOP",  1, IMP }, // 6b
	{ "JMP",  3, IND }, // 6c
	{ "ADC",  3, ABS }, // 6d
	{ "ROR",  3, ABS }, // 6e
	{ "NOP",  1, IMP }, // 6f
	{ "BVS",  2, REL }, // 70
	{ "ADC",  2, INY }, // 71
	{ "ADC",  2, IND }, // 72
	{ "NOP",  1, IMP }, // 73
	{ "STZ",  2, ZPX }, // 74
	{ "ADC",  2, ZPX }, // 75
	{ "ROR",  2, ZPX }, // 76
	{ "NOP",  1, IMP }, // 77
	{ "SEI",  1, IMP }, // 78
	{ "ADC",  3, ABY }, // 79
	{ "PLY",  1, IMP }, // 7a
	{ "NOP",  1, IMP }, // 7b
	{ "JMP",  3, INX }, // 7c
	{ "ADC",  3, ABX }, // 7d
	{ "ROR",  3, ABX }, // 7e
	{ "NOP",  1, IMP }, // 7f
	{ "BRA",  2, REL }, // 80
	{ "STA",  2, INX }, // 81
	{ "NOP",  2, IMM }, // 82
	{ "NOP",  1, IMP }, // 83
	{ "STY",  2, ZPG }, // 84
	{ "STA",  2, ZPG }, // 85
	{ "STX",  2, ZPG }, // 86
	{ "NOP",  1, IMP }, // 87
	{ "DEY",  1, IMP }, // 88
	{ "BIT",  2, IMM }, // 89
	{ "TXA",  1, IMP }, // 8a
	{ "NOP",  1, IMP }, // 8b
	{ "STY",  3, ABS }, // 8c
	{ "STA",  3, ABS }, // 8d
	{ "STX",  3, ABS }, // 8e
	{ "NOP",  1, IMP }, // 8f
	{ "BCC",  2, REL }, // 90
	{ "STA",  2, INY }, // 91
	{ "STA",  2, IND }, // 92
	{ "NOP",  1, IMP }, // 93
	{ "STY",  2, ZPX }, // 94
	{ "STA",  2, ZPX }, // 95
	{ "STX",  2, ZPY }, // 96
	{ "NOP",  1, IMP }, // 97
	{ "TYA",  1, IMP }, // 98
	{ "STA",  3, ABY }, // 99
	{ "TXS",  1, IMP }, // 9a
	{ "NOP",  1, IMP }, // 9b
	{ "STZ",  3, ABS }, // 9c
	{ "STA",  3, ABX }, // 9d
	{ "STZ",  3, ABX }, // 9e
	{ "NOP",  1, IMP }, // 9f
	{ "LDY",  2, IMM }, // a0
	{ "LDA",  2, INX }, // a1
	{ "LDX",  2, IMM }, // a2
	{ "NOP",  1, IMP }, // a3
	{ "LDY",  2, ZPG }, // a4
	{ "LDA",  2, ZPG }, // a5
	{ "LDX",  2, ZPG }, // a6
	{ "NOP",  1, IMP }, // a7
	{ "TAY",  1, IMP }, // a8
	{ "LDA",  2, IMM }, // a9
	{ "TAX",  1, IMP }, // aa
	{ "NOP",  1, IMP }, // ab
	{ "LDY",  3, ABS }, // ac
	{ "LDA",  3, ABS }, // ad
	{ "LDX",  3, ABS }, // ae
	{ "NOP",  1, IMP }, // af
	{ "BCS",  2, REL }, // b0
	{ "LDA",  2, INY }, // b1
	{ "LDA",  2, IND }, // b2
	{ "NOP",  1, IMP }, // b3
	{ "LDY",  2, ZPX }, // b4
	{ "LDA",  2, ZPX }, // b5
	{ "LDX",  2, ZPY }, // b6
	{ "NOP",  1, IMP }, // b7
	{ "CLV",  1, IMP }, // b8
	{ "LDA",  3, ABY }, // b9
	{ "TSX",  1, IMP }, // ba
	{ "NOP",  1, IMP }, // bb
	{ "LDY",  3, ABX }, // bc
	{ "LDA",  3, ABX }, // bd
	{ "LDX",  3, ABY }, // be
	{ "NOP",  1, IMP }, // bf
	{ "CPY",  2, IMM }, // c0
	{ "CMP",  2, INX }, // c1
	{ "NOP",  2, IMM }, // c2
	{ "NOP",  1, IMP }, // c3
	{ "CPY",  2, ZPG }, // c4
	{ "CMP",  2, ZPG }, // c5
	{ "DEC",  2, ZPG }, // c6
	{ "NOP",  1, IMP }, // c7
	{ "INY",  1, IMP }, // c8
	{ "CMP",  2, IMM }, // c9
	{ "DEX",  1, IMP }, // ca
	{ "NOP",  1, IMP }, // cb
	{ "CPY",  3, ABS }, // cc
	{ "CMP",  3, ABS }, // cd
	{ "DEC",  3, ABS }, // ce
	{ "NOP",  1, IMP }, // cf
	{ "BNE",  2, REL }, // d0
	{ "CMP",  2, INY }, // d1
	{ "CMP",  2, IND }, // d2
	{ "NOP",  1, IMP }, // d3
	{ "NOP",  2, ZPX }, // d4
	{ "CMP",  2, ZPX }, // d5
	{ "DEC",  2, ZPX }, // d6
	{ "NOP",  1, IMP }, // d7
	{ "CLD",  1, IMP }, // d8
	{ "CMP",  3, ABY }, // d9
	{ "PHX",  1, IMP }, // da
	{ "NOP",  1, IMP }, // db
	{ "NOP",  3, ABS }, // dc
	{ "CMP",  3, ABX }, // dd
	{ "DEC",  3, ABX }, // de
	{ "NOP",  1, IMP }, // df
	{ "CPX",  2, IMM }, // e0
	{ "SBC",  2, INX }, // e1
	{ "NOP",  2, IMM }, // e2
	{ "NOP",  1, IMP }, // e3
	{ "CPX",  2, ZPG }, // e4
	{ "SBC",  2, ZPG }, // e5
	{ "INC",  2, ZPG }, // e6
	{ "NOP",  1, IMP }, // e7
	{ "INX",  1, IMP }, // e8
	{ "SBC",  2, IMM }, // e9
	{ "NOP",  1, IMP }, // ea
	{ "NOP",  1, IMP }, // eb
	{ "CPX",  3, ABS }, // ec
	{ "SBC",  3, ABS }, // ed
	{ "INC",  3, ABS }, // ee
	{ "NOP",  1, IMP }, // ef
	{ "BEQ",  2, REL }, // f0
	{ "SBC",  2, INY }, // f1
	{ "SBC",  2, IND }, // f2
	{ "NOP",  1, IMP }, // f3
	{ "NOP",  2, ZPX }, // f4
	{ "SBC",  2, ZPX }, // f5
	{ "INC",  2, ZPX }, // f6
	{ "NOP",  1, IMP }, // f7
	{ "SED",  1, IMP }, // f8
	{ "SBC",  3, ABY }, // f9
	{ "PLX",  1, IMP }, // fa
	{ "NOP",  1, IMP }, // fb
	{ "NOP",  3, ABS }, // fc
	{ "SBC",  3, ABX }, // fd
	{ "INC",  3, ABX }, // fe
	{ "NOP",  1, IMP }  // ff
};

/****************************************************************************/

static const InstInfo* GetOpcodeTable(bool host)
{
	if (host)
	{
		if (MachineType == Model::Master128 || MachineType == Model::MasterET)
		{
			return optable_65sc12;
		}
		else
		{
			return optable_6502;
		}
	}
	else
	{
		return optable_65c02;
	}
}

/****************************************************************************/

static int DebugGetAddressBits(bool Host)
{
	static int AddressBits[7] =
	{
		16, // None (host)
		16, // Acorn65C02
		32, // Master512CoPro
		16, // AcornZ80
		16, // TorchZ80
		32, // AcornArm
		32  // SprowArm
	};

	if (Host)
	{
		return AddressBits[0];
	}
	else
	{
		return AddressBits[static_cast<int>(TubeType)];
	}
}

/****************************************************************************/

static int DebugGetMaxAddress(bool Host)
{
	static int MaxAddress[7] =
	{
		0xFFFF,    // None (host)
		0xFFFF,    // Acorn65C02
		0xFFFFF,   // Master512CoPro
		0xFFFF,    // AcornZ80
		0xFFFF,    // TorchZ80
		0x3FFFFFF, // AcornArm
		0x3FFFFFF  // SprowArm
	};

	if (Host)
	{
		return MaxAddress[0];
	}
	else
	{
		return MaxAddress[static_cast<int>(TubeType)];
	}
}

/****************************************************************************/

static bool DebugIsIOAddress(unsigned long Address, bool Host)
{
	if (Host)
	{
		return Address >= 0xFC00 && Address < 0xFF00;
	}
	else
	{
		switch (TubeType)
		{
			case TubeDevice::Acorn65C02:
				return Address >= 0xFEF8 && Address < 0xFF00;

			case TubeDevice::Master512CoPro:
			case TubeDevice::AcornZ80:
			case TubeDevice::TorchZ80:
				return false;

			case TubeDevice::AcornArm:
				return (Address & ~0x1f) == 0x1000000;

			case TubeDevice::SprowArm:
				return Address >= 0xF0000000 && Address <= 0xF0000010;

			case TubeDevice::None:
			default:
				break;
		}
	}

	return false;
}

/****************************************************************************/

static int DebugFindLabel(const char* pszLabel, bool Host)
{
	for (int i = 0; i < (int)Labels[(int)Host].size(); ++i)
	{
		if (StrCaseCmp(pszLabel, Labels[(int)Host][i].name.c_str()) == 0)
		{
			return i;
		}
	}

	return -1;
}

/****************************************************************************/

static bool ParseAddressOrLabel(const char* pszAddress,
                                bool Host,
                                unsigned long* pAddress,
                                const char** ppszLabel)
{
	bool Success = true;

	if (pszAddress[0] == '.')
	{
		const char* pszLabel = pszAddress + 1;

		int Index = DebugFindLabel(pszLabel, Host);

		if (Index >= 0)
		{
			*pAddress = (unsigned long)Labels[(int)Host][Index].addr;

			if (ppszLabel != nullptr)
			{
				*ppszLabel = Labels[(int)Host][Index].name.c_str();
			}
		}
		else
		{
			DebugDisplayInfoF("Unknown label: %s", pszLabel);
			Success = false;
		}
	}
	else if (ParseHexNumber(pszAddress, pAddress))
	{
		// Wrap values outside the address range
		*pAddress &= DebugGetMaxAddress(Host);
	}
	else
	{
		DebugDisplayInfoF("Invalid address: %s", pszAddress);
		Success = false;
	}

	return Success;
}

/****************************************************************************/

static bool IsDlgItemChecked(HWND hDlg, int nIDDlgItem)
{
	return SendDlgItemMessage(hDlg, nIDDlgItem, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

/****************************************************************************/

static void SetDlgItemChecked(HWND hDlg, int nIDDlgItem, bool checked)
{
	SendDlgItemMessage(hDlg, nIDDlgItem, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

#endif

/****************************************************************************/

void DebugOpenDialog(HINSTANCE hinst, HWND /* hwndMain */)
{
	#ifdef WIN32

	if (hwndInvisibleOwner == nullptr)
	{
		// Keep the debugger off the taskbar with an invisible owner window.
		// This persists until the process closes.
		hwndInvisibleOwner = CreateWindowEx(0, "STATIC", 0, 0, 0, 0, 0, 0, 0, 0, hinst, 0);
	}

	if (hwndDebug != nullptr)
	{
		DebugCloseDialog();
	}

	DebugEnabled = true;

	haccelDebug = LoadAccelerators(hinst, MAKEINTRESOURCE(IDR_ACCELERATORS));
	hwndDebug = CreateDialog(hinst, MAKEINTRESOURCE(IDD_DEBUG),
	                         hwndInvisibleOwner, DebugDlgProc);

	hCurrentDialog = hwndDebug;
	hCurrentAccelTable = haccelDebug;

	DisableRoundedCorners(hwndDebug);
	ShowWindow(hwndDebug, SW_SHOW);

	#else

	UNREFERENCED_PARAMETER(hinst);

	#endif
}

/****************************************************************************/

void DebugCloseDialog()
{
	#ifdef WIN32

	DestroyWindow(hwndDebug);
	hwndDebug = nullptr;
	hwndInfo = nullptr;
	hCurrentDialog = nullptr;
	hCurrentAccelTable = nullptr;
	DebugEnabled = false;
	DebugSource = DebugType::None;
	LinesDisplayed = 0;
	DebugInfoWidth = 0;

	InstCount = 0;
	StepOver = false;
	ReturnAddress = 0;
	LastAddrInOS = false;
	LastAddrInBIOS = false;
	LastAddrInROM = false;

	#endif
}

/****************************************************************************/

void DebugDisplayInfoF(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	// _vscprintf doesn't count terminating '\0'
	int len = _vscprintf(format, args) + 1;

	char *buffer = (char*)malloc(len * sizeof(char));

	if (buffer != nullptr)
	{
		vsprintf(buffer, format, args);

		DebugDisplayInfo(buffer);
		free(buffer);
	}
}

/****************************************************************************/

#ifdef WIN32

void DebugDisplayInfo(const char *info)
{
	HDC hDC = GetDC(hwndInfo);
	HGDIOBJ hOldFont = SelectObject(hDC, (HFONT)SendMessage(hwndInfo, WM_GETFONT, 0, 0));

	SIZE size;
	GetTextExtentPoint(hDC, info, (int)strlen(info), &size);

	size.cx += 3;

	SelectObject(hDC, hOldFont);
	ReleaseDC(hwndInfo, hDC);

	SendMessage(hwndInfo, LB_ADDSTRING, 0, (LPARAM)info);

	if ((int)size.cx > DebugInfoWidth)
	{
		DebugInfoWidth = (int)size.cx;
		SendMessage(hwndInfo, LB_SETHORIZONTALEXTENT, DebugInfoWidth, 0);
	}

	LinesDisplayed++;

	if (LinesDisplayed > MAX_LINES)
	{
		SendMessage(hwndInfo, LB_DELETESTRING, 0, 0);
		LinesDisplayed = MAX_LINES;
	}

	if (LinesDisplayed > LINES_IN_INFO)
	{
		SendMessage(hwndInfo, LB_SETTOPINDEX, LinesDisplayed - LINES_IN_INFO, 0);
	}
}

#else

void DebugDisplayInfo(const char *info)
{
	UNREFERENCED_PARAMETER(info);
}

#endif

/****************************************************************************/

#ifdef WIN32

INT_PTR CALLBACK DebugDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM /* lParam */)
{
	switch (message)
	{
		case WM_INITDIALOG:
			SendDlgItemMessage(hwndDlg, IDC_DEBUGCOMMAND, EM_SETLIMITTEXT, MAX_COMMAND_LEN, 0);
			return TRUE;

		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE)
			{
				hCurrentDialog = NULL;
				hCurrentAccelTable = NULL;
			}
			else
			{
				hCurrentDialog = hWnd;
				hCurrentAccelTable = haccelDebug;
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_ACCELUP:
					if (GetFocus() == GetDlgItem(hWnd, IDC_DEBUGCOMMAND))
					{
						DebugHistoryMove(-1);
					}
					break;

				case ID_ACCELDOWN:
					if (GetFocus() == GetDlgItem(hWnd, IDC_DEBUGCOMMAND))
					{
						DebugHistoryMove(1);
					}
					break;

				case IDC_DEBUGBREAK:
					DebugToggleRun();
					break;

				case IDC_DEBUGEXECUTE:
					DebugExecuteCommand();
					SetFocus(GetDlgItem(hWnd, IDC_DEBUGCOMMAND));
					break;

				case IDC_DEBUGBPS:
					BPSOn = IsDlgItemChecked(hWnd, IDC_DEBUGBPS);
					break;

				case IDC_DEBUGBRK:
					DebugBreakEnable[(int)DebugType::BRK] = IsDlgItemChecked(hWnd, IDC_DEBUGBRK);
					break;

				case IDC_DEBUGOS:
					DebugOS = IsDlgItemChecked(hWnd, IDC_DEBUGOS);
					break;

				case IDC_DEBUGROM:
					DebugROM = IsDlgItemChecked(hWnd, IDC_DEBUGROM);
					break;

				case IDC_DEBUGHOST:
					DebugHost = IsDlgItemChecked(hWnd, IDC_DEBUGHOST);
					break;

				case IDC_DEBUGPARASITE:
					DebugParasite = IsDlgItemChecked(hWnd, IDC_DEBUGPARASITE);
					break;

				case IDC_WATCHDECIMAL:
				case IDC_WATCHENDIAN:
					WatchDecimal = IsDlgItemChecked(hWnd, IDC_WATCHDECIMAL);
					WatchBigEndian = IsDlgItemChecked(hWnd, IDC_WATCHENDIAN);
					DebugUpdateWatches(true);
					break;

				case IDC_DEBUG_VIDEO:
					DebugTraceEnable[(int)DebugType::Video] = IsDlgItemChecked(hWnd, IDC_DEBUG_VIDEO);
					break;

				case IDC_DEBUG_VIDEO_BRK:
					DebugBreakEnable[(int)DebugType::Video] = IsDlgItemChecked(hWnd, IDC_DEBUG_VIDEO_BRK);
					break;

				case IDC_DEBUG_SYSVIA:
					DebugTraceEnable[(int)DebugType::SysVIA] = IsDlgItemChecked(hWnd, IDC_DEBUG_SYSVIA);
					break;

				case IDC_DEBUG_SYSVIA_BRK:
					DebugBreakEnable[(int)DebugType::SysVIA] = IsDlgItemChecked(hWnd, IDC_DEBUG_SYSVIA_BRK);
					break;

				case IDC_DEBUG_USERVIA:
					DebugTraceEnable[(int)DebugType::UserVIA] = IsDlgItemChecked(hWnd, IDC_DEBUG_USERVIA);
					break;

				case IDC_DEBUG_USERVIA_BRK:
					DebugBreakEnable[(int)DebugType::UserVIA] = IsDlgItemChecked(hWnd, IDC_DEBUG_USERVIA_BRK);
					break;

				case IDC_DEBUG_TUBE:
					DebugTraceEnable[(int)DebugType::Tube] = IsDlgItemChecked(hWnd, IDC_DEBUG_TUBE);
					break;

				case IDC_DEBUG_TUBE_BRK:
					DebugBreakEnable[(int)DebugType::Tube] = IsDlgItemChecked(hWnd, IDC_DEBUG_TUBE_BRK);
					break;

				case IDC_DEBUG_SERIAL:
					DebugTraceEnable[(int)DebugType::Serial] = IsDlgItemChecked(hWnd, IDC_DEBUG_SERIAL);
					break;

				case IDC_DEBUG_SERIAL_BRK:
					DebugBreakEnable[(int)DebugType::Serial] = IsDlgItemChecked(hWnd, IDC_DEBUG_SERIAL_BRK);
					break;

				case IDC_DEBUG_ECONET:
					DebugTraceEnable[(int)DebugType::Econet] = IsDlgItemChecked(hWnd, IDC_DEBUG_ECONET);
					break;

				case IDC_DEBUG_ECONET_BRK:
					DebugBreakEnable[(int)DebugType::Econet] = IsDlgItemChecked(hWnd, IDC_DEBUG_ECONET_BRK);
					break;

				case IDC_DEBUG_REMSER:
					DebugTraceEnable[(int)DebugType::RemoteServer] = IsDlgItemChecked(hWnd, IDC_DEBUG_REMSER);
					break;

				case IDC_DEBUG_REMSER_BRK:
					DebugBreakEnable[(int)DebugType::RemoteServer] = IsDlgItemChecked(hWnd, IDC_DEBUG_REMSER_BRK);
					break;

				case IDC_DEBUG_TELETEXT:
					DebugTraceEnable[(int)DebugType::Teletext] = IsDlgItemChecked(hWnd, IDC_DEBUG_TELETEXT);
					break;

				case IDC_DEBUG_TELETEXT_BRK:
					DebugBreakEnable[(int)DebugType::Teletext] = IsDlgItemChecked(hWnd, IDC_DEBUG_TELETEXT_BRK);
					break;

				case IDC_DEBUG_CMOS:
					DebugTraceEnable[(int)DebugType::CMOS] = IsDlgItemChecked(hWnd, IDC_DEBUG_CMOS);
					break;

				case IDC_DEBUG_CMOS_BRK:
					DebugBreakEnable[(int)DebugType::CMOS] = IsDlgItemChecked(hWnd, IDC_DEBUG_CMOS_BRK);
					break;

				case IDCANCEL:
					DebugCloseDialog();
					break;
			}
			break;
	}

	return FALSE;
}

//*******************************************************************

static void DebugToggleRun()
{
	if (DebugSource != DebugType::None)
	{
		// Resume execution
		DebugBreakExecution(DebugType::None);
	}
	else
	{
		// Cause manual break
		DebugBreakExecution(DebugType::Manual);
	}
}

/****************************************************************************/

void DebugBreakExecution(DebugType Type)
{
	DebugSource = Type;

	if (Type == DebugType::None)
	{
		InstCount = 0;
		LastBreakAddr = 0;
		SetDlgItemText(hwndDebug, IDC_DEBUGBREAK, "Break");
	}
	else
	{
		InstCount = 1;
		SetDlgItemText(hwndDebug, IDC_DEBUGBREAK, "Continue");
		LastAddrInBIOS = LastAddrInOS = LastAddrInROM = false;

		DebugUpdateWatches(true);
	}
}

/****************************************************************************/

static const char* GetDebugSourceString()
{
	const char* source = "Unknown";

	switch(DebugSource)
	{
	case DebugType::None:
		break;

	case DebugType::Video:
		source = "Video";
		break;

	case DebugType::UserVIA:
		source = "User VIA";
		break;

	case DebugType::SysVIA:
		source = "System VIA";
		break;

	case DebugType::Tube:
		source = "Tube";
		break;

	case DebugType::Serial:
		source = "Serial";
		break;

	case DebugType::Econet:
		source = "Econet";
		break;

	case DebugType::Teletext:
		source = "Teletext";
		break;

	case DebugType::RemoteServer:
		source = "Remote server";
		break;

	case DebugType::Manual:
		source = "Manual";
		break;

	case DebugType::Breakpoint:
		source = "Breakpoint";
		break;

	case DebugType::CMOS:
		source = "CMOS";
		break;

	case DebugType::BRK:
		source = "BRK instruction";
		break;
	}

	return source;
}

/****************************************************************************/

static void DebugDisplayPreviousAddress(int PrevAddr, bool Host)
{
	if (PrevAddr > 0)
	{
		AddrInfo Info;

		if (DebugLookupAddress(PrevAddr, Host, &Info))
		{
			DebugDisplayInfoF("  Previous PC 0x%04X (%s)", PrevAddr, Info.desc.c_str());
		}
		else
		{
			DebugDisplayInfoF("  Previous PC 0x%04X", PrevAddr);
		}
	}
}

/****************************************************************************/

static void DebugAssertBreak(int Addr, int PrevAddr, bool Host)
{
	AddrInfo Info;

	DebugUpdateWatches(false);
	SetDlgItemText(hwndDebug, IDC_DEBUGBREAK, "Continue");

	if (LastBreakAddr == 0)
	{
		LastBreakAddr = Addr;
	}
	else
	{
		return;
	}

	if (DebugSource == DebugType::Breakpoint)
	{
		for (size_t i = 0; i < Breakpoints.size(); ++i)
		{
			const Breakpoint& bp = Breakpoints[i];

			if (bp.start == Addr)
			{
				if (DebugLookupAddress(Addr, Host, &Info))
				{
					DebugDisplayInfoF("%s break at 0x%04X (Breakpoint '%s' / %s)",
					                  Host ? "Host" : "Parasite",
					                  Addr,
					                  bp.name.c_str(),
					                  Info.desc.c_str());
				}
				else
				{
					DebugDisplayInfoF("%s break at 0x%04X (Breakpoint '%s')",
					                  Host ? "Host" : "Parasite",
					                  Addr,
					                  bp.name.c_str());
				}

				DebugDisplayPreviousAddress(PrevAddr, Host);
				return;
			}
		}
	}

	if (DebugLookupAddress(Addr, Host, &Info))
	{
		DebugDisplayInfoF("%s break at 0x%04X (%s / %s)",
		                  Host ? "Host" : "Parasite",
		                  Addr,
		                  GetDebugSourceString(),
		                  Info.desc.c_str());
	}
	else
	{
		DebugDisplayInfoF("%s break at 0x%04X (%s)",
		                  Host ? "Host" : "Parasite",
		                  Addr,
		                  GetDebugSourceString());
	}

	DebugDisplayPreviousAddress(PrevAddr, Host);
}

/****************************************************************************/

void DebugDisplayTrace(DebugType Type, bool Host, const char *Info)
{
	if (DebugEnabled && ((DebugHost && Host) || (DebugParasite && !Host)))
	{
		if (DebugTraceEnable[(int)Type])
		{
			DebugDisplayInfo(Info);
		}

		if (DebugBreakEnable[(int)Type])
		{
			DebugBreakExecution(Type);
		}
	}
}

#else

void DebugDisplayTrace(DebugType type, bool host, const char *info)
{
	UNREFERENCED_PARAMETER(type);
	UNREFERENCED_PARAMETER(host);
	UNREFERENCED_PARAMETER(info);
}

/****************************************************************************/

void DebugDisplayTraceF(DebugType type, bool host, const char *format, ...)
{
	va_list args;
	va_start(args, format);

	DebugDisplayTraceV(type, host, format, args);

	va_end(args);
}

/****************************************************************************/

void DebugDisplayTraceV(DebugType type, bool host, const char *format, va_list args)
{
	// _vscprintf doesn't count terminating '\0'
	int len = _vscprintf(format, args) + 1;

	char *buffer = (char*)malloc(len * sizeof(char));

	if (buffer != nullptr)
	{
		vsprintf(buffer, format, args);

		DebugDisplayTrace(type, host, buffer);
		free(buffer);
	}
}

#endif

/****************************************************************************/

#ifdef WIN32

static void DebugUpdateWatches(bool UpdateAll)
{
	int Value = 0;

	for (size_t i = 0; i < Watches.size(); ++i)
	{
		Watch& watch = Watches[i];

		switch (watch.type)
		{
			case 'b':
				Value = DebugReadMem(watch.start, watch.host);
				break;

			case 'w':
				if (WatchBigEndian)
				{
					Value = (DebugReadMem(watch.start,     watch.host) << 8) +
					         DebugReadMem(watch.start + 1, watch.host);
				}
				else
				{
					Value = (DebugReadMem(watch.start + 1, watch.host) << 8) +
					         DebugReadMem(watch.start,     watch.host);
				}
				break;

			case 'd':
				if (WatchBigEndian)
				{
					Value = (DebugReadMem(watch.start,     watch.host) << 24) +
					        (DebugReadMem(watch.start + 1, watch.host) << 16) +
					        (DebugReadMem(watch.start + 2, watch.host) << 8) +
					         DebugReadMem(watch.start + 3, watch.host);
				}
				else
				{
					Value = (DebugReadMem(watch.start + 3, watch.host) << 24) +
					        (DebugReadMem(watch.start + 2, watch.host) << 16) +
					        (DebugReadMem(watch.start + 1, watch.host) << 8) +
					         DebugReadMem(watch.start,     watch.host);
				}
				break;
		}

		if (UpdateAll || Value != watch.value)
		{
			watch.value = Value;

			char str[200];

			if (WatchDecimal)
			{
				sprintf(str, "%s%04X %s=%d (%c)", (watch.host ? "" : "p"), watch.start, watch.name.c_str(), watch.value, watch.type);
			}
			else
			{
				switch (watch.type)
				{
					case 'b':
						sprintf(str, "%s%04X %s=$%02X", watch.host ? "" : "p", watch.start, watch.name.c_str(), watch.value);
						break;

					case 'w':
						sprintf(str, "%s%04X %s=$%04X", watch.host ? "" : "p", watch.start, watch.name.c_str(), watch.value);
						break;

					case 'd':
						sprintf(str, "%s%04X %s=$%08X", watch.host ? "" : "p", watch.start, watch.name.c_str(), watch.value);
						break;
				}
			}

			SendMessage(hwndW, LB_DELETESTRING, i, 0);
			SendMessage(hwndW, LB_INSERTSTRING, i, (LPARAM)str);
		}
	}
}

#endif

/****************************************************************************/

bool DebugDisassembler(int Addr,
                       int PrevAddr,
                       int Accumulator,
                       int XReg,
                       int YReg,
                       unsigned char PSR,
                       unsigned char StackReg,
                       bool Host)
{
	#ifdef WIN32

	// Update memory watches. Prevent emulator slowdown by limiting updates
	// to every 100ms, or on timer wrap-around.
	static DWORD LastTickCount = 0;
	const DWORD TickCount = GetTickCount();

	if (TickCount - LastTickCount > 100 || TickCount < LastTickCount)
	{
		LastTickCount = TickCount;
		DebugUpdateWatches(false);
	}

	// If this is the host and we're debugging that and have no further
	// instructions to execute, halt.
	if (Host && DebugHost && DebugSource != DebugType::None && InstCount == 0)
	{
		return false;
	}

	// Don't process further if we're not debugging the parasite either
	if (!Host && !DebugParasite)
	{
		return true;
	}

	if (DebugBreakEnable[(int)DebugType::BRK])
	{
		if (Host)
		{
			if (DebugReadMem(Addr, Host) == 0)
			{
				DebugBreakExecution(DebugType::BRK);
				ProgramCounter++;
			}
		}
	}

	if (Host && StepOver && Addr == ReturnAddress)
	{
		StepOver = false;
		DebugBreakExecution(DebugType::Breakpoint);
	}

	// Check breakpoints

	if (BPSOn && DebugSource != DebugType::Breakpoint)
	{
		// TODO: Allow for host/copro breakpoints
		for (size_t i = 0; i < Breakpoints.size(); i++)
		{
			const Breakpoint& bp = Breakpoints[i];

			if (bp.end == -1)
			{
				if (Addr == bp.start)
				{
					DebugBreakExecution(DebugType::Breakpoint);
				}
			}
			else
			{
				if (Addr >= bp.start && Addr <= bp.end)
				{
					DebugBreakExecution(DebugType::Breakpoint);
				}
			}
		}
	}

	if (DebugSource == DebugType::None)
	{
		return true;
	}

	if (Host)
	{
		if (!DebugOS && Addr >= 0xc000 && Addr <= 0xfbff)
		{
			if (!LastAddrInOS)
			{
				AddrInfo Info;

				if (DebugLookupAddress(Addr, Host, &Info))
				{
					DebugDisplayInfoF("Entered OS (0xC000-0xFBFF) at 0x%04X (%s)", Addr, Info.desc.c_str());
				}
				else
				{
					DebugDisplayInfoF("Entered OS (0xC000-0xFBFF) at 0x%04X", Addr);
				}

				LastAddrInOS = true;
				LastAddrInBIOS = LastAddrInROM = false;
			}

			return true;
		}

		LastAddrInOS = false;

		if (!DebugROM && Addr >= 0x8000 && Addr <= 0xbfff)
		{
			if (!LastAddrInROM)
			{
				RomInfo Info;

				if (ReadRomInfo(ROMSEL, &Info))
				{
					DebugDisplayInfoF("Entered paged ROM bank %d \"%s\" (0x8000-0xBFFF) at 0x%04X", ROMSEL, Info.Title, Addr);
				}
				else
				{
					DebugDisplayInfoF("Entered paged ROM bank %d (0x8000-0xBFFF) at 0x%04X", ROMSEL, Addr);
				}

				LastAddrInROM = true;
				LastAddrInOS = LastAddrInBIOS = false;
			}

			return true;
		}

		LastAddrInROM = false;
	}
	else
	{
		if (TubeType == TubeDevice::AcornZ80 || TubeType == TubeDevice::TorchZ80)
		{
			if (!DebugOS && Addr >= 0xf800 && Addr <= 0xffff)
			{
				if (!LastAddrInBIOS)
				{
					AddrInfo Info;

					if (DebugLookupAddress(Addr, Host, &Info))
					{
						DebugDisplayInfoF("Entered BIOS (0xF800-0xFFFF) at 0x%04X (%s)",
						                  Addr,
						                  Info.desc.c_str());
					}
					else
					{
						DebugDisplayInfoF("Entered BIOS (0xF800-0xFFFF) at 0x%04X",
						                  Addr);
					}

					LastAddrInBIOS = true;
					LastAddrInOS = LastAddrInROM = false;
				}

				return true;
			}

			LastAddrInBIOS = false;
		}
	}

	if (Host && InstCount == 0)
	{
		return false;
	}

	DebugAssertBreak(Addr, PrevAddr, Host);

	char str[150];

	if (Host || (!Host && TubeType == TubeDevice::Acorn65C02))
	{
		int Length = DebugDisassembleInstructionWithCPUStatus(
			Addr, Host, Accumulator, XReg, YReg, StackReg, PSR, str
		);

		if (!Host)
		{
			strcpy(&str[Length], "  Parasite");
		}

		DebugDisplayInfo(str);
	}
	else
	{
		switch (TubeType)
		{
			case TubeDevice::Acorn65C02:
				// Already handled.
				break;

			case TubeDevice::AcornZ80:
			case TubeDevice::TorchZ80: {
				char Str[128];
				Z80Disassemble(Addr, Str);
				DebugDisplayInfo(Str);
				break;
			}

			case TubeDevice::Master512CoPro:
			case TubeDevice::AcornArm:
			case TubeDevice::SprowArm:
				// Not implemented.
				break;

			case TubeDevice::None:
				break;
		}
	}

	// If host debug is enabled then only count host instructions
	// and display all parasite instructions (otherwise we lose them).
	if ((DebugHost && Host) || !DebugHost)
	{
		if (InstCount > 0)
		{
			InstCount--;
		}
	}

	return true;

	#else

	UNREFERENCED_PARAMETER(Addr);
	UNREFERENCED_PARAMETER(PrevAddr);
	UNREFERENCED_PARAMETER(Accumulator);
	UNREFERENCED_PARAMETER(XReg);
	UNREFERENCED_PARAMETER(YReg);
	UNREFERENCED_PARAMETER(PSR);
	UNREFERENCED_PARAMETER(StackReg);
	UNREFERENCED_PARAMETER(Host);

	return true;

	#endif
}

/****************************************************************************/

#ifdef WIN32

static void DebugLookupSWRAddress(AddrInfo* addrInfo)
{
	addrInfo->start = 0x8000;
	addrInfo->end   = 0xbfff;

	RomInfo rom;
	char desc[100];

	const char* ROMType = RomWritable[ROMSEL] ? "Paged ROM" : "Sideways RAM";

	// Try ROM info:
	if (ReadRomInfo(ROMSEL, &rom))
	{
		sprintf(desc, "%s bank %d: %.80s", ROMType, ROMSEL, rom.Title);
	}
	else
	{
		sprintf(desc, "%s bank %d", ROMType, ROMSEL);
	}

	addrInfo->desc = desc;
}

/****************************************************************************/

static bool DebugLookupAddress(int Addr, bool Host, AddrInfo* addrInfo)
{
	if (Host)
	{
		RomInfo rom;

		// Try current ROM's map
		if (!MemoryMaps[ROMSEL].empty())
		{
			for (size_t i = 0; i < MemoryMaps[ROMSEL].size(); i++)
			{
				if (Addr >= MemoryMaps[ROMSEL][i].start && Addr <= MemoryMaps[ROMSEL][i].end)
				{
					addrInfo->start = MemoryMaps[ROMSEL][i].start;
					addrInfo->end   = MemoryMaps[ROMSEL][i].end;

					char desc[100];
					sprintf(desc, "%.99s", ReadRomInfo(ROMSEL, &rom) ? rom.Title : "ROM");
					addrInfo->desc = desc;

					return true;
				}
			}
		}

		// Try OS map
		if (!MemoryMaps[16].empty())
		{
			for (size_t i = 0; i < MemoryMaps[16].size(); i++)
			{
				if (Addr >= MemoryMaps[16][i].start && Addr <= MemoryMaps[16][i].end)
				{
					*addrInfo = MemoryMaps[16][i];

					return true;
				}
			}
		}

		if (MachineType == Model::B)
		{
			if (Addr >= 0x8000 && Addr < 0xc000)
			{
				DebugLookupSWRAddress(addrInfo);
				return true;
			}
		}
		else if (MachineType == Model::IntegraB)
		{
			if (ShEn && !MemSel && Addr >= 0x3000 && Addr <= 0x7fff)
			{
				addrInfo->start = 0x3000;
				addrInfo->end   = 0x7fff;
				addrInfo->desc  = "Shadow RAM";
				return true;
			}

			if (PrvEn)
			{
				if (Prvs8 && Addr >= 0x8000 && Addr <= 0x83ff)
				{
					addrInfo->start = 0x8000;
					addrInfo->end   = 0x83ff;
					addrInfo->desc  = "1K private area";
					return true;
				}
				else if (Prvs4 && Addr >= 0x8000 && Addr <= 0x8fff)
				{
					addrInfo->start = 0x8400;
					addrInfo->end   = 0x8fff;
					addrInfo->desc  = "4K private area";
					return true;
				}
				else if (Prvs1 && Addr >= 0x9000 && Addr <= 0xafff)
				{
					addrInfo->start = 0x9000;
					addrInfo->end   = 0xafff;
					addrInfo->desc  = "8K private area";
					return true;
				}
			}

			if (Addr >= 0x8000 && Addr < 0xc000)
			{
				DebugLookupSWRAddress(addrInfo);
				return true;
			}
		}
		else if (MachineType == Model::BPlus)
		{
			if (Addr >= 0x3000 && Addr <= 0x7fff)
			{
				addrInfo->start = 0x3000;
				addrInfo->end   = 0x7fff;

				if (Sh_Display && PrePC >= 0xc000 && PrePC <= 0xdfff)
				{
					addrInfo->desc = "Shadow RAM (PC in VDU driver)";
					return true;
				}
				else if (Sh_Display && MemSel && PrePC >= 0xa000 && PrePC <= 0xafff)
				{
					addrInfo->start = 0x3000;
					addrInfo->end   = 0x7fff;
					addrInfo->desc  = "Shadow RAM (PC in upper 4K of ROM and shadow selected)";
					return true;
				}
			}
			else if (Addr >= 0x8000 && Addr <= 0xafff && MemSel)
			{
				addrInfo->start = 0x8000;
				addrInfo->end   = 0xafff;
				addrInfo->desc  = "Paged RAM";
				return true;
			}
			else if (Addr >= 0x8000 && Addr < 0xc000)
			{
				DebugLookupSWRAddress(addrInfo);
				return true;
			}
		}
		else if (MachineType == Model::Master128 || MachineType == Model::MasterET)
		{
			// Master cartridge (not implemented in BeebEm yet)
			if ((ACCCON & 0x20) && Addr >= 0xfc00 && Addr <= 0xfdff)
			{
				addrInfo->start = 0xfc00;
				addrInfo->end   = 0xfdff;
				addrInfo->desc  = "Cartridge (ACCCON bit 5 set)";
				return true;
			}

			// Master private and shadow RAM.
			if ((ACCCON & 0x08) && Addr >= 0xc000 && Addr <= 0xdfff)
			{
				addrInfo->start = 0xc000;
				addrInfo->end   = 0xdfff;
				addrInfo->desc  = "8K Private RAM (ACCCON bit 3 set)";
				return true;
			}

			if ((ACCCON & 0x04) && Addr >= 0x3000 && Addr <= 0x7fff)
			{
				addrInfo->start = 0x3000;
				addrInfo->end   = 0x7fff;
				addrInfo->desc  = "Shadow RAM (ACCCON bit 2 set)";
				return true;
			}

			if ((ACCCON & 0x02) && PrePC >= 0xC000 && PrePC <= 0xDFFF && Addr >= 0x3000 && Addr <= 0x7FFF)
			{
				addrInfo->start = 0x3000;
				addrInfo->end   = 0x7fff;
				addrInfo->desc  = "Shadow RAM (ACCCON bit 1 set and PC in VDU driver)";
				return true;
			}

			// Master private RAM.
			if ((PagedRomReg & 0x80) && Addr >= 0x8000 && Addr <= 0x8fff)
			{
				addrInfo->start = 0x8000;
				addrInfo->end   = 0x8fff;
				addrInfo->desc  = "4K Private RAM (ROMSEL bit 7 set)";
				return true;
			}

			if (Addr >= 0x8000 && Addr < 0xc000)
			{
				DebugLookupSWRAddress(addrInfo);
				return true;
			}
		}
	}

	return false;
}

/****************************************************************************/

static void DebugExecuteCommand()
{
	char command[MAX_COMMAND_LEN + 1];
	GetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, command, MAX_COMMAND_LEN);
	DebugParseCommand(command);
}

#endif

/****************************************************************************/

void DebugInitMemoryMaps()
{
	for (size_t i = 0; i < _countof(MemoryMaps); i++)
	{
		MemoryMaps[i].clear();
	}
}

/****************************************************************************/

bool DebugLoadMemoryMap(const char* filename, int bank)
{
	if (bank < 0 || bank > 16)
	{
		return false;
	}

	MemoryMap* map = &MemoryMaps[bank];

	FILE *infile = fopen(filename, "r");

	if (infile != nullptr)
	{
		map->clear();

		char line[1024];

		while (fgets(line, _countof(line), infile) != nullptr)
		{
			DebugChompString(line);
			char *buf = line;

			while(buf[0] == ' ' || buf[0] == '\t' || buf[0] == '\r' || buf[0] == '\n')
				buf++;

			if (buf[0] == ';' || buf[0] == '\0') // Skip comments and empty lines
				continue;

			AddrInfo entry;

			char desc[256];
			memset(desc, 0, sizeof(desc));

			int result = sscanf(buf, "%x %x %99c", (unsigned int*)&entry.start, (unsigned int*)&entry.end, desc);

			if (result >= 2 && strlen(desc) > 0)
			{
				entry.desc = desc;
				map->push_back(entry);
			}
			else
			{
				mainWin->Report(MessageType::Error, "Invalid memory map format!");

				map->clear();

				fclose(infile);
				return false;
			}
		}

		fclose(infile);
	}
	else
	{
		return false;
	}

	return true;
}

/****************************************************************************/

// Parse a string containing Swift format labels, used by BeebAsm

static bool DebugParseSwiftLabels(const std::string& Line, bool Host)
{
	bool Valid = true;
	const int Index = (int)Host;

	std::size_t i = 2;

	while (Line[i] == '\'')
	{
		std::size_t end = Line.find('\'', i + 1);

		if (end == std::string::npos)
		{
			Valid = false;
			break;
		}

		std::string symbol = Line.substr(i + 1, end - (i + 1));
		i = end + 1;

		if (Line[i] == ':')
		{
			end = Line.find('L', i + 1);

			if (end == std::string::npos)
			{
				Valid = false;
				break;
			}

			std::string address = Line.substr(i + 1, end - (i + 1));
			i = end + 1;

			Label label;
			label.name = symbol;

			if (!ParseNumber(address, &label.addr))
			{
				Valid = false;
				break;
			}

			if (label.addr < 0 || label.addr > 0xFFFF)
			{
				Valid = false;
				break;
			}

			Labels[Index].push_back(label);
		}

		if (Line[i] == ',')
		{
			i++;
		}
		else if (Line[i] == '}')
		{
			break;
		}
		else
		{
			Valid = false;
			break;
		}
	}

	return Valid;
}

/****************************************************************************/

bool DebugLoadLabels(const char* FileName, bool Host)
{
	std::ifstream Input(FileName);

	if (!Input)
	{
		DebugDisplayInfoF("Error: Failed to open labels from %s", FileName);
		return false;
	}

	Labels[(int)Host].clear();

	bool Valid = true;

	std::string Line;

	while (std::getline(Input, Line))
	{
		// Skip blank lines and comments
		if (Line.empty() || Line[0] == '#' || Line[0] == ';')
		{
			continue;
		}

		if (Line.length() > 4 && Line[0] == '[' && Line[1] == '{')
		{
			if (!DebugParseSwiftLabels(Line, Host))
			{
				DebugDisplayInfoF("Failed to load symbols file:\n  %s", FileName);

				Valid = false;
				break;
			}
		}
		else
		{
			std::vector<std::string> Tokens;

			ParseLine(Line, Tokens);

			unsigned long Addr;

			if (Tokens.size() != 3)
			{
				DebugDisplayInfoF("Error: Invalid labels format in symbols file:\n  %s", FileName);

				Valid = false;
				break;
			}

			if (Valid && Tokens.size() >= 2)
			{
				if (!ParseHexNumber(Tokens[1], &Addr))
				{
					DebugDisplayInfoF("Invalid address %s in symbols file:\n  %s", Tokens[1].c_str(), FileName);

					Valid = false;
					break;
				}

				if (Addr > 0xFFFF)
				{
					DebugDisplayInfoF("Invalid address %X in symbols file:\n  %s", Addr, FileName);

					Valid = false;
					break;
				}
			}

			if (Valid)
			{
				Labels[(int)Host].emplace_back(Tokens[2], Addr);
			}
		}
	}

	if (Valid)
	{
		DebugDisplayInfoF("Loaded %u labels from %s", Labels[(int)Host].size(), FileName);
	}
	else
	{
		Labels[(int)Host].clear();
	}

	return Valid;
}

/****************************************************************************/

#ifdef WIN32

void DebugRunScript(const char* FileName)
{
	std::ifstream Input(FileName);

	if (!Input)
	{
		DebugDisplayInfoF("Failed to read script file:\n  %s", FileName);
		return;
	}

	DebugDisplayInfoF("Running script %s", FileName);

	std::string Line;

	while (std::getline(Input, Line))
	{
		DebugParseCommand(Line.c_str());
	}
}

#endif

/****************************************************************************/

static void DebugChompString(char *str)
{
	const size_t length = strlen(str);

	if (length > 0)
	{
		size_t end = length - 1;

		while (end > 0 && (str[end] == '\r' || str[end] == '\n' || str[end] == ' ' || str[end] == '\t'))
		{
			str[end] = '\0';
			end--;
		}
	}
}

/****************************************************************************/

#ifdef WIN32

static void DebugHistoryAdd(const char *command)
{
	// Do nothing if this is the same as the last command

	if (DebugHistory.size() == 0 ||
	    (DebugHistory.size() > 0 && StrCaseCmp(DebugHistory[0].c_str(), command) != 0))
	{
		// Otherwise insert command string at index 0.
		DebugHistory.push_front(command);

		if (DebugHistory.size() > MAX_HISTORY)
		{
			DebugHistory.pop_back();
		}
	}

	DebugHistoryIndex = -1;
}

/****************************************************************************/

static void DebugHistoryMove(int delta)
{
	int newIndex = DebugHistoryIndex - delta;

	if (newIndex < 0)
	{
		DebugHistoryIndex = -1;
		SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");
		return;
	}

	const int HistorySize = (int)DebugHistory.size();

	if (newIndex >= HistorySize)
	{
		if (HistorySize > 0)
		{
			newIndex = HistorySize - 1;
		}
		else
		{
			return;
		}
	}

	DebugHistoryIndex = newIndex;
	DebugSetCommandString(DebugHistory[DebugHistoryIndex].c_str());
}

/****************************************************************************/

static void DebugSetCommandString(const char* str)
{
	if (DebugHistoryIndex == -1 &&
	    DebugHistory.size() > 0 &&
	    StrCaseCmp(DebugHistory[0].c_str(), str) == 0)
	{
		// The string we're about to set is the same as the top history one,
		// so use history to set it. This is just a nicety to make the up
		// key work as expected when commands such as 'next' and 'peek'
		// have automatically filled in the command box.
		DebugHistoryMove(-1);
	}
	else
	{
		SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, str);
		SendDlgItemMessage(hwndDebug, IDC_DEBUGCOMMAND, EM_SETSEL, strlen(str), strlen(str));
	}
}

/****************************************************************************/

static void DebugParseCommand(const char *command)
{
	while (isspace(command[0]))
	{
		command++;
	}

	if (command[0] == '\0' || command[0] == '/' || command[0] == ';' || command[0] == '#')
	{
		return;
	}

	DebugHistoryAdd(command);

	char info[MAX_PATH + 100];
	info[0] = '\0';

	const char *p = command;

	std::string cmd;

	while (!isspace(*p) && *p != '\0')
	{
		cmd.push_back(*p);
		p++;
	}

	while (isspace(*p) && *p != '\0')
	{
		p++;
	}

	SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");

	for (size_t i = 0; i < _countof(DebugCmdTable); i++)
	{
		if (StrCaseCmp(DebugCmdTable[i].name, cmd.c_str()) == 0)
		{
			if (!DebugCmdTable[i].handler(p))
			{
				DebugCmdHelp(cmd.c_str());
			}

			return;
		}
	}

	DebugDisplayInfoF("Invalid command %s - try 'help'", command);
}

/**************************************************************
 * Start of debugger command handlers                         *
 **************************************************************/

static bool DebugCmdEcho(const char* args)
{
	DebugDisplayInfo(args);
	return true;
}

/****************************************************************************/

static bool DebugCmdGoto(const char* args)
{
	std::vector<std::string> Args;

	ParseLine(args, Args);

	if (Args.empty())
	{
		return false;
	}

	bool Host = true;
	unsigned long Address = 0;
	size_t Index = 0;

	// Host / parasite

	if (StrCaseCmp(Args[Index].c_str(), "p") == 0)
	{
		Host = false;

		Index++;
	}

	// Address

	if (Index == Args.size())
	{
		return false;
	}

	if (!ParseAddressOrLabel(Args[Index].c_str(), Host, &Address, nullptr))
	{
		return true;
	}

	if (Host)
	{
		ProgramCounter = (int)Address;
	}
	else
	{
		switch (TubeType)
		{
			case TubeDevice::Acorn65C02:
				TubeProgramCounter = (int)Address;
				break;

			case TubeDevice::Master512CoPro:
			case TubeDevice::AcornZ80:
			case TubeDevice::TorchZ80:
			case TubeDevice::AcornArm:
			case TubeDevice::SprowArm:
			case TubeDevice::None:
			default:
				DebugDisplayInfo("Not implemented for this coprocessor");
				return true;
		}
	}

	const int AddressWidth = DebugGetAddressBits(Host) / 4;

	DebugDisplayInfoF("Next %s instruction address 0x%0*X",
	                  Host ? "host" : "parasite", AddressWidth, Address);

	return true;
}

/****************************************************************************/

static bool DebugCmdFile(const char* args)
{
	bool Host = true;
	bool Write = false;
	int StartAddress = 0;
	int Count = 0x10000;
	char FileName[MAX_PATH];
	ZeroMemory(FileName, MAX_PATH);
	int Arg = 0;
	const char* prevArgs;

	while (args[0] != '\0')
	{
		prevArgs = args;

		std::string Token;
		args = ParseToken(args, Token);

		if (Token.empty())
		{
			return false;
		}

		if (Arg == 0)
		{
			if (StrCaseCmp(Token.c_str(), "r") == 0)
			{
				Write = false;
			}
			else if (StrCaseCmp(Token.c_str(), "w") == 0)
			{
				Write = true;
			}
			else
			{
				return false;
			}

			Arg++;
		}
		else if (Arg == 1)
		{
			if (StrCaseCmp(Token.c_str(), "p") == 0)
			{
				Host = false;
				Arg++;
			}
			else if (ParseHexNumber(Token, &StartAddress))
			{
				Arg = 3;
			}
			else
			{
				return false;
			}
		}
		else if (Arg == 2)
		{
			if (!ParseHexNumber(Token, &StartAddress))
			{
				return false;
			}

			Arg++;
		}
		else if (Arg == 3)
		{
			if (!ParseNumber(Token, &Count))
			{
				if (strlen(prevArgs) < MAX_PATH)
				{
					strcpy(FileName, prevArgs);
				}
			}

			Arg++;
		}
		else if (Arg == 4)
		{
			if (strlen(prevArgs) < MAX_PATH)
			{
				strcpy(FileName, prevArgs);
			}
		}
	}

	if (Arg < 3)
	{
		return false;
	}

	const int MaxAddress = DebugGetMaxAddress(Host);

	if (StartAddress > MaxAddress)
	{
		DebugDisplayInfoF("Invalid start address: %x (max %x)", StartAddress, MaxAddress);
		return true;
	}

	if (FileName[0] == '\0')
	{
		const char* Filter = "Memory Dump Files (*.dat)\0*.dat\0" "All Files (*.*)\0*.*\0";

		FileDialog Dialog(hwndDebug, FileName, MAX_PATH, nullptr, Filter);

		if (Write)
		{
			if (!Dialog.Save())
			{
				return true;
			}

			// Add a file extension if the user did not specify one
			if (strchr(FileName, '.') == nullptr)
			{
				strcat(FileName, ".dat");
			}
		}
		else
		{
			if (!Dialog.Open())
			{
				return true;
			}
		}
	}

	if (FileName[0] != '\0')
	{
		const size_t BUFFER_SIZE = 1024;
		unsigned char Buffer[BUFFER_SIZE];

		if (!Write)
		{
			FILE *pFile = fopen(FileName, "rb");

			if (pFile == nullptr)
			{
				DebugDisplayInfoF("Failed to open file: %s", FileName);
				return true;
			}

			if (Count > MaxAddress + 1)
			{
				Count = MaxAddress + 1;
			}

			int BytesRemaining = Count;
			int Address = StartAddress;

			while (BytesRemaining > 0)
			{
				const int BytesToRead = BytesRemaining > BUFFER_SIZE ? BUFFER_SIZE : BytesRemaining;

				int BytesRead = (int)fread(Buffer, 1, BytesToRead, pFile);

				if (BytesRead != BytesToRead)
				{
					DebugDisplayInfoF("Failed to read file: %s", FileName);

					fclose(pFile);
					return true;
				}

				for (int i = 0; i < BytesRead; i++)
				{
					BeebWriteMem(Address, Buffer[i]);

					Address++;
				}

				BytesRemaining -= BytesRead;
			}

			fclose(pFile);

			DebugDisplayInfoF("Read %d bytes from %s to address 0x%04X", Count, FileName, StartAddress);

			DebugUpdateWatches(true);

			return true;
		}
		else
		{
			FILE *pFile = fopen(FileName, "wb");

			if (pFile == nullptr)
			{
				DebugDisplayInfoF("Failed to open file: %s", FileName);
				return true;
			}

			int Address = StartAddress;
			int EndAddress = StartAddress + Count;

			if (EndAddress > MaxAddress + 1)
			{
				EndAddress = MaxAddress + 1;
			}

			int BytesRemaining = EndAddress - StartAddress;

			while (BytesRemaining > 0)
			{
				const int BytesToRead = BytesRemaining > BUFFER_SIZE ? BUFFER_SIZE : BytesRemaining;

				for (int i = 0; i < BytesToRead; i++)
				{
					Buffer[i] = DebugReadMem(Address, Host);

					Address++;
				}

				int BytesWritten = (int)fwrite(Buffer, 1, BytesToRead, pFile);

				if (BytesWritten != BytesToRead)
				{
					DebugDisplayInfoF("Failed to write to file: %s", FileName);

					fclose(pFile);
					return true;
				}

				BytesRemaining -= BytesWritten;
			}

			fclose(pFile);

			DebugDisplayInfoF("Wrote %u bytes from address 0x%04X to %s", Address - StartAddress, StartAddress, FileName);

			return true;
		}
	}

	return false;
}

/****************************************************************************/

static bool DebugCmdPoke(const char* args)
{
	std::vector<std::string> Args;

	ParseLine(args, Args);

	if (Args.empty())
	{
		return false;
	}

	unsigned long StartAddress = 0;
	bool Host = true;
	size_t Index = 0;

	// Host / parasite

	if (StrCaseCmp(Args[Index].c_str(), "p") == 0)
	{
		Host = false;

		Index++;
	}

	// Address

	if (Index == Args.size())
	{
		return false;
	}

	if (!ParseAddressOrLabel(Args[Index].c_str(), Host, &StartAddress, nullptr))
	{
		return true;
	}

	Index++;

	// Data bytes

	if (Index == Args.size())
	{
		return false;
	}

	unsigned long Address = StartAddress;

	while (Index < Args.size())
	{
		unsigned long Data;

		bool Valid = ParseHexNumber(Args[Index], &Data);

		if (!Valid || Data > 0xFF)
		{
			DebugDisplayInfoF("Invalid data value: %s", Args[Index].c_str());
			return true;
		}

		DebugWriteMem(Address, Host, (unsigned char)Data);

		Address++;
		Index++;
	}

	DebugUpdateWatches(true);

	DebugDisplayInfoF("Changed %d bytes starting at 0x%04X", Address - StartAddress, StartAddress);

	return true;
}

/****************************************************************************/

static bool DebugCmdSave(const char* args)
{
	int count = 0;
	char* info = NULL;
	int infoSize = 0;
	char filename[MAX_PATH];
	ZeroMemory(filename, MAX_PATH);

	int result = sscanf(args, "%u %259c", &count, filename);

	if (result < 1) {
		sscanf(args, "%259c", filename);
	}

	if (filename[0] == '\0')
	{
		const char* filter = "Text Files (*.txt)\0*.txt\0";

		FileDialog Dialog(hwndDebug, filename, MAX_PATH, nullptr, filter);

		if (!Dialog.Save())
		{
			return true;
		}

		// Add a file extension if the user did not specify one
		if (strchr(filename, '.') == nullptr)
		{
			strcat(filename, ".txt");
		}
	}

	if (filename[0] != '\0')
	{
		if (count <= 0 || count > LinesDisplayed)
		{
			count = LinesDisplayed;
		}

		FILE *fd = fopen(filename, "w");

		if (fd != nullptr)
		{
			for (int i = LinesDisplayed - count; i < LinesDisplayed; ++i)
			{
				int len = (int)(SendMessage(hwndInfo, LB_GETTEXTLEN, i, NULL) + 1) * sizeof(TCHAR);

				if (len > infoSize)
				{
					infoSize = len;
					info = (char*)realloc(info, len);
				}

				if (info != nullptr)
				{
					SendMessage(hwndInfo, LB_GETTEXT, i, (LPARAM)info);
					fprintf(fd, "%s\n", info);
				}
				else
				{
					DebugDisplayInfoF("Allocation failure while writing to %s", filename);
					fclose(fd);
					return true;
				}
			}
			fclose(fd);
			free(info);
			DebugDisplayInfoF("Wrote %d lines to: %s", count, filename);
		}
		else
		{
			DebugDisplayInfoF("Failed open for write: %s", filename);
		}

		return true;
	}

	return false;
}

/****************************************************************************/

static bool DebugCmdState(const char* args)
{
	std::vector<std::string> Args;

	ParseLine(args, Args);

	if (Args.size() != 1)
	{
		return false;
	}

	switch (tolower(Args[0][0]))
	{
		case 'v': // Video state
			DebugVideoState();
			break;

		case 'u': // User via state
			DebugUserVIAState();
			break;

		case 's': // Sys via state
			DebugSysVIAState();
			break;

		case 'e': // Serial ACIA / ULA state
			DebugSerialState();
			break;

		case 'n': // Econet state
			DebugEconetState();
			break;

		case 't': // Tube state
			DebugTubeState();
			break;

		case 'm': // Memory state
			DebugMemoryState();
			break;

		case 'r': // ROM state
			DebugDisplayInfo("ROMs by priority:");
			for (int i = 15; i >= 0; i--)
			{
				RomInfo rom;

				char flags[50];
				flags[0] = '\0';

				if (ReadRomInfo(i, &rom))
				{
					if(RomWritable[i])
						strcat(flags, "Writable, ");
					if(rom.Flags & RomLanguage)
						strcat(flags, "Language, ");
					if(rom.Flags & RomService)
						strcat(flags, "Service, ");
					if(rom.Flags & RomRelocate)
						strcat(flags, "Relocate, ");
					if(rom.Flags & RomSoftKey)
						strcat(flags, "SoftKey, ");
					flags[strlen(flags) - 2] = '\0';
					DebugDisplayInfoF("Bank %d: %s %s",i,rom.Title,(PagedRomReg == i ? "(Paged in)" : ""));
					if(strlen(rom.VersionStr) > 0)
						DebugDisplayInfoF("         Version: 0x%02X (%s)",rom.Version, rom.VersionStr);
					else
						DebugDisplayInfoF("         Version: 0x%02X",rom.Version);
					DebugDisplayInfoF("       Copyright: %s",rom.Copyright);
					DebugDisplayInfoF("           Flags: %s",flags);
					DebugDisplayInfoF("   Language Addr: 0x%04X",rom.LanguageAddr);
					DebugDisplayInfoF("    Service Addr: 0x%04X",rom.ServiceAddr);
					DebugDisplayInfoF("  Workspace Addr: 0x%04X",rom.WorkspaceAddr);
					DebugDisplayInfoF(" Relocation Addr: 0x%08X",rom.RelocationAddr);
					DebugDisplayInfo("");
				}
			}
			break;

		default:
			return false;
	}

	return true;
}

/****************************************************************************/

static bool DebugCmdCode(const char* args)
{
	std::vector<std::string> Args;

	ParseLine(args, Args);

	bool Host = true;
	unsigned long Address = 0;
	int Count = LINES_IN_INFO;
	size_t Index = 0;

	// Host / parasite

	if (Index < Args.size())
	{
		if (StrCaseCmp(Args[Index].c_str(), "p") == 0)
		{
			Host = false;

			Index++;
		}
	}

	// Address

	if (Index < Args.size())
	{
		if (!ParseAddressOrLabel(Args[Index].c_str(), Host, &Address, nullptr))
		{
			return true;
		}

		DisAddress = (int)Address;

		Index++;
	}

	// Count

	if (Index < Args.size())
	{
		if (!ParseNumber(Args[Index], &Count))
		{
			DebugDisplayInfoF("Invalid count: %s", Args[Index].c_str());
			return true;
		}

		if (Count < 0)
		{
			Count = 0;
		}
	}

	const int MaxAddress = DebugGetMaxAddress(Host);

	DisAddress += DebugDisassembleCommand(DisAddress, Count, Host);

	if (DisAddress > MaxAddress)
	{
		DisAddress = 0;
	}

	DebugSetCommandString(Host ? "code" : "code p");

	return true;
}

/****************************************************************************/

static bool DebugCmdPeek(const char* args)
{
	int Count = 256;
	bool Host = true;

	std::vector<std::string> Args;

	ParseLine(args, Args);

	size_t Index = 0;

	// Host / parasite

	if (Index < Args.size())
	{
		if (StrCaseCmp(Args[Index].c_str(), "p") == 0)
		{
			Host = false;

			Index++;
		}
	}

	// Address

	if (Index < Args.size())
	{
		unsigned long Address;

		if (!ParseAddressOrLabel(Args[Index].c_str(), Host, &Address, nullptr))
		{
			return true;
		}

		DumpAddress = Address;

		Index++;
	}

	// Count

	if (Index < Args.size())
	{
		if (!ParseNumber(Args[Index], &Count))
		{
			DebugDisplayInfoF("Invalid count: %s", Args[Index].c_str());
			return true;
		}

		if (Count < 0)
		{
			Count = 0;
		}
	}

	DebugMemoryDump(DumpAddress, Count, Host);

	DumpAddress += Count;

	if (DumpAddress > DebugGetMaxAddress(Host))
	{
		DumpAddress = 0;
	}

	DebugSetCommandString(Host ? "peek" : "peek p");

	return true;
}

/****************************************************************************/

static bool DebugCmdNext(const char* args)
{
	std::vector<std::string> Args;

	ParseLine(args, Args);

	if (Args.size() > 1)
	{
		return false;
	}

	int Count = 1;

	if (Args.size() == 1)
	{
		if (!ParseNumber(Args[0], &Count))
		{
			DebugDisplayInfoF("Invalid count: %s", Args[0].c_str());
			return true;
		}

		if (Count < 1)
		{
			DebugDisplayInfoF("Invalid count: %s", Args[0].c_str());
			return true;
		}
	}

	if (Count > MAX_LINES)
	{
		Count = MAX_LINES;
	}

	InstCount = Count;

	DebugSetCommandString("next");

	return true;
}

/****************************************************************************/

// TODO: currently host only, enable for Tube debugging

static bool DebugCmdOver(const char* args)
{
	// If current instruction is JSR, run to the following instruction,
	// otherwise do a regular 'Next'
	int Instruction = DebugReadMem(PrePC, true);

	if (Instruction == 0x20)
	{
		StepOver = true;
		InstCount = 1;

		const InstInfo* optable = GetOpcodeTable(true);

		ReturnAddress = PrePC + optable[Instruction].bytes;

		// Resume execution
		DebugBreakExecution(DebugType::None);
	}
	else
	{
		DebugCmdNext(args);
	}

	DebugSetCommandString("over");

	return true;
}

/****************************************************************************/

static bool DebugCmdSet(const char* args)
{
	std::vector<std::string> Args;

	ParseLine(args, Args);

	if (Args.size() != 2)
	{
		return false;
	}

	const char* pszFlag = Args[0].c_str();
	const char* pszState = Args[1].c_str();

	bool Checked = StrCaseCmp(pszState, "on") == 0;

	int DlgItemID = 0;

	if (StrCaseCmp(pszFlag, "host") == 0)
	{
		DlgItemID = IDC_DEBUGHOST;
		DebugHost = Checked;
	}
	else if (StrCaseCmp(pszFlag, "parasite") == 0)
	{
		DlgItemID = IDC_DEBUGPARASITE;
		DebugParasite = Checked;
	}
	else if (StrCaseCmp(pszFlag, "rom") == 0)
	{
		DlgItemID = IDC_DEBUGROM;
		DebugROM = Checked;
	}
	else if (StrCaseCmp(pszFlag, "os") == 0)
	{
		DlgItemID = IDC_DEBUGOS;
		DebugOS = Checked;
	}
	else if (StrCaseCmp(pszFlag, "endian") == 0)
	{
		DlgItemID = IDC_WATCHENDIAN;
		WatchBigEndian = Checked;
		DebugUpdateWatches(true);
	}
	else if (StrCaseCmp(pszFlag, "breakpoints") == 0)
	{
		DlgItemID = IDC_DEBUGBPS;
		BPSOn = Checked;
	}
	else if (StrCaseCmp(pszFlag, "decimal") == 0)
	{
		DlgItemID = IDC_WATCHDECIMAL;
		WatchDecimal = Checked;
	}
	else if (StrCaseCmp(pszFlag, "brk") == 0)
	{
		DlgItemID = IDC_DEBUGBRK;
		DebugBreakEnable[(int)DebugType::BRK] = Checked;
	}
	else
	{
		return false;
	}

	SetDlgItemChecked(hwndDebug, DlgItemID, Checked);

	return true;
}

/****************************************************************************/

static bool DebugCmdBreakContinue(const char* /* args */)
{
	DebugToggleRun();
	DebugSetCommandString(".");
	return true;
}

/****************************************************************************/

static bool DebugCmdHelp(const char* args)
{
	int addr;
	int li = 0;
	char aliasInfo[300];
	aliasInfo[0] = 0;

	if (args[0] == '\0')
	{
		DebugDisplayInfo("");
		DebugDisplayInfo("BeebEm debugger help");
		DebugDisplayInfo("");
		DebugDisplayInfo("Parameters in [] are optional. 'p' can be specified in some commands");
		DebugDisplayInfo("to specify parasite processor. Words preceded with a . will be");
		DebugDisplayInfo("interpreted as labels and may be used in place of addresses.");

		// Display help for basic commands:
		for (int i = 0; i < _countof(DebugCmdTable); i++)
		{
			if (DebugCmdTable[i].help[0] != '\0')
			{
				DebugDisplayInfo("");
				DebugDisplayInfoF("  %s",DebugCmdTable[i].name);

				if (DebugCmdTable[i].argdesc[0] != '\0')
				{
					DebugDisplayInfoF("  %s",DebugCmdTable[i].argdesc);
				}

				DebugDisplayInfoF("    %s",DebugCmdTable[i].help);
			}
		}

		// Display help for aliases

		DebugDisplayInfo("");
		DebugDisplayInfo("Command aliases:");

		for (int i = 0; i < _countof(DebugCmdTable); i++)
		{
			if (strlen(DebugCmdTable[i].help) > 0)
			{
				if(strlen(aliasInfo) > 0)
				{
					aliasInfo[strlen(aliasInfo) - 2] = 0;
					DebugDisplayInfoF("%8s: %s",DebugCmdTable[li].name, aliasInfo);
				}
				aliasInfo[0] = 0;
				li = i;
			}
			else if (strlen(DebugCmdTable[i].help) == 0 &&
			         strlen(DebugCmdTable[i].argdesc) == 0 &&
			         DebugCmdTable[li].handler == DebugCmdTable[i].handler)
			{
				strcat(aliasInfo, DebugCmdTable[i].name);
				strcat(aliasInfo, ", ");
			}
		}

		if (aliasInfo[0] != 0)
		{
			aliasInfo[strlen(aliasInfo) - 2] = 0;
			DebugDisplayInfoF("%8s: %s",DebugCmdTable[li].name, aliasInfo);
		}

		DebugDisplayInfo("");
	}
	else
	{
		// Display help for specific command/alias
		for (int i = 0; i < _countof(DebugCmdTable); i++)
		{
			// Remember the last index with args and help so we can support aliases.
			if (strlen(DebugCmdTable[i].help) > 0 && strlen(DebugCmdTable[i].argdesc) > 0)
			{
				li = i;
			}

			if (StrCaseCmp(args, DebugCmdTable[i].name) == 0)
			{
				if (strlen(DebugCmdTable[i].help) == 0 &&
				    strlen(DebugCmdTable[i].argdesc) == 0 &&
				    DebugCmdTable[li].handler == DebugCmdTable[i].handler)
				{
					// This is an alias:
					DebugDisplayInfoF("%s - alias of %s",DebugCmdTable[i].name,DebugCmdTable[li].name);
					DebugCmdHelp(DebugCmdTable[li].name);
				}
				else
				{
					DebugDisplayInfoF("%s - %s",DebugCmdTable[i].name,DebugCmdTable[i].help);
					DebugDisplayInfoF("  Usage: %s %s",DebugCmdTable[i].name,DebugCmdTable[i].argdesc);
				}

				return true;
			}
		}

		// Display help for address
		if (sscanf(args, "%x", &addr) == 1)
		{
			AddrInfo Info;

			if (DebugLookupAddress(addr, true, &Info))
			{
				DebugDisplayInfoF("0x%04X: %s (0x%04X-0x%04X)", addr, Info.desc.c_str(), Info.start, Info.end);
			}
			else
			{
				DebugDisplayInfoF("0x%04X: No description", addr);
			}
		}
		else
		{
			DebugDisplayInfoF("Help: Command %s was not recognised.", args);
		}
	}

	return true;
}

/****************************************************************************/

static bool DebugCmdScript(const char* args)
{
	char filename[MAX_PATH];
	memset(filename, 0, sizeof(filename));

	strncpy(filename, args, sizeof(filename) - 1);

	if (filename[0] == '\0')
	{
		const char* filter = "Debugger Script Files (*.txt)\0*.txt\0" "All Files (*.*)\0*.*\0";

		FileDialog Dialog(hwndDebug, filename, MAX_PATH, nullptr, filter);

		if (!Dialog.Open())
		{
			return true;
		}
	}

	if (filename[0] != '\0')
	{
		DebugRunScript(filename);
	}

	return true;
}

/****************************************************************************/

static bool DebugCmdClear(const char* /* args */)
{
	LinesDisplayed = 0;
	SendMessage(hwndInfo, LB_RESETCONTENT, 0, 0);
	return true;
}

/****************************************************************************/

static void DebugShowLabels(bool Host)
{
	const int Index = (int)Host;

	if (Labels[Index].empty())
	{
		DebugDisplayInfo("No labels defined");
	}
	else
	{
		DebugDisplayInfoF("%d known labels:", Labels[Index].size());

		for (std::size_t i = 0; i < Labels[Index].size(); i++)
		{
			DebugDisplayInfoF("%04X %s", Labels[Index][i].addr, Labels[Index][i].name.c_str());
		}
	}
}

/****************************************************************************/

static bool DebugCmdLabels(const char* args)
{
	std::vector<std::string> Args;

	ParseLine(args, Args);

	if (Args.empty())
	{
		return false;
	}

	size_t Index = 0;

	const std::string& Command = Args[Index];

	Index++;

	bool Host = true;

	if (Index < Args.size())
	{
		if (StrCaseCmp(Args[Index].c_str(), "p") == 0)
		{
			Host = false;
		}

		Index++;
	}

	if (StrCaseCmp(Command.c_str(), "show") == 0)
	{
		if (Index < Args.size())
		{
			return false;
		}

		DebugShowLabels(Host);
	}
	else if (StrCaseCmp(Command.c_str(), "clear") == 0)
	{
		if (Index < Args.size())
		{
			return false;
		}

		Labels[(int)Host].clear();

		DebugDisplayInfo("Labels cleared");
	}
	else if (StrCaseCmp(Command.c_str(), "load") == 0)
	{
		char FileName[MAX_PATH];
		ZeroMemory(FileName, MAX_PATH);

		sscanf(args, Host ? "%*s %259c" : "%*s %*s %259c", FileName);

		bool Success = true;

		if (FileName[0] == '\0')
		{
			const char* Filter = "Label Files (*.txt)\0*.txt\0" "All Files (*.*)\0*.*\0";

			FileDialog Dialog(hwndDebug, FileName, MAX_PATH, nullptr, Filter);

			Success = Dialog.Open();
		}

		if (Success && FileName[0] != '\0')
		{
			DebugLoadLabels(FileName, Host);
		}
	}
	else
	{
		return false;
	}

	return true;
}

/****************************************************************************/

static bool DebugCmdWatch(const char* args)
{
	std::vector<std::string> Args;

	ParseLine(args, Args);

	if (Args.empty())
	{
		return false;
	}

	size_t Index = 0;
	const char* pszLabel = nullptr;

	Watch w;

	if (StrCaseCmp(Args[Index].c_str(), "p") == 0) // Parasite
	{
		w.host = false;

		Index++;
	}

	// Address

	if (Index == Args.size())
	{
		return false;
	}

	unsigned long Address;

	if (!ParseAddressOrLabel(Args[Index].c_str(), w.host, &Address, &pszLabel))
	{
		return true;
	}

	w.start = (int)Address;

	Index++;

	// Type

	if (Index < Args.size())
	{
		if (Args[Index].size() != 1)
		{
			DebugDisplayInfoF("Invalid watch type: %s", Args[Index].c_str());
			return true;
		}

		w.type = (char)tolower(Args[Index][0]);

		// Check type is valid
		if (w.type != 'b' && w.type != 'w' && w.type != 'd')
		{
			DebugDisplayInfoF("Invalid watch type: %s", Args[Index].c_str());
			return true;
		}

		Index++;
	}

	// Name

	if (Index < Args.size())
	{
		w.name = Args[Index];
	}
	else
	{
		if (pszLabel != nullptr)
		{
			w.name = pszLabel;
		}
	}

	// Check if watch in list
	int i = DebugFindWatch(w);

	if (i != LB_ERR)
	{
		SendMessage(hwndW, LB_DELETESTRING, i, 0);

		auto it = std::find_if(Watches.begin(), Watches.end(), [&w](const Watch& watch){
			return watch.start == w.start;
		});

		Watches.erase(it);
	}
	else if (Watches.size() >= MAX_BPS)
	{
		DebugDisplayInfo("You have too many watches!");
		return true;
	}
	else
	{
		Watches.push_back(w);
		DebugAddWatch(w);

		DebugUpdateWatches(true);
	}

	SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");

	return true;
}

/****************************************************************************/

static bool DebugCmdToggleBreak(const char* args)
{
	std::vector<std::string> Args;

	ParseLine(args, Args);

	if (Args.empty())
	{
		return false;
	}

	const char* pszLabel = nullptr;
	Breakpoint bp;

	std::size_t SeparatorPos = Args[0].find('-');

	unsigned long Address;

	if (SeparatorPos == std::string::npos)
	{
		if (ParseAddressOrLabel(Args[0].c_str(), true, &Address, &pszLabel))
		{
			bp.start = (int)Address;
		}
		else
		{
			return true;
		}
	}
	else
	{
		std::string Start = Args[0].substr(0, SeparatorPos);
		std::string End   = Args[0].substr(SeparatorPos + 1);

		if (ParseAddressOrLabel(Start.c_str(), true, &Address, &pszLabel))
		{
			bp.start = (int)Address;
		}
		else
		{
			return true;
		}

		if (ParseAddressOrLabel(End.c_str(), true, &Address, nullptr))
		{
			bp.end = (int)Address;
		}
		else
		{
			return true;
		}
	}

	// Set breakpoint name. Default is address label, if there is one.

	if (Args.size() == 2)
	{
		bp.name = Args[1];
	}
	else
	{
		if (pszLabel != nullptr)
		{
			bp.name = pszLabel;
		}
	}

	// Check if breakpoint already exists.

	int Index = DebugFindBreakpoint(bp);

	if (Index != LB_ERR)
	{
		// Yes, delete it.
		SendMessage(hwndBP, LB_DELETESTRING, Index, 0);

		auto it = std::find_if(Breakpoints.begin(), Breakpoints.end(), [&bp](const Breakpoint& b) {
			return b.start == bp.start;
		});

		Breakpoints.erase(it);
	}
	else
	{
		// No, add a new breakpoint.

		if (Breakpoints.size() >= MAX_BPS)
		{
			DebugDisplayInfo("You have too many breakpoints!");
			return true;
		}

		if (bp.end >= 0 && bp.end < bp.start)
		{
			DebugDisplayInfo("Error: Invalid breakpoint range.");
			return true;
		}

		Breakpoints.push_back(bp);

		DebugAddBreakpoint(bp);
	}

	SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");

	return true;
}

/**************************************************************
 * End of debugger command handlers                           *
 **************************************************************/

unsigned char DebugReadMem(int Address, bool Host)
{
	if (Host)
	{
		return BeebReadMem(Address);
	}
	else
	{
		switch (TubeType)
		{
			case TubeDevice::Acorn65C02:
				return TubeReadMem(Address);

			case TubeDevice::Master512CoPro:
				return master512CoPro.DebugReadMemory(Address);

			case TubeDevice::AcornZ80:
			case TubeDevice::TorchZ80:
				return ReadZ80Mem(Address);

			case TubeDevice::AcornArm: {
				unsigned char Data;

				arm->readByte(Address, Data);

				return Data;
			}

			case TubeDevice::SprowArm:
				return sprow->DebugReadMemory(Address);

			case TubeDevice::None:
			default:
				break;
		}
	}

	return 0;
}

/****************************************************************************/

static void DebugWriteMem(int Address, bool Host, unsigned char Data)
{
	if (Host)
	{
		BeebWriteMem(Address, Data);
	}
	else
	{
		switch (TubeType)
		{
			case TubeDevice::Acorn65C02:
				TubeWriteMem(Address, Data);
				break;

			case TubeDevice::Master512CoPro:
				master512CoPro.DebugWriteMemory(Address, Data);
				break;

			case TubeDevice::AcornZ80:
			case TubeDevice::TorchZ80:
				WriteZ80Mem(Address, Data);
				break;

			case TubeDevice::AcornArm:
				arm->writeByte(Address, Data);
				break;

			case TubeDevice::SprowArm:
				sprow->DebugWriteMemory(Address, Data);
				break;

			case TubeDevice::None:
			default:
				break;
		}
	}
}

/****************************************************************************/

int DebugDisassembleInstruction(int Addr, bool Host, char *pszOutput)
{
	int operand = 0;
	int zpaddr = 0;
	int l = 0;

	char *s = pszOutput;

	s += sprintf(s, "%04X ", Addr);

	int opcode = DebugReadMem(Addr, Host);

	const InstInfo *optable = GetOpcodeTable(Host);

	const InstInfo *ip = &optable[opcode];

	switch (ip->bytes)
	{
		case 1:
			s += sprintf(s, "%02X           ",
			             DebugReadMem(Addr, Host));
			break;
		case 2:
			s += sprintf(s, "%02X %02X        ",
			             DebugReadMem(Addr, Host),
			             DebugReadMem(Addr + 1, Host));
			break;
		case 3:
			s += sprintf(s, "%02X %02X %02X     ",
			             DebugReadMem(Addr, Host),
			             DebugReadMem(Addr + 1, Host),
			             DebugReadMem(Addr + 2, Host));
			break;
	}

	if (!Host) {
		s += sprintf(s, "            ");
	}

	s += sprintf(s, "%s ", ip->opcode);
	Addr++;

	switch (ip->bytes)
	{
		case 1:
			l = 0;
			break;
		case 2:
			operand = DebugReadMem(Addr, Host);
			l = 2;
			break;
		case 3:
			operand = DebugReadMem(Addr, Host) | (DebugReadMem(Addr + 1, Host) << 8);
			l = 4;
			break;
	}

	if (ip->flag & REL)
	{
		if (operand > 127)
		{
			operand = (~0xff | operand);
		}

		operand = operand + ip->bytes + Addr - 1;
		l = 4;
	}
	else if (ip->flag & ZPR)
	{
		zpaddr = operand & 0xff;
		int Offset  = (operand & 0xff00) >> 8;

		if (Offset > 127)
		{
			Offset = (~0xff | Offset);
		}

		operand = Addr + ip->bytes - 1 + Offset;
	}

	switch (ip->flag & ADRMASK)
	{
	case IMM:
		s += sprintf(s, "#%0*X    ", l, operand);
		break;
	case REL:
	case ABS:
	case ZPG:
		s += sprintf(s, "%0*X     ", l, operand);
		break;
	case IND:
		s += sprintf(s, "(%0*X)   ", l, operand);
		break;
	case ABX:
	case ZPX:
		s += sprintf(s, "%0*X,X   ", l, operand);
		break;
	case ABY:
	case ZPY:
		s += sprintf(s, "%0*X,Y   ", l, operand);
		break;
	case INX:
		s += sprintf(s, "(%0*X,X) ", l, operand);
		break;
	case INY:
		s += sprintf(s, "(%0*X),Y ", l, operand);
		break;
	case ACC:
		s += sprintf(s, "A        ");
		break;
	case ZPR:
		s += sprintf(s, "%02X,%04X ", zpaddr, operand);
		break;
	case IMP:
	default:
		s += sprintf(s, "         ");
		break;
	}

	if (l == 2) {
		s += sprintf(s, "  ");
	}

	if (Host) {
		s += sprintf(s, "            ");
	}

	return ip->bytes;
}

/****************************************************************************/

int DebugDisassembleInstructionWithCPUStatus(int Addr,
                                             bool Host,
                                             int Accumulator,
                                             int XReg,
                                             int YReg,
                                             unsigned char StackReg,
                                             unsigned char PSR,
                                             char *pszOutput)
{
	DebugDisassembleInstruction(Addr, Host, pszOutput);

	char* p = pszOutput + strlen(pszOutput);

	p += sprintf(p, "A=%02X X=%02X Y=%02X S=%02X ", Accumulator, XReg, YReg, StackReg);

	*p++ = (PSR & FlagC) ? 'C' : '.';
	*p++ = (PSR & FlagZ) ? 'Z' : '.';
	*p++ = (PSR & FlagI) ? 'I' : '.';
	*p++ = (PSR & FlagD) ? 'D' : '.';
	*p++ = (PSR & FlagB) ? 'B' : '.';
	*p++ = (PSR & FlagV) ? 'V' : '.';
	*p++ = (PSR & FlagN) ? 'N' : '.';
	*p = '\0';

	return (int)(p - pszOutput);
}

/****************************************************************************/

static int DebugDisassembleCommand(int Addr, int Count, bool Host)
{
	int StartAddress = Addr;

	if (Count > MAX_LINES)
	{
		Count = MAX_LINES;
	}

	const int MaxAddress = DebugGetMaxAddress(Host);

	while (Count > 0 && Addr <= MaxAddress)
	{
		char Str[128];

		if (Host)
		{
			Addr += DebugDisassembleInstruction(Addr, Host, Str);
		}
		else
		{
			switch (TubeType)
			{
				case TubeDevice::Acorn65C02:
					Addr += DebugDisassembleInstruction(Addr, Host, Str);
					break;

				case TubeDevice::AcornZ80:
				case TubeDevice::TorchZ80:
					Addr += Z80Disassemble(Addr, Str);
					break;

				case TubeDevice::AcornArm: {
					uint32 Instr;

					if (!arm->readWord(Addr, Instr))
					{
						DebugDisplayInfoF("Invalid address: %08X", Addr);
						goto Exit;
					}

					char* p = Str;

					p += sprintf(p, "%08X %02X %02X %02X %02X  ",
					             Addr,
					             Instr & 0xff, (Instr >> 8) & 0xff,
					             (Instr >> 16) & 0xff, (Instr >> 24) & 0xff);

					Arm_disassemble(Addr, Instr, p);
					Addr += 4;
					break;
				}

				case TubeDevice::Master512CoPro:
				case TubeDevice::SprowArm:
					DebugDisplayInfo("Not implemented for this coprocessor");
					return 0;

				case TubeDevice::None:
				default:
					DebugDisplayInfo("No coprocessor enabled");
					return 0;
			}
		}

		DebugDisplayInfo(Str);
		Count--;
	}

Exit:
	return Addr - StartAddress;
}

/****************************************************************************/

static void DebugMemoryDump(int StartAddress, int Count, bool Host)
{
	if (Count > MAX_LINES * 16)
	{
		Count = MAX_LINES * 16;
	}

	StartAddress &= ~0xF;
	int EndAddress = (StartAddress + Count - 1) | 0xF;

	const int MaxAddress = DebugGetMaxAddress(Host);

	if (EndAddress > MaxAddress)
	{
		EndAddress = MaxAddress;
	}

	if (StartAddress >= EndAddress)
	{
		return;
	}

	const int AddressWidth = DebugGetAddressBits(Host) / 4;

	DebugDisplayInfo("");

	// Header row

	char Info[80];
	char* p = Info;

	for (int i = 0; i < AddressWidth; ++i)
	{
		*p++ = ' ';
	}

	*p++ = ' ';
	*p++ = ' ';

	for (int i = 0; i < 16; ++i)
	{
		*p++ = (char)toupper(ToHexDigit(i));
		*p++ = ' ';
		*p++ = ' ';
	}

	*p++ = ' ';

	for (int i = 0; i < 16; ++i)
	{
		*p++ = (char)toupper(ToHexDigit(i));
	}

	*p = '\0';

	DebugDisplayInfo(Info);

	// Data rows

	for (int Address = StartAddress; Address < EndAddress; Address += 16)
	{
		ZeroMemory(Info, sizeof(Info));
		p = Info;

		p += sprintf(p, "%0*X  ", AddressWidth, Address);

		for (int i = 0; i < 16; ++i)
		{
			if (DebugIsIOAddress(Address + i, Host))
			{
				p += sprintf(p, "IO ");

				Info[AddressWidth + 2 + 16 * 3 + 1 + i] = '.';
			}
			else
			{
				int Value = DebugReadMem(Address + i, Host);

				p += sprintf(p, "%02X ", Value);

				if (!isprint(Value))
				{
					Value = '.';
				}

				Info[AddressWidth + 2 + 16 * 3 + 1 + i] = (char)Value;
			}
		}

		*p++ = ' ';

		DebugDisplayInfo(Info);
	}
}

/****************************************************************************/

#endif

/****************************************************************************/

