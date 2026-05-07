/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2023 Chris Needham

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

#include <vector>

#include "Dialog.h"

/****************************************************************************/

Dialog::Dialog(HINSTANCE hInstance,
               HWND hwndParent,
               int DialogID) :
	m_hInstance(hInstance),
	m_hwndParent(hwndParent),
	m_DialogID(DialogID),
	m_hwnd(nullptr)
{
}

/****************************************************************************/

bool Dialog::DoModal()
{
	#ifdef WIN32

	// Show dialog box
	INT_PTR Result = DialogBoxParam(m_hInstance,
	                                MAKEINTRESOURCE(m_DialogID),
	                                m_hwndParent,
	                                sDlgProc,
	                                reinterpret_cast<LPARAM>(this));

	return Result == IDOK;

	#else

	return false;

	#endif
}

/****************************************************************************/

INT_PTR CALLBACK Dialog::sDlgProc(HWND   hwnd,
                                  UINT   nMessage,
                                  WPARAM wParam,
                                  LPARAM lParam)
{
	#ifdef WIN32

	Dialog* dialog;

	if (nMessage == WM_INITDIALOG)
	{
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		dialog = reinterpret_cast<Dialog*>(lParam);
		dialog->m_hwnd = hwnd;

		DisableRoundedCorners(hwnd);

		CentreWindow(dialog->m_hwndParent, hwnd);
	}
	else
	{
		dialog = reinterpret_cast<Dialog*>(
			GetWindowLongPtr(hwnd, DWLP_USER)
		);
	}

	if (dialog)
	{
		return dialog->DlgProc(nMessage, wParam, lParam);
	}
	else
	{
		return FALSE;
	}

	#else

	UNREFERENCED_PARAMETER(hwnd);
	UNREFERENCED_PARAMETER(nMessage);
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);

	return FALSE;

	#endif
}

/****************************************************************************/

std::string Dialog::GetDlgItemText(int nID)
{
	#ifdef WIN32

	int Length = GetWindowTextLength(GetDlgItem(m_hwnd, nID));

	std::vector<char> Text;
	Text.resize(Length + 1);

	::GetDlgItemText(m_hwnd, nID, &Text[0], (int)Text.size());

	return std::string(&Text[0]);

	#else

	UNREFERENCED_PARAMETER(nID);

	return "";

	#endif
}

/****************************************************************************/

void Dialog::SetDlgItemText(int nID, const std::string& str)
{
	#ifdef WIN32

	SetWindowText(GetDlgItem(m_hwnd, nID), str.c_str());

	#else

	UNREFERENCED_PARAMETER(nID);
	UNREFERENCED_PARAMETER(str);

	#endif
}

/****************************************************************************/

bool Dialog::IsDlgItemChecked(int nID)
{
	#ifdef WIN32

	return SendDlgItemMessage(m_hwnd, nID, BM_GETCHECK, 0, 0) == BST_CHECKED;

	#else

	UNREFERENCED_PARAMETER(nID);

	return false;

	#endif
}

/****************************************************************************/

void Dialog::SetDlgItemChecked(int nID, bool bChecked)
{
	#ifdef WIN32

	SendDlgItemMessage(m_hwnd, nID, BM_SETCHECK, bChecked ? BST_CHECKED : BST_UNCHECKED, 0);

	#else

	UNREFERENCED_PARAMETER(nID);
	UNREFERENCED_PARAMETER(bChecked);

	#endif
}

/****************************************************************************/

void Dialog::SetDlgItemFocus(int nID)
{
	#ifdef WIN32

	SetFocus(GetDlgItem(m_hwnd, nID));

	#else

	UNREFERENCED_PARAMETER(nID);

	#endif
}

/****************************************************************************/

void Dialog::EnableDlgItem(int nID, bool bEnable)
{
	#ifdef WIN32

	EnableWindow(GetDlgItem(m_hwnd, nID), bEnable);

	#else

	UNREFERENCED_PARAMETER(nID);
	UNREFERENCED_PARAMETER(bEnable);

	#endif
}

/****************************************************************************/
