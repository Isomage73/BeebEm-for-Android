/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2009  Mike Wyatt

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
#include <commctrl.h>

#include "ListView.h"

#ifdef WIN32

/****************************************************************************/

ListView::ListView() :
	m_hWnd(nullptr)
{
}

/****************************************************************************/

void ListView::Init(HWND hWnd, UINT nDlgItemID)
{
	m_hWnd = GetDlgItem(hWnd, nDlgItemID);
}

/****************************************************************************/

HWND ListView::GetHWnd() const
{
	return m_hWnd;
}

/****************************************************************************/

void ListView::SetExtendedStyle(UINT Style)
{
	ListView_SetExtendedListViewStyle(m_hWnd, Style);
}

/****************************************************************************/

int ListView::InsertColumn(UINT uCol, const char* pszText, int iAlignment, UINT uWidth)
{
	LVCOLUMN lc = { 0 };
	lc.mask = LVCF_SUBITEM | LVCF_TEXT | LVCF_FMT | LVCF_WIDTH;
	lc.fmt = iAlignment;
	lc.pszText = const_cast<char*>(pszText);
	lc.iSubItem = uCol;
	lc.cx = uWidth;
	return ListView_InsertColumn(m_hWnd, uCol, &lc);
}

/****************************************************************************/

void ListView::SetColumnWidth(UINT uCol, int Width)
{
	ListView_SetColumnWidth(m_hWnd, uCol, Width);
}

/****************************************************************************/

int ListView::InsertItem(UINT uRow, UINT uCol, const char* pszText, LPARAM lParam)
{
	LVITEM li = { 0 };
	li.mask = LVIF_TEXT | LVIF_PARAM;
	li.iItem = uRow;
	li.iSubItem = uCol;
	li.pszText = const_cast<char*>(pszText);
	li.lParam = (lParam ? lParam : uRow);
	return ListView_InsertItem(m_hWnd, &li);
}

/****************************************************************************/

void ListView::DeleteAllItems()
{
	ListView_DeleteAllItems(m_hWnd);
}

/****************************************************************************/

int ListView::GetItemCount()
{
	return ListView_GetItemCount(m_hWnd);
}

/****************************************************************************/

LPARAM ListView::GetItemData(UINT uRow)
{
	LVITEM li = { 0 };
	li.mask = LVIF_PARAM;
	li.iItem = uRow;
	ListView_GetItem(m_hWnd, &li);

	return li.lParam;
}

/****************************************************************************/

void ListView::SetItemText(UINT uRow, UINT uCol, const LPTSTR pszText)
{
	ListView_SetItemText(m_hWnd, uRow, uCol, pszText);
}

/****************************************************************************/

int ListView::FindItemData(LPARAM ItemData)
{
	LVFINDINFO FindInfo;
	ZeroMemory(&FindInfo, sizeof(FindInfo));
	FindInfo.flags = LVFI_PARAM;
	FindInfo.lParam = ItemData;

	int Index = ListView_FindItem(m_hWnd,
	                              0,
	                              &FindInfo);

	return Index;
}

/****************************************************************************/

int ListView::GetNextItem(int Index, UINT Flags)
{
	return ListView_GetNextItem(m_hWnd, Index, Flags);
}

/****************************************************************************/

int ListView::SubItemHitTest(const POINT& Point)
{
	LVHITTESTINFO HitTestInfo = { 0 };
	HitTestInfo.pt = Point;
	ListView_SubItemHitTest(m_hWnd, &HitTestInfo);

	return HitTestInfo.iItem;
}

/****************************************************************************/

void ListView::SetFocus()
{
	int row = ListView_GetSelectionMark(m_hWnd);

	ListView_SetItemState(m_hWnd,
	                      row,
	                      LVIS_SELECTED | LVIS_FOCUSED,
	                      LVIS_SELECTED | LVIS_FOCUSED);

	::SetFocus(m_hWnd);
}

/****************************************************************************/

void ListView::SelectItem(int Index)
{
	ListView_SetItemState(m_hWnd,
	                      Index,
	                      LVIS_SELECTED | LVIS_FOCUSED,
	                      LVIS_SELECTED | LVIS_FOCUSED);

}

/****************************************************************************/

int ListView::GetSelectedCount()
{
	return ListView_GetSelectedCount(m_hWnd);
}

/****************************************************************************/

int ListView::GetSelectionMark()
{
	return ListView_GetSelectionMark(m_hWnd);
}

/****************************************************************************/

void ListView::SetSelectionMark(int Index)
{
	ListView_SetSelectionMark(m_hWnd, Index);
}

/****************************************************************************/

#endif // WIN32
