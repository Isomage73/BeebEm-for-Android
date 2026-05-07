/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2001  Richard Gellman
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

// System VIA support file for the Beeb emulator. Includes things
// like the keyboard emulation - David Alan Gilbert 30/10/94
// CMOS Ram finalised 06/01/2001 - Richard Gellman

#include "Windows.h"

#include <stdio.h>
#include <time.h>

#include "SysVia.h"
#include "6502core.h"
#include "Bcd.h"
#include "BeebMem.h"
#include "BeebWin.h"
#include "Debug.h"
#include "DebugTrace.h"
#include "IC32Latch.h"
#include "Main.h"
#include "Model.h"
#include "Rtc.h"
#include "Sound.h"
#include "Speech.h"
#include "UefState.h"
#include "Via.h"

// #define DEBUG_IC32
// #define DEBUG_KEYBOARD
// #define DEBUG_SLOW_DATA_BUS

// Shift register stuff
// static unsigned char SRCount;
static int SRTrigger = 0;
static void SRPoll();
static void UpdateSRState(bool SRrw);

// Fire button for joystick 1 and 2, false=not pressed, true=pressed
bool SysVIAButton[2] = { false, false };

// My raw VIA state
VIAState SysVIAState;
// static char WECycles = 0;
// static char WEState = 0;

// Last value written to the slow data bus - sound reads it later
static unsigned char SlowDataBusWriteValue = 0;

// Currently selected keyboard row, column
static unsigned int KBDRow = 0;
static unsigned int KBDCol = 0;

static bool SysViaKbdState[16][8]; // Col, row
static int KeysDown=0;

unsigned char KeyboardLinks = 0;

/*--------------------------------------------------------------------------*/

static void TranslateKeyboardLinks(unsigned char Value)
{
	for (int i = 0; i < 8; i++)
	{
		const int Row = 0;
		const int Column = 9 - i;

		if (Value & (1 << i))
		{
			BeebKeyDown(Row, Column);
		}
	}
}

/*--------------------------------------------------------------------------*/

void BeebReleaseAllKeys()
{
	KeysDown = 0;

	for (int Row = 0; Row < 8; Row++)
	{
		for (int Column = 0; Column < 16; Column++)
		{
			SysViaKbdState[Column][Row] = false;
		}
	}

	if (MachineType != Model::Master128 && MachineType != Model::MasterET)
	{
		TranslateKeyboardLinks(KeyboardLinks);
	}
}

/*--------------------------------------------------------------------------*/

static void UpdateIFRTopBit()
{
	if (SysVIAState.ifr & (SysVIAState.ier & 0x7f))
	{
		SysVIAState.ifr |= IFR_IRQ;
		IntStatus |= IRQ_SYSVIA;
	}
	else
	{
		SysVIAState.ifr &= ~IFR_IRQ;
		IntStatus &= ~IRQ_SYSVIA;
	}
}

/*--------------------------------------------------------------------------*/

void SysVIAPulseCB1()
{
	// Set IFR bit 4 - AtoD end of conversion interrupt
	if (SysVIAState.ier & IFR_CB1)
	{
		SysVIAState.ifr |= IFR_CB1;
		UpdateIFRTopBit();
	}
}

/*--------------------------------------------------------------------------*/

void DoKbdIntCheck()
{
	// Now lets see if we just caused a CA2 interrupt - note we will flag
	// it multiply - we aren't going to test for the edge
	// Two cases - write enable is OFF the keyboard - basically any key will cause an
	// interrupt in a few cycles.

	#ifdef DEBUG_KEYBOARD
	int Oldflag = SysVIAState.ifr & IFR_CA2;
	#endif

	if (KeysDown > 0 && (SysVIAState.pcr & 0xc) == 4)
	{
		if (IC32State & IC32_KEYBOARD_WRITE)
		{
			SysVIAState.ifr |= IFR_CA2;
			//DebugTrace("DoKbdIntCheck: Caused interrupt case 1\n");
			UpdateIFRTopBit();
		}
		else
		{
			if (KBDCol < 15)
			{
				for (int Row = 1; Row < 8; Row++)
				{
					if (SysViaKbdState[KBDCol][Row])
					{
						// DebugTrace("DoKbdIntCheck: Caused interrupt case 2\n");

						SysVIAState.ifr |= IFR_CA2;
						UpdateIFRTopBit();
					}
				}
			}
		} // WriteEnable on
	} // Keys down and CA2 input enabled

	#ifdef DEBUG_KEYBOARD

	DebugTrace("DoKbdIntCheck KeysDown=%d pcr & c=%d IC32State & 8=%d "
	           "KBDRow=%d KBDCol=%d oldIFRflag=%d Newflag=%d\n",
	           KeysDown, SysVIAState.pcr & 0xc, IC32State & IC32_KEYBOARD_WRITE,
	           KBDRow, KBDCol, Oldflag, SysVIAState.ifr & IFR_CA2);

	#endif
}

/*--------------------------------------------------------------------------*/

void BeebKeyDown(int Row, int Column)
{
	if (Row < 0 || Column < 0)
	{
		return;
	}

	// Update keys down count - unless it's shift/control
	if (!SysViaKbdState[Column][Row] && Row != 0)
	{
		KeysDown++;
	}

	SysViaKbdState[Column][Row] = true;

	DoKbdIntCheck();
}

/*--------------------------------------------------------------------------*/

void BeebKeyUp(int Row, int Column)
{
	if (Row < 0 || Column < 0)
	{
		return;
	}

	// Update keys down count - unless it's shift/control
	if (SysViaKbdState[Column][Row] && Row != 0)
	{
		KeysDown--;
	}

	SysViaKbdState[Column][Row] = false;
}

/*--------------------------------------------------------------------------*/

// Return current state of the single bit output of the keyboard matrix
// - NOT the any keypressed interrupt

static bool KbdOP()
{
	// Check range validity
	if (KBDCol > 14 || KBDRow > 7) return false; // Key not down if overrange - perhaps we should do something more?

	return SysViaKbdState[KBDCol][KBDRow];
}

/*--------------------------------------------------------------------------*/

static void IC32Write(unsigned char Value)
{
	// Hello. This is Richard Gellman. It is 10:25pm, Friday 2nd February 2001
	// I have to do CMOS RAM now. And I think I'm going slightly potty.
	// Additional, Sunday 4th February 2001. I must have been potty. the line above did read January 2000.
	int PrevIC32State = IC32State;

	int Bit = Value & 7;

	if (Value & 8)
	{
		#ifdef DEBUG_IC32

		if (!(IC32State & (1 << Bit)))
		{
			DebugTrace("IC32 set bit %d (0x%02d)\n", Bit, 1 << Bit);
		}

		#endif

		IC32State |= 1 << Bit;
	}
	else
	{
		#ifdef DEBUG_IC32

		if (IC32State & (1 << Bit))
		{
			DebugTrace("IC32 clear bit %d (0x%02d)\n", Bit, 1 << Bit);
		}

		#endif

		IC32State &= ~(1 << Bit);
	}

	LEDs.CapsLock = (IC32State & IC32_CAPS_LOCK) == 0;
	LEDs.ShiftLock = (IC32State & IC32_SHIFT_LOCK) == 0;

	if (MachineType == Model::Master128 || MachineType == Model::MasterET)
	{
		if (RTCIsChipEnable())
		{
			if (IC32State & IC32_RTC_READ)
			{
				// During read cycles, DS signifies the time that the RTC
				// is to drive the bidirectional bus.
				if (!(PrevIC32State & IC32_RTC_DATA_STROBE) && (IC32State & IC32_RTC_DATA_STROBE))
				{
					SysVIAState.ora = RTCReadData();
				}
			}
			else
			{
				// In write cycles, the trailing edge of DS causes the RTC
				// to latch the written data.
				if ((PrevIC32State & IC32_RTC_DATA_STROBE) && !(IC32State & IC32_RTC_DATA_STROBE))
				{
					RTCWriteData(SysVIAState.ora);
				}
			}
		}
	}

	// Must do sound reg access when write line changes

	if ((PrevIC32State & IC32_SOUND_WRITE) && !(IC32State & IC32_SOUND_WRITE))
	{
		Sound_RegWrite(SlowDataBusWriteValue);
	}

	if (MachineType != Model::Master128 && MachineType != Model::MasterET)
	{
		if ((PrevIC32State & IC32_SPEECH_WRITE) && !(IC32State & IC32_SPEECH_WRITE))
		{
			SpeechWrite(SlowDataBusWriteValue);
		}

		if ((PrevIC32State & IC32_SPEECH_READ) && !(IC32State & IC32_SPEECH_READ))
		{
			SpeechReadEnable();
		}
	}

	if (!(IC32State & IC32_KEYBOARD_WRITE) && (PrevIC32State & IC32_KEYBOARD_WRITE))
	{
		KBDRow = (SlowDataBusWriteValue >> 4) & 7;
		KBDCol = SlowDataBusWriteValue & 0xf;
		DoKbdIntCheck(); /* Should really only if write enable on KBD changes */
	}
}

void ChipClock(int /* nCycles */) {
//	if (WECycles > 0) WECycles -= nCycles;
//	else
//	if (WEState) Sound_RegWrite(SlowDataBusWriteValue);
}

/*--------------------------------------------------------------------------*/

static void SlowDataBusWrite(unsigned char Value)
{
	SlowDataBusWriteValue = Value;

	#ifdef DEBUG_SLOW_DATA_BUS
	DebugTrace("Slow data bus write IC32State=%d Value=0x%02x\n", IC32State, Value);
	#endif

	if ((IC32State & IC32_KEYBOARD_WRITE) == 0)
	{
		// kbd write
		KBDRow = (Value >> 4) & 7;
		KBDCol = Value & 0xf;
		// DebugTrace("SlowDataBusWrite to kbd  Row=%d Col=%d\n", KBDRow, KBDCol);
		DoKbdIntCheck(); /* Should really only if write enable on KBD changes */
	}

	if ((IC32State & IC32_SOUND_WRITE) == 0)
	{
		Sound_RegWrite(SlowDataBusWriteValue);
	}
}

/*--------------------------------------------------------------------------*/

static unsigned char SlowDataBusRead()
{
	if (MachineType == Model::Master128 || MachineType == Model::MasterET)
	{
		if (RTCIsChipEnable() && (IC32State & IC32_RTC_READ))
		{
			return SysVIAState.ora & ~SysVIAState.ddra;
		}
	}

	unsigned char result = SysVIAState.ora & SysVIAState.ddra;

	if (MachineType == Model::Master128 || MachineType == Model::MasterET)
	{
		// I don't know this lot properly - just put in things as we figure them out

		if (KbdOP()) result |= 128;
	}
	else
	{
		if ((IC32State & IC32_KEYBOARD_WRITE) == 0)
		{
			if (KbdOP()) result |= 128;
		}

		if ((IC32State & IC32_SPEECH_READ) == 0)
		{
			result = SpeechRead();
		}

		if ((IC32State & IC32_SPEECH_WRITE) == 0)
		{
			result = 0xff;
		}
	}

	#ifdef DEBUG_SLOW_DATA_BUS
	DebugTrace("SlowDataBusRead giving 0x%02x\n", result);
	#endif

	return result;
}

/*--------------------------------------------------------------------------*/

// Address is in the range 0-f - with the fe40 stripped out

void SysVIAWrite(int Address, unsigned char Value)
{
	// DebugTrace("SysVIAWrite: Address=0x%02x Value=0x%02x\n", Address, Value);

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::SysVIA, true,
		                   "SysVia: Write address %X value %02X",
		                   Address & 0xf, Value);
	}

	switch (Address)
	{
		case 0: // ORB
			SysVIAState.orb = Value;

			if (MachineType == Model::Master128 || MachineType == Model::MasterET)
			{
				// In the Master series, PB6 is the MC146818AP RTC Chip Enable
				// and PB7 is the MC146818AP RTC Address Strobe (AS)

				RTCChipEnable((Value & 0x40) != 0);

				if ((Value & 0xC0)== 0xC0)
				{
					// Address must be valid just prior to the fall of AS,
					// at which time the address is latched.
					RTCWriteAddress(SysVIAState.ora);
				}
			}

			// The bottom 4 bits of ORB connect to the IC32 latch.
			IC32Write(Value);

			if ((SysVIAState.ifr & IFR_CB2) && ((SysVIAState.pcr & 0x20) == 0))
			{
				SysVIAState.ifr &= ~IFR_CB2;
			}

			// Clear bit 4 of IFR from AtoD Conversion
			SysVIAState.ifr &= ~IFR_CB1;
			UpdateIFRTopBit();
			break;

		case 1: // ORA
			SysVIAState.ora = Value;

			SysVIAState.ifr &= ~(IFR_CA2 | IFR_CA1);
			UpdateIFRTopBit();

			SlowDataBusWrite(Value);
			break;

		case 2:
			SysVIAState.ddrb = Value;
			break;

		case 3:
			SysVIAState.ddra = Value;
			break;

		case 4:
		case 6:
			SysVIAState.timer1l &= 0xff00;
			SysVIAState.timer1l |= Value;
			break;

		case 5:
			SysVIAState.timer1l &= 0xff;
			SysVIAState.timer1l |= Value << 8;
			SysVIAState.timer1c = SysVIAState.timer1l * 2 + 1;

			// If PB7 toggling enabled, then lower PB7 now
			if (SysVIAState.acr & ACR_TIMER1_OUTPUT_ENABLE)
			{
				SysVIAState.orb &= 0x7f;
				SysVIAState.irb &= 0x7f;
			}

			SysVIAState.ifr &= ~IFR_TIMER1;
			UpdateIFRTopBit();

			SysVIAState.timer1hasshot = false;
			break;

		case 7:
			SysVIAState.timer1l &= 0xff;
			SysVIAState.timer1l |= Value << 8;

			SysVIAState.ifr &= ~IFR_TIMER1; // Clear timer 1 IFR (this is what Model-B does)
			UpdateIFRTopBit();
			break;

		case 8:
			SysVIAState.timer2l &= 0xff00;
			SysVIAState.timer2l |= Value;
			break;

		case 9:
			SysVIAState.timer2l &= 0xff;
			SysVIAState.timer2l |= Value << 8;
			SysVIAState.timer2c = SysVIAState.timer2l * 2 + 1;

			if (SysVIAState.timer2c == 0)
			{
				SysVIAState.timer2c = 0x20000;
			}

			SysVIAState.ifr &= ~IFR_TIMER2;
			UpdateIFRTopBit();

			SysVIAState.timer2hasshot = false;
			break;

		case 10:
			SysVIAState.sr = Value;
			UpdateSRState(true);
			break;

		case 11:
			SysVIAState.acr = Value;
			UpdateSRState(false);
			break;

		case 12:
			SysVIAState.pcr = Value;

			if ((Value & PCR_CA2_CONTROL) == PCR_CA2_OUTPUT_HIGH)
			{
				SysVIAState.ca2 = true;
			}
			else if ((Value & PCR_CA2_CONTROL) == PCR_CA2_OUTPUT_LOW)
			{
				SysVIAState.ca2 = false;
			}

			if ((Value & PCR_CB2_CONTROL) == PCR_CB2_OUTPUT_HIGH)
			{
				if (!SysVIAState.cb2)
				{
					// Light pen strobe on CB2 low -> high transition
					VideoLightPenStrobe();
				}

				SysVIAState.cb2 = true;
			}
			else if ((Value & PCR_CB2_CONTROL) == PCR_CB2_OUTPUT_LOW)
			{
				SysVIAState.cb2 = false;
			}
			break;

		case 13:
			SysVIAState.ifr &= ~Value;
			UpdateIFRTopBit();
			break;

		case 14:
			// DebugTrace("Write ier Value=0x%02x\n", Value);

			if (Value & 0x80)
			{
				SysVIAState.ier |= Value;
			}
			else
			{
				SysVIAState.ier &= ~Value;
			}

			SysVIAState.ier &= ~IER_SET_CLEAR;
			UpdateIFRTopBit();
			break;

		case 15:
			SysVIAState.ora = Value;

			SlowDataBusWrite(Value);
			break;
	}
}

/*--------------------------------------------------------------------------*/

// Address is in the range 0-f - with the fe40 stripped out

unsigned char SysVIARead(int Address)
{
	unsigned char Value = 0xff;

	// DebugTrace("SysVIARead: Address=0x%02x at %d\n", Address, TotalCycles);

	switch (Address)
	{
		case 0: // IRB read
			Value = SysVIAState.orb & SysVIAState.ddrb;

			if (!SysVIAButton[1])
			{
				Value |= 0x20;
			}

			if (!SysVIAButton[0])
			{
				Value |= 0x10;
			}

			if (!SpeechStarted)
			{
				// Speech system not present.
				Value |= 0xC0;
			}
			else
			{
				if (SpeechInterrupt()) // Flag is active low
				{
					Value |= 0x40;
				}
				else
				{
					Value &= ~0x40;
				}

				if (SpeechReady()) // Flag is active low
				{
					Value |= 0x80;
				}
				else
				{
					Value &= ~0x80;
				}
			}

			// Clear bit 4 of IFR from AtoD Conversion
			SysVIAState.ifr &= ~IFR_CB1;
			UpdateIFRTopBit();
			break;

		case 2:
			Value = SysVIAState.ddrb;
			break;

		case 3:
			Value = SysVIAState.ddra;
			break;

		case 4: // Timer 1 lo counter
			if (SysVIAState.timer1c < 0)
			{
				Value = 0xff;
			}
			else
			{
				Value = (SysVIAState.timer1c / 2) & 0xff;
			}

			SysVIAState.ifr &= ~IFR_TIMER1;
			UpdateIFRTopBit();
			break;

		case 5: // Timer 1 hi counter
			Value = (SysVIAState.timer1c >> 9) & 0xff; // K.Lowe
			break;

		case 6: // Timer 1 lo latch
			Value = SysVIAState.timer1l & 0xff;
			break;

		case 7: // Timer 1 hi latch
			Value = (SysVIAState.timer1l >> 8) & 0xff; // K.Lowe
			break;

		case 8: // Timer 2 lo counter
			if (SysVIAState.timer2c < 0) // Adjust for dividing -ve count by 2
			{
				Value = ((SysVIAState.timer2c - 1) / 2) & 0xff;
			}
			else
			{
				Value = (SysVIAState.timer2c / 2) & 0xff;
			}

			SysVIAState.ifr &= ~IFR_TIMER2;
			UpdateIFRTopBit();
			break;

		case 9: // Timer 2 hi counter
			Value = (SysVIAState.timer2c >> 9) & 0xff; // K.Lowe
			break;

		case 10:
			Value = SysVIAState.sr;
			UpdateSRState(true);
			break;

		case 11:
			Value = SysVIAState.acr;
			break;

		case 12:
			Value = SysVIAState.pcr;
			break;

		case 13:
			UpdateIFRTopBit();

			#ifdef DEBUG_KEYBOARD
			// DebugTrace("Read IFR got=0x%02x\n", SysVIAState.ifr);
			#endif

			Value = SysVIAState.ifr;
			break;

		case 14:
			Value = SysVIAState.ier | IER_SET_CLEAR;
			break;

		case 1:
			SysVIAState.ifr &= ~(IFR_CA2 | IFR_CA1);
			UpdateIFRTopBit();
			// Fall through...

		case 15:
			Value = SlowDataBusRead();
			break;
	}

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::SysVIA, true,
		                   "SysVia: Read address %X value %02X",
		                   Address & 0xf, Value & 0xff);
	}

	return Value;
}

/*--------------------------------------------------------------------------*/

// Value denotes the new value - i.e. 1 for a rising edge

void SysVIATriggerCA1Int(int Value)
{
	// value^=1;
	// DebugTrace("SysVIATriggerCA1Int at %d\n", TotalCycles);

	// Cause interrupt on appropriate edge
	if (!((SysVIAState.pcr & 1) ^ Value))
	{
		SysVIAState.ifr |= IFR_CA1;
		UpdateIFRTopBit();
	}
}

/*--------------------------------------------------------------------------*/

static void SysVIAPollReal()
{
	static bool t1int = false;

	if (SysVIAState.timer1c < -2 && !t1int)
	{
		t1int = true;

		if (!SysVIAState.timer1hasshot || (SysVIAState.acr & ACR_TIMER1_CONTINUOUS))
		{
			// DebugTrace("SysVia timer1 int at %d\n", TotalCycles);
			SysVIAState.ifr |= IFR_TIMER1; // Timer 1 interrupt
			UpdateIFRTopBit();

			if (SysVIAState.acr & ACR_TIMER1_OUTPUT_ENABLE)
			{
				SysVIAState.orb ^= 0x80; // Toggle PB7
				SysVIAState.irb ^= 0x80; // Toggle PB7
			}

			if ((SysVIAState.ier & IER_TIMER1) && CyclesToInt == NO_TIMER_INT_DUE)
			{
				CyclesToInt = 3 + SysVIAState.timer1c;
			}

			SysVIAState.timer1hasshot = true;
		}
	}

	if (SysVIAState.timer1c < -3)
	{
		SysVIAState.timer1c += (SysVIAState.timer1l * 2) + 4;
		t1int = false;
	}

	if (SysVIAState.timer2c < -2)
	{
		if (!SysVIAState.timer2hasshot)
		{
			// DebugTrace("SysVia timer2 int at %d\n", TotalCycles);
			SysVIAState.ifr |= IFR_TIMER2;
			UpdateIFRTopBit();

			if ((SysVIAState.ier & IER_TIMER2) && CyclesToInt == NO_TIMER_INT_DUE)
			{
				CyclesToInt = 3 + SysVIAState.timer2c;
			}

			SysVIAState.timer2hasshot = true;
		}
	}

	if (SysVIAState.timer2c < -3)
	{
		SysVIAState.timer2c += 0x20000; // Do not reload latches for T2
	}
}

void SysVIAPoll(unsigned int Cycles)
{
	// Converted to a proc to allow shift register functions
	// ChipClock(Cycles);

	SysVIAState.timer1c -= Cycles;

	if (!(SysVIAState.acr & ACR_TIMER2_CONTROL))
	{
		SysVIAState.timer2c -= Cycles;
	}

	if (SysVIAState.timer1c < 0 || SysVIAState.timer2c < 0)
	{
		SysVIAPollReal();
	}

	if (SRTrigger <= TotalCycles)
	{
		SRPoll();
	}

	// Ensure that CA2 keyboard interrupt is asserted when key pressed
	DoKbdIntCheck();

	// Do Shift register stuff
	// if (SRMode == 2) {
	//   Shift IN under control of Clock 2
	// SRCount = 8 - (ncycles % 8);
	// }
}

/*--------------------------------------------------------------------------*/

void SysVIAReset()
{
	VIAReset(&SysVIAState);

	// Make it no keys down and set dip switches
	BeebReleaseAllKeys();

	ClearTrigger(SRTrigger);
}

/*--------------------------------------------------------------------------*/

static void SRPoll()
{
	if (SysVIAState.SRMode == 6 || SysVIAState.SRMode == 2)
	{
		if (!(SysVIAState.ifr & IFR_SHIFTREG))
		{
			// Shift complete
			SysVIAState.ifr |= IFR_SHIFTREG;
			UpdateIFRTopBit();
		}

		ClearTrigger(SRTrigger);
	}
}

static void UpdateSRState(bool SRrw)
{
	SysVIAState.SRMode = (SysVIAState.acr >> 2) & 7;

	// TODO: Implement all SR modes, and actually shift the SR contents.

	if ((SysVIAState.SRMode == 6 || SysVIAState.SRMode == 2) && SRTrigger == CycleCountTMax)
	{
		// Set a timer to trigger setting the Shift Register interrupt.
		// 16 cycles at the VIA input clock rate (1 MHz),
		// so 32 cycles at 2 MHz.
		SetTrigger(32, SRTrigger);
	}

	if (SRrw)
	{
		if (SysVIAState.ifr & IFR_SHIFTREG)
		{
			SysVIAState.ifr &= ~IFR_SHIFTREG;
			UpdateIFRTopBit();
		}
	}
}

/*--------------------------------------------------------------------------*/

void DebugSysVIAState()
{
	DebugVIAState("SysVia", &SysVIAState);
}

/*--------------------------------------------------------------------------*/

void SaveSysVIAUEF(FILE *SUEF)
{
	UEFWrite8(0, SUEF); // 0: SysVIA, 1: UserVIA
	SaveVIAUEF(SUEF, &SysVIAState);
	UEFWrite8(IC32State, SUEF);
}

/*--------------------------------------------------------------------------*/

void LoadSysVIAUEF(FILE *SUEF, int Version)
{
	LoadVIAUEF(SUEF, Version, &SysVIAState);

	IC32State = UEFRead8(SUEF);

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
