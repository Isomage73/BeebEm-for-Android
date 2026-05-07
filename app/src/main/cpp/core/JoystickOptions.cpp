/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2026  Chris Needham

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

#include <windows.h>

#include <assert.h>

#include "JoystickOptions.h"

#include "JoystickController.h"
#include "StringUtils.h"

#ifdef WIN32

/****************************************************************************/

const OptionValue JoystickButtonOptions[] =
{
	{ "A",             "A",       JOYSTICK_BUTTON_A },
	{ "B",             "B",       JOYSTICK_BUTTON_B },
	{ "A / B",         "A+B",     JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B },
	{ "X",             "X",       JOYSTICK_BUTTON_X },
	{ "Y",             "Y",       JOYSTICK_BUTTON_Y },
	{ "X / Y",         "X+Y",     JOYSTICK_BUTTON_X | JOYSTICK_BUTTON_Y },
	{ "A / B / X / Y", "A+B+X+Y", JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B | JOYSTICK_BUTTON_X | JOYSTICK_BUTTON_Y },
	{ "None",          "None",    0 },
	{ nullptr,         nullptr,   0 }
};

const OptionValue JoystickControlOptions[] =
{
	{ "Left Thumbstick",          "Left",       JOYSTICK_ANALOGUE_INPUT_LEFT_THUMBSTICK },
	{ "Left Thumbstick / D-Pad",  "Left+DPad",  JOYSTICK_ANALOGUE_INPUT_LEFT_THUMBSTICK | JOYSTICK_ANALOGUE_INPUT_DPAD },
	{ "Right Thumbstick",         "Right",      JOYSTICK_ANALOGUE_INPUT_RIGHT_THUMBSTICK },
	{ "Right Thumbstick / D-Pad", "Right+DPad", JOYSTICK_ANALOGUE_INPUT_RIGHT_THUMBSTICK | JOYSTICK_ANALOGUE_INPUT_DPAD },
	{ "D-Pad",                    "DPad",       JOYSTICK_ANALOGUE_INPUT_DPAD },
	{ "None",                     "None",       0 },
	{ nullptr,                    nullptr,      0 }
};

const OptionValue MousestickOptions[] =
{
	{ "Analogue Mousestick", "AnalogueMousestick", 0 },
	{ "Digital Mousestick",  "DigitalMousestick",  1 },
	{ nullptr,               nullptr,              0 }
};

const OptionValue MouseButtonOptions[] =
{
	{ "Left Mouse Button",         "Left",       MOUSE_BUTTON_LEFT },
	{ "Right Mouse Button",        "Right",      MOUSE_BUTTON_RIGHT },
	{ "Left / Right Mouse Button", "Left+Right", MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT },
	{ "None",                      "None",       0 },
	{ nullptr,                     nullptr,      0 }
};

/****************************************************************************/

int FindOptionValue(const std::string& Value, const OptionValue* pValues, int Default)
{
	for (int i = 0; pValues[i].ConfigName != nullptr; i++)
	{
		if (StrCaseCmp(Value.c_str(), pValues[i].ConfigName) == 0)
		{
			return pValues[i].Value;
		}
	}

	return Default;
}

/****************************************************************************/

const char* GetOptionValueStr(const OptionValue* pValues, int Value)
{
	for (int i = 0; pValues[i].ConfigName != nullptr; i++)
	{
		if (Value == pValues[i].Value)
		{
			return pValues[i].ConfigName;
		}
	}

	assert(false);

	return nullptr;
}

/****************************************************************************/

#endif // WIN32
