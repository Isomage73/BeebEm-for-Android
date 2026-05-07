/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1997  Laurie Whiffen

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

// User defined keyboard functionality.

#include "Windows.h"

#include "KeyMap.h"
#include "Main.h"

// Keyboard mappings
KeyMap UserKeyMap;
KeyMap DefaultKeyMap;
KeyMap LogicalKeyMap;

// Currently selected translation table
const KeyMap *transTable = &DefaultKeyMap;

// Token written to start of map file
#define KEYMAP_TOKEN "*** BeebEm Keymap ***"

static const char* GetVirtualKeyCode(int Key);

/*--------------------------------------------------------------------------*/

static void ResetKeyMap(KeyMap* keymap)
{
	for (int PCKey = 0; PCKey < KEYMAP_SIZE; PCKey++)
	{
		for (int PCShift = 0; PCShift < 2; PCShift++)
		{
			(*keymap)[PCKey][PCShift].row = -9;
			(*keymap)[PCKey][PCShift].row = 0;
			UserKeyMap[PCKey][PCShift].shift = PCShift == 1;
		}
	}
}

/*--------------------------------------------------------------------------*/

void InitKeyMap()
{
	ResetKeyMap(&DefaultKeyMap);
	ResetKeyMap(&LogicalKeyMap);
	ResetKeyMap(&UserKeyMap);
}

/*--------------------------------------------------------------------------*/

bool ReadKeyMap(const char *filename, KeyMap *keymap)
{
	bool success = true;
	char buf[256];

	FILE *infile = fopen(filename,"r");

	if (infile == NULL)
	{
		mainWin->Report(MessageType::Error,
		                "Failed to read key map file:\n  %s", filename);

		success = false;
	}
	else
	{
		if (fgets(buf, 255, infile) == nullptr ||
		    strcmp(buf, KEYMAP_TOKEN "\n") != 0)
		{
			mainWin->Report(MessageType::Error,
			                "Invalid key map file:\n  %s\n", filename);

			success = false;
		}
		else
		{
			fgets(buf, 255, infile);

			for (int i = 0; i < KEYMAP_SIZE; ++i)
			{
				if (fgets(buf, 255, infile) == NULL)
				{
					// File has fewer entries than KEYMAP_SIZE — leave remainder unbound.
					break;
				}

				int shift0 = 0, shift1 = 0;

				sscanf(buf, "%d %d %d %d %d %d",
				       &(*keymap)[i][0].row,
				       &(*keymap)[i][0].col,
				       &shift0,
				       &(*keymap)[i][1].row,
				       &(*keymap)[i][1].col,
				       &shift1);

				(*keymap)[i][0].shift = shift0 != 0;
				(*keymap)[i][1].shift = shift1 != 0;
			}
		}

		fclose(infile);
	}

	return success;
}

/*--------------------------------------------------------------------------*/

bool WriteKeyMap(const char *filename, KeyMap *keymap)
{
	FILE *outfile = fopen(filename, "w");

	if (outfile == nullptr)
	{
		mainWin->Report(MessageType::Error,
		                "Failed to write key map file:\n  %s", filename);
		return false;
	}

	fprintf(outfile, KEYMAP_TOKEN "\n\n");

	for (int i = 0; i < KEYMAP_SIZE; i++)
	{
		fprintf(outfile, "%d %d %d %d %d %d # 0x%02X",
		        (*keymap)[i][0].row,
		        (*keymap)[i][0].col,
		        (*keymap)[i][0].shift,
		        (*keymap)[i][1].row,
		        (*keymap)[i][1].col,
		        (*keymap)[i][1].shift,
		        i);

		const char* KeyCode = GetVirtualKeyCode(i);

		if (KeyCode != nullptr)
		{
			fprintf(outfile, " %s", KeyCode);
		}

		fprintf(outfile, "\n");
	}

	fclose(outfile);

	return true;
}

/*--------------------------------------------------------------------------*/

#if !defined(__APPLE__) && !defined(BEEBEM_ANDROID)

#define CASE(constant) case constant: return (# constant)

static const char* GetVirtualKeyCode(int Key)
{
	static char Name[2];

	if ((Key >= '0' && Key <= '9') ||
	    (Key >= 'A' && Key <= 'Z'))
	{
		Name[0] = (char)Key;
		Name[1] = '\0';

		return Name;
	}

	switch (Key)
	{
		CASE(SDLK_BACKSPACE);
		CASE(SDLK_TAB);
		CASE(SDLK_CLEAR);
		CASE(SDLK_RETURN);
		CASE(SDLK_PAUSE);
		CASE(SDLK_ESCAPE);
		CASE(SDLK_SPACE);
		CASE(SDLK_EXCLAIM);
		CASE(SDLK_QUOTEDBL);
		CASE(SDLK_HASH);
		CASE(SDLK_DOLLAR);
		CASE(SDLK_AMPERSAND);
		CASE(SDLK_QUOTE);
		CASE(SDLK_LEFTPAREN);
		CASE(SDLK_RIGHTPAREN);
		CASE(SDLK_ASTERISK);
		CASE(SDLK_PLUS);
		CASE(SDLK_COMMA);
		CASE(SDLK_MINUS);
		CASE(SDLK_PERIOD);
		CASE(SDLK_SLASH);
		CASE(SDLK_0);
		CASE(SDLK_1);
		CASE(SDLK_2);
		CASE(SDLK_3);
		CASE(SDLK_4);
		CASE(SDLK_5);
		CASE(SDLK_6);
		CASE(SDLK_7);
		CASE(SDLK_8);
		CASE(SDLK_9);
		CASE(SDLK_COLON);
		CASE(SDLK_SEMICOLON);
		CASE(SDLK_LESS);
		CASE(SDLK_EQUALS);
		CASE(SDLK_GREATER);
		CASE(SDLK_QUESTION);
		CASE(SDLK_AT);
		CASE(SDLK_LEFTBRACKET);
		CASE(SDLK_BACKSLASH);
		CASE(SDLK_RIGHTBRACKET);
		CASE(SDLK_CARET);
		CASE(SDLK_UNDERSCORE);
		CASE(SDLK_BACKQUOTE);
		CASE(SDLK_a);
		CASE(SDLK_b);
		CASE(SDLK_c);
		CASE(SDLK_d);
		CASE(SDLK_e);
		CASE(SDLK_f);
		CASE(SDLK_g);
		CASE(SDLK_h);
		CASE(SDLK_i);
		CASE(SDLK_j);
		CASE(SDLK_k);
		CASE(SDLK_l);
		CASE(SDLK_m);
		CASE(SDLK_n);
		CASE(SDLK_o);
		CASE(SDLK_p);
		CASE(SDLK_q);
		CASE(SDLK_r);
		CASE(SDLK_s);
		CASE(SDLK_t);
		CASE(SDLK_u);
		CASE(SDLK_v);
		CASE(SDLK_w);
		CASE(SDLK_x);
		CASE(SDLK_y);
		CASE(SDLK_z);
		CASE(SDLK_DELETE);
		CASE(SDLK_WORLD_0);
		CASE(SDLK_WORLD_1);
		CASE(SDLK_WORLD_2);
		CASE(SDLK_WORLD_3);
		CASE(SDLK_WORLD_4);
		CASE(SDLK_WORLD_5);
		CASE(SDLK_WORLD_6);
		CASE(SDLK_WORLD_7);
		CASE(SDLK_WORLD_8);
		CASE(SDLK_WORLD_9);
		CASE(SDLK_WORLD_10);
		CASE(SDLK_WORLD_11);
		CASE(SDLK_WORLD_12);
		CASE(SDLK_WORLD_13);
		CASE(SDLK_WORLD_14);
		CASE(SDLK_WORLD_15);
		CASE(SDLK_WORLD_16);
		CASE(SDLK_WORLD_17);
		CASE(SDLK_WORLD_18);
		CASE(SDLK_WORLD_19);
		CASE(SDLK_WORLD_20);
		CASE(SDLK_WORLD_21);
		CASE(SDLK_WORLD_22);
		CASE(SDLK_WORLD_23);
		CASE(SDLK_WORLD_24);
		CASE(SDLK_WORLD_25);
		CASE(SDLK_WORLD_26);
		CASE(SDLK_WORLD_27);
		CASE(SDLK_WORLD_28);
		CASE(SDLK_WORLD_29);
		CASE(SDLK_WORLD_30);
		CASE(SDLK_WORLD_31);
		CASE(SDLK_WORLD_32);
		CASE(SDLK_WORLD_33);
		CASE(SDLK_WORLD_34);
		CASE(SDLK_WORLD_35);
		CASE(SDLK_WORLD_36);
		CASE(SDLK_WORLD_37);
		CASE(SDLK_WORLD_38);
		CASE(SDLK_WORLD_39);
		CASE(SDLK_WORLD_40);
		CASE(SDLK_WORLD_41);
		CASE(SDLK_WORLD_42);
		CASE(SDLK_WORLD_43);
		CASE(SDLK_WORLD_44);
		CASE(SDLK_WORLD_45);
		CASE(SDLK_WORLD_46);
		CASE(SDLK_WORLD_47);
		CASE(SDLK_WORLD_48);
		CASE(SDLK_WORLD_49);
		CASE(SDLK_WORLD_50);
		CASE(SDLK_WORLD_51);
		CASE(SDLK_WORLD_52);
		CASE(SDLK_WORLD_53);
		CASE(SDLK_WORLD_54);
		CASE(SDLK_WORLD_55);
		CASE(SDLK_WORLD_56);
		CASE(SDLK_WORLD_57);
		CASE(SDLK_WORLD_58);
		CASE(SDLK_WORLD_59);
		CASE(SDLK_WORLD_60);
		CASE(SDLK_WORLD_61);
		CASE(SDLK_WORLD_62);
		CASE(SDLK_WORLD_63);
		CASE(SDLK_WORLD_64);
		CASE(SDLK_WORLD_65);
		CASE(SDLK_WORLD_66);
		CASE(SDLK_WORLD_67);
		CASE(SDLK_WORLD_68);
		CASE(SDLK_WORLD_69);
		CASE(SDLK_WORLD_70);
		CASE(SDLK_WORLD_71);
		CASE(SDLK_WORLD_72);
		CASE(SDLK_WORLD_73);
		CASE(SDLK_WORLD_74);
		CASE(SDLK_WORLD_75);
		CASE(SDLK_WORLD_76);
		CASE(SDLK_WORLD_77);
		CASE(SDLK_WORLD_78);
		CASE(SDLK_WORLD_79);
		CASE(SDLK_WORLD_80);
		CASE(SDLK_WORLD_81);
		CASE(SDLK_WORLD_82);
		CASE(SDLK_WORLD_83);
		CASE(SDLK_WORLD_84);
		CASE(SDLK_WORLD_85);
		CASE(SDLK_WORLD_86);
		CASE(SDLK_WORLD_87);
		CASE(SDLK_WORLD_88);
		CASE(SDLK_WORLD_89);
		CASE(SDLK_WORLD_90);
		CASE(SDLK_WORLD_91);
		CASE(SDLK_WORLD_92);
		CASE(SDLK_WORLD_93);
		CASE(SDLK_WORLD_94);
		CASE(SDLK_WORLD_95);
		CASE(SDLK_KP0);
		CASE(SDLK_KP1);
		CASE(SDLK_KP2);
		CASE(SDLK_KP3);
		CASE(SDLK_KP4);
		CASE(SDLK_KP5);
		CASE(SDLK_KP6);
		CASE(SDLK_KP7);
		CASE(SDLK_KP8);
		CASE(SDLK_KP9);
		CASE(SDLK_KP_PERIOD);
		CASE(SDLK_KP_DIVIDE);
		CASE(SDLK_KP_MULTIPLY);
		CASE(SDLK_KP_MINUS);
		CASE(SDLK_KP_PLUS);
		CASE(SDLK_KP_ENTER);
		CASE(SDLK_KP_EQUALS);
		CASE(SDLK_UP);
		CASE(SDLK_DOWN);
		CASE(SDLK_RIGHT);
		CASE(SDLK_LEFT);
		CASE(SDLK_INSERT);
		CASE(SDLK_HOME);
		CASE(SDLK_END);
		CASE(SDLK_PAGEUP);
		CASE(SDLK_PAGEDOWN);
		CASE(SDLK_F1);
		CASE(SDLK_F2);
		CASE(SDLK_F3);
		CASE(SDLK_F4);
		CASE(SDLK_F5);
		CASE(SDLK_F6);
		CASE(SDLK_F7);
		CASE(SDLK_F8);
		CASE(SDLK_F9);
		CASE(SDLK_F10);
		CASE(SDLK_F11);
		CASE(SDLK_F12);
		CASE(SDLK_F13);
		CASE(SDLK_F14);
		CASE(SDLK_F15);
		CASE(SDLK_NUMLOCK);
		CASE(SDLK_CAPSLOCK);
		CASE(SDLK_SCROLLOCK);
		CASE(SDLK_RSHIFT);
		CASE(SDLK_LSHIFT);
		CASE(SDLK_RCTRL);
		CASE(SDLK_LCTRL);
		CASE(SDLK_RALT);
		CASE(SDLK_LALT);
		CASE(SDLK_RMETA);
		CASE(SDLK_LMETA);
		CASE(SDLK_LSUPER);
		CASE(SDLK_RSUPER);
		CASE(SDLK_MODE);
		CASE(SDLK_COMPOSE);
		CASE(SDLK_HELP);
		CASE(SDLK_PRINT);
		CASE(SDLK_SYSREQ);
		CASE(SDLK_BREAK);
		CASE(SDLK_MENU);
		CASE(SDLK_POWER);
		CASE(SDLK_EURO);
		CASE(SDLK_UNDO);
	}

	return nullptr;
}

#else // __APPLE__ or BEEBEM_ANDROID

static const char* GetVirtualKeyCode(int /*Key*/)
{
	return nullptr;
}

#endif

/*--------------------------------------------------------------------------*/

const char* GetPCKeyName(int Key)
{
	static char Character[2]; // Used to return single characters.

	switch (Key)
	{
	case 0x08: return "Backspace";
	case 0x09: return "Tab";
	case 0x0D: return "Enter";
	case 0x10: return "Shift";
	case 0x11: return "Ctrl";
	case 0x12: return "Alt";
	case 0x13: return "Break";
	case 0x14: return "Caps";
	case 0x1B: return "Esc";
	case 0x20: return "Spacebar";
	case 0x21: return "PgUp";
	case 0x22: return "PgDn";
	case 0x23: return "End";
	case 0x24: return "Home";
	case 0x25: return "Left";
	case 0x26: return "Up";
	case 0x27: return "Right";
	case 0x28: return "Down";
	case 0x2D: return "Insert";
	case 0x2E: return "Del";
	case 0x5D: return "Menu";
	case 0x60: return "Pad0";
	case 0x61: return "Pad1";
	case 0x62: return "Pad2";
	case 0x63: return "Pad3";
	case 0x64: return "Pad4";
	case 0x65: return "Pad5";
	case 0x66: return "Pad6";
	case 0x67: return "Pad7";
	case 0x68: return "Pad8";
	case 0x69: return "Pad9";
	case 0x6A: return "Pad*";
	case 0x6B: return "Pad+";
	case 0x6D: return "Pad-";
	case 0x6E: return "Pad.";
	case 0x6F: return "Pad/";
	case 0x70: return "F1";
	case 0x71: return "F2";
	case 0x72: return "F3";
	case 0x73: return "F4";
	case 0x74: return "F5";
	case 0x75: return "F6";
	case 0x76: return "F7";
	case 0x77: return "F8";
	case 0x78: return "F9";
	case 0x79: return "F10";
	case 0x7A: return "F11";
	case 0x7B: return "F12";
	case 0x90: return "NumLock";
	case 0x91: return "SclLock";
	case 0xBA: return ";";
	case 0xBB: return "=";
	case 0xBC: return ",";
	case 0xBD: return "-";
	case 0xBE: return ".";
	case 0xBF: return "/";
	case 0xC0: return "\'";
	case 0xDB: return "[";
	case 0xDC: return "\\";
	case 0xDD: return "]";
	case 0xDE: return "#";
	case 0xDF: return "`";

	default:
		Character[0] = (char)LOBYTE(Key);
		Character[1] = '\0';
		return Character;
	}
}

/*--------------------------------------------------------------------------*/

void SetUserKeyMapping(int Row, int Column, bool BBCShift, int PCKey, bool PCShift)
{
	if (PCKey >= 0 && PCKey < KEYMAP_SIZE)
	{
		UserKeyMap[PCKey][static_cast<int>(PCShift)].row = Row;
		UserKeyMap[PCKey][static_cast<int>(PCShift)].col = Column;
		UserKeyMap[PCKey][static_cast<int>(PCShift)].shift = BBCShift;

		// DebugTrace("SetBBCKey: key=%d, shift=%d, row=%d, col=%d, bbcshift=%d\n",
		//            Key, (int)PCShift, Row, Col, BBCShift);
	}
}

/*--------------------------------------------------------------------------*/

// Clear any PC keys that correspond to a given BBC keyboard column, row,
// and shift state.

void ClearUserKeyMapping(int Row, int Column, bool Shift)
{
	for (int PCKey = 0; PCKey < KEYMAP_SIZE; PCKey++)
	{
		for (int PCShift = 0; PCShift < 2; PCShift++)
		{
			if (UserKeyMap[PCKey][PCShift].row == Row &&
			    UserKeyMap[PCKey][PCShift].col == Column &&
			    UserKeyMap[PCKey][PCShift].shift == Shift)
			{
				UserKeyMap[PCKey][PCShift].row = -9;
				UserKeyMap[PCKey][PCShift].col = 0;
				UserKeyMap[PCKey][PCShift].shift = PCShift  == 1;
			}
		}
	}
}

/*--------------------------------------------------------------------------*/

std::string GetKeysUsed(int Row, int Column, bool Shift)
{
	std::string Keys;

	// First see if this key is defined.
	if (Row != -9)
	{
		for (int PCKey = 0; PCKey < KEYMAP_SIZE; PCKey++)
		{
			for (int PCShift = 0; PCShift < 2; PCShift++)
			{
				if (UserKeyMap[PCKey][PCShift].row == Row &&
				    UserKeyMap[PCKey][PCShift].col == Column &&
				    UserKeyMap[PCKey][PCShift].shift == Shift)
				{
					// We have found a key that matches.
					if (!Keys.empty())
					{
						Keys += ", ";
					}

					if (PCShift == 1)
					{
						Keys += "Sh-";
					}

					Keys += GetPCKeyName(PCKey);
				}
			}
		}
	}

	if (Keys.empty())
	{
		Keys = "Not Assigned";
	}

	return Keys;
}
