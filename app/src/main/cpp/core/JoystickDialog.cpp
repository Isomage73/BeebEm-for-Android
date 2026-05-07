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
#include <windowsx.h>

#pragma warning(push)
#pragma warning(disable: 4091) // ignored on left of 'tagGPFIDL_FLAGS' when no variable is declared
#include <shlobj.h>
#pragma warning(pop)

#include "JoystickDialog.h"
#include "JoystickController.h"
#include "JoystickOptions.h"
#include "ListView.h"
#include "Resource.h"

#ifdef WIN32

/****************************************************************************/

JoystickDialog::JoystickDialog(HINSTANCE hInstance,
                               HWND hwndParent,
                               JoystickController& Controller,
                               AnalogueInputDevice Device[2],
                               JoystickDeviceType DeviceType[2],
                               const std::string* pDeviceID,
                               int JoystickControl[2],
                               int JoystickButton[2],
                               MousestickType Mousestick[2],
                               int JoystickMouseButton[2]) :
	Dialog(hInstance, hwndParent, IDD_JOYSTICK),
	m_JoystickController(Controller)
{
	for (int Joystick = 0; Joystick < 2; Joystick++)
	{
		m_AnalogueInputDevice[Joystick] = Device[Joystick];
		m_JoystickDeviceType[Joystick] = DeviceType[Joystick];
		m_JoystickDeviceID[Joystick] = pDeviceID[Joystick];
		m_JoystickControl[Joystick] = JoystickControl[Joystick];
		m_JoystickButton[Joystick] = JoystickButton[Joystick];
		m_MousestickType[Joystick] = Mousestick[Joystick];
		m_JoystickMouseButton[Joystick] = JoystickMouseButton[Joystick];
	}
}

/****************************************************************************/

INT_PTR JoystickDialog::DlgProc(UINT   nMessage,
                                WPARAM wParam,
                                LPARAM /* lParam */)
{
	switch (nMessage)
	{
		case WM_INITDIALOG: {
			m_JoystickDevice[0].Init(m_hwnd, IDC_CHANNEL01_DEVICE_COMBO);
			m_JoystickDevice[1].Init(m_hwnd, IDC_CHANNEL23_DEVICE_COMBO);

			m_AnalogInput[0].Init(m_hwnd, IDC_CHANNEL01_INPUT_COMBO);
			m_AnalogInput[1].Init(m_hwnd, IDC_CHANNEL23_INPUT_COMBO);

			m_ButtonInput[0].Init(m_hwnd, IDC_BUTTON0_INPUT_COMBO);
			m_ButtonInput[1].Init(m_hwnd, IDC_BUTTON1_INPUT_COMBO);

			InitDeviceList();

			for (int Joystick = 0; Joystick < 2; Joystick++)
			{
				LPARAM ItemToSelect;

				switch (m_AnalogueInputDevice[Joystick])
				{
					case AnalogueInputDevice::None:
					default:
						ItemToSelect = 0;
						break;

					case AnalogueInputDevice::Joystick: {
						int DeviceIndex = m_JoystickController.FindDevice(m_JoystickDeviceType[Joystick], m_JoystickDeviceID[Joystick]);

						if (DeviceIndex == -1)
						{
							DeviceIndex = 0;
						}

						ItemToSelect = MAKELPARAM(DeviceIndex, (int)m_AnalogueInputDevice[Joystick]);
						break;
					}

					case AnalogueInputDevice::Mouse:
						ItemToSelect = MAKELPARAM(0, (int)m_AnalogueInputDevice[Joystick]);
						break;
				}

				int Index = m_JoystickDevice[Joystick].FindItemData(ItemToSelect);

				if (Index == CB_ERR)
				{
					Index = 0;
				}

				m_JoystickDevice[Joystick].SetCurSel(Index);

				OnDeviceSelChange(Joystick);
			}

			return TRUE;
		}

		case WM_COMMAND: {
			int nDlgItemID = GET_WM_COMMAND_ID(wParam, lParam);
			int Notification = GET_WM_COMMAND_CMD(wParam, lParam);

			switch (nDlgItemID)
			{
				case IDC_CHANNEL01_DEVICE_COMBO:
					if (Notification == CBN_SELCHANGE)
					{
						OnDeviceSelChange(0);
					}
					break;

				case IDC_CHANNEL23_DEVICE_COMBO:
					if (Notification == CBN_SELCHANGE)
					{
						OnDeviceSelChange(1);
					}
					break;

				case IDOK:
					UpdateSelected();
					EndDialog(m_hwnd, wParam);
					return TRUE;

				case IDCANCEL:
					EndDialog(m_hwnd, wParam);
					return TRUE;

				case IDC_REFRESH:
					UpdateJoystickList();
					return TRUE;
			}
			break;
		}
	}

	return FALSE;
}

/****************************************************************************/

void JoystickDialog::InitDeviceList()
{
	for (int i = 0; i < 2; i++)
	{
		m_JoystickDevice[i].ResetContent();

		m_JoystickDevice[i].AddItem("None",
		                            MAKELPARAM(0, (int)AnalogueInputDevice::None));

		size_t Count = m_JoystickController.GetDeviceCount();

		for (size_t DeviceIndex = 0; DeviceIndex < Count; DeviceIndex++)
		{
			const JoystickDevice& Device = m_JoystickController.GetDevice(DeviceIndex);

			m_JoystickDevice[i].AddItem(Device.GetName().c_str(),
			                            MAKELPARAM(DeviceIndex, (int)AnalogueInputDevice::Joystick));
		}

		m_JoystickDevice[i].AddItem("Mouse",
		                            MAKELPARAM(0, (int)AnalogueInputDevice::Mouse));
	}
}

/****************************************************************************/

void JoystickDialog::UpdateJoystickList()
{
	m_JoystickController.EnumerateDevices();

	for (int i = 0; i < 2; i++)
	{
		int SelectedIndex = m_JoystickDevice[i].GetCurSel();

		LPARAM SelectedItem = m_JoystickDevice[i].GetItemData(SelectedIndex);

		m_JoystickDevice[i].ResetContent();

		m_JoystickDevice[i].AddItem("None",
		                            MAKELPARAM(0, (int)AnalogueInputDevice::None));

		size_t Count = m_JoystickController.GetDeviceCount();

		for (size_t DeviceIndex = 0; DeviceIndex < Count; DeviceIndex++)
		{
			const JoystickDevice& Device = m_JoystickController.GetDevice(DeviceIndex);

			m_JoystickDevice[i].AddItem(Device.GetName().c_str(),
			                            MAKELPARAM(DeviceIndex, (int)AnalogueInputDevice::Joystick));
		}

		m_JoystickDevice[i].AddItem("Mouse",
		                            MAKELPARAM(0, (int)AnalogueInputDevice::Mouse));

		SelectedIndex = m_JoystickDevice[i].FindItemData(SelectedItem);

		if (SelectedIndex == CB_ERR)
		{
			SelectedIndex = 0;
		}

		m_JoystickDevice[i].SetCurSel(SelectedIndex);

		OnDeviceSelChange(i);
	}
}

/****************************************************************************/

void JoystickDialog::OnDeviceSelChange(int Index)
{
	int SelectedIndex = m_JoystickDevice[Index].GetCurSel();

	LPARAM ItemData = m_JoystickDevice[Index].GetItemData(SelectedIndex);

	m_AnalogueInputDevice[Index] = static_cast<AnalogueInputDevice>(HIWORD(ItemData));

	m_AnalogInput[Index].ResetContent();
	m_ButtonInput[Index].ResetContent();

	switch (m_AnalogueInputDevice[Index])
	{
		case AnalogueInputDevice::None:
			m_AnalogInput[Index].EnableWindow(false);
			m_ButtonInput[Index].EnableWindow(false);

			m_AnalogInput[Index].AddString("None");
			m_ButtonInput[Index].AddString("None");

			m_AnalogInput[Index].SetCurSel(0);
			m_ButtonInput[Index].SetCurSel(0);
			break;

		case AnalogueInputDevice::Joystick: {
			m_AnalogInput[Index].EnableWindow(true);
			m_ButtonInput[Index].EnableWindow(true);

			const OptionValue* pOptionValue = JoystickControlOptions;

			for (; pOptionValue->Name != nullptr; pOptionValue++)
			{
				m_AnalogInput[Index].AddItem(pOptionValue->Name, pOptionValue->Value);
			}

			SelectedIndex = m_AnalogInput[Index].FindItemData(m_JoystickControl[Index]);

			if (SelectedIndex == CB_ERR)
			{
				SelectedIndex = 0;
			}

			m_AnalogInput[Index].SetCurSel(SelectedIndex);

			pOptionValue = JoystickButtonOptions;

			for (; pOptionValue->Name != nullptr; pOptionValue++)
			{
				m_ButtonInput[Index].AddItem(pOptionValue->Name, pOptionValue->Value);
			}

			SelectedIndex = m_ButtonInput[Index].FindItemData(m_JoystickButton[Index]);

			if (SelectedIndex == CB_ERR)
			{
				SelectedIndex = 0;
			}

			m_ButtonInput[Index].SetCurSel(SelectedIndex);
			break;
		}

		case AnalogueInputDevice::Mouse: {
			m_AnalogInput[Index].EnableWindow(true);
			m_ButtonInput[Index].EnableWindow(true);

			const OptionValue* pOptionValue = MousestickOptions;

			for (; pOptionValue->Name != nullptr; pOptionValue++)
			{
				m_AnalogInput[Index].AddString(pOptionValue->Name);
			}

			pOptionValue = MouseButtonOptions;

			for (; pOptionValue->Name != nullptr; pOptionValue++)
			{
				m_ButtonInput[Index].AddItem(pOptionValue->Name, pOptionValue->Value);
			}

			m_AnalogInput[Index].SetCurSel(static_cast<int>(m_MousestickType[Index]));

			SelectedIndex = m_ButtonInput[Index].FindItemData(m_JoystickMouseButton[Index]);

			if (SelectedIndex == CB_ERR)
			{
				SelectedIndex = 0;
			}

			m_ButtonInput[Index].SetCurSel(SelectedIndex);
			break;
		}
	}
}

/****************************************************************************/

void JoystickDialog::UpdateSelected()
{
	for (int Joystick = 0; Joystick < 2; Joystick++)
	{
		int DeviceIndex = m_JoystickDevice[Joystick].GetCurSel();
		LPARAM ItemData = m_JoystickDevice[Joystick].GetItemData(DeviceIndex);

		m_AnalogueInputDevice[Joystick] = static_cast<AnalogueInputDevice>(HIWORD(ItemData));

		switch (m_AnalogueInputDevice[Joystick])
		{
			case AnalogueInputDevice::Joystick: {
				DWORD Index = LOWORD(ItemData);
				const JoystickDevice& Device = m_JoystickController.GetDevice(Index);

				m_JoystickDeviceType[Joystick] = Device.GetType();
				m_JoystickDeviceID[Joystick] = Device.GetID();

				int Selected = m_AnalogInput[Joystick].GetCurSel();
				m_JoystickControl[Joystick] = (int)m_AnalogInput[Joystick].GetItemData(Selected);

				Selected = m_ButtonInput[Joystick].GetCurSel();
				m_JoystickButton[Joystick] = (int)m_ButtonInput[Joystick].GetItemData(Selected);

				m_MousestickType[Joystick] = MousestickType::Analogue;
				m_JoystickMouseButton[Joystick] = 0;
				break;
			}

			case AnalogueInputDevice::Mouse: {
				m_JoystickDeviceType[Joystick] = JoystickDeviceType::XInput;
				m_JoystickDeviceID[Joystick].clear();

				m_JoystickControl[Joystick] = 0;

				int Selected = m_ButtonInput[Joystick].GetCurSel();
				m_JoystickMouseButton[Joystick] = (int)m_ButtonInput[Joystick].GetItemData(Selected);

				m_JoystickButton[Joystick] = 0;

				Selected = m_AnalogInput[Joystick].GetCurSel();
				m_MousestickType[Joystick] = static_cast<MousestickType>(Selected);
				break;
			}

			case AnalogueInputDevice::None:
			default:
				m_JoystickDeviceType[Joystick] = JoystickDeviceType::XInput;
				m_JoystickDeviceID[Joystick].clear();
				m_JoystickControl[Joystick] = 0;
				m_JoystickButton[Joystick] = 0;
				m_MousestickType[Joystick] = MousestickType::Analogue;
				m_JoystickMouseButton[Joystick] = 0;
				break;
		}
	}
}

/****************************************************************************/

AnalogueInputDevice JoystickDialog::GetAnalogueInputDevice(int Joystick) const
{
	return m_AnalogueInputDevice[Joystick];
}

/****************************************************************************/

int JoystickDialog::GetJoystickControl(int Joystick) const
{
	return m_JoystickControl[Joystick];
}

/****************************************************************************/

int JoystickDialog::GetJoystickButton(int Joystick) const
{
	return m_JoystickButton[Joystick];
}

/****************************************************************************/

MousestickType JoystickDialog::GetMousestickType(int Joystick) const
{
	return m_MousestickType[Joystick];
}

/****************************************************************************/

int JoystickDialog::GetJoystickMouseButton(int Joystick) const
{
	return m_JoystickMouseButton[Joystick];
}

/****************************************************************************/

#endif // WIN32
