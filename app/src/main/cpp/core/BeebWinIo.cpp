/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  Nigel Magnay
Copyright (C) 1997  Mike Wyatt

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

// BeebEm IO support - disk, tape, state, printer, AVI capture

#include "Windows.h"

#include <assert.h>
#include <stdio.h>

#ifdef __APPLE__
#include <ctime>
#include "macos/MacPlatform.h"
#endif

#include <algorithm>

#include "BeebWin.h"
#include "6502core.h"
#include "AviWriter.h"
#include "BeebMem.h"
#include "BeebWinPrefs.h"
#include "Disc1770.h"
#include "Disc8271.h"
#include "DiscEdit.h"
#include "DiscInfo.h"
#include "DiscType.h"
#include "ExportFileDialog.h"
#include "Ext1770.h"
#include "FileDialog.h"
#include "FileType.h"
#include "FileUtils.h"
#include "KeyMap.h"
#include "Main.h"
#include "Resource.h"
#include "Serial.h"
#include "Sound.h"
#include "StringUtils.h"
#include "TapeControlDialog.h"
#include "Tube.h"
#include "UefState.h"
#include "UserVia.h"
#include "Version.h"

/****************************************************************************/

void BeebWin::SetImageName(const char *DiscName, int Drive, DiscType Type)
{
	strcpy(DiscInfo[Drive].FileName, DiscName);
	DiscInfo[Drive].Type = Type;
	DiscInfo[Drive].Loaded = true;

	#ifdef WIN32

	const int maxMenuItemLen = 100;
	char menuStr[maxMenuItemLen+1];

	const char* FileName = GetFileNameFromPath(DiscInfo[Drive].FileName);

	sprintf(menuStr, "Eject Disc %d: ", Drive);
	strncat(menuStr, FileName, maxMenuItemLen-strlen(menuStr));
	menuStr[maxMenuItemLen] = '\0';

	MENUITEMINFO mii = {0};
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STRING;
	mii.fType = MFT_STRING;
	mii.dwTypeData = menuStr;

	SetMenuItemInfo(m_hMenu, Drive == 0 ? IDM_EJECTDISC0 : IDM_EJECTDISC1, FALSE, &mii);

	#endif
}

/****************************************************************************/

void BeebWin::EjectDiscImage(int Drive)
{
	FreeDiscImage(Drive);
	Close1770Disc(Drive);

	DiscInfo[Drive].FileName[0] = '\0';
	DiscInfo[Drive].Type = DiscType::SSD;
	DiscInfo[Drive].Loaded = false;

	#ifdef WIN32

	MENUITEMINFO mii = {0};
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STRING;
	mii.fType = MFT_STRING;

	if (Drive == 0)
	{
		mii.dwTypeData = "Eject Disc 0";
		SetMenuItemInfo(m_hMenu, IDM_EJECTDISC0, FALSE, &mii);
	}
	else
	{
		mii.dwTypeData = "Eject Disc 1";
		SetMenuItemInfo(m_hMenu, IDM_EJECTDISC1, FALSE, &mii);
	}

	#endif

	SetDiscWriteProtects();
}

/****************************************************************************/

bool BeebWin::ReadDisc(int Drive, bool bCheckForPrefs)
{
	char FileName[MAX_PATH];
	FileName[0] = '\0';

	const char* Filter =
		"Auto (*.ssd;*.dsd;*.ad*;*.img;*.dos;*.fsd)\0*.ssd;*.dsd;*.adl;*.adf;*.img;*.dos;*.fsd\0"
		"ADFS Disc (*.adl;*.adf)\0*.adl;*.adf\0"
		"Single Sided Disc (*.ssd)\0*.ssd\0"
		"Double Sided Disc (*.dsd)\0*.dsd\0"
		"Single Sided Disc (*.*)\0*.*\0"
		"Double Sided Disc (*.*)\0*.*\0";

	char DefaultPath[MAX_PATH];
	DefaultPath[0] = '\0';

	m_Preferences.GetStringValue(CFG_DISCS_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, Filter);

	bool gotName = Dialog.Open();

	if (gotName)
	{
		if (bCheckForPrefs)
		{
			// Check for file specific preferences files
			CheckForLocalPrefs(FileName, true);
		}

		if (m_AutoSavePrefsFolders)
		{
			GetPathFromFileName(FileName, DefaultPath, sizeof(DefaultPath));

			m_Preferences.SetStringValue(CFG_DISCS_PATH, DefaultPath);
		}

		FileType Type = FileType::None;

		switch (Dialog.GetFilterIndex())
		{
			case 1:
			default:
				Type = GetFileTypeFromExtension(FileName);
				break;

			case 2:
				Type = FileType::ADFS;
				break;

			case 3:
			case 5:
				Type = FileType::SSD;
				break;

			case 4:
			case 6:
				Type = FileType::DSD;
				break;
		}

		// Another Master 128 Update, brought to you by Richard Gellman
		if (MachineType != Model::Master128 && MachineType != Model::MasterET)
		{
			if (Type == FileType::SSD)
			{
				if (NativeFDC)
				{
					Load8271DiscImage(FileName, Drive, 80, DiscType::SSD);
				}
				else
				{
					Load1770DiscImage(FileName, Drive, DiscType::SSD);
				}
			}
			else if (Type == FileType::DSD)
			{
				if (NativeFDC)
				{
					Load8271DiscImage(FileName, Drive, 80, DiscType::DSD);
				}
				else
				{
					Load1770DiscImage(FileName, Drive, DiscType::DSD);
				}
			}
			else if (Type == FileType::ADFS)
			{
				if (NativeFDC)
				{
					Report(MessageType::Error, "The native 8271 FDC cannot read ADFS discs");
				}
				else
				{
					Load1770DiscImage(FileName, Drive, DiscType::ADFS);
				}
			}
			else if (Type == FileType::FSD)
			{
				if (NativeFDC)
				{
					Load8271DiscImage(FileName, Drive, 0, DiscType::FSD);
					SetDiscWriteProtect(Drive, true);
				}
				else
				{
					Report(MessageType::Error, "FSD disc images are only supported with the 8271 FDC");
					return false;
				}
			}
			else if (Type == FileType::DOS || Type == FileType::IMG)
			{
				Report(MessageType::Error, "DOS and IMG format discs can only be opened when in Master 128 mode");
				return false;
			}
		}
		else
		{
			// Master 128
			if (Type == FileType::SSD)
			{
				Load1770DiscImage(FileName, Drive, DiscType::SSD);
			}
			else if (Type == FileType::DSD)
			{
				Load1770DiscImage(FileName, Drive, DiscType::DSD);
			}
			else if (Type == FileType::ADFS)
			{
				Load1770DiscImage(FileName, Drive, DiscType::ADFS); // ADFS OO La La!
			}
			else if (Type == FileType::IMG)
			{
				Load1770DiscImage(FileName, Drive, DiscType::IMG);
			}
			else if (Type == FileType::DOS)
			{
				Load1770DiscImage(FileName, Drive, DiscType::DOS);
			}
			else if (Type == FileType::FSD)
			{
				Report(MessageType::Error, "FSD disc images are only supported with the 8271 FDC");
				return false;
			}
		}

		// Write protect the disc
		if (m_WriteProtectOnLoad)
		{
			SetDiscWriteProtect(Drive, true);
		}
	}

	return(gotName);
}

/****************************************************************************/

bool BeebWin::Load1770DiscImage(const char *FileName, int Drive, DiscType Type)
{
	Disc1770Result Result = ::Load1770DiscImage(FileName, Drive, Type);

	if (Result == Disc1770Result::OpenedReadWrite)
	{
		SetImageName(FileName, Drive, Type);
		EnableMenuItem(Drive == 0 ? IDM_WRITE_PROTECT_DISC0 : IDM_WRITE_PROTECT_DISC1, true);

		return true;
	}
	else if (Result == Disc1770Result::OpenedReadOnly)
	{
		SetImageName(FileName, Drive, Type);
		EnableMenuItem(Drive == 0 ? IDM_WRITE_PROTECT_DISC0 : IDM_WRITE_PROTECT_DISC1, false);

		return true;
	}
	else
	{
		// Disc1770Result::Failed
		Report(MessageType::Error, "Could not open disc file:\n  %s",
		       FileName);

		return false;
	}
}

/****************************************************************************/

bool BeebWin::Load8271DiscImage(const char *FileName, int Drive, int Tracks, DiscType Type)
{
	Disc8271Result Result;

	if (Type == DiscType::SSD)
	{
		Result = LoadSimpleDiscImage(FileName, Drive, 0, Tracks);
	}
	else if (Type == DiscType::DSD)
	{
		Result = LoadSimpleDSDiscImage(FileName, Drive, Tracks);
	}
	else
	{
		assert(Type == DiscType::FSD);
		Result = LoadFSDDiscImage(FileName, Drive);
	}

	if (Result == Disc8271Result::Success)
	{
		SetImageName(FileName, Drive, Type);

		return true;
	}
	else if (Result == Disc8271Result::InvalidFSD)
	{
		Report(MessageType::Error, "Not a valid FSD file:\n  %s", FileName);
	}
	else if (Result == Disc8271Result::InvalidTracks)
	{
		Report(MessageType::Error,
		       "Could not open disc file:\n  %s\n\nInvalid number of tracks",
		       FileName);

		Report(MessageType::Error, "Not a valid FSD file:\n  %s", FileName);
	}
	else
	{
		Report(MessageType::Error, "Could not open disc file:\n  %s", FileName);
	}

	return false;
}

/****************************************************************************/

void BeebWin::LoadTape()
{
	char FileName[MAX_PATH];
	FileName[0] = '\0';

	const char* Filter =
		"Auto (*.uef;*.csw)\0*.uef;*.csw\0"
		"UEF Tape File (*.uef)\0*.uef\0"
		"CSW Tape File (*.csw)\0*.csw\0";

	char DefaultPath[MAX_PATH];
	DefaultPath[0] = '\0';

	m_Preferences.GetStringValue(CFG_TAPES_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, Filter);

	if (Dialog.Open())
	{
		if (m_AutoSavePrefsFolders)
		{
			GetPathFromFileName(FileName, DefaultPath, sizeof(DefaultPath));

			m_Preferences.SetStringValue(CFG_TAPES_PATH, DefaultPath);
		}

		LoadTape(FileName);
	}
}

/****************************************************************************/

bool BeebWin::LoadTape(const char *FileName)
{
	bool Success = false;

	if (HasFileExt(FileName, ".uef"))
	{
		Success = LoadUEFTape(FileName);
	}
	else if (HasFileExt(FileName, ".csw"))
	{
		Success = LoadCSWTape(FileName);
	}
	else
	{
		Report(MessageType::Error, "Unknown tape format:\n  %a", FileName);
	}

	if (Success && TapeControlEnabled)
	{
		TapeControlSetFileName(FileName);
	}

	return Success;
}

/****************************************************************************/

bool BeebWin::NewTape(char *FileName, int Size)
{
	char DefaultPath[MAX_PATH];
	const char* filter = "UEF Tape File (*.uef)\0*.uef\0";

	m_Preferences.GetStringValue(CFG_TAPES_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog Dialog(m_hWnd, FileName, Size, DefaultPath, filter);

	if (!Dialog.Save())
	{
		FileName[0] = '\0';
		return false;
	}

	// Add a file extension if the user did not specify one
	if (strchr(FileName, '.') == NULL)
	{
		strcat(FileName, ".uef");
	}

	SerialNewTape();

	return UEFFile.Save(FileName) == UEFResult::Success;
}

/*******************************************************************/

void BeebWin::SelectFDC()
{
	#ifdef WIN32

	char DefaultPath[MAX_PATH];
	char FileName[256];
	FileName[0] = '\0';

	const char* filter = "FDC Extension Board Plugin DLL (*.dll)\0*.dll\0";

	strcpy(DefaultPath, m_AppPath);
	AppendPath(DefaultPath, "Hardware");

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);

	if (Dialog.Open())
	{
		// Make path relative to app path
		const size_t AppPathLen = strlen(m_AppPath);

		if (_strnicmp(FileName, m_AppPath, AppPathLen) == 0)
		{
			size_t RelPathStartPos = AppPathLen;

			if (FileName[AppPathLen] == '\\')
			{
				RelPathStartPos++;
			}

			strcpy(FDCDLL, FileName + RelPathStartPos);
		}
		else
		{
			strcpy(FDCDLL, FileName);
		}

		LoadFDC(FDCDLL, true);
	}

	#endif
}

/****************************************************************************/
void BeebWin::NewDiscImage(int Drive)
{
	#ifdef WIN32

	const char* Filter =
		"Single Sided Disc (*.ssd)\0*.ssd\0"
		"Double Sided Disc (*.dsd)\0*.dsd\0"
		"Single Sided Disc (*.*)\0*.*\0"
		"Double Sided Disc (*.*)\0*.*\0"
		"ADFS M (80 Track) Disc (*.adf)\0*.adf\0"
		"ADFS L (160 Track) Disc (*.adl)\0*.adl\0";

	char DefaultPath[MAX_PATH];
	DefaultPath[0] = '\0';

	m_Preferences.GetStringValue(CFG_DISCS_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	int FilterIndex;
	m_Preferences.GetDecimalValue(CFG_DISCS_FILTER, FilterIndex, 1);

	if ((MachineType != Model::Master128 && MachineType != Model::MasterET) && NativeFDC && FilterIndex >= 5)
		FilterIndex = 1;

	char FileName[MAX_PATH];
	FileName[0] = '\0';

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, Filter);
	Dialog.SetFilterIndex(FilterIndex);

	if (Dialog.Save())
	{
		FilterIndex = Dialog.GetFilterIndex();

		if (m_AutoSavePrefsFolders)
		{
			GetPathFromFileName(FileName, DefaultPath, sizeof(DefaultPath));

			m_Preferences.SetStringValue(CFG_DISCS_PATH, DefaultPath);
			m_Preferences.SetDecimalValue(CFG_DISCS_FILTER, FilterIndex);
		}

		/* Add a file extension if the user did not specify one */
		if (strchr(FileName, '.') == NULL)
		{
			if (FilterIndex == 1 || FilterIndex == 3)
				strcat(FileName, ".ssd");
			else if (FilterIndex == 2 || FilterIndex == 4)
				strcat(FileName, ".dsd");
			else if (FilterIndex == 5)
				strcat(FileName, ".adf");
			else if (FilterIndex == 6)
				strcat(FileName, ".adl");
		}

		if (FilterIndex == 1 || FilterIndex == 3)
		{
			CreateDFSDiscImage(FileName, Drive, 1, 80);
		}
		else if (FilterIndex == 2 || FilterIndex == 4)
		{
			CreateDFSDiscImage(FileName, Drive, 2, 80);
		}
		else if (FilterIndex == 5 || FilterIndex == 6)
		{
			const int Tracks = FilterIndex == 5 ? 80 : 160;
			bool Success = CreateADFSImage(FileName, Tracks);
			if (Success)
			{
				Load1770DiscImage(FileName, Drive, DiscType::ADFS);
			}
		}

		// Allow disc writes
		SetDiscWriteProtect(Drive, false);

		DiscInfo[Drive].Loaded = true;
		strcpy(DiscInfo[Drive].FileName, FileName);
	}

	#else

	UNREFERENCED_PARAMETER(Drive);

	#endif
}

/****************************************************************************/
void BeebWin::CreateDFSDiscImage(const char *FileName, int Drive,
                                 int Heads, int Tracks)
{
	FILE *outfile = fopen(FileName, "wb");

	if (outfile == nullptr)
	{
		Report(MessageType::Error, "Could not create disc file:\n  %s",
		       FileName);
		return;
	}

	bool Success = true;

	const int NumSectors = Tracks * 10;

	// Create the first two sectors on each side - the rest will get created when
	// data is written to it
	for (int Sector = 0; Success && Sector < (Heads == 1 ? 2 : 12); Sector++)
	{
		unsigned char SecData[256];
		memset(SecData, 0, sizeof(SecData));

		if (Sector == 1 || Sector == 11)
		{
			SecData[6] = (NumSectors >> 8) & 0xff;
			SecData[7] = NumSectors & 0xff;
		}

		if (fwrite(SecData, 1, 256, outfile) != 256)
			Success = false;
	}

	if (fclose(outfile) != 0)
		Success = false;

	if (!Success)
	{
		Report(MessageType::Error, "Failed writing to disc file:\n  %s",
		       FileName);
	}
	else
	{
		// Now load the new image into the correct drive

		if (MachineType == Model::Master128 || !NativeFDC)
		{
			Load1770DiscImage(FileName, Drive, Heads == 1 ? DiscType::SSD : DiscType::DSD);
		}
		else
		{
			Load8271DiscImage(FileName, Drive, Tracks, Heads == 1 ? DiscType::SSD : DiscType::DSD);
		}
	}
}

/****************************************************************************/

void BeebWin::ToggleWriteProtect(int Drive)
{
	if (DiscInfo[Drive].Type != DiscType::FSD)
	{
		SetDiscWriteProtect(Drive, !m_WriteProtectDisc[Drive]);
	}
}

void BeebWin::SetDiscWriteProtect(int Drive, bool WriteProtect)
{
	m_WriteProtectDisc[Drive] = WriteProtect;

	// Keep 8271 and 1770 write enable flags in sync
	DiscWriteEnable(Drive, !WriteProtect);
	DWriteable[Drive] = !WriteProtect;

	int MenuItemID = Drive == 0 ? IDM_WRITE_PROTECT_DISC0 : IDM_WRITE_PROTECT_DISC1;

	CheckMenuItem(MenuItemID, m_WriteProtectDisc[Drive]);
}

void BeebWin::SetDiscWriteProtects()
{
	if ((MachineType != Model::Master128 && MachineType != Model::MasterET) && NativeFDC)
	{
		m_WriteProtectDisc[0] = !IsDiscWritable(0);
		m_WriteProtectDisc[1] = !IsDiscWritable(1);

		CheckMenuItem(IDM_WRITE_PROTECT_DISC0, m_WriteProtectDisc[0]);
		CheckMenuItem(IDM_WRITE_PROTECT_DISC1, m_WriteProtectDisc[1]);
	}
	else
	{
		CheckMenuItem(IDM_WRITE_PROTECT_DISC0, !DWriteable[0]);
		CheckMenuItem(IDM_WRITE_PROTECT_DISC1, !DWriteable[1]);
	}
}

/****************************************************************************/

void BeebWin::SetPrinterPort(PrinterPortType PrinterPort)
{
	if (PrinterPort == PrinterPortType::File)
	{
		if (GetPrinterFileName())
		{
			// If printer is enabled then need to
			// disable it before changing file
			if (PrinterEnabled)
			{
				EnablePrinter(false);
			}

			// Add file name to menu
			std::string MenuString = "File: ";
			MenuString += m_PrinterFileName;

			ModifyMenu(m_hMenu, IDM_PRINTER_FILE,
			           MF_BYCOMMAND, IDM_PRINTER_FILE,
			           MenuString.c_str());

			m_PrinterPort = PrinterPort;

			TranslatePrinterPort();
		}
	}
	else if (PrinterPort == PrinterPortType::Clipboard)
	{
		if (PrinterEnabled)
		{
			EnablePrinter(false);
		}

		m_PrinterPort = PrinterPort;

		TranslatePrinterPort();
	}
	else
	{
		if (PrinterPort != m_PrinterPort)
		{
			// If printer is enabled then need to
			// disable it before changing file
			if (PrinterEnabled)
			{
				EnablePrinter(false);
			}

			m_PrinterPort = PrinterPort;

			TranslatePrinterPort();
		}
	}

	UpdatePrinterPortMenu();
}

void BeebWin::UpdatePrinterPortMenu()
{
	static struct { UINT ID; PrinterPortType PrinterPort; } MenuItems[] =
	{
		{ IDM_PRINTER_FILE,      PrinterPortType::File },
		{ IDM_PRINTER_CLIPBOARD, PrinterPortType::Clipboard },
		{ IDM_PRINTER_LPT1,      PrinterPortType::Lpt1 },
		{ IDM_PRINTER_LPT2,      PrinterPortType::Lpt2 },
		{ IDM_PRINTER_LPT3,      PrinterPortType::Lpt3 },
		{ IDM_PRINTER_LPT4,      PrinterPortType::Lpt4 },
	};

	UINT SelectedMenuItemID = 0;

	for (size_t i = 0; i < _countof(MenuItems); i++)
	{
		if (m_PrinterPort == MenuItems[i].PrinterPort)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(
		IDM_PRINTER_FILE,
		IDM_PRINTER_LPT4,
		SelectedMenuItemID
	);

	EnableMenuItem(IDM_PRINTER_FILE, !PrinterEnabled);
	EnableMenuItem(IDM_PRINTER_CLIPBOARD, !PrinterEnabled);
	EnableMenuItem(IDM_PRINTER_LPT1, !PrinterEnabled);
	EnableMenuItem(IDM_PRINTER_LPT2, !PrinterEnabled);
	EnableMenuItem(IDM_PRINTER_LPT3, !PrinterEnabled);
	EnableMenuItem(IDM_PRINTER_LPT4, !PrinterEnabled);
}

bool BeebWin::GetPrinterFileName()
{
	#ifdef Win32

	char StartPath[MAX_PATH];
	char FileName[MAX_PATH];
	FileName[0] = '\0';

	const char* Filter = "Printer Output (*.*)\0*.*\0";

	if (m_PrinterFileName.empty())
	{
		strcpy(StartPath, m_UserDataPath);
		FileName[0] = '\0';
	}
	else
	{
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_splitpath(m_PrinterFileName.c_str(), drive, dir, fname, ext);
		_makepath(StartPath, drive, dir, NULL, NULL);
		_makepath(FileName, NULL, NULL, fname, ext);
	}

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), StartPath, Filter);
	Dialog.SetTitle("Select printer output file");
	Dialog.NoOverwritePrompt();

	bool Success = Dialog.Save();

	if (Success)
	{
		m_PrinterFileName = FileName;
	}

	return Success;

	#else

	return false;

	#endif
}

/****************************************************************************/

bool BeebWin::EnablePrinter(bool Enable)
{
	bool Success = true;

	if (Enable)
	{
		m_PrinterBuffer.clear();
		KillTimer(m_hWnd, TIMER_PRINTER);

		if (m_PrinterPort == PrinterPortType::File)
		{
			if (m_PrinterFileName.empty())
			{
				GetPrinterFileName();
			}

			if (!m_PrinterFileName.empty())
			{
				Success = PrinterEnable(m_PrinterFileName.c_str());
			}
		}
		else if (m_PrinterPort == PrinterPortType::Clipboard)
		{
			Success = PrinterEnable(nullptr);
		}
		else
		{
			Success = PrinterEnable(m_PrinterDevice.c_str());
		}
	}
	else
	{
		PrinterDisable();
	}

	if (Success)
	{
		PrinterEnabled = Enable;
		CheckMenuItem(IDM_PRINTERONOFF, PrinterEnabled);
	}

	UpdatePrinterPortMenu();

	return Success;
}

/****************************************************************************/

void BeebWin::TranslatePrinterPort()
{
	switch (m_PrinterPort)
	{
		case PrinterPortType::File:
			m_PrinterDevice = m_PrinterFileName;
			break;

		case PrinterPortType::Clipboard:
			m_PrinterDevice.clear();
			break;

		case PrinterPortType::Lpt1:
		default:
			m_PrinterDevice = "LPT1";
			break;

		case PrinterPortType::Lpt2:
			m_PrinterDevice = "LPT2";
			break;

		case PrinterPortType::Lpt3:
			m_PrinterDevice = "LPT3";
			break;

		case PrinterPortType::Lpt4:
			m_PrinterDevice = "LPT4";
			break;
	}
}

/****************************************************************************/

void BeebWin::SetVideoCaptureResolution(VideoCaptureResolution Resolution)
{
	m_VideoCaptureResolution = Resolution;

	UpdateVideoCaptureResolutionMenu();
}

void BeebWin::SetVideoCaptureFrameSkip(int FrameSkip)
{
	m_AviFrameSkip = FrameSkip;

	UpdateVideoCaptureFrameSkipMenu();
}

void BeebWin::UpdateVideoCaptureMenu()
{
	UpdateVideoCaptureResolutionMenu();
	UpdateVideoCaptureFrameSkipMenu();

	EnableMenuItem(IDM_CAPTUREVIDEO, !IsCapturing());
	EnableMenuItem(IDM_ENDVIDEO, IsCapturing());
}

void BeebWin::UpdateVideoCaptureResolutionMenu()
{
	static const struct { UINT ID; VideoCaptureResolution Resolution; } MenuItems[] =
	{
		{ IDM_VIDEORES1, VideoCaptureResolution::Display },
		{ IDM_VIDEORES2, VideoCaptureResolution::_640x512 },
		{ IDM_VIDEORES3, VideoCaptureResolution::_320x256 }
	};

	UINT SelectedMenuItemID = 0;

	for (size_t i = 0; i < _countof(MenuItems); i++)
	{
		if (m_VideoCaptureResolution == MenuItems[i].Resolution)
		{
			SelectedMenuItemID = MenuItems[i].ID;
		}

		EnableMenuItem(MenuItems[i].ID, !IsCapturing());
	}

	CheckMenuRadioItem(IDM_VIDEORES1, IDM_VIDEORES3, SelectedMenuItemID);
}

void BeebWin::UpdateVideoCaptureFrameSkipMenu()
{
	static const struct { UINT ID; int FrameSkip; } MenuItems[] =
	{
		{ IDM_VIDEOSKIP0, 0 },
		{ IDM_VIDEOSKIP1, 1 },
		{ IDM_VIDEOSKIP2, 2 },
		{ IDM_VIDEOSKIP3, 3 },
		{ IDM_VIDEOSKIP4, 4 },
		{ IDM_VIDEOSKIP5, 5 }
	};

	UINT SelectedMenuItemID = 0;

	for (size_t i = 0; i < _countof(MenuItems); i++)
	{
		if (m_AviFrameSkip == MenuItems[i].FrameSkip)
		{
			SelectedMenuItemID = MenuItems[i].ID;
		}

		EnableMenuItem(MenuItems[i].ID, !IsCapturing());
	}

	CheckMenuRadioItem(IDM_VIDEOSKIP0, IDM_VIDEOSKIP5, SelectedMenuItemID);
}

/****************************************************************************/

void BeebWin::CaptureVideo()
{
	#ifdef WIN32

	char DefaultPath[MAX_PATH];
	char FileName[MAX_PATH];
	FileName[0] = '\0';

	const char* filter = "AVI File (*.avi)\0*.avi\0";

	m_Preferences.GetStringValue(CFG_AVI_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);

	if (Dialog.Save())
	{
		// Add avi extension
		if (!HasFileExt(FileName, ".avi"))
		{
			strcat(FileName,".avi");
		}

		if (m_AutoSavePrefsFolders)
		{
			GetPathFromFileName(FileName, DefaultPath, sizeof(DefaultPath));

			m_Preferences.SetStringValue(CFG_AVI_PATH, DefaultPath);
		}

		// Close AVI file if currently capturing
		EndVideo();

		aviWriter = new AviWriter();

		WAVEFORMATEX wf;
		WAVEFORMATEX *wfp = nullptr;

		if (SoundEnabled)
		{
			memset(&wf, 0, sizeof(WAVEFORMATEX));
			wf.wFormatTag = WAVE_FORMAT_PCM;
			wf.nChannels = 1;
			wf.nSamplesPerSec = SoundSampleRate;
			wf.wBitsPerSample = 8;
			wf.nBlockAlign = 1;
			wf.nAvgBytesPerSec = SoundSampleRate;
			wf.cbSize = 0;
			wfp = &wf;
		}

		// Create DIB for AVI frames
		m_AviBitmapInfo = m_BitmapInfo;

		if (m_VideoCaptureResolution == VideoCaptureResolution::Display)
		{
			// Must be multiple of 4
			m_AviBitmapInfo.Header.biWidth = m_XWinSize & (~3);
			m_AviBitmapInfo.Header.biHeight = m_YWinSize;
		}
		else if (m_VideoCaptureResolution == VideoCaptureResolution::_640x512)
		{
			m_AviBitmapInfo.Header.biWidth = 640;
			m_AviBitmapInfo.Header.biHeight = 512;
		}
		else
		{
			m_AviBitmapInfo.Header.biWidth = 320;
			m_AviBitmapInfo.Header.biHeight = 256;
		}

		m_AviBitmapInfo.Header.biSizeImage = m_AviBitmapInfo.Header.biWidth * m_AviBitmapInfo.Header.biHeight;

		m_AviDC = CreateCompatibleDC(nullptr);
		m_AviDIB = CreateDIBSection(m_AviDC,
		                            (BITMAPINFO*)&m_AviBitmapInfo,
		                            DIB_RGB_COLORS,
		                            (void**)&m_AviScreen,
		                            nullptr,
		                            0);

		if (SelectObject(m_AviDC, m_AviDIB) == nullptr)
		{
			Report(MessageType::Error, "Failed to initialise AVI buffers");

			EndVideo();
		}
		else
		{
			m_AviFrameSkipCount = 0;
			m_AviFrameCount = 0;

			HRESULT hr = aviWriter->Initialise(FileName,
			                                   wfp,
			                                   &m_AviBitmapInfo,
			                                   (int)(50 / (m_AviFrameSkip + 1)));

			if (FAILED(hr))
			{
				Report(MessageType::Error, "Failed to create AVI file");

				EndVideo();
			}
		}
	}

	#endif
}

/****************************************************************************/

void BeebWin::EndVideo()
{
	#ifdef WIN32

	if (aviWriter != nullptr)
	{
		delete aviWriter;
		aviWriter = nullptr;
	}

	if (m_AviDIB != nullptr)
	{
		DeleteObject(m_AviDIB);
		m_AviDIB = nullptr;
	}

	if (m_AviDC != nullptr)
	{
		DeleteDC(m_AviDC);
		m_AviDC = nullptr;
	}

	#endif
}

bool BeebWin::IsCapturing() const
{
	#ifdef WIN32

	return aviWriter != nullptr;

	#else

	return false;

	#endif
}

/****************************************************************************/

void BeebWin::RestoreState()
{
	char FileName[MAX_PATH];
	FileName[0] = '\0';

	const char* filter = "UEF State File (*.uefstate; *.uef)\0*.uefstate;*.uef\0";

	char DefaultPath[MAX_PATH];
	DefaultPath[0] = '\0';

	m_Preferences.GetStringValue(CFG_STATES_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);

	if (Dialog.Open())
	{
		// Check for file specific preferences files
		CheckForLocalPrefs(FileName, true);

		if (m_AutoSavePrefsFolders)
		{
			GetPathFromFileName(FileName, DefaultPath, sizeof(DefaultPath));

			m_Preferences.SetStringValue(CFG_STATES_PATH, DefaultPath);
		}

		LoadUEFState(FileName);
	}
}

/****************************************************************************/

void BeebWin::SaveState()
{
	char FileName[MAX_PATH];
	FileName[0] = '\0';

	const char* Filter = "UEF State File (*.uefstate)\0*.uefstate\0";

	char DefaultPath[MAX_PATH];
	DefaultPath[0] = '\0';

	m_Preferences.GetStringValue(CFG_STATES_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, Filter);

	if (Dialog.Save())
	{
		if (m_AutoSavePrefsFolders)
		{
			GetPathFromFileName(FileName, DefaultPath, sizeof(DefaultPath));

			m_Preferences.SetStringValue(CFG_STATES_PATH, DefaultPath);
		}

		// Add UEF extension if not already set and is UEF
		if (!HasFileExt(FileName, ".uefstate"))
		{
			strcat(FileName, ".uefstate");
		}

		SaveUEFState(FileName);
	}
}

/****************************************************************************/

void BeebWin::QuickLoad()
{
	char FileName[MAX_PATH];
	strcpy(FileName, m_UserDataPath);
	AppendPath(FileName, "BeebState");
	AppendPath(FileName, "quicksave.uefstate");

	if (FileExists(FileName))
	{
		LoadUEFState(FileName);
	}
	else
	{
		// For backwards compatibility with existing quicksave files:
		strcpy(FileName, m_UserDataPath);
		AppendPath(FileName, "BeebState");
		AppendPath(FileName, "quicksave.uef");
		LoadUEFState(FileName);
	}
}

/****************************************************************************/

void BeebWin::QuickSave()
{
	char FileName1[MAX_PATH];
	char FileName2[MAX_PATH];

	// Bump old quicksave files down
	for (int i = 1; i <= 9; ++i)
	{
		char FileName[100];
		sprintf(FileName, "quicksave%d.uefstate", i);

		strcpy(FileName1, m_UserDataPath);
		AppendPath(FileName1, "BeebState");
		AppendPath(FileName1, FileName);

		if (i == 9)
		{
			strcpy(FileName2, m_UserDataPath);
			AppendPath(FileName2, "BeebState");
			AppendPath(FileName2, "quicksave.uefstate");
		}
		else
		{
			sprintf(FileName, "quicksave%d.uefstate", i);

			strcpy(FileName2, m_UserDataPath);
			AppendPath(FileName2, "BeebState");
			AppendPath(FileName2, FileName);
		}

		MoveFileEx(FileName2, FileName1, MOVEFILE_REPLACE_EXISTING);
	}

	SaveUEFState(FileName2);
}

/****************************************************************************/

void BeebWin::LoadUEFState(const char *FileName)
{
	UEFStateResult Result = ::LoadUEFState(FileName);

	switch (Result)
	{
		case UEFStateResult::Success:
			SetRomMenu();
			SetDiscWriteProtects();
			break;

		case UEFStateResult::OpenFailed:
			Report(MessageType::Error, "Cannot open state file:\n  %s",
			       FileName);
			break;

		case UEFStateResult::ReadFailed:
			Report(MessageType::Error, "Failed to read state file:\n  %s",
			       FileName);
			break;

		case UEFStateResult::InvalidUEFFile:
			Report(MessageType::Error, "The file selected is not a UEF file:\n  %s",
			       FileName);
			break;

		case UEFStateResult::InvalidUEFVersion:
			Report(MessageType::Error,
			       "Cannot open state file:\n  %s\n\nPlease upgrade to the latest version of BeebEm",
			       FileName);
			break;

		default:
			break;
	}
}

/****************************************************************************/

void BeebWin::SaveUEFState(const char *FileName)
{
	UEFStateResult Result = ::SaveUEFState(FileName);

	switch (Result)
	{
		case UEFStateResult::Success:
			break;

		case UEFStateResult::WriteFailed:
			Report(MessageType::Error, "Failed to write state file:\n  %s",
			       FileName);
			break;

		default:
			break;
	}
}

/****************************************************************************/

void BeebWin::EnableSaveState(bool Enable)
{
	EnableMenuItem(IDM_SAVESTATE, Enable);
	EnableMenuItem(IDM_QUICKSAVE, Enable);
}

/****************************************************************************/

bool BeebWin::LoadUEFTape(const char *FileName)
{
	UEFResult Result = ::LoadUEFTape(FileName);

	switch (Result)
	{
		case UEFResult::Success:
			return true;

		case UEFResult::NotUEF:
		case UEFResult::NotTape:
			Report(MessageType::Error, "The file selected is not a UEF tape image:\n  %s",
			       FileName);
			return false;

		case UEFResult::ReadFailed:
			Report(MessageType::Error, "Failed to read UEF file:\n  %s",
			       FileName);
			return false;

		case UEFResult::NoFile:
		default:
			Report(MessageType::Error, "Cannot open UEF file:\n  %s", FileName);
			return false;
	}
}

/****************************************************************************/

bool BeebWin::LoadCSWTape(const char *FileName)
{
	CSWResult Result = ::LoadCSWTape(FileName);

	switch (Result)
	{
		case CSWResult::Success:
			return true;

		case CSWResult::ReadFailed:
			Report(MessageType::Error, "Failed to read CSW file:\n  %s",
			       FileName);
			return false;

		case CSWResult::InvalidCSWFile:
			Report(MessageType::Error, "The file selected is not a CSW file:\n  %s",
			       FileName);
			return false;

		case CSWResult::InvalidHeaderExtension:
			Report(MessageType::Error, "Failed to read CSW header extension:\n  %s",
			       FileName);
			return false;

		case CSWResult::OpenFailed:
		default:
			Report(MessageType::Error, "Cannot open CSW file:\n  %s", FileName);
			return false;
	}
}

/****************************************************************************/

// If DLLName is NULL then FDC setting is read from Preferences.cfg
// else the named DLL is read in
// If Save is true then DLL selection is saved to Preferences.cfg

bool BeebWin::LoadFDC(const char *DLLName, bool Save)
{
	Ext1770Result Result = Ext1770Result::Success;

	char CfgName[20];
	sprintf(CfgName, "FDCDLL%d", static_cast<int>(MachineType));

	Ext1770Reset();

	NativeFDC = true;

	if (DLLName == nullptr)
	{
		if (!m_Preferences.GetStringValue(CfgName, FDCDLL))
		{
			strcpy(FDCDLL, "None");
		}

		DLLName = FDCDLL;
	}

	if (strcmp(DLLName, "None") != 0)
	{
		Result = Ext1770Init(DLLName);

		if (Result == Ext1770Result::Success)
		{
			NativeFDC = false; // at last, a working DLL!
		}
		else
		{
			if (Result == Ext1770Result::LoadFailed)
			{
				Report(MessageType::Error, "Unable to load FDD Extension Board DLL\nReverting to native 8271");
			}
			else // if (Result == Ext1770Result::InvalidDLL)
			{
				Report(MessageType::Error, "Invalid FDD Extension Board DLL\nReverting to native 8271");
			}

			DLLName = "None";
		}
	}

	// Set menu options
	CheckMenuItem(IDM_8271, NativeFDC);
	CheckMenuItem(IDM_FDC_DLL, !NativeFDC);

	if (Result == Ext1770Result::Success)
	{
		if (Save)
		{
			m_Preferences.SetStringValue(CfgName, DLLName);
		}

		if (NativeFDC || MachineType == Model::Master128)
		{
			DisplayCycles = 0;
		}
		else
		{
			DisplayCycles = 7000000;
		}

		return true;
	}
	else
	{
		return false;
	}
}

void BeebWin::SetDriveControl(unsigned char Value)
{
	// This takes a value from the mem/io decoder, as written by the CPU,
	// runs it through the DLL's translator, then sends it on to the
	// 1770 FDC in Master 128 form.
	WriteFDCControlReg(::SetDriveControl(Value));
}

unsigned char BeebWin::GetDriveControl()
{
	// Same as above, but in reverse, i.e., reading.
	unsigned char temp = ReadFDCControlReg();
	return ::GetDriveControl(temp);
}

/****************************************************************************/

void BeebWin::SaveBeebEmID(FILE *SUEF)
{
	char EmuName[16];
	memset(EmuName, 0, sizeof(EmuName));

	// BeebEm Title Block
	strcpy(EmuName, "BeebEm");
	EmuName[14] = VERSION_MAJOR;
	EmuName[15] = VERSION_MINOR;
	UEFWriteBuf(EmuName, sizeof(EmuName), SUEF);
}

void BeebWin::SaveEmuUEF(FILE *SUEF)
{
	// Emulator Specifics
	// Note about this block: It should only be handled by BeebEm from UefState.cpp if
	// the UEF has been determined to be from BeebEm (Block 046C)
	UEFWrite8(static_cast<unsigned char>(MachineType), SUEF);
	UEFWrite8(NativeFDC ? 0 : 1, SUEF);
	UEFWrite8(static_cast<unsigned char>(TubeType), SUEF);
	UEFWrite16((int)m_KeyboardMapping, SUEF);
	if (m_KeyboardMapping == KeyboardMappingType::User)
	{
		UEFWriteString(m_UserKeyMapPath, SUEF);
	}
	else
	{
		char blank[256];
		memset(blank, 0, sizeof(blank));

		UEFWriteBuf(blank, sizeof(blank), SUEF);
	}

	UEFWrite8(0, SUEF);
}

void BeebWin::LoadEmuUEF(FILE *SUEF, int Version)
{
	int Type = UEFRead8(SUEF);

	if (Version <= 8 && Type == 1)
	{
		MachineType = Model::Master128;
	}
	else
	{
		MachineType = static_cast<Model>(Type);
	}

	NativeFDC = UEFRead8(SUEF) == 0;
	TubeType = static_cast<TubeDevice>(UEFRead8(SUEF));

	if (Version >= 11)
	{
		int KeyboardMapping = UEFRead16(SUEF);

		const int UEF_USER_KEYBOARD_MAPPING = 40060; // Was a menu item ID in BeebEm <= 4.19

		if (KeyboardMapping == (int)KeyboardMappingType::User || KeyboardMapping == UEF_USER_KEYBOARD_MAPPING)
		{
			char FileName[256];
			memset(FileName, 0, sizeof(FileName));

			if (Version >= 14)
			{
				UEFReadString(FileName, sizeof(FileName), SUEF);
			}
			else
			{
				UEFReadBuf(FileName, sizeof(FileName), SUEF);
			}

			GetDataPath(m_UserDataPath, FileName);

			if (ReadKeyMap(FileName, &UserKeyMap))
			{
				strcpy(m_UserKeyMapPath, FileName);
			}
			else
			{
				KeyboardMapping = (int)m_KeyboardMapping;
			}
		}

		SetKeyboardMapping(static_cast<KeyboardMappingType>(KeyboardMapping));
	}

	ResetBeebSystem(MachineType, true);
	UpdateModelMenu();
	UpdateTubeMenu();
}

/****************************************************************************/

void BeebWin::GetDataPath(const char *Folder, char *Path)
{
	char NewPath[MAX_PATH];

	if (IsRelativePath(Path))
	{
		strcpy(NewPath, Folder);
		AppendPath(NewPath, Path);
		strcpy(Path, NewPath);
	}
	else
	{
		// Absolute path - just use it as is
	}
}

/****************************************************************************/

void BeebWin::LoadUserKeyMap()
{
	char FileName[MAX_PATH];
	FileName[0] = '\0';

	const char* filter = "Key Map File (*.kmap)\0*.kmap\0";

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), m_UserDataPath, filter);

	if (Dialog.Open())
	{
		if (ReadKeyMap(FileName, &UserKeyMap))
			strcpy(m_UserKeyMapPath, FileName);
	}
}

/****************************************************************************/

void BeebWin::SaveUserKeyMap()
{
	char FileName[MAX_PATH];
	FileName[0] = '\0';

	const char* filter = "Key Map File (*.kmap)\0*.kmap\0";

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), m_UserDataPath, filter);

	if (Dialog.Save())
	{
		if (!HasFileExt(FileName, ".kmap"))
		{
			strcat(FileName,".kmap");
		}

		if (WriteKeyMap(FileName, &UserKeyMap))
		{
			strcpy(m_UserKeyMapPath, FileName);
		}
	}
}

/****************************************************************************/

// Clipboard support

// Handles the Edit/Copy menu command

void BeebWin::OnCopy()
{
	if (PrinterEnabled)
	{
		EnablePrinter(false);
	}

	m_PrinterPort = PrinterPortType::Clipboard;

	TranslatePrinterPort();
	EnablePrinter(true); // Turn printer back on
	UpdatePrinterPortMenu();

	m_PrinterBuffer.clear();

	m_ClipboardBuffer.resize(5);
	m_ClipboardLength = 5;
	m_ClipboardBuffer[0] = 2;
	m_ClipboardBuffer[1] = 'L';
	m_ClipboardBuffer[2] = '.';
	m_ClipboardBuffer[3] = 13;
	m_ClipboardBuffer[4] = 3;
	m_ClipboardIndex = 0;
}

// Handles the Edit/Paste menu command

void BeebWin::OnPaste()
{
	#ifdef WIN32

	if (!IsClipboardFormatAvailable(CF_TEXT))
		return;

	if (!OpenClipboard(m_hWnd))
		return;

	HGLOBAL hClipboardData = GetClipboardData(CF_TEXT);

	if (hClipboardData != nullptr)
	{
		size_t Size = GlobalSize(hClipboardData); // Includes NUL-terminator

		LPTSTR pData = (LPTSTR)GlobalLock(hClipboardData);

		if (pData != nullptr)
		{
			m_ClipboardBuffer.resize(Size);
			memcpy(&m_ClipboardBuffer[0], pData, Size);

			GlobalUnlock(hClipboardData);

			m_ClipboardIndex = 0;
			m_ClipboardLength = Size - 1;
		}
	}

	CloseClipboard();

	#elif defined(__APPLE__)

	{
		char buf[65536];
		int len = MacGetClipboardText(buf, (int)sizeof(buf));
		if (len > 0) {
			m_ClipboardBuffer.resize((size_t)len);
			memcpy(&m_ClipboardBuffer[0], buf, (size_t)len);
			m_ClipboardIndex = 0;
			m_ClipboardLength = (size_t)len;
		}
	}

	#endif
}

void BeebWin::ClearClipboardBuffer()
{
	m_ClipboardBuffer.clear();
	m_ClipboardIndex = 0;
	m_ClipboardLength = 0;
}

/****************************************************************************/

// Called when a character is written to the Beeb's printer port,
// and adds to the data buffered to send to the clipboard

void BeebWin::PrintChar(unsigned char Value)
{
	m_PrinterBuffer.push_back(Value);

	if (m_TranslateCRLF && Value == 0xD)
	{
		m_PrinterBuffer.push_back(0xA);
	}

	// To avoid unnecessary copies, wait 1 second after the last write
	// to put the data into the clipboard.
	SetTimer(m_hWnd, TIMER_PRINTER, 1000, nullptr);
}

void BeebWin::CopyPrinterBufferToClipboard()
{
	#ifdef WIN32

	if (m_PrinterBuffer.size() == 0)
	{
		return;
	}

	if (!OpenClipboard(m_hWnd))
	{
		return;
	}

	EmptyClipboard();

	HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, m_PrinterBuffer.size() + 1);

	if (hClipboardData == nullptr)
	{
		CloseClipboard();
		return;
	}

	unsigned char* pData = (unsigned char*)GlobalLock(hClipboardData);

	if (pData != nullptr)
	{
		memcpy(pData, &m_PrinterBuffer[0], m_PrinterBuffer.size());
		pData[m_PrinterBuffer.size()] = '\0';
		GlobalUnlock(hClipboardData);

		SetClipboardData(CF_TEXT, hClipboardData);
	}

	CloseClipboard();

	#endif
}

/****************************************************************************/
/* Disc Import / Export */

void BeebWin::ExportDiscFiles(int menuId)
{
	#ifdef WIN32

	const int Drive = menuId == IDM_DISC_EXPORT_0 || menuId == IDM_DISC_EXPORT_2 ? 0 : 1;

	if (MachineType != Model::Master128 && NativeFDC)
	{
		// 8271 controller
		if (DiscInfo[Drive].Type == DiscType::FSD)
		{
			Report(MessageType::Warning, "Export from FSD discs is not supported");
			return;
		}
	}
	else
	{
		// 1770 controller
		if (DiscInfo[Drive].Type != DiscType::SSD && DiscInfo[Drive].Type != DiscType::DSD)
		{
			// ADFS - not currently supported
			Report(MessageType::Warning, "Export from ADFS discs is not supported");
			return;
		}
	}

	// Check for no disk loaded
	if (DiscInfo[Drive].FileName[0] == '\0' ||
	    (DiscInfo[Drive].Type == DiscType::SSD && !DiscInfo[Drive].DoubleSidedSSD && (menuId == IDM_DISC_EXPORT_2 || menuId == IDM_DISC_EXPORT_3)))
	{
		Report(MessageType::Warning, "No disc loaded in drive %d", menuId - IDM_DISC_EXPORT_0);
		return;
	}

	// Read the catalogue
	const int Heads = DiscInfo[Drive].Type == DiscType::DSD || DiscInfo[Drive].DoubleSidedSSD ? 2 : 1;
	const int Side = menuId == IDM_DISC_EXPORT_0 || menuId == IDM_DISC_EXPORT_1 ? 0 : 1;

	DFS_DISC_CATALOGUE dfsCat;

	bool Success = dfs_get_catalogue(DiscInfo[Drive].FileName, Heads, Side, &dfsCat);

	if (!Success)
	{
		Report(MessageType::Error, "Failed to read catalogue from disc:\n  %s", DiscInfo[Drive].FileName);
		return;
	}

	char szExportPath[MAX_PATH];
	szExportPath[0] = '\0';

	m_Preferences.GetStringValue(CFG_EXPORT_PATH, szExportPath);
	GetDataPath(m_UserDataPath, szExportPath);

	// Show export dialog
	ExportFileDialog Dialog(hInst, m_hWnd, DiscInfo[Drive].FileName, Heads, Side, &dfsCat, szExportPath);

	if (!Dialog.DoModal())
	{
		return;
	}

	const std::string& ExportPath = Dialog.GetPath();

	if (m_AutoSavePrefsFolders)
	{
		m_Preferences.SetStringValue(CFG_EXPORT_PATH, ExportPath.c_str());
	}

	#else

	UNREFERENCED_PARAMETER(menuId);

	#endif
}

// File import
void BeebWin::ImportDiscFiles(int menuId)
{
	const int Drive = menuId == IDM_DISC_IMPORT_0 || menuId == IDM_DISC_IMPORT_2 ? 0 : 1;

	if (MachineType != Model::Master128 && NativeFDC)
	{
		// 8271 controller
		if (DiscInfo[Drive].Type == DiscType::FSD)
		{
			Report(MessageType::Warning, "Import to FSD discs is not supported");
			return;
		}
	}
	else
	{
		// 1770 controller
		if (DiscInfo[Drive].Type != DiscType::SSD && DiscInfo[Drive].Type != DiscType::DSD)
		{
			// ADFS - not currently supported
			Report(MessageType::Error, "Import to ADFS discs is not supported");
			return;
		}
	}

	// Check for no disk loaded
	if (DiscInfo[Drive].FileName[0] == '\0' ||
	    (DiscInfo[Drive].Type == DiscType::SSD && !DiscInfo[Drive].DoubleSidedSSD && (menuId == IDM_DISC_IMPORT_2 || menuId == IDM_DISC_IMPORT_3)))
	{
		Report(MessageType::Warning, "No disc loaded in drive %d", menuId - IDM_DISC_IMPORT_0);
		return;
	}

	// Read the catalogue
	const int Heads = DiscInfo[Drive].Type == DiscType::DSD || DiscInfo[Drive].DoubleSidedSSD ? 2 : 1;
	const int Side = menuId == IDM_DISC_IMPORT_0 || menuId == IDM_DISC_IMPORT_1 ? 0 : 1;

	DFS_DISC_CATALOGUE dfsCat;

	bool Success = dfs_get_catalogue(DiscInfo[Drive].FileName, Heads, Side, &dfsCat);

	if (!Success)
	{
		Report(MessageType::Error, "Failed to read catalogue from disc:\n  %s", DiscInfo[Drive].FileName);
		return;
	}

	// Get list of files to import
	char fileSelection[4096];
	fileSelection[0] = '\0';

	char szExportPath[MAX_PATH];
	szExportPath[0] = '\0';

	m_Preferences.GetStringValue(CFG_EXPORT_PATH, szExportPath);
	GetDataPath(m_UserDataPath, szExportPath);

	const char* Filter = "INF files (*.inf)\0*.inf\0" "All files (*.*)\0*.*\0";

	FileDialog Dialog(m_hWnd, fileSelection, sizeof(fileSelection), szExportPath, Filter);
	Dialog.AllowMultiSelect();
	Dialog.SetTitle("Select files to import");

	if (!Dialog.Open())
	{
		return;
	}

	// Parse the file selection string
	char *fileName = fileSelection + strlen(fileSelection) + 1;
	if (fileName[0] == 0)
	{
		// Only one file selected
		fileName = strrchr(fileSelection, '\\');
		if (fileName != NULL)
			*fileName = 0;
		fileName++;
	}
	strcpy(szExportPath, fileSelection);

	if (m_AutoSavePrefsFolders)
	{
		m_Preferences.SetStringValue(CFG_EXPORT_PATH, szExportPath);
	}

	int numFiles = 0;

	char fileNames[DFS_MAX_CAT_SIZE * 2][MAX_PATH]; // Allow user to select > cat size

	while (numFiles < DFS_MAX_CAT_SIZE*2 && fileName[0] != '\0')
	{
		// Strip .inf off
		char baseName[MAX_PATH];
		strcpy(baseName, fileName);
		int Len = (int)strlen(baseName);
		if (Len > 4 && StrCaseCmp(baseName + Len - 4, ".inf") == 0)
			baseName[Len - 4] = 0;

		// Check for duplicate
		int i;

		for (i = 0; i < numFiles; ++i)
		{
			if (StrCaseCmp(baseName, fileNames[i]) == 0)
				break;
		}

		if (i == numFiles)
		{
			strcpy(fileNames[numFiles], baseName);
			numFiles++;
		}

		fileName = fileName + strlen(fileName) + 1;
	}

	// Import the files
	int NumImported = 0;

	for (int i = 0; i < numFiles; ++i)
	{
		char szErrStr[500];

		Success = dfs_import_file(DiscInfo[Drive].FileName, Heads, Side, &dfsCat, fileNames[i], szExportPath, szErrStr);

		if (Success)
		{
			NumImported++;
		}
		else
		{
			Success = true;

			if (Report(MessageType::Confirm, szErrStr) == MessageResult::Cancel)
			{
				Success = false;
				break;
			}
		}
	}

	Report(MessageType::Info, "Files successfully imported: %d", NumImported);

	// Re-read disc image
	char szFileName[MAX_PATH];
	strcpy(szFileName, DiscInfo[Drive].FileName);

	if (MachineType != Model::Master128 && NativeFDC)
	{
		// 8271 controller
		FreeDiscImage(Drive);

		Load8271DiscImage(szFileName, Drive, 80, DiscInfo[Drive].Type);
	}
	else
	{
		// 1770 controller
		Close1770Disc(Drive);

		Load1770DiscImage(szFileName, Drive, DiscInfo[Drive].Type);
	}
}

void BeebWin::SelectHardDriveFolder()
{
	char DefaultPath[MAX_PATH];
	char FileName[MAX_PATH];
	FileName[0] = '\0';

	const char* filter =
		"Hard Drive Images (*.dat)\0*.dat\0"
		"All files (*.*)\0*.*\0";

	m_Preferences.GetStringValue(CFG_HARD_DRIVE_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);

	if (Dialog.Open())
	{
		GetPathFromFileName(FileName, DefaultPath, sizeof(DefaultPath));

		strcpy(HardDrivePath, DefaultPath);
		m_Preferences.SetStringValue(CFG_HARD_DRIVE_PATH, DefaultPath);

		MessageResult Result = Report(MessageType::Question,
		                              "The emulator must be reset for the change to take effect.\n\nDo you want to reset now?");

		if (Result == MessageResult::Yes)
		{
			ResetBeebSystem(MachineType, false);
		}
	}
}

/****************************************************************************/

// Bitmap capture support

void BeebWin::SetBitmapCaptureFormat(BitmapCaptureFormat Format)
{
	m_BitmapCaptureFormat = Format;

	UpdateBitmapCaptureFormatMenu();
}

void BeebWin::UpdateBitmapCaptureFormatMenu()
{
	static const struct { UINT ID; BitmapCaptureFormat Format; } MenuItems[] =
	{
		{ IDM_CAPTUREBMP,  BitmapCaptureFormat::Bmp },
		{ IDM_CAPTUREJPEG, BitmapCaptureFormat::Jpeg },
		{ IDM_CAPTUREGIF,  BitmapCaptureFormat::Gif },
		{ IDM_CAPTUREPNG,  BitmapCaptureFormat::Png }
	};

	UINT SelectedMenuItemID = 0;

	for (size_t i = 0; i < _countof(MenuItems); i++)
	{
		if (m_BitmapCaptureFormat == MenuItems[i].Format)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_CAPTUREBMP, IDM_CAPTUREPNG, SelectedMenuItemID);
}

void BeebWin::SetBitmapCaptureResolution(BitmapCaptureResolution Resolution)
{
	m_BitmapCaptureResolution = Resolution;

	UpdateBitmapCaptureResolutionMenu();
}

void BeebWin::UpdateBitmapCaptureResolutionMenu()
{
	static const struct { UINT ID; BitmapCaptureResolution Resolution; } MenuItems[] =
	{
		{ IDM_CAPTURERES_DISPLAY, BitmapCaptureResolution::Display },
		{ IDM_CAPTURERES_1280,    BitmapCaptureResolution::_1280x1024 },
		{ IDM_CAPTURERES_640,     BitmapCaptureResolution::_640x512 },
		{ IDM_CAPTURERES_320,     BitmapCaptureResolution::_320x256 }
	};

	UINT SelectedMenuItemID = 0;

	for (size_t i = 0; i < _countof(MenuItems); i++)
	{
		if (m_BitmapCaptureResolution == MenuItems[i].Resolution)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_CAPTURERES_DISPLAY, IDM_CAPTURERES_320, SelectedMenuItemID);
}

void BeebWin::CaptureBitmapPending(bool autoFilename)
{
	m_CaptureBitmapPending = true;
	m_CaptureBitmapAutoFilename = autoFilename;
}

bool BeebWin::GetImageEncoderClsid(const WCHAR *mimeType, CLSID *encoderClsid)
{
	#ifdef WIN32

	bool Success = false;
	UINT num;
	UINT size;
	UINT i;

	GetImageEncodersSize(&num, &size);

	if (size == 0)
	{
		return false;
	}

	ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)malloc(size);

	if (pImageCodecInfo == nullptr)
	{
		return false;
	}

	if (GetImageEncoders(num, size, pImageCodecInfo) != Ok)
	{
		goto Exit;
	}

	for (i = 0; i < num; ++i)
	{
		if (wcscmp(pImageCodecInfo[i].MimeType, mimeType) == 0)
		{
			*encoderClsid = pImageCodecInfo[i].Clsid;
			break;
		}
	}

	if (i == num)
	{
		Report(MessageType::Error, "Failed to get image encoder");
	}

	Success = true;

Exit:
	if (pImageCodecInfo != nullptr)
	{
		free(pImageCodecInfo);
	}

	return Success;

	#else

	UNREFERENCED_PARAMETER(mimeType);
	UNREFERENCED_PARAMETER(encoderClsid);

	return false;

	#endif
}

static const char* GetCaptureFormatFileExt(BitmapCaptureFormat Format)
{
	switch (Format)
	{
		case BitmapCaptureFormat::Bmp:
		default:
			return ".bmp";

		case BitmapCaptureFormat::Jpeg:
			return ".jpg";

		case BitmapCaptureFormat::Gif:
			return ".gif";

		case BitmapCaptureFormat::Png:
			return ".png";
	}
}

#ifdef WIN32

static const WCHAR* GetCaptureFormatMimeType(BitmapCaptureFormat Format)
{
	switch (Format)
	{
		case BitmapCaptureFormat::Bmp:
		default:
			return L"image/bmp";

		case BitmapCaptureFormat::Jpeg:
			return L"image/jpeg";

		case BitmapCaptureFormat::Gif:
			return L"image/gif";

		case BitmapCaptureFormat::Png:
			return L"image/png";
	}
}

#endif

bool BeebWin::GetImageFile(char *FileName, int Size)
{
	#ifdef WIN32

	bool success = false;

	FileName[0] = '\0';

	char DefaultPath[MAX_PATH];
	m_Preferences.GetStringValue(CFG_IMAGE_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	const char *fileExt = GetCaptureFormatFileExt(m_BitmapCaptureFormat);

	// A literal \0 in the format string terminates the string so use %c
	char filter[200];
	sprintf(filter, "Image File (*%s)%c*%s%c", fileExt, 0, fileExt, 0);

	FileDialog Dialog(m_hWnd, FileName, Size, DefaultPath, filter);

	if (Dialog.Save())
	{
		// Add extension
		if (!HasFileExt(FileName, fileExt))
		{
			strcat(FileName, fileExt);
		}

		if (m_AutoSavePrefsFolders)
		{
			GetPathFromFileName(FileName, DefaultPath, sizeof(DefaultPath));

			m_Preferences.SetStringValue(CFG_IMAGE_PATH, DefaultPath);
		}

		success = true;
	}

	UNREFERENCED_PARAMETER(FileName);
	UNREFERENCED_PARAMETER(Size);

	return success;

	#elif defined(__APPLE__)

	const char *fileExt = GetCaptureFormatFileExt(m_BitmapCaptureFormat);

	char DefaultPath[MAX_PATH];
	m_Preferences.GetStringValue(CFG_IMAGE_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileName[0] = '\0';
	if (!MacGetImageSavePath(FileName, Size, fileExt))
		return false;

	if (!HasFileExt(FileName, fileExt))
		strncat(FileName, fileExt, (size_t)(Size - (int)strlen(FileName) - 1));

	if (m_AutoSavePrefsFolders)
	{
		GetPathFromFileName(FileName, DefaultPath, sizeof(DefaultPath));
		m_Preferences.SetStringValue(CFG_IMAGE_PATH, DefaultPath);
	}

	return true;

	#else

	UNREFERENCED_PARAMETER(FileName);
	UNREFERENCED_PARAMETER(Size);

	return false;

	#endif
}

void BeebWin::CaptureBitmap(int SourceX,
                            int SourceY,
                            int SourceWidth,
                            int SourceHeight,
                            bool Teletext)
{
	#ifdef WIN32

	const WCHAR *mimeType = GetCaptureFormatMimeType(m_BitmapCaptureFormat);
	const char *fileExt = GetCaptureFormatFileExt(m_BitmapCaptureFormat);

	CLSID encoderClsid;

	if (!GetImageEncoderClsid(mimeType, &encoderClsid))
		return;

	// Auto generate filename?
	if (m_CaptureBitmapAutoFilename)
	{
		m_Preferences.GetStringValue(CFG_IMAGE_PATH, m_CaptureFileName);
		GetDataPath(m_UserDataPath, m_CaptureFileName);

		SYSTEMTIME systemTime;
		GetLocalTime(&systemTime);

		char AutoName[MAX_PATH];

		sprintf(AutoName, "BeebEm_%04d%02d%02d_%02d%02d%02d_%d%s",
		        systemTime.wYear, systemTime.wMonth,  systemTime.wDay,
		        systemTime.wHour, systemTime.wMinute, systemTime.wSecond,
		        systemTime.wMilliseconds / 100,
		        fileExt);

		AppendPath(m_CaptureFileName, AutoName);
	}

	int BitmapWidth, BitmapHeight;
	int DestX, DestY;
	int DestWidth, DestHeight;

	switch (m_BitmapCaptureResolution)
	{
		case BitmapCaptureResolution::Display:
			BitmapWidth  = m_XWinSize;
			BitmapHeight = m_YWinSize;
			DestWidth  = BitmapWidth;
			DestHeight = BitmapHeight;
			DestX = 0;
			DestY = 0;
			break;

		case BitmapCaptureResolution::_1280x1024:
			BitmapWidth  = 1280;
			BitmapHeight = 1024;

			if (Teletext)
			{
				DestWidth  = SourceWidth  * 2;
				DestHeight = SourceHeight * 2;
				DestX = (BitmapWidth  - DestWidth)  / 2;
				DestY = (BitmapHeight - DestHeight) / 2;
			}
			else
			{
				DestWidth  = BitmapWidth;
				DestHeight = BitmapHeight;
				DestX = 0;
				DestY = 0;
			}
			break;

		case BitmapCaptureResolution::_640x512:
		default:
			BitmapWidth  = 640;
			BitmapHeight = 512;

			if (Teletext)
			{
				DestWidth  = SourceWidth;
				DestHeight = SourceHeight;
				DestX = (BitmapWidth  - DestWidth)  / 2;
				DestY = (BitmapHeight - DestHeight) / 2;
			}
			else
			{
				DestWidth  = BitmapWidth;
				DestHeight = BitmapHeight;
				DestX = 0;
				DestY = 0;
			}
			break;

		case BitmapCaptureResolution::_320x256:
			BitmapWidth  = 320;
			BitmapHeight = 256;
			DestWidth  = BitmapWidth;
			DestHeight = BitmapHeight;
			DestX = 0;
			DestY = 0;
			break;
	}

	// Capture the bitmap
	BitmapInfo CaptureBitmapInfo = m_BitmapInfo;
	CaptureBitmapInfo.Header.biWidth = BitmapWidth;
	CaptureBitmapInfo.Header.biHeight = BitmapHeight;
	CaptureBitmapInfo.Header.biSizeImage = CaptureBitmapInfo.Header.biWidth * CaptureBitmapInfo.Header.biHeight;

	HDC CaptureDC = CreateCompatibleDC(nullptr);

	char* CaptureScreen = nullptr;

	HBITMAP CaptureDIB = CreateDIBSection(CaptureDC,
	                                      (BITMAPINFO*)&CaptureBitmapInfo,
	                                      DIB_RGB_COLORS,
	                                      (void**)&CaptureScreen,
	                                      nullptr,
	                                      0);

	HGDIOBJ prevObj = SelectObject(CaptureDC, CaptureDIB);

	if (prevObj == nullptr)
	{
		Report(MessageType::Error, "Failed to initialise capture buffers");
	}
	else
	{
		StretchBlt(CaptureDC,
		           DestX, DestY,
		           DestWidth, DestHeight,
		           m_hDCBitmap,
		           SourceX, SourceY,
		           SourceWidth, SourceHeight,
		           SRCCOPY);

		SelectObject(CaptureDC, prevObj);

		// Use GDI+ Bitmap to save bitmap
		Bitmap *bitmap = new Bitmap(CaptureDIB, 0);

		WCHAR wFileName[MAX_PATH];
		mbstowcs(wFileName, m_CaptureFileName, MAX_PATH);

		Status status = bitmap->Save(wFileName, &encoderClsid, NULL);

		if (status != Ok)
		{
			Report(MessageType::Error, "Failed to save screen capture");
		}
		else if (m_CaptureBitmapAutoFilename)
		{
			// Let user know bitmap has been saved
			FlashWindow();
		}

		delete bitmap;
	}

	if (CaptureDIB != nullptr)
	{
		DeleteObject(CaptureDIB);
	}

	if (CaptureDC != nullptr)
	{
		DeleteDC(CaptureDC);
	}

	#elif defined(__APPLE__)

	const char *fileExt = GetCaptureFormatFileExt(m_BitmapCaptureFormat);

	if (m_CaptureBitmapAutoFilename)
	{
		m_Preferences.GetStringValue(CFG_IMAGE_PATH, m_CaptureFileName);
		GetDataPath(m_UserDataPath, m_CaptureFileName);

		time_t now = time(nullptr);
		struct tm *t = localtime(&now);
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);

		char AutoName[MAX_PATH];
		snprintf(AutoName, sizeof(AutoName), "BeebEm_%04d%02d%02d_%02d%02d%02d_%ld%s",
		         t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
		         t->tm_hour, t->tm_min, t->tm_sec,
		         ts.tv_nsec / 100000000L,
		         fileExt);

		AppendPath(m_CaptureFileName, AutoName);
	}

	int BitmapWidth, BitmapHeight;
	int DestX, DestY;
	int DestWidth, DestHeight;

	switch (m_BitmapCaptureResolution)
	{
		case BitmapCaptureResolution::Display:
			BitmapWidth  = SourceWidth;
			BitmapHeight = SourceHeight;
			DestX = 0;
			DestY = 0;
			DestWidth  = BitmapWidth;
			DestHeight = BitmapHeight;
			break;

		case BitmapCaptureResolution::_1280x1024:
			BitmapWidth  = 1280;
			BitmapHeight = 1024;

			if (Teletext)
			{
				DestWidth  = SourceWidth  * 2;
				DestHeight = SourceHeight * 2;
				DestX = (BitmapWidth  - DestWidth)  / 2;
				DestY = (BitmapHeight - DestHeight) / 2;
			}
			else
			{
				DestWidth  = BitmapWidth;
				DestHeight = BitmapHeight;
				DestX = 0;
				DestY = 0;
			}
			break;

		case BitmapCaptureResolution::_640x512:
		default:
			BitmapWidth  = 640;
			BitmapHeight = 512;

			if (Teletext)
			{
				DestWidth  = SourceWidth;
				DestHeight = SourceHeight;
				DestX = (BitmapWidth  - DestWidth)  / 2;
				DestY = (BitmapHeight - DestHeight) / 2;
			}
			else
			{
				DestWidth  = BitmapWidth;
				DestHeight = BitmapHeight;
				DestX = 0;
				DestY = 0;
			}
			break;

		case BitmapCaptureResolution::_320x256:
			BitmapWidth  = 320;
			BitmapHeight = 256;
			DestX = 0;
			DestY = 0;
			DestWidth  = BitmapWidth;
			DestHeight = BitmapHeight;
			break;
	}

	const char *uti;
	switch (m_BitmapCaptureFormat) {
		case BitmapCaptureFormat::Jpeg: uti = "public.jpeg";        break;
		case BitmapCaptureFormat::Gif:  uti = "com.compuserve.gif"; break;
		case BitmapCaptureFormat::Bmp:  uti = "com.microsoft.bmp";  break;
		default:                        uti = "public.png";          break;
	}

	MacCaptureBitmap(m_CaptureFileName,
	                 SourceX, SourceY, SourceWidth, SourceHeight,
	                 BitmapWidth, BitmapHeight,
	                 DestX, DestY, DestWidth, DestHeight,
	                 uti);

	if (m_CaptureBitmapAutoFilename)
	{
		FlashWindow();
	}

	#else

	UNREFERENCED_PARAMETER(SourceX);
	UNREFERENCED_PARAMETER(SourceY);
	UNREFERENCED_PARAMETER(SourceWidth);
	UNREFERENCED_PARAMETER(SourceHeight);
	UNREFERENCED_PARAMETER(Teletext);

	#endif
}
