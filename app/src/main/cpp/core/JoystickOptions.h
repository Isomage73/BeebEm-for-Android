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

#ifndef JOYSTICK_OPTIONS_HEADER
#define JOYSTICK_OPTIONS_HEADER

#include <string>

constexpr int JOYSTICK_ANALOGUE_INPUT_LEFT_THUMBSTICK  = 0x01;
constexpr int JOYSTICK_ANALOGUE_INPUT_RIGHT_THUMBSTICK = 0x02;
constexpr int JOYSTICK_ANALOGUE_INPUT_DPAD             = 0x04;

constexpr int MOUSE_BUTTON_LEFT  = 0x01;
constexpr int MOUSE_BUTTON_RIGHT = 0x02;

struct OptionValue
{
	const char*	Name;
	const char* ConfigName;
	int Value;
};

extern const OptionValue JoystickButtonOptions[];
extern const OptionValue JoystickControlOptions[];
extern const OptionValue MousestickOptions[];
extern const OptionValue MouseButtonOptions[];

int FindOptionValue(const std::string& Value, const OptionValue* pValues, int Default);
const char* GetOptionValueStr(const OptionValue* pValues, int Value);

#endif
