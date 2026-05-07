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

#include <stdio.h>

#include "Via.h"
#include "Debug.h"
#include "SysVia.h"
#include "UefState.h"
#include "UserVia.h"

/*-------------------------------------------------------------------------*/

void VIAReset(VIAState *pVIA)
{
	pVIA->ora           = 0xff;
	pVIA->orb           = 0xff;
	pVIA->ira           = 0xff;
	pVIA->irb           = 0xff;
	pVIA->ddra          = 0; // All inputs
	pVIA->ddrb          = 0; // All inputs
	pVIA->acr           = 0; // Timed ints on t1, t2, no pb7 hacking, no latching, no shifting
	pVIA->pcr           = 0; // Neg edge inputs for cb2,ca2 and CA1 and CB1
	pVIA->ifr           = 0; // No interrupts presently interrupting
	pVIA->ier           = IER_SET_CLEAR; // No interrupts enabled
	pVIA->timer1l       = 0xffff; // 0xffff
	pVIA->timer1c       = 0xffff; // 0x1ffff
	pVIA->timer2l       = 0xffff; // 0xffff
	pVIA->timer2c       = 0xffff; // 0x1ffff
	pVIA->timer1hasshot = false;
	pVIA->timer2hasshot = false;
	pVIA->timer1adjust  = 0; // Added by Ken Lowe 24/08/03
	pVIA->timer2adjust  = 0;
	pVIA->ca2           = false;
	pVIA->cb2           = false;
	pVIA->SRMode        = 0;
}

/*-------------------------------------------------------------------------*/

void DebugVIAState(const char *Name, VIAState *pVIA)
{
	DebugDisplayInfo("");

	DebugDisplayInfoF("%s: ora=%02X orb=%02X ira=%02X irb=%02X ddra=%02X ddrb=%02X",
	                  Name,
	                  (int)pVIA->ora, (int)pVIA->orb,
	                  (int)pVIA->ira, (int)pVIA->irb,
	                  (int)pVIA->ddra, (int)pVIA->ddrb);

	DebugDisplayInfoF("%s: acr=%02X pcr=%02X ifr=%02X ier=%02X",
	                  Name,
	                  (int)pVIA->acr, (int)pVIA->pcr,
	                  (int)pVIA->ifr, (int)pVIA->ier);

	DebugDisplayInfoF("%s: t1=%04X%s t2=%04X%s t1l=%04X t2l=%04X t1s=%d t2s=%d",
	                  Name,
	                  (int)(pVIA->timer1c < 0 ? ((pVIA->timer1c - 1) / 2) & 0xffff : pVIA->timer1c / 2),
	                  pVIA->timer1c & 1 ? "+" : " ",
	                  (int)(pVIA->timer2c < 0 ? ((pVIA->timer2c - 1) / 2) & 0xffff : pVIA->timer2c / 2),
	                  pVIA->timer2c & 1 ? "+" : " ",
	                  (int)pVIA->timer1l, (int)pVIA->timer2l,
	                  (int)pVIA->timer1hasshot, (int)pVIA->timer2hasshot);

	DebugDisplayInfoF("%s: ca2=%d cb2=%d",
	                  Name,
	                  (int)pVIA->ca2, (int)pVIA->cb2);
}

/*-------------------------------------------------------------------------*/

void SaveVIAUEF(FILE *SUEF, VIAState* pVIA)
{
	UEFWrite8(pVIA->orb, SUEF);
	UEFWrite8(pVIA->irb, SUEF);
	UEFWrite8(pVIA->ora, SUEF);
	UEFWrite8(pVIA->ira, SUEF);
	UEFWrite8(pVIA->ddrb, SUEF);
	UEFWrite8(pVIA->ddra, SUEF);
	UEFWrite32(pVIA->timer1c, SUEF);
	UEFWrite16(pVIA->timer1l, SUEF);
	UEFWrite32(pVIA->timer2c, SUEF);
	UEFWrite16(pVIA->timer2l, SUEF);
	UEFWrite8(pVIA->acr, SUEF);
	UEFWrite8(pVIA->pcr, SUEF);
	UEFWrite8(pVIA->ifr, SUEF);
	UEFWrite8(pVIA->ier, SUEF);
	UEFWrite8(pVIA->timer1hasshot, SUEF);
	UEFWrite8(pVIA->timer2hasshot, SUEF);
}

/*-------------------------------------------------------------------------*/

void LoadVIAUEF(FILE *SUEF, int Version, VIAState* pVIA)
{
	pVIA->orb = UEFRead8(SUEF);
	pVIA->irb = UEFRead8(SUEF);
	pVIA->ora = UEFRead8(SUEF);
	pVIA->ira = UEFRead8(SUEF);
	pVIA->ddrb = UEFRead8(SUEF);
	pVIA->ddra = UEFRead8(SUEF);

	if (Version >= 16)
	{
		// Store as 2MHz counters.
		pVIA->timer1c = UEFRead32(SUEF);
	}
	else
	{
		// Prior to version 16, the VIA timer counters were
		// stored as 1MHz values.
		pVIA->timer1c = UEFRead16(SUEF) * 2;
	}

	pVIA->timer1l = UEFRead16(SUEF);

	if (Version >= 16)
	{
		pVIA->timer2c = UEFRead32(SUEF);
	}
	else
	{
		pVIA->timer2c = UEFRead16(SUEF) * 2;
	}

	pVIA->timer2l = UEFRead16(SUEF);
	pVIA->acr = UEFRead8(SUEF);
	pVIA->pcr = UEFRead8(SUEF);
	pVIA->ifr = UEFRead8(SUEF);
	pVIA->ier = UEFRead8(SUEF);
	pVIA->timer1hasshot = UEFReadBool(SUEF);
	pVIA->timer2hasshot = UEFReadBool(SUEF);

	pVIA->ca2 = ((pVIA->pcr & PCR_CA2_CONTROL) == PCR_CA2_OUTPUT_HIGH);
	pVIA->cb2 = ((pVIA->pcr & PCR_CB2_CONTROL) == PCR_CB2_OUTPUT_HIGH);
	pVIA->SRMode = (pVIA->acr >> 2) & 7;
}

/*-------------------------------------------------------------------------*/
