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

#ifndef LIST_VIEW_HEADER
#define LIST_VIEW_HEADER

#include <string>

class ListView
{
	public:
		ListView();
		ListView(const ListView&) = delete;
		ListView& operator=(const ListView&) = delete;

	public:
		void Init(HWND hWnd, UINT nDlgItemID);
		HWND GetHWnd() const;

		void SetExtendedStyle(UINT Style);

		int InsertColumn(UINT uCol, const char* pszText, int iAlignment, UINT uWidth);
		void SetColumnWidth(UINT uCol, int Width);

		int InsertItem(UINT uRow, UINT uCol, const char* pszText, LPARAM lParam);
		void DeleteAllItems();
		int GetItemCount();
		LPARAM GetItemData(UINT uRow);
		void SetItemText(UINT uRow, UINT uCol, const LPTSTR pszText);
		int FindItemData(LPARAM ItemData);
		int GetNextItem(int Index, UINT Flags);

		int SubItemHitTest(const POINT& Point);

		void SetFocus();
		void SelectItem(int Index);
		int GetSelectedCount();
		int GetSelectionMark();
		void SetSelectionMark(int Index);

	private:
		HWND m_hWnd;
};

#endif
