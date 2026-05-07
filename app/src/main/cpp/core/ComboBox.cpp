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

#include "ComboBox.h"

#ifndef BEEBEM_ANDROID

/****************************************************************************/

ComboBox::ComboBox() :
	m_hWnd(nullptr)
{
}

/****************************************************************************/

void ComboBox::Init(HWND hWnd, UINT nDlgItemID)
{
	m_hWnd = GetDlgItem(hWnd, nDlgItemID);
}

/****************************************************************************/

void ComboBox::ResetContent()
{
	ComboBox_ResetContent(m_hWnd);
}

/****************************************************************************/

int ComboBox::AddString(const char* pszItem)
{
	return ComboBox_AddString(m_hWnd, pszItem);
}

/****************************************************************************/

int ComboBox::AddItem(const char* pszItem, LPARAM ItemData)
{
	int Index = AddString(pszItem);
	SetItemData(Index, ItemData);

	return Index;
}

/****************************************************************************/

LPARAM ComboBox::GetItemData(int Index)
{
	return ComboBox_GetItemData(m_hWnd, Index);
}

/****************************************************************************/

void ComboBox::SetItemData(int Index, LPARAM ItemData)
{
	ComboBox_SetItemData(m_hWnd, Index, ItemData);
}

/****************************************************************************/

int ComboBox::FindItemData(LPARAM ItemData)
{
	int Count = ComboBox_GetCount(m_hWnd);

	for (int i = 0; i < Count; i++)
	{
		if (ComboBox_GetItemData(m_hWnd, i) == ItemData)
		{
			return i;
		}
	}

	return CB_ERR;
}

/****************************************************************************/

int ComboBox::GetCurSel()
{
	return ComboBox_GetCurSel(m_hWnd);
}

/****************************************************************************/

void ComboBox::SetCurSel(int Index)
{
	ComboBox_SetCurSel(m_hWnd, Index);
}

/****************************************************************************/

void ComboBox::EnableWindow(bool Enable)
{
	::EnableWindow(m_hWnd, Enable);
}

/****************************************************************************/

#else // BEEBEM_ANDROID — stub implementations

#include "ComboBox.h"
ComboBox::ComboBox() : m_hWnd(nullptr) {}
void ComboBox::Init(HWND, UINT) {}
void ComboBox::ResetContent() {}
int  ComboBox::AddString(const char *) { return 0; }
int  ComboBox::AddItem(const char *, LPARAM) { return 0; }
LPARAM ComboBox::GetItemData(int) { return 0; }
void ComboBox::SetItemData(int, LPARAM) {}
int  ComboBox::FindItemData(LPARAM) { return -1; }
int  ComboBox::GetCurSel() { return 0; }
void ComboBox::SetCurSel(int) {}
void ComboBox::EnableWindow(bool) {}

#endif // BEEBEM_ANDROID
