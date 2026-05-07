/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2026 Chris Needham

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

#ifndef JOYSTICKDIALOG_HEADER
#define JOYSTICKDIALOG_HEADER

#ifdef WIN32

#include "BeebWin.h"
#include "ComboBox.h"
#include "Dialog.h"

class JoystickController;

class JoystickDialog : public Dialog
{
	public:
		JoystickDialog(HINSTANCE hInstance,
		               HWND hwndParent,
		               JoystickController& Controller,
		               AnalogueInputDevice Device[2],
		               JoystickDeviceType DeviceType[2],
		               const std::string* pDeviceID,
		               int JoystickControl[2],
		               int JoystickButton[2],
		               MousestickType Mousestick[2],
		               int JoystickMouseButton[2]);

	public:
		AnalogueInputDevice GetAnalogueInputDevice(int Joystick) const;
		JoystickDeviceType GetJoystickDeviceType(int Joystick) const { return m_JoystickDeviceType[Joystick]; }
		const std::string& GetJoystickDeviceID(int Joystick) const { return m_JoystickDeviceID[Joystick]; }
		int GetJoystickControl(int Joystick) const;
		int GetJoystickButton(int Joystick) const;
		MousestickType GetMousestickType(int Joystick) const;
		int GetJoystickMouseButton(int Joystick) const;

	private:
		virtual INT_PTR DlgProc(UINT nMessage,
		                        WPARAM wParam,
		                        LPARAM lParam);

		void InitDeviceList();
		void OnDeviceSelChange(int Index);
		void UpdateJoystickList();
		void UpdateSelected();

	private:
		JoystickController& m_JoystickController;
		AnalogueInputDevice m_AnalogueInputDevice[2];
		JoystickDeviceType m_JoystickDeviceType[2];
		std::string m_JoystickDeviceID[2];
		int m_JoystickControl[2];
		int m_JoystickButton[2];
		MousestickType m_MousestickType[2];
		int m_JoystickMouseButton[2];
		ComboBox m_JoystickDevice[2];
		ComboBox m_AnalogInput[2];
		ComboBox m_ButtonInput[2];
};

#endif

#endif
