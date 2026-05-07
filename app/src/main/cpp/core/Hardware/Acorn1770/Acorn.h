/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman

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

// Acorn 1770 DFS Board Drive Controller Chip DLL
// (C) September 2001 - Richard Gellman

#ifndef ACORN_1770_HEADER
#define ACORN_1770_HEADER

// Forward declaration
struct DriveControlBlock;

unsigned char Acorn1770SetDriveControl(unsigned char value);
unsigned char Acorn1770GetDriveControl(unsigned char value);
void Acorn1770GetBoardProperties(struct DriveControlBlock *FDBoard);

#endif
