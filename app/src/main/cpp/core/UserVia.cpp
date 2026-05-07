/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2004  Ken Lowe

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

#include <stdio.h>
#include <string>
#include <time.h>

#include "UserVia.h"
#include "6502core.h"
#include "Bcd.h"
#include "Debug.h"
#include "Log.h"
#include "Main.h"
#include "Tube.h"
#include "UefState.h"
#include "UserPortBreakoutBox.h"
#include "UserPortRTC.h"
#include "Via.h"

// AMX mouse (see UserVia.h)
bool AMXMouseEnabled = false;
bool AMXLRForMiddle = false;
int AMXTrigger = 0;
int AMXButtons = 0;
int AMXTargetX = 0;
int AMXTargetY = 0;
int AMXCurrentX = 0;
int AMXCurrentY = 0;
int AMXDeltaX = 0;
int AMXDeltaY = 0;

// Printer port
bool PrinterEnabled = false;
int PrinterTrigger = 0;
static std::string PrinterFileName;
static FILE *PrinterFileHandle = nullptr;

// Shift Register
static int SRTrigger = 0;
static void SRPoll();
static void UpdateSRState(bool SRrw);

// Solidisk SW RAM board
bool SolidiskSWRAMBoardEnabled = false;

// My raw VIA state
VIAState UserVIAState;

/*--------------------------------------------------------------------------*/

static void WriteToPrinter(unsigned char Value);

/*--------------------------------------------------------------------------*/

static void UpdateIFRTopBit()
{
	if (UserVIAState.ifr & (UserVIAState.ier & 0x7f))
	{
		UserVIAState.ifr |= IFR_IRQ;
		IntStatus |= IRQ_USERVIA;
	}
	else
	{
		UserVIAState.ifr &= ~IFR_IRQ;
		IntStatus &= ~IRQ_USERVIA;
	}
}

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fe60 stripped out */
void UserVIAWrite(int Address, unsigned char Value)
{
	// DebugTrace("UserVIAWrite: Address=0x%02x Value=0x%02x\n", Address, Value);

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::UserVIA, true,
		                   "UserVia: Write address %X value %02X",
		                   Address & 0xf, Value);
	}

	switch (Address)
	{
		case 0:
			UserVIAState.orb = Value;

			if ((UserVIAState.ifr & IFR_CB2) && ((UserVIAState.pcr & 0x20) == 0))
			{
				UserVIAState.ifr &= ~IFR_CB2;
				UpdateIFRTopBit();
			}

			if (userPortBreakoutDialog != nullptr)
			{
				userPortBreakoutDialog->ShowOutputs(UserVIAState.orb);
			}

			if (UserPortRTCEnabled)
			{
				UserPortRTCWrite(Value);
			}
			break;

		case 1:
			UserVIAState.ora = Value;

			UserVIAState.ifr &= ~(IFR_CA2 | IFR_CA1);
			UpdateIFRTopBit();

			if (PrinterEnabled)
			{
				WriteToPrinter(UserVIAState.ora);
			}
			break;

		case 2:
			UserVIAState.ddrb = Value;

			if (UserPortRTCEnabled)
			{
				if ((Value & 0x07) == 0x07)
				{
					UserPortRTCResetWrite();
				}
			}
			break;

		case 3:
			UserVIAState.ddra = Value;
			break;

		case 4:
		case 6:
			// DebugTrace("UserVia Reg4 Timer1 lo Counter Write val=0x%02x at %d\n", Value, TotalCycles);
			UserVIAState.timer1l &= 0xff00;
			UserVIAState.timer1l |= Value & 0xff;
			break;

		case 5:
			// DebugTrace("UserVia Reg5 Timer1 hi Counter Write val=0x%02x at %d\n", Value, TotalCycles);
			UserVIAState.timer1l &= 0xff;
			UserVIAState.timer1l |= Value << 8;
			UserVIAState.timer1c = UserVIAState.timer1l * 2 + 1;

			// If PB7 toggling enabled, then lower PB7 now
			if (UserVIAState.acr & ACR_TIMER1_OUTPUT_ENABLE)
			{
				UserVIAState.orb &= 0x7f;
				UserVIAState.irb &= 0x7f;
			}

			UserVIAState.ifr &= ~IFR_TIMER1;
			UpdateIFRTopBit();

			UserVIAState.timer1hasshot = false; // Added by K.Lowe 24/08/03
			break;

		case 7:
			// DebugTrace("UserVia Reg7 Timer1 hi latch Write val=0x%02x at %d\n", Value, TotalCycles);
			UserVIAState.timer1l &= 0xff;
			UserVIAState.timer1l |= Value << 8;

			UserVIAState.ifr &= ~IFR_TIMER1; // Clear timer 1 IFR (this is what Model-B does)
			UpdateIFRTopBit();
			break;

		case 8:
			// DebugTrace("UserVia Reg8 Timer2 lo Counter Write val=0x%02x at %d\n", Value, TotalCycles);
			UserVIAState.timer2l &= 0xff00;
			UserVIAState.timer2l |= Value;
			break;

		case 9:
			// DebugTrace("UserVia Reg9 Timer2 hi Counter Write val=0x%02x at %d\n", Value, TotalCycles);
			UserVIAState.timer2l &= 0xff;
			UserVIAState.timer2l |= Value << 8;
			UserVIAState.timer2c = UserVIAState.timer2l * 2 + 1;

			if (UserVIAState.timer2c == 0)
			{
				UserVIAState.timer2c = 0x20000;
			}

			UserVIAState.ifr &= ~IFR_TIMER2;
			UpdateIFRTopBit();

			UserVIAState.timer2hasshot = false; // Added by K.Lowe 24/08/03
			break;

		case 10:
			UserVIAState.sr = Value;
			UpdateSRState(true);
			break;

		case 11:
			UserVIAState.acr = Value;
			UpdateSRState(false);
			break;

		case 12:
			UserVIAState.pcr = Value;
			break;

		case 13:
			UserVIAState.ifr &= ~Value;
			UpdateIFRTopBit();
			break;

		case 14:
			// DebugTrace("User VIA Write ier Value=0x%02x\n", Value);
			if (Value & 0x80)
			{
				UserVIAState.ier |= Value;
			}
			else
			{
				UserVIAState.ier &= ~Value;
			}

			UserVIAState.ier &= ~IER_SET_CLEAR;
			UpdateIFRTopBit();
			break;

		case 15:
			UserVIAState.ora = Value;
			break;
	}
}

/*--------------------------------------------------------------------------*/

// Address is in the range 0-f - with the fe60 stripped out

unsigned char UserVIARead(int Address)
{
	unsigned char Value = 0xff;
	// Local copy for processing middle button
	int amxButtons = AMXButtons;

	// DebugTrace("UserVIARead: Address=0x%02x at %d\n", Address, TotalCycles);

	switch (Address)
	{
		case 0: /* IRB read */
			Value = (UserVIAState.orb & UserVIAState.ddrb) | (UserVIAState.irb & ~UserVIAState.ddrb);

			if (UserPortRTCEnabled)
			{
				Value = (Value & 0xfe) | (unsigned char)UserPortRTCReadBit();
			}

			if (userPortBreakoutDialog != nullptr)
			{
				userPortBreakoutDialog->ShowInputs(Value);
			}

			if (AMXMouseEnabled)
			{
				if (AMXLRForMiddle)
				{
					if ((amxButtons & AMX_LEFT_BUTTON) && (amxButtons & AMX_RIGHT_BUTTON))
					{
						amxButtons = AMX_MIDDLE_BUTTON;
					}
				}

				if (TubeType == TubeDevice::Master512CoPro)
				{
					Value &= 0xf8;
					Value |= (amxButtons ^ 7);
				}
				else
				{
					Value &= 0x1f;
					Value |= (amxButtons ^ 7) << 5;

					UserVIAState.ifr &= ~(IFR_CB2 | IFR_CB1);
					UpdateIFRTopBit();
				}

				/* Set up another interrupt if not at target */
				if ((AMXTargetX != AMXCurrentX) || (AMXTargetY != AMXCurrentY) || AMXDeltaX || AMXDeltaY)
				{
					SetTrigger(AMX_TRIGGER, AMXTrigger);
				}
				else
				{
					ClearTrigger(AMXTrigger);
				}
			}
			break;

		case 2:
			Value = UserVIAState.ddrb;
			break;

		case 3:
			Value = UserVIAState.ddra;
			break;

		case 4: /* Timer 1 lo counter */
			if (UserVIAState.timer1c < 0)
			{
				Value = 0xff;
			}
			else
			{
				Value = (UserVIAState.timer1c / 2) & 0xff;
			}

			UserVIAState.ifr &= ~IFR_TIMER1;
			UpdateIFRTopBit();
			break;

		case 5: /* Timer 1 hi counter */
			Value = (UserVIAState.timer1c >> 9) & 0xff;
			break;

		case 6: /* Timer 1 lo latch */
			Value = UserVIAState.timer1l & 0xff;
			break;

		case 7: /* Timer 1 hi latch */
			Value = (UserVIAState.timer1l >> 8) & 0xff;
			break;

		case 8: /* Timer 2 lo counter */
			if (UserVIAState.timer2c < 0) /* Adjust for dividing -ve count by 2 */
			{
				Value = ((UserVIAState.timer2c - 1) / 2) & 0xff;
			}
			else
			{
				Value = (UserVIAState.timer2c / 2) & 0xff;
			}

			UserVIAState.ifr &= ~IFR_TIMER2;
			UpdateIFRTopBit();
			break;

		case 9: /* Timer 2 hi counter */
			Value = (UserVIAState.timer2c >> 9) & 0xff;
			break;

		case 10:
			Value = UserVIAState.sr;
			UpdateSRState(true);
			break;

		case 11:
			Value = UserVIAState.acr;
			break;

		case 12:
			Value = UserVIAState.pcr;
			break;

		case 13:
			UpdateIFRTopBit();
			Value = UserVIAState.ifr;
			break;

		case 14:
			Value = UserVIAState.ier | IER_SET_CLEAR;
			break;

		case 1:
			UserVIAState.ifr &= ~(IFR_CA2 | IFR_CA1);
			UpdateIFRTopBit();
			// Fall through...

		case 15:
			Value = 255;
			break;
	}

	if (DebugEnabled)
	{
	  DebugDisplayTraceF(DebugType::UserVIA, true,
	                     "UserVia: Read address %X value %02X",
	                     Address & 0xf, Value & 0xff);
	}

	return Value;
}

/*--------------------------------------------------------------------------*/
void UserVIATriggerCA1Int()
{
	/* We should be concerned with active edges etc. */
	UserVIAState.ifr |= IFR_CA1;
	UpdateIFRTopBit();
}

/*--------------------------------------------------------------------------*/

static void UserVIAPollReal()
{
	static bool t1int = false;

	if (UserVIAState.timer1c < -2 && !t1int)
	{
		t1int = true;

		if (!UserVIAState.timer1hasshot || (UserVIAState.acr & ACR_TIMER1_CONTINUOUS))
		{
			// DebugTrace("UserVIA timer1c - int at %d\n", TotalCycles);
			UserVIAState.ifr |= IFR_TIMER1; /* Timer 1 interrupt */
			UpdateIFRTopBit();

			if (UserVIAState.acr & ACR_TIMER1_OUTPUT_ENABLE)
			{
				UserVIAState.orb ^= 0x80; // Toggle PB7
				UserVIAState.irb ^= 0x80; // Toggle PB7
			}

			if ((UserVIAState.ier & IFR_TIMER1) && CyclesToInt == NO_TIMER_INT_DUE)
			{
				CyclesToInt = 3 + UserVIAState.timer1c;
			}

			UserVIAState.timer1hasshot = true;
		}
	}

	if (UserVIAState.timer1c < -3)
	{
		// DebugTrace("UserVIA timer1c\n");
		UserVIAState.timer1c += (UserVIAState.timer1l * 2) + 4;
		t1int = false;
	}

	if (UserVIAState.timer2c < -2)
	{
		if (!UserVIAState.timer2hasshot)
		{
			// DebugTrace("UserVIA timer2c - int\n");
			UserVIAState.ifr |= IFR_TIMER2;
			UpdateIFRTopBit();

			if ((UserVIAState.ier & IER_TIMER2) && CyclesToInt == NO_TIMER_INT_DUE)
			{
				CyclesToInt = 3 + UserVIAState.timer2c;
			}

			UserVIAState.timer2hasshot = true; // Added by K.Lowe 24/08/03
		}
	}

	if (UserVIAState.timer2c < -3)
	{
		// DebugTrace("UserVIA timer2c\n");
		UserVIAState.timer2c += 0x20000; // Do not reload latches for T2
	}
}

void UserVIAPoll(unsigned int Cycles)
{
	// Converted to a proc to allow shift register functions

	UserVIAState.timer1c -= Cycles;

	if (!(UserVIAState.acr & ACR_TIMER2_CONTROL))
	{
		UserVIAState.timer2c -= Cycles;
	}

	if (UserVIAState.timer1c < 0 || UserVIAState.timer2c < 0)
	{
		UserVIAPollReal();
	}

	if (AMXMouseEnabled && AMXTrigger <= TotalCycles)
	{
		AMXMouseMovement();
	}

	if (PrinterEnabled && PrinterTrigger <= TotalCycles)
	{
		PrinterPoll();
	}

	if (SRTrigger <= TotalCycles)
	{
		SRPoll();
	}
}

/*--------------------------------------------------------------------------*/

void UserVIAReset()
{
	VIAReset(&UserVIAState);

	ClearTrigger(AMXTrigger);
	ClearTrigger(PrinterTrigger);
	ClearTrigger(SRTrigger);
}

/*--------------------------------------------------------------------------*/

static void SRPoll()
{
	if (UserVIAState.SRMode == 6 || UserVIAState.SRMode == 2)
	{
		if (!(UserVIAState.ifr & IFR_SHIFTREG))
		{
			// Shift complete
			UserVIAState.ifr |= IFR_SHIFTREG;
			UpdateIFRTopBit();
		}

		ClearTrigger(SRTrigger);
	}
}

static void UpdateSRState(bool SRrw)
{
	UserVIAState.SRMode = (UserVIAState.acr >> 2) & 7;

	// TODO: Implement all SR modes, and actually shift the SR contents.

	if ((UserVIAState.SRMode == 6 || UserVIAState.SRMode == 2) && SRTrigger == CycleCountTMax)
	{
		// Set a timer to trigger setting the Shift Register interrupt.
		// 16 cycles at the VIA input clock rate (1 MHz),
		// so 32 cycles at 2 MHz.
		SetTrigger(32, SRTrigger);
	}

	if (SRrw)
	{
		if (UserVIAState.ifr & IFR_SHIFTREG)
		{
			UserVIAState.ifr &= ~IFR_SHIFTREG;
			UpdateIFRTopBit();
		}
	}
}

/*-------------------------------------------------------------------------*/

static int sgn(int number)
{
	if (number > 0) return 1;
	if (number < 0) return -1;
	return 0;
}

void AMXMouseMovement()
{
	ClearTrigger(AMXTrigger);

	// Check if there is an outstanding interrupt.
	if (AMXMouseEnabled && (UserVIAState.ifr & (IFR_CB1 | IFR_CB2)) == 0)
	{
		int deltaX = AMXDeltaX == 0 ? AMXTargetX - AMXCurrentX : AMXDeltaX;
		int deltaY = AMXDeltaY == 0 ? AMXTargetY - AMXCurrentY : AMXDeltaY;

		if (deltaX != 0 || deltaY != 0)
		{
			int xdir = sgn(deltaX);
			int ydir = sgn(deltaY);

			int xpulse, ypulse;

			if (TubeType == TubeDevice::Master512CoPro)
			{
				xpulse = 0x08;
				ypulse = 0x10;
			}
			else
			{
				xpulse = 0x01;
				ypulse = 0x04;
			}

			if (xdir)
			{
				if (xdir > 0)
				{
					UserVIAState.irb &= ~xpulse;
				}
				else
				{
					UserVIAState.irb |= xpulse;
				}

				if (!(UserVIAState.pcr & 0x10)) // Interrupt on falling CB1 edge
				{
					// Warp time to the falling edge, invert the input
					UserVIAState.irb ^= xpulse;
				}

				// Trigger the interrupt
				UserVIAState.ifr |= IFR_CB1;
			}

			if (ydir)
			{
				if (ydir > 0)
				{
					UserVIAState.irb |= ypulse;
				}
				else
				{
					UserVIAState.irb &= ~ypulse;
				}

				if (!(UserVIAState.pcr & 0x40)) // Interrupt on falling CB2 edge
				{
					// Warp time to the falling edge, invert the input
					UserVIAState.irb ^= ypulse;
				}

				// Trigger the interrupt
				UserVIAState.ifr |= IFR_CB2;
			}

			if (AMXDeltaX != 0)
			{
				AMXDeltaX -= xdir;
			}
			else
			{
				AMXCurrentX += xdir;
			}

			if (AMXDeltaY != 0)
			{
				AMXDeltaY -= ydir;
			}
			else
			{
				AMXCurrentY += ydir;
			}

			UpdateIFRTopBit();
		}
	}
}

/*-------------------------------------------------------------------------*/

// Close file if already open

static void ClosePrinterOutputFile()
{
	if (PrinterFileHandle != nullptr)
	{
		fclose(PrinterFileHandle);
		PrinterFileHandle = nullptr;
	}
}

/*-------------------------------------------------------------------------*/

bool PrinterEnable(const char *FileName)
{
	ClosePrinterOutputFile();

	if (FileName == nullptr)
	{
		PrinterEnabled = true;
		SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
		return true;
	}
	else
	{
		PrinterFileName = FileName;

		PrinterFileHandle = fopen(PrinterFileName.c_str(), "ab");

		if (PrinterFileHandle == nullptr)
		{
			mainWin->Report(MessageType::Error,
			                "Failed to open printer:\n  %s", PrinterFileName.c_str());
			return false;
		}
		else
		{
			PrinterEnabled = true;
			SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
			return true;
		}
	}
}

/*-------------------------------------------------------------------------*/

void PrinterDisable()
{
	ClosePrinterOutputFile();

	PrinterEnabled = false;
	ClearTrigger(PrinterTrigger);
}

/*-------------------------------------------------------------------------*/

void PrinterPoll()
{
	ClearTrigger(PrinterTrigger);
	UserVIATriggerCA1Int();

	// The CA1 interrupt is not always picked up,
	// set up a trigger just in case.
	SetTrigger(100000, PrinterTrigger);
}

/*--------------------------------------------------------------------------*/

static void WriteToPrinter(unsigned char Value)
{
	if (PrinterFileHandle != nullptr)
	{
		if (fputc(Value, PrinterFileHandle) == EOF)
		{
			mainWin->Report(MessageType::Error,
			                "Failed to write to printer file:\n  %s", PrinterFileName.c_str());

			fclose(PrinterFileHandle);
			PrinterFileHandle = nullptr;
		}
		else
		{
			fflush(PrinterFileHandle);
			SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
		}
	}
	else
	{
		// Write to clipboard
		mainWin->PrintChar(Value);
		SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
	}
}

/*--------------------------------------------------------------------------*/

void DebugUserVIAState()
{
	DebugVIAState("UserVia", &UserVIAState);
}

/*--------------------------------------------------------------------------*/

void SaveUserVIAUEF(FILE *SUEF)
{
	UEFWrite8(1, SUEF); // 0: SysVIA, 1: UserVIA
	SaveVIAUEF(SUEF, &UserVIAState);
	UEFWrite8(0, SUEF); // Unused (IC32State in SysVIA)

	UEFWrite32(SRTrigger, SUEF);
}

/*--------------------------------------------------------------------------*/

void LoadUserVIAUEF(FILE *SUEF, int Version)
{
	LoadVIAUEF(SUEF, Version, &UserVIAState);

	if (Version >= 16)
	{
		SRTrigger = UEFRead32(SUEF);

		if (SRTrigger != CycleCountTMax)
		{
			SRTrigger += TotalCycles;
		}
	}
}

/*--------------------------------------------------------------------------*/
