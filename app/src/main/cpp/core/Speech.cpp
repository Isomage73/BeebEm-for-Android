/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1997-2016  MAMEDev and contributors
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

// TMS5200/5220 simulator
//
// Written for MAME by Frank Palazzolo
// With help from Neill Corlett
// Additional tweaking by Aaron Giles
// TMS6100 Speech Rom support added by Raphael Nabet
// PRNG code by Jarek Burczynski backported from tms5110.c by Lord Nightmare
// Chirp/excitation table fixes by Lord Nightmare
// Various fixes by Lord Nightmare
// Modularization by Lord Nightmare
// Sub-interpolation-cycle parameter updating added by Lord Nightmare
// Preliminary MASSIVE merge of tms5110 and tms5220 cores by Lord Nightmare
// Lattice Filter, Multiplier, and clipping redone by Lord Nightmare
// TMS5220C multi-rate feature added by Lord Nightmare
// Massive rewrite and reorganization by Lord Nightmare
// Additional IP, PC, subcycle timing rewrite by Lord Nightmare
// Updated based on the chip decaps done by digshadow
//
// Much information regarding the lpc encoding used here comes from US patent
// 4,209,844
// US patent 4,331,836 describes the complete 51xx chip
// US patent 4,335,277 describes the complete 52xx chip
// Special Thanks to Larry Brantingham for answering questions regarding the
// chip details
//
// TMS5220/TMS5220C:
//
//                  +-----------------+
//         D7(d0)   |  1           28 |  /RS
//         ADD1     |  2           27 |  /WS
//         ROMCLK   |  3           26 |  D6(d1)
//         VDD(-5)  |  4           25 |  ADD2
//         VSS(+5)  |  5           24 |  D5(d2)
//         OSC      |  6           23 |  ADD4
//         T11      |  7           22 |  D4(d3)
//         SPKR     |  8           21 |  ADD8/DATA
//         I/O      |  9           20 |  TEST
//         PROMOUT  | 10           19 |  D3(d4)
//         VREF(GND)| 11           18 |  /READY
//         D2(d5)   | 12           17 |  /INT
//         D1(d6)   | 13           16 |  M1
//         D0(d7)   | 14           15 |  M0
//                  +-----------------+
//
// Note the standard naming for d* data bits with 7 as MSB and 0 as LSB is in
// lowercase. TI's naming has D7 as LSB and D0 as MSB and is in uppercase.
//
// TODO:
// * Samples repeat over and over in the 'eprom' test mode. Needs investigation.
// * Implement a ready callback for pc interfaces
//   - this will be quite a challenge since for it to be really accurate
//     the whole emulation has to run in sync (lots of timers) with the
//     cpu cores.
// * If a command is still executing, /READY will be kept high until the command
//   has finished if the next command is written.
// * tomcat has a 5220 which is not hooked up at all
//
// Pedantic detail from observation of real chip:
// The 5200 and 5220 chips outputs the following coefficients over PROMOUT while
// 'idle' and not speaking, in this order:
// e[0 or f] p[0] k1[0] k2[0] k3[0] k4[0] k5[f] k6[f] k7[f] k8[7] k9[7] k10[7]
//
// Patent notes (important timing info for interpolation):
// * TCycle ranges from 1 to 20, is clocked based on the clock input or RC clock
//   to the chip / 4. This emulation core completely ignores TCycle, as it isn't
//   very relevant.
//     Every full TCycle count (i.e. overflow from 20 to 1), Subcycle is
//     incremented.
// * Subcycle ranges from 0 to 2, reload is 0 in SPKSLOW mode, 1 normally, and
//   corresponds to whether an interpolation value is being calculated (0 or 1)
//   or being written to ram (2). 0 and 1 correspond to 'A' cycles on the
//   patent, while 2 corresponds to 'B' cycles.
//     Every Subcycle full count (i.e. overflow from 2 to (0 or 1)), PC is
//     incremented. (NOTE: if PC=12, overflow happens on the 1->2 transition,
//     not 2->0; PC=12 has no B cycle.)
// * PC ranges from 0 to 12, and corresponds to the parameter being interpolated
//   or otherwise read from rom using PROMOUT.
//   The order is:
//   0 = Energy
//   1 = Pitch
//   2 = K1
//   3 = K2
//   ...
//   11 = K10
//   12 = nothing
//     Every PC full count (i.e. overflow from 12 to 0), IP (aka "Interpolation
//     Period") is incremented.
// * IP (aka "Interpolation Period") ranges from 0 to 7, and corresponds with
//   the amount of rightshift that the difference between current and target
//   for a given parameter will have applied to it, before being added to the
//   current parameter. Note that when interpolation is inhibited, only IP=0
//   will cause any change to the current values of the coefficients.
//   The order is, after new frame parse (last ip was 0 before parse):
//   1 = >>3 (/8)
//   2 = >>3 (/8)
//   3 = >>3 (/8)
//   4 = >>2 (/4)
//   5 = >>2 (/4)
//   6 = >>1 (/2) (NOTE: the patent has an error regarding this value on one
//                table implying it should be /4, but circuit simulation of
//                parts of the patent shows that the /2 value is correct.)
//   7 = >>1 (/2)
//   0 = >>0 (/1, forcing current values to equal target values)
//     Every IP full count, a new frame is parsed, but ONLY on the 0->*
//     transition.
//     NOTE: on TMS5220C ONLY, the datasheet IMPLIES the following:
//     Upon new frame parse (end of IP=0), the IP is forced to a value depending
//     on the TMS5220C-specific rate setting. For rate settings 0, 1, 2, 3, it
//     will be forced to 1, 3, 5 or 7 respectively. On non-TMS5220 chips, it
//     counts as expected (IP=1 follows IP=0) always.
//     This means, the tms5220c with rates set to n counts IP as follows:
//     (new frame parse is indicated with a #)
//     Rate    IP Count
//     00      7 0#1 2 3 4 5 6 7 0#1 2 3 4 5 6 7    <- non-tms5220c chips always follow this pattern
//     01      7 0#3 4 5 6 7 0#3 4 5 6 7 0#3 4 5
//     10      7 0#5 6 7 0#5 6 7 0#5 6 7 0#5 6 7
//     11      7 0#7 0#7 0#7 0#7 0#7 0#7 0#7 0#7
//     Based on the behavior tested on the CD2501ECD this is assumed to be the
//     same for that chip as well.
//
// Most of the following is based on figure 8c of 4,331,836, which is the
// TMS5100/TMC0280 patent, but the same information applies to the TMS52xx
// as well.
//
// OLDP is a status flag which controls whether unvoiced or voiced excitation
//   is being generated. It is latched from "P=0" at IP=7 PC=12 T=16.
//   (This means that, during normal operation, between IP=7 PC=12 T16 and
//   IP=0 PC=1 T17, OLDP and P=0 are the same)
// "P=0" is a status flag which is set if the index value for pitch for the new
//   frame being parsed (which will become the new target frame) is zero.
//   It is used for determining whether interpolation of the next frame is
//   inhibited or not. It is updated at IP=0 PC=1 T17. See next section.
// OLDE is a status flag which is only used for determining whether
//   interpolation is inhibited or not.
//   It is latched from "E=0" at IP=7 PC=12 T=16.
//   (This means that, during normal operation, between IP=7 PC=12 T16 and
//   IP=0 PC=0 T17, OLDE and E=0 are the same)
// "E=0" is a status flag which is set if the index value for energy for the
//   new frame being parsed (which will become the new target frame) is zero.
//   It is used for determining whether interpolation of the next frame is
//   inhibited or not. It is updated at IP=0 PC=0 T17. See next section.
//
// Interpolation is inhibited (i.e. interpolation at IP frames will not happen
//   except for IP=0) under the following circumstances:
//   "P=0" != "OLDP" ("P=0" = 1, and OLDP = 0; OR "P=0" = 0, and OLDP = 1)
//     This means the new frame is unvoiced and the old one was voiced, or vice
//     versa.
// * TODO the 5100 and 5200 patents are inconsistent about the above. Trace the decaps!
//   "OLDE" = 1 and "E=0" = 0
//     This means the new frame is not silent, and the old frame was silent.

// ****Documentation of chip commands:***
//
// x0x0xbcc: on 5200/5220: NOP (does nothing); on 5220C and CD2501ECD: Select
//           frame length by cc, and b selects whether every frame is preceded
//           by 2 bits to select the frame length (instead of using the value
//           set by cc); the default (and after a reset command) is as if '0x00'
//           was written, i.e. for frame length (200 samples) and 0 for whether
//           the preceding 2 bits are enabled (off)
//
// x001xxxx: READ BYTE (RDBY) Sends eight read bit commands (M0 high M1 low) to
//           VSM and reads the resulting bits serially into a temporary register,
//           which becomes readable as the next byte read from the tms52xx once
//           ready goes active. Note the bit order of the byte read from the
//           TMS52xx is BACKWARDS as compared to the actual data order as in the
//           rom on the VSM chips; the read byte command of the tms5100 reads the
//           bits in the 'correct' order. This was IMHO a rather silly design
//           decision of TI. (I (LN) asked Larry Brantingham about this but he
//           wasn't involved with the TMS52xx chips, just the 5100); There's
//           ASCII data in the TI 99/4 speech module VSMs which has the bit
//           order reversed on purpose because of this!
//
//           TALK STATUS must be CLEAR for this command to work; otherwise it is
//           treated as a NOP.
//
// x011xxxx: READ AND BRANCH (RB) Sends a read and branch command (M0 high,
//           M1 high) to force VSM to set its data pointer to whatever the data
//           is at its current pointer location is)
//
//           TALK STATUS must be CLEAR for this command to work; otherwise it is
//           treated as a NOP.
//
// x100aaaa: LOAD ADDRESS (LA) Send a load address command (M0 low M1 high) to
//           VSM with the 4 'a' bits; Note you need to send four or five of
//           these in sequence to actually specify an address to the vsm.
//
//           TALK STATUS must be CLEAR for this command to work; otherwise it is
//           treated as a NOP.
//
// x101xxxx: SPEAK (SPK) Begins speaking, pulling speech data from the current
//           address pointer location of the VSM modules.
//
// x110xxxx: SPEAK EXTERNAL (SPKEXT) Clears the FIFO using SPKEE line, then sets
//           TALKD (TALKST remains zero) until 8 bytes have been written to the
//           FIFO, at which point it begins speaking, pulling data from the
//           16 byte fifo.
//
//           The patent implies TALK STATUS must be CLEAR for this command to
//           work; otherwise it is treated as a NOP, but the decap shows that
//           this is not true, and is an error on the patent diagram.
//
// x111xxxx: RESET (RST) Resets the speech synthesis core immediately, and
//           clears the FIFO.
//
// Other chip differences:
//
// The 5220C (and CD2501ECD maybe?) are quieter due to a better dac arrangement
// on die which allows less crossover between bits, based on the decap
// differences.

#include <fstream>
#include <stdio.h>

#include "Windows.h"

#include "Speech.h"
#include "6502core.h"
#include "BeebWin.h"
#include "DebugTrace.h"
#include "FileUtils.h"
#include "Log.h"
#include "Main.h"
#include "Sound.h"
#include "StringUtils.h"
#include "UefState.h"

// Coefficient definitions.
constexpr int MAX_K = 10;
constexpr int MAX_SCALE_BITS = 6;
constexpr int MAX_SCALE = 1 << MAX_SCALE_BITS;
constexpr int MAX_CHIRP_SIZE = 52;

struct tms5100_coeffs
{
	int num_k;
	int energy_bits;
	int pitch_bits;
	int kbits[MAX_K];
	unsigned short energytable[MAX_SCALE];
	unsigned short pitchtable[MAX_SCALE];
	int ktable[MAX_K][MAX_SCALE];
	int16_t chirptable[MAX_CHIRP_SIZE];
	int8_t interp_coeff[8];
};

// TMS5220/5220C:
// (1983 era for 5220, 1986-1992 era for 5220C; 5220C may also be called TSP5220C)
// The TMS5220NL was decapped and imaged by digshadow in April, 2013.
// The LPC table table is verified to match the decap.
// The chirp table is verified to match the decap. (sum = 0x3da)
// Note that all the LPC K* values match the TMS5110a table (as read via PROMOUT)
// exactly.
// The TMS5220CNL was decapped and imaged by digshadow in April, 2013.
// The LPC table table is verified to match the decap and exactly matches TMS5220NL.
// The chirp table is verified to match the decap. (sum = 0x3da)

#define TI_028X_LATER_ENERGY \
	/* E  */\
	{  0,  1,  2,  3,  4,  6,   8, 11, \
	  16, 23, 33, 47, 63, 85, 114, 0 },

#define TI_5220_PITCH \
	/* P */\
	{   0,  15,  16,  17,  18,  19,  20,  21, \
	   22,  23,  24,  25,  26,  27,  28,  29, \
	   30,  31,  32,  33,  34,  35,  36,  37, \
	   38,  39,  40,  41,  42,  44,  46,  48, \
	   50,  52,  53,  56,  58,  60,  62,  65, \
	   68,  70,  72,  76,  78,  80,  84,  86, \
	   91,  94,  98, 101, 105, 109, 114, 118, \
	  122, 127, 132, 137, 142, 148, 153, 159 },

#define TI_5110_5220_LPC \
	/* K1  */\
	{ -501, -498, -497, -495, -493, -491, -488, -482, \
	  -478, -474, -469, -464, -459, -452, -445, -437, \
	  -412, -380, -339, -288, -227, -158,  -81,   -1, \
	    80,  157,  226,  287,  337,  379,  411,  436 }, \
	/* K2  */\
	{ -328, -303, -274, -244, -211, -175, -138,  -99, \
	   -59,  -18,   24,   64,  105,  143,  180,  215, \
	   248,  278,  306,  331,  354,  374,  392,  408, \
	   422,  435,  445,  455,  463,  470,  476,  506 }, \
	/* K3  */\
	{ -441, -387, -333, -279, -225, -171, -117,  -63, \
	    -9,   45,   98,  152,  206,  260,  314,  368  }, \
	/* K4  */\
	{ -328, -273, -217, -161, -106,  -50,    5,   61, \
	   116,  172,  228,  283,  339,  394,  450,  506  }, \
	/* K5  */\
	{ -328, -282, -235, -189, -142,  -96,  -50,   -3, \
	    43,   90,  136,  182,  229,  275,  322,  368  }, \
	/* K6  */\
	{ -256, -212, -168, -123,  -79,  -35,   10,   54, \
	    98,  143,  187,  232,  276,  320,  365,  409  }, \
	/* K7  */\
	{ -308, -260, -212, -164, -117,  -69,  -21,   27, \
	    75,  122,  170,  218,  266,  314,  361,  409  }, \
	/* K8  */\
	{ -256, -161,  -66,   29,  124,  219,  314,  409  }, \
	/* K9  */\
	{ -256, -176,  -96,  -15,   65,  146,  226,  307  }, \
	/* K10 */\
	{ -205, -132,  -59,   14,   87,  160,  234,  307  },

#define TI_LATER_CHIRP \
	/* Chirp table */\
	{ 0x00, 0x03, 0x0f, 0x28, 0x4c, 0x6c, 0x71, 0x50, \
	  0x25, 0x26, 0x4c, 0x44, 0x1a, 0x32, 0x3b, 0x13, \
	  0x37, 0x1a, 0x25, 0x1f, 0x1d, 0x00, 0x00, 0x00, \
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	  0x00, 0x00, 0x00, 0x00 },

// Interpolation Table
#define TI_INTERP \
	/* interpolation shift coefficients */\
	{ 0, 3, 3, 3, 2, 2, 1, 1 }

static const struct tms5100_coeffs tms5220_coeffs =
{
	10,
	4,
	6,
	{ 5, 5, 4, 4, 4, 4, 4, 3, 3, 3 },
	TI_028X_LATER_ENERGY
	TI_5220_PITCH
	{
		TI_5110_5220_LPC
	},
	TI_LATER_CHIRP
	TI_INTERP
};

// 18-bit mask for TMS5220 address.
constexpr int TMS5220_ADDRESS_MASK = 0x3FFFF;

/*----------------------------------------------------------------------------*/

class TMS5220
{
	public:
		TMS5220();
		TMS5220(const TMS5220&) = delete;
		TMS5220& operator=(const TMS5220&) = delete;
		~TMS5220();

	public:
		void Reset();
		void ReadEnable();
		unsigned char ReadStatus();
		void WriteData(unsigned char data);
		bool ReadInt() const;
		bool ReadReady() const;

		void Poll(int Cycles);

		void ProcessSamples(short* buffer, int size);

		void LoadState(FILE *SUEF);
		void SaveState(FILE *SUEF) const;

	private:
		int16_t LatticeFilter();
		void ProcessCommand(unsigned char cmd);
		uint8_t ExtractBits(int count);
		void ParseFrame();
		void UpdateReadyState();
		void UpdateFifoStatusAndInts();

		// PHROM methods.
		void LoadPhromAddress(int data);
		void ReadAndBranch();
		uint8_t ReadPhrom(int count);

	private:
		// These contain data that describes the 128-bit data FIFO.
		static constexpr int FIFO_SIZE = 16;
		uint8_t m_fifo[FIFO_SIZE];
		uint8_t m_fifo_head;
		uint8_t m_fifo_tail;
		uint8_t m_fifo_count;
		uint8_t m_fifo_bits_taken;

		// These contain global status bits.

		// R Nabet: m_speak_external is only set when a speak external command
		// is going on.
		// m_tms5220_speaking is set whenever a speak or speak external command
		// is going on.
		// Note that we really need to do anything in tms5220_process and play
		// samples only when m_tms5220_speaking is true. Else, we can play
		// nothing as well, which is a speed-up...

		// This is the OLD value of TALK_STATUS (i.e., previous value of
		// m_SPEN | m_TALKD), needed for generating interrupts on a falling
		// TALK_STATUS edge.
		bool m_previous_TALK_STATUS;
		// Set on speak (or speak external and BL falling edge) command,
		// cleared on stop command, reset command, or buffer out.
		bool m_SPEN;
		// If true, DDIS is 1, i.e. Speak External command in progress,
		// writes go to FIFO.
		bool m_DDIS;
		// Set on SPEN & RESETL4(pc12->pc0 transition), cleared on stop
		// command or reset command.
		bool m_TALK;
		// TALK(TCON) value, latched every RESETL4
		bool m_TALKD;
		// If true, FIFO has less than 8 bytes in it.
		bool m_buffer_low;
		// If true, FIFO is empty.
		bool m_buffer_empty;
		// State of the IRQ output pin (active high),
		// convert to active low in SpeechInterrupt().
		bool m_irq_pin;
		// State of the READY output pin (active high),
		// convert to active low in SpeechReady().
		bool m_ready_pin;

		// These contain data describing the current and previous voice frames.
		bool m_OLDE;
		bool m_OLDP;

		uint8_t m_new_frame_energy_idx;
		uint8_t m_new_frame_pitch_idx;
		uint8_t m_new_frame_k_idx[10];

		// These are all used to contain the current state
		// of the sound generation.
		int16_t m_current_energy;
		int16_t m_current_pitch;
		int16_t m_current_k[10];

		// Needed for lattice filter to match patent.
		uint16_t m_previous_energy;

		// Contains the current subcycle for a given PC:
		// 0 is A' (only used on SPKSLOW mode on 51xx), 1 is A, 2 is B.
		uint8_t m_subcycle;
		// Contains 1 for normal speech, 0 when SPKSLOW is active.
		uint8_t m_subc_reload;
		// Current parameter counter (what param is being interpolated),
		// ranges from 0 to 12.
		uint8_t m_PC;

		// NOTE: the interpolation period counts 1,2,3,4,5,6,7,0
		// for divide by 8,8,8,4,4,2,2,1.

		// The current interpolation period.
		uint8_t m_IP;
		// If true, interpolation is inhibited until the DIV1 period.
		bool m_inhibit;
		// If 1, zero k5 thru k10 coefficients.
		uint8_t m_uv_zpar;
		// If 1, zero ALL parameters.
		uint8_t m_zpar;
		// Circuit 412; pitch is forced to zero under certain circumstances.
		bool m_pitch_zero;

		// Pitch counter; provides chirp ROM address.
		uint16_t m_pitch_count;

		int32_t m_u[11];
		int32_t m_x[10];

		uint16_t m_RNG; // The random noise generator configuration is: 1 + x + x^3 + x^4 + x^13 TODO: no it isn't
		int16_t m_excitation_data;

		// R Nabet: These have been added to emulate speech ROMs

		// Set after each load address, so that next read operation
		// is preceded by a dummy read.
		bool m_schedule_dummy_read;
		// Data register, used by read command.
		uint8_t m_data_register;
		// Whether we should read data register or status register.
		bool m_RDB_flag;

		// The TMS52xx has two different ways of providing output data: the
		// analog speaker pin (which was usually used) and the Digital I/O pin.
		// The internal DAC used to feed the analog pin is only 8 bits, and has the
		// funny clipping/clamping logic, while the digital pin gives full 10 bit
		// resolution of the output data.
		// TODO: add a way to set/reset this other than the FORCE_DIGITAL define.
		bool m_digital_select;

		// Page 3 of the datasheet specifies that READY will be asserted until
		// data is available or processed by the system.
		bool m_io_ready;

		// Countdown timer to reset the ready flag after read/write enable
		// is asserted.
		int m_ready_count;

		// PHROM variables.

		// Length of data pointed by speechrom_data, from 0 to 2^18.
		uint32_t m_speechROMlen;
		// 18 bit pointer in ROM.
		uint32_t m_speechROMaddr;
		// Which 4-bit nibble will be affected by load address.
		int m_load_pointer;
		// Current bit position in ROM
		uint8_t m_ROM_bits_count;
};

/*----------------------------------------------------------------------------*/

// The state of the streamed output.

class TMS5220StreamState
{
	public:
		TMS5220StreamState(int Clock);
		TMS5220StreamState(const TMS5220StreamState&) = delete;
		TMS5220StreamState& operator=(const TMS5220StreamState&) = delete;
		~TMS5220StreamState();

	public:
		void Update(unsigned char *buff, int length);

		void LoadState(FILE *SUEF);
		void SaveState(FILE *SUEF) const;

	public:
		TMS5220 chip;

	private:
		static constexpr int MAX_SAMPLE_CHUNK = 8000;

		static constexpr int FRAC_BITS = 14;
		static constexpr int FRAC_ONE  = 1 << FRAC_BITS;
		static constexpr int FRAC_MASK = FRAC_ONE - 1;

		int m_last_sample;
		int m_curr_sample;
		int m_source_step;
		int m_source_pos;
};

/*----------------------------------------------------------------------------*/

bool SpeechEnabled;
bool SpeechStarted;

static TMS5220StreamState* tms5220;
static unsigned char speechrom_data[16 * 16384];

// #define DEBUG_SPEECH

/*----------------------------------------------------------------------------*/

TMS5220::TMS5220()
{
	Reset();
}

/*----------------------------------------------------------------------------*/

TMS5220::~TMS5220()
{
}

/*--------------------------------------------------------------------------*/

// Resets the TMS5220.

void TMS5220::Reset()
{
	m_digital_select = false; // Analog output

	// Initialise the FIFO.
	ZeroMemory(m_fifo, sizeof(m_fifo));
	m_fifo_head = 0;
	m_fifo_tail = 0;
	m_fifo_count = 0;
	m_fifo_bits_taken = 0;

	// Initialize the chip state.
	// Note that we do not actually clear IRQ on start-up: IRQ is even raised
	// if m_buffer_empty or buffer_low are false.
	m_SPEN = false;
	m_DDIS = false;
	m_TALK = false;
	m_TALKD = false;
	m_previous_TALK_STATUS = false;
	m_irq_pin = false;
	m_ready_pin = false;

	m_buffer_empty = true;
	m_buffer_low = true;

	m_RDB_flag = false;

	// Initialize the energy/pitch/k states.
	m_new_frame_energy_idx = 0;
	m_current_energy = 0;
	m_previous_energy = 0;
	m_new_frame_pitch_idx = 0;
	m_current_pitch = 0;
	m_zpar = 0;
	m_uv_zpar = 0;

	ZeroMemory(m_new_frame_k_idx, sizeof(m_new_frame_k_idx));
	ZeroMemory(m_current_k, sizeof(m_current_k));

	// Initialize the sample generators.
	m_inhibit = true;
	m_subcycle = 0;
	m_pitch_count = 0;
	m_PC = 0;

	// If 1, normal speech (one A cycle, one B cycle per interpolation step).
	// If 0, speak as if SPKSLOW was used (two A cycles, one B cycle per interpolation step).
	m_subc_reload = 1;
	m_OLDE = true;
	m_OLDP = true;
	m_IP = 0;
	m_RNG = 0x1FFF;

	ZeroMemory(m_u, sizeof(m_u));
	ZeroMemory(m_x, sizeof(m_x));

	m_schedule_dummy_read = false;

	// rs and ws are assumed to be inactive on device startup.
	m_io_ready = true;

	m_ready_count = 0;

	m_speechROMlen = 16 * 16384;
	m_speechROMaddr = 0;
	m_load_pointer = 0;
	m_ROM_bits_count = 0;

	LoadPhromAddress(0);

	// MZ: Do the dummy read immediately. The previous line will cause a
	// shift in the address pointer in the VSM. When the next command is a
	// load_address, no dummy read will occur, hence the address will be
	// falsely shifted.
	ReadPhrom(1);
	m_schedule_dummy_read = false;
}

/*----------------------------------------------------------------------------*/

// Called when the TMS5200 /RS pin has been pulled low.

void TMS5220::ReadEnable()
{
	// On high to low transition, schedule ready cycle.

	// Upon /RS being activated, /READY goes inactive after 100 nsec
	// from data sheet, through 3 asynchronous gates on patent.
	// This is effectively within one clock, so we immediately set
	// io_ready to false and activate the callback.
	m_io_ready = false;
	UpdateReadyState();

	// How long does /READY stay inactive, when /RS is pulled low?
	// I believe its almost always ~16 clocks (25 usec at 800khz as
	// shown on the datasheet).

	// This should take around 10-16 (closer to ~11?) cycles to complete.

	m_ready_count = 30;
}

/*----------------------------------------------------------------------------*/

void TMS5220::UpdateReadyState()
{
	m_ready_pin = ReadReady();
}

/*--------------------------------------------------------------------------*/

// Read status or data from the TMS5220

// From the datasheet:
//
// bit D0(bit 7) = TS - Talk Status is active (high) when the VSP is processing
//     speech data. Talk Status goes active at the initiation of a Speak
//     command or after nine bytes of data are loaded into the FIFO following
//     a Speak External command. It goes inactive (low) when the stop code
//     (Energy=1111) is processed, or immediately by a buffer empty condition
//     or a reset command.
//
// bit D1(bit 6) = BL - Buffer Low is active (high) when the FIFO buffer is
//     more than half empty. Buffer Low is set when the "Last-In" byte is
//     shifted down past the half-full boundary of the stack. Buffer Low is
//     cleared when data is loaded to the stack so that the "Last-In" byte lies
//     above the half-full boundary and becomes the ninth data byte of the stack.
//
// bit D2(bit 5) = BE - Buffer Empty is active (high) when the FIFO buffer has
//     run out of data while executing a Speak External command. Buffer Empty is
//     set when the last bit of the "Last-In" byte is shifted out to the
//     Synthesis Section. This causes Talk Status to be cleared. Speed is
//     terminated at some abnormal point and the Speak External command
//     execution is terminated.

unsigned char TMS5220::ReadStatus()
{
	if (m_RDB_flag)
	{
		// If last command was read, return data register.
		m_RDB_flag = false;

		#ifdef DEBUG_SPEECH
		DebugTrace("%04X TMS5220: Read Data: %02X\n", PrePC, m_data_register);
		#endif

		return m_data_register;
	}
	else
	{
		// Read status

		// Clear the interrupt pin on status read.
		m_irq_pin = false;

		#ifdef DEBUG_SPEECH
		DebugTrace("%04X TMS5220: Read Status: TS=%d BL=%d BE=%d\n", PrePC, m_SPEN || m_TALKD, m_buffer_low, m_buffer_empty);
		DebugTrace("%04X TMS5220: Clear interrupt\n", PrePC);
		#endif

		return ((m_SPEN || m_TALKD) ? 0x80 : 0x00) |
		       (m_buffer_low   ? 0x40 : 0x00) |
		       (m_buffer_empty ? 0x20 : 0x00);
	}
}

/*--------------------------------------------------------------------------*/

// Handle a write to the TMS5220.

void TMS5220::WriteData(unsigned char data)
{
	#ifdef DEBUG_SPEECH
	DebugTrace("%04X TMS5220: Write %02X\n", PrePC, data);
	#endif

	bool old_buffer_low = m_buffer_low;

	if (m_DDIS)
	{
		// We're in speak external mode, add this byte to the FIFO.

		if (m_fifo_count < FIFO_SIZE)
		{
			m_fifo[m_fifo_tail] = data;
			m_fifo_tail = (m_fifo_tail + 1) % FIFO_SIZE;
			m_fifo_count++;

			#ifdef DEBUG_SPEECH
			DebugTrace("%04X TMS5220: Added byte to FIFO (size=%d)\n", PrePC, m_fifo_count);
			#endif

			UpdateFifoStatusAndInts();

			// If we just unset buffer low with that last write,
			// and SPEN *was* zero (see circuit 251, sheet 12).

			if (!m_SPEN && old_buffer_low && !m_buffer_low)
			{
				m_zpar = 1;
				m_uv_zpar = 1;
				m_OLDE = true;
				m_OLDP = true;
				m_SPEN = true;
				m_TALK = true;

				m_new_frame_energy_idx = 0;
				m_new_frame_pitch_idx = 0;

				for (int i = 0; i < 4; i++)
				{
					m_new_frame_k_idx[i] = 0;
				}

				for (int i = 4; i < 7; i++)
				{
					m_new_frame_k_idx[i] = 0xF;
				}

				for (int i = 7; i < tms5220_coeffs.num_k; i++)
				{
					m_new_frame_k_idx[i] = 0x7;
				}
			}
		}
		else
		{
			#ifdef DEBUG_SPEECH
			DebugTrace("%04X TMS5220: Ran out of room in the FIFO!\n", PrePC);
			#endif
		}
	}
	else // !m_DDIS
	{
		ProcessCommand(data);
	}
}

/*--------------------------------------------------------------------------*/

// Returns the interrupt state of the TMS5220
// (true: interrupt, false: no interrupt).

bool TMS5220::ReadInt() const
{
	return m_irq_pin;
}

/*--------------------------------------------------------------------------*/

// Returns the ready state of the TMS5220
// (true: ready, false: not ready).

bool TMS5220::ReadReady() const
{
	return ((m_fifo_count < FIFO_SIZE) || !m_DDIS) && m_io_ready;
}

/*--------------------------------------------------------------------------*/

void TMS5220::Poll(int Cycles)
{
	if (!m_io_ready)
	{
		if (m_ready_count > 0)
		{
			m_ready_count -= Cycles;

			if (m_ready_count <= 0)
			{
				m_ready_count = 0;
				m_io_ready = true;

				#ifdef DEBUG_SPEECH
				DebugTrace("%04X TMS5220: Ready\n", PrePC);
				#endif

				UpdateReadyState();
			}
		}
	}
}

/*--------------------------------------------------------------------------*/

// Clips the 14 bit return value from the lattice filter to its final 10 bit
// value (-512 to 511), and upshifts/range extends this to 16 bits.

static int16_t ClipAnalog(int16_t cliptemp)
{
	// Clipping, just like the patent shows:
	// The top 10 bits of this result are visible on the digital output IO pin.
	// Next, if the top 3 bits of the 14 bit result are all the same,
	// the lowest of those 3 bits plus the next 7 bits are the signed analog
	// output, otherwise the low bits are all forced to match the inverse
	// of the topmost bit, i.e.:
	//
	// 1x xxxx xxxx xxxx -> 0b10000000
	// 11 1bcd efgh xxxx -> 0b1bcdefgh
	// 00 0bcd efgh xxxx -> 0b0bcdefgh
	// 0x xxxx xxxx xxxx -> 0b01111111

	if (cliptemp > 2047)
	{
		cliptemp = 2047;
	}
	else if (cliptemp < -2048)
	{
		cliptemp = -2048;
	}

	cliptemp &= ~0xF;

	// Input:  ssss snnn nnnn 0000
	// N taps:       ^^^ ^^^^      = 0x07F0
	// P taps:       ^             = 0x0400
	// Output: snnn nnnn NNNN NNNP

	// Upshift and range adjust.
	return (cliptemp << 4) |
	       ((cliptemp & 0x7F0) >> 3) |
	       ((cliptemp & 0x400) >> 10);
}

/*--------------------------------------------------------------------------*/

// Does the proper multiply and shift.
// a is the k coefficient and is clamped to 10 bits (9 bits plus a sign)
// b is the running result and is clamped to 14 bits.
// output is 14 bits, but note the result LSB bit is always 1.
// Because the low 4 bits of the result are trimmed off before output,
// this makes almost no difference in the computation.

static int32_t MatrixMultiply(int32_t a, int32_t b)
{
	while (a > 511) { a -= 1024; }
	while (a < -512) { a += 1024; }
	while (b > 16383) { b -= 32768; }
	while (b < -16384) { b += 32768; }

	// TODO: this isn't technically right to the chip, which truncates
	// the lowest result bit, but it causes glitches otherwise.
	return (a * b) >> 9;
}

/*--------------------------------------------------------------------------*/

// Executes one 'full run' of the lattice filter on a specific byte of
// excitation data, and specific values of all the current k constants,
// and returns the resulting sample.
//
// Note: the current_k processing here by dividing the result by 32768 is
// necessary, as the stored parameters in the lookup table are the 10 bit
// coefficients but are pre-multiplied by 512 for ease of storage. This is
// undone on the real chip by a shifter here, after the multiply.

int16_t TMS5220::LatticeFilter()
{
	// Lattice filter here
	//
	// Aug/05/07: redone as unrolled loop, for clarity - LN
	// Copied verbatim from table I in US patent 4,209,804, now updated to be in
	// same order as the actual chip does it, not that it matters.
	// Notation equivalencies from table:
	// Yn(i) = m_u[n-1]
	// Kn = m_current_k[n-1]
	// bn = m_x[n-1]

	m_u[10] = MatrixMultiply(m_previous_energy, m_excitation_data << 6); // Y(11)
	m_u[9] = m_u[10] - MatrixMultiply(m_current_k[9], m_x[9]);
	m_u[8] = m_u[9] - MatrixMultiply(m_current_k[8], m_x[8]);
	m_u[7] = m_u[8] - MatrixMultiply(m_current_k[7], m_x[7]);
	m_u[6] = m_u[7] - MatrixMultiply(m_current_k[6], m_x[6]);
	m_u[5] = m_u[6] - MatrixMultiply(m_current_k[5], m_x[5]);
	m_u[4] = m_u[5] - MatrixMultiply(m_current_k[4], m_x[4]);
	m_u[3] = m_u[4] - MatrixMultiply(m_current_k[3], m_x[3]);
	m_u[2] = m_u[3] - MatrixMultiply(m_current_k[2], m_x[2]);
	m_u[1] = m_u[2] - MatrixMultiply(m_current_k[1], m_x[1]);
	m_u[0] = m_u[1] - MatrixMultiply(m_current_k[0], m_x[0]);

	m_x[9] = m_x[8] + MatrixMultiply(m_current_k[8], m_u[8]);
	m_x[8] = m_x[7] + MatrixMultiply(m_current_k[7], m_u[7]);
	m_x[7] = m_x[6] + MatrixMultiply(m_current_k[6], m_u[6]);
	m_x[6] = m_x[5] + MatrixMultiply(m_current_k[5], m_u[5]);
	m_x[5] = m_x[4] + MatrixMultiply(m_current_k[4], m_u[4]);
	m_x[4] = m_x[3] + MatrixMultiply(m_current_k[3], m_u[3]);
	m_x[3] = m_x[2] + MatrixMultiply(m_current_k[2], m_u[2]);
	m_x[2] = m_x[1] + MatrixMultiply(m_current_k[1], m_u[1]);
	m_x[1] = m_x[0] + MatrixMultiply(m_current_k[0], m_u[0]);
	m_x[0] = m_u[0];

	m_previous_energy = m_current_energy;

	return (int16_t)m_u[0];
}

/*--------------------------------------------------------------------------*/

// Fill the buffer with a specific number of samples.

#define OLD_FRAME_SILENCE_FLAG m_OLDE // 1 if E=0, 0 otherwise.
#define OLD_FRAME_UNVOICED_FLAG m_OLDP // 1 if P=0 (unvoiced), 0 if voiced.

#define NEW_FRAME_STOP_FLAG (m_new_frame_energy_idx == 0xF) // 1 if this is a stop (Energy = 0xF) frame.
#define NEW_FRAME_SILENCE_FLAG (m_new_frame_energy_idx == 0) // ditto as above
#define NEW_FRAME_UNVOICED_FLAG (m_new_frame_pitch_idx == 0) // ditto as above

void TMS5220::ProcessSamples(short* buffer, int size)
{
	int buf_count = 0;
	int32_t this_sample;

	// Loop until the buffer is full or we've stopped speaking.
	while (size > 0)
	{
		if (m_TALKD) // Speaking
		{
			// If we're ready for a new frame to be applied, i.e. when IP=0, PC=12,
			// Sub=1. (In reality, the frame was really loaded incrementally during
			// the entire IP=0 PC=x time period, but it doesn't affect anything until
			// IP=0 PC=12 happens.
			if (m_IP == 0 && m_PC == 12 && m_subcycle == 1)
			{
				// Appropriately override the interp count if needed; this will be
				// incremented after the frame parse!
				m_IP = 0;

				// Parse a new frame into the new_target_energy, new_target_pitch
				// and new_target_k[].
				ParseFrame();

				// If the new frame is a stop frame, unset both TALK and SPEN
				// (via TCON). TALKD remains active while the energy is ramping
				// to 0.
				if (m_new_frame_energy_idx == 0xF)
				{
					m_TALK = 0;
					m_SPEN = 0;
					UpdateFifoStatusAndInts(); // Probably not necessary...
				}

				// In all cases where interpolation would be inhibited,
				// set the inhibit flag; otherwise clear it.
				//
				// Interpolation inhibit cases:
				// Old frame was voiced, new is unvoiced
				// Old frame was silence/zero energy, new has non-zero energy
				// Old frame was unvoiced, new is voiced
				// Old frame was unvoiced, new frame is silence/zero energy
				// (non-existent on tms51xx rev D and F (present and working
				// on tms52xx, present but buggy on tms51xx rev A and B).
				if (((OLD_FRAME_UNVOICED_FLAG == 0) && NEW_FRAME_UNVOICED_FLAG)
				    || ((OLD_FRAME_UNVOICED_FLAG == 1) && !NEW_FRAME_UNVOICED_FLAG)
				    || ((OLD_FRAME_SILENCE_FLAG == 1) && !NEW_FRAME_SILENCE_FLAG)
				  //|| ((m_inhibit == 1) && (OLD_FRAME_UNVOICED_FLAG == 1) && (NEW_FRAME_SILENCE_FLAG == 1)) ) //TMS51xx INTERP BUG1
				    || ((OLD_FRAME_UNVOICED_FLAG == 1) && NEW_FRAME_SILENCE_FLAG))
				{
					m_inhibit = true;
				}
				else
				{
					// Normal frame, normal interpolation.
					m_inhibit = false;
				}
			}
			else
			{
				// Not a new frame, just interpolate the existing frame.

				// Disable inhibit when reaching the last interp period,
				// but don't overwrite the m_inhibit value.
				int inhibit_state = m_inhibit && (m_IP != 0);

				// Updates to parameters only happen on subcycle '2'
				// (B cycle) of PCs.
				if (m_subcycle == 2)
				{
					switch (m_PC)
					{
						case 0: // PC = 0, B cycle, write updated energy.
							if (m_IP == 0)
							{
								// This reset happens around the second subcycle during IP=0.
								m_pitch_zero = false;
							}

							m_current_energy = (int16_t)(
								m_current_energy +
								(
									(
										(tms5220_coeffs.energytable[m_new_frame_energy_idx] - m_current_energy) *
										(1 - inhibit_state)
									) >> tms5220_coeffs.interp_coeff[m_IP]
								)
							) * (1 - m_zpar);
							break;

						case 1: // PC = 1, B cycle, write updated pitch
							m_current_pitch = (int16_t)(
								m_current_pitch +
								(
									(
										(tms5220_coeffs.pitchtable[m_new_frame_pitch_idx] - m_current_pitch) *
										(1 - inhibit_state)
									) >> tms5220_coeffs.interp_coeff[m_IP]
								)
							) * (1 - m_zpar);
							break;

						case 2: case 3: case 4: case 5: case 6:
						case 7: case 8: case 9: case 10: case 11:
							// PC = 2 through 11, B cycle, write updated K1 through K10.
							m_current_k[m_PC - 2] = (int16_t)(
								m_current_k[m_PC - 2] + (
									(
										(tms5220_coeffs.ktable[m_PC - 2][m_new_frame_k_idx[m_PC - 2]] - m_current_k[m_PC - 2]) *
										(1 - inhibit_state)
									) >> tms5220_coeffs.interp_coeff[m_IP]
								)
							) *
							(
								1 - (
									(
										(m_PC - 2) < 4
									) ? m_zpar : m_uv_zpar
								)
							);
							break;

						case 12: // PC = 12
							// We should NEVER reach this point, PC=12 doesn't
							// have a subcycle 2.
							break;
					}
				}
			}

			// Calculate the output.
			if (OLD_FRAME_UNVOICED_FLAG == 1)
			{
				// Generate unvoiced samples here.
				if (m_RNG & 1)
				{
					// According to the patent it is (either + or -) half
					// of the maximum value in the chirp table, so either
					// 01000000 (0x40) or 11000000 (0xC0).
					m_excitation_data = ~0x3F;
				}
				else
				{
					m_excitation_data = 0x40;
				}
			}
			else
			{
				// Generate voiced samples here.
				//
				// US patent 4331836 Figure 14B shows, and logic would hold,
				// that a pitch based chirp function has a chirp/peak and then
				// a long chain of zeroes. The last entry of the chirp rom is
				// at address 0b110011 (51d), the 52nd sample, and if the
				// address reaches that point the ADDRESS incrementer is
				// disabled, forcing all samples beyond 51d to be == 51d.
				if (m_pitch_count >= 51)
				{
					m_excitation_data = (int8_t)tms5220_coeffs.chirptable[51];
				}
				else
				{
					m_excitation_data = (int8_t)tms5220_coeffs.chirptable[m_pitch_count];
				}
			}

			// Update LFSR *20* times every sample (once per T cycle),
			// like patent shows.
			for (int i = 0; i < 20; i++)
			{
				int bitout = ((m_RNG >> 12) & 1) ^
				             ((m_RNG >> 10) & 1) ^
				             ((m_RNG >>  9) & 1) ^
				             ((m_RNG >>  0) & 1);

				m_RNG >>= 1;
				m_RNG |= bitout << 12;
			}

			this_sample = LatticeFilter(); // Execute lattice filter.

			// Next, force result to 14 bits (since its possible that the addition
			// at the final (k1) stage of the lattice overflowed).
			while (this_sample > 16383) this_sample -= 32768;
			while (this_sample < -16384) this_sample += 32768;

			if (!m_digital_select)
			{
				// Analog SPK pin output is only 8 bits, with clipping.
				buffer[buf_count] = ClipAnalog((int16_t)this_sample);
			}
			else
			{
				// Digital I/O pin output is 12 bits.
				this_sample &= ~0xF;
				// Input:  ssss ssss ssss ssss ssnn nnnn nnnn 0000
				// N taps:                       ^^ ^^^            = 0x3E00;
				// Output: ssss ssss ssss ssss snnn nnnn nnnN NNNN
				buffer[buf_count] = (short)((this_sample << 1) | ((this_sample & 0x3E00) >> 9));
			}

			// Update all counts.
			m_subcycle++;

			if (m_subcycle == 2 && m_PC == 12) // RESETF3
			{
				// Circuit 412 in the patent acts a reset, resetting the pitch
				// counter to 0 if INHIBIT was true during the most recent frame
				// transition. The exact time this occurs is between IP=7, PC=12
				// sub=0, T=t12 and m_IP = 0, PC=0 sub=0, T=t12, a period of
				// exactly 20 cycles, which overlaps the time OLDE and OLDP are
				// updated at IP=7 PC=12 T17 (and hence INHIBIT itself 2 t-cycles
				// later). According to testing the pitch zeroing lasts
				// approximately 2 samples. We set the zeroing latch here,
				// and unset it on PC=1 in the generator.

				if (m_IP == 7 && m_inhibit)
				{
					m_pitch_zero = true;
				}

				if (m_IP == 7) // RESETL4
				{
					// Latch OLDE and OLDP
					// if (OLD_FRAME_SILENCE_FLAG) m_uv_zpar = 0; // TMS51xx INTERP BUG2
					OLD_FRAME_SILENCE_FLAG = NEW_FRAME_SILENCE_FLAG; // m_OLDE
					OLD_FRAME_UNVOICED_FLAG = NEW_FRAME_UNVOICED_FLAG; // m_OLDP
					// if TALK was clear last frame, halt speech now,
					// since TALKD (latched from TALK on new frame)
					// just went inactive.
					m_TALKD = m_TALK; // TALKD is latched from TALK
					// Trigger an interrupt if TALK_STATUS has changed.
					UpdateFifoStatusAndInts();

					if (!m_TALK && m_SPEN)
					{
						// TALK is only activated if it wasn't already active,
						// if m_SPEN is active, and if we're in RESETL4
						// (which we are).
						m_TALK = 1;
					}
				}

				m_subcycle = m_subc_reload;
				m_PC = 0;
				m_IP++;
				m_IP &= 0x7;
			}
			else if (m_subcycle == 3)
			{
				m_subcycle = m_subc_reload;
				m_PC++;
			}

			m_pitch_count++;

			if (m_pitch_count >= m_current_pitch || m_pitch_zero)
			{
				m_pitch_count = 0;
			}

			m_pitch_count &= 0x1FF;
		}
		else // m_TALKD == 0
		{
			m_subcycle++;

			if (m_subcycle == 2 && m_PC == 12) // RESETF3
			{
				if (m_IP == 7) // RESETL4
				{
					m_TALKD = m_TALK; // TALKD is latched from TALK

					UpdateFifoStatusAndInts(); // Probably not necessary.

					if (!m_TALK && m_SPEN)
					{
						// TALK is only activated if it wasn't already active,
						// if m_SPEN is active, and if we're in RESETL4
						// (which we are).
						m_TALK = true;
					}
				}

				m_subcycle = m_subc_reload;
				m_PC = 0;
				m_IP++;
				m_IP &= 0x7;
			}
			else if (m_subcycle == 3)
			{
				m_subcycle = m_subc_reload;
				m_PC++;
			}

			// Should be just -1; actual chip outputs -1 every idle sample;
			// (c.f. note in data sheet, p 10, table 4).
			buffer[buf_count] = -1;
		}

		buf_count++;
		size--;
	}
}

/*--------------------------------------------------------------------------*/

// Extract a byte from the FIFO and interpret it as a command.

void TMS5220::ProcessCommand(unsigned char cmd)
{
	// Parse the command.
	switch (cmd & 0x70)
	{
		case 0x10: // Read byte
			if (!m_SPEN && !m_TALKD) // TALKST must be clear for RDBY.
			{
				if (m_schedule_dummy_read)
				{
					m_schedule_dummy_read = false;

					ReadPhrom(1);
				}

				// Read one byte from speech ROM.
				m_data_register = ReadPhrom(8);
				m_RDB_flag = true;
			}
			break;

		case 0x30: // Read and branch
			if (!m_SPEN && !m_TALKD) // TALKST must be clear for RB
			{
				m_RDB_flag = false;

				ReadAndBranch();
			}
			break;

		case 0x40: // Load address
			if (!m_SPEN && !m_TALKD) // TALKST must be clear for LA
			{
				// TMS5220 datasheet says that if we load only one 4-bit nibble,
				// it won't work. This code does not care about this.

				#ifdef DEBUG_SPEECH
				DebugTrace("%04X TMS5220: load address cmd with data = 0x%02x\n", PrePC, cmd & 0x0F);
				#endif

				LoadPhromAddress(cmd & 0x0F);

				#ifdef DEBUG_SPEECH
				DebugTrace("%04X TMS5220: load address cmd with data = 0x%02x, new address = 0x%05x\n", PrePC, cmd & 0x0F, m_speechROMaddr);
				#endif

				m_schedule_dummy_read = true;
			}
			break;

		case 0x50: // Speak
			#ifdef DEBUG_SPEECH
			DebugTrace("%04X TMS5220: speak\n", PrePC);
			#endif

			if (m_schedule_dummy_read)
			{
				m_schedule_dummy_read = false;
				ReadPhrom(1);
			}

			m_SPEN = true;
			m_TALK = true;
			m_DDIS = false;
			m_zpar = 1; // zero all the parameters
			m_uv_zpar = 1; // zero k4-k10 as well
			m_OLDE = 1; // 'silence/zpar' frames are zero energy
			m_OLDP = 1; // 'silence/zpar' frames are zero pitch

			// Following is semi-hack but matches idle state observed on chip.
			m_new_frame_energy_idx = 0;
			m_new_frame_pitch_idx = 0;

			for (int i = 0; i < 4; i++)
			{
				m_new_frame_k_idx[i] = 0;
			}

			for (int i = 4; i < 7; i++)
			{
				m_new_frame_k_idx[i] = 0xF;
			}

			for (int i = 7; i < tms5220_coeffs.num_k; i++)
			{
				m_new_frame_k_idx[i] = 0x7;
			}
			break;

		case 0x60: // Speak external
			#ifdef DEBUG_SPEECH
			DebugTrace("%04X TMS5220: speak external\n", PrePC);
			#endif

			// SPKEXT going active activates SPKEE which clears the fifo.
			m_fifo_head = 0;
			m_fifo_tail = 0;
			m_fifo_count = 0;
			m_fifo_bits_taken = 0;
			// SPEN is enabled when the fifo passes half full (falling edge
			// of BL signal).
			m_DDIS = true;
			m_zpar = 1; // zero all the parameters
			m_uv_zpar = 1; // zero k4-k10 as well
			m_OLDE = 1; // 'silence/zpar' frames are zero energy
			m_OLDP = 1; // 'silence/zpar' frames are zero pitch

			// Following is semi-hack but matches idle state observed on chip.
			m_new_frame_energy_idx = 0;
			m_new_frame_pitch_idx = 0;

			for (int i = 0; i < 4; i++)
			{
				m_new_frame_k_idx[i] = 0;
			}

			for (int i = 4; i < 7; i++)
			{
				m_new_frame_k_idx[i] = 0xF;
			}

			for (int i = 7; i < tms5220_coeffs.num_k; i++)
			{
				m_new_frame_k_idx[i] = 0x7;
			}

			m_RDB_flag = false;
			break;

		case 0x70: // Reset
			#ifdef DEBUG_SPEECH
			DebugTrace("%04X TMS5220: reset\n", PrePC);
			#endif

			if (m_schedule_dummy_read)
			{
				m_schedule_dummy_read = false;
				ReadPhrom(1);
			}

			Reset();
			break;
	}

	UpdateFifoStatusAndInts();
}

/*--------------------------------------------------------------------------*/

// Extract a specific number of bits from the FIFO.

uint8_t TMS5220::ExtractBits(int count)
{
	uint8_t val = 0;

	if (m_DDIS)
	{
		// Extract from FIFO.
		while (count--)
		{
			val = (val << 1) | ((m_fifo[m_fifo_head] >> m_fifo_bits_taken) & 1);

			m_fifo_bits_taken++;

			if (m_fifo_bits_taken >= 8)
			{
				m_fifo_count--;
				m_fifo[m_fifo_head] = 0; // Zero the newly depleted fifo head byte.
				m_fifo_head = (m_fifo_head + 1) % FIFO_SIZE;
				m_fifo_bits_taken = 0;
			}
		}
	}
	else
	{
		// Extract from speech ROM.
		val = ReadPhrom(count);
	}

	return val;
}

/*--------------------------------------------------------------------------*/

// Parse a new frame's worth of data.
// Returns false if not enough bits in buffer.

#define NEW_FRAME_STOP_FLAG (m_new_frame_energy_idx == 0xF) // 1 if this is a stop (Energy = 0xF) frame
#define NEW_FRAME_SILENCE_FLAG (m_new_frame_energy_idx == 0) // ditto as above
#define NEW_FRAME_UNVOICED_FLAG (m_new_frame_pitch_idx == 0) // ditto as above

void TMS5220::ParseFrame()
{
	#ifdef DEBUG_SPEECH
	DebugTrace("%02X TMS5220: ParseFrame\n", PrePC);
	#endif

	int rep_flag;

	// Since we're parsing a frame, we must be talking, so clear zpar here.
	// Before we start parsing a frame, the P=0 and E=0 latches were both
	// reset by RESETL4, so clear m_uv_zpar here.
	m_uv_zpar = 0;
	m_zpar = 0;

	// We actually don't care how many bits are left in the fifo here;
	// the frame subpart will be processed normally, and any bits
	// extracted 'past the end' of the fifo will be read as zeroes;
	// the fifo being emptied will set the /BE latch which will halt
	// speech exactly as if a stop frame had been encountered (instead
	// of whatever partial frame was read); the same exact circuitry is
	// used for both on the real chip, see us patent 4335277 sheet 16,
	// gates 232a (decode stop frame) and 232b (decode /BE plus DDIS
	// (decode disable) which is active during speak external).

	m_IP = 0;

	UpdateFifoStatusAndInts();

	if (m_DDIS && m_buffer_empty) goto ranout;

	// Attempt to extract the energy index.
	m_new_frame_energy_idx = ExtractBits(tms5220_coeffs.energy_bits);

	UpdateFifoStatusAndInts();

	if (m_DDIS && m_buffer_empty) goto ranout;

	// If the energy index is 0 or 15, we're done.
	if (m_new_frame_energy_idx == 0 || m_new_frame_energy_idx == 15)
	{
		return;
	}

	// Attempt to extract the repeat flag.
	rep_flag = ExtractBits(1);

	// Attempt to extract the pitch.
	m_new_frame_pitch_idx = ExtractBits(tms5220_coeffs.pitch_bits);

	// If the new frame is unvoiced, be sure to zero out the k5-k10 parameters.
	m_uv_zpar = NEW_FRAME_UNVOICED_FLAG;

	UpdateFifoStatusAndInts();

	if (m_DDIS && m_buffer_empty) goto ranout;

	// If this is a repeat frame, just do nothing, it will reuse the old
	// coefficients.
	if (rep_flag)
	{
		return;
	}

	// extract first 4 K coefficients
	for (int i = 0; i < 4; i++)
	{
		m_new_frame_k_idx[i] = ExtractBits(tms5220_coeffs.kbits[i]);
		UpdateFifoStatusAndInts();
		if (m_DDIS && m_buffer_empty) goto ranout;
	}

	// If the pitch index was zero, we only need 4 K's...
	if (m_new_frame_pitch_idx == 0)
	{
		// And the rest of the coefficients are zeroed,
		// but that's done in the generator code.
		return;
	}

	// If we got here, we need the remaining 6 K's.
	for (int i = 4; i < tms5220_coeffs.num_k; i++)
	{
		m_new_frame_k_idx[i] = ExtractBits(tms5220_coeffs.kbits[i]);
		UpdateFifoStatusAndInts();
		if (m_DDIS && m_buffer_empty) goto ranout;
	}

	return;

ranout:
	#ifdef DEBUG_SPEECH
	DebugTrace("%04X TMS5220: Ran out of bits on a parse!\n", PrePC);
	#endif

	return;
}

/*--------------------------------------------------------------------------*/

// Check to see if the buffer low flag should be on or off.

void TMS5220::UpdateFifoStatusAndInts()
{
	// BL is set if neither byte 9 nor 8 of the fifo are in use;
	// this translates to having fifo_count (which ranges from 0 bytes
	// in use to 16	bytes used) being less than or equal to 8.
	// Victory/Victorba depends on this.

	if (m_fifo_count <= 8)
	{
		// Generate an interrupt if necessary; if /BL was inactive and
		// is now active, set int.
		if (!m_buffer_low)
		{
			#ifdef DEBUG_SPEECH
			DebugTrace("%04X TMS5220: Interrupt set\n", PrePC);
			DebugTrace("%04X TMS5220: Buffer low set\n", PrePC);
			#endif

			m_irq_pin = true;
		}

		m_buffer_low = true;
	}
	else
	{
		m_buffer_low = false;

		#ifdef DEBUG_SPEECH
		DebugTrace("%04X TMS5220: Buffer low cleared\n", PrePC);
		#endif
	}

	// BE is set if neither byte 15 nor 14 of the fifo are in use;
	// this translates to having fifo_count equal to exactly 0.
	if (m_fifo_count == 0)
	{
		// Generate an interrupt if necessary;
		// if /BE was inactive and is now active, set int.
		if (!m_buffer_empty)
		{
			m_irq_pin = true;
		}

		m_buffer_empty = true;

		if (m_DDIS)
		{
			// /BE being active clears the TALK status via TCON, which in turn
			// clears SPEN, but ONLY if m_DDIS is set! See patent page 16,
			// gate 232b.
			m_TALK = false;
			m_SPEN = false;
		}
	}
	else
	{
		m_buffer_empty = 0;
	}

	// Generate an interrupt if /TS was active, and is now inactive.
	// also, in this case, regardless if DDIS was set, unset it.
	if (m_previous_TALK_STATUS && (!m_SPEN && !m_TALKD))
	{
		m_irq_pin = true;
		m_DDIS = false;
	}

	m_previous_TALK_STATUS = m_SPEN || m_TALKD;
}

/*----------------------------------------------------------------------------*/

// Write an address nibble to the speech ROM.

void TMS5220::LoadPhromAddress(int data)
{
	// The TMS5220 datasheet says that if we load only one 4-bit nibble,
	// it won't work. This code does not care about this.
	m_speechROMaddr = ((m_speechROMaddr & ~(0xf << m_load_pointer))
	                   | (((unsigned long)(data & 0xf)) << m_load_pointer)) & TMS5220_ADDRESS_MASK;
	m_load_pointer += 4;
	m_ROM_bits_count = 8;
}

/*----------------------------------------------------------------------------*/

// Perform a PHROM read and branch command.

void TMS5220::ReadAndBranch()
{
	if (m_speechROMaddr < m_speechROMlen - 1)
	{
		m_speechROMaddr = (m_speechROMaddr & 0x3C000)
		                  | (((((unsigned long)speechrom_data[m_speechROMaddr]) << 8)
		                  | speechrom_data[m_speechROMaddr + 1]) & 0x3FFF);
	}
	else if (m_speechROMaddr == m_speechROMlen - 1)
	{
		m_speechROMaddr = (m_speechROMaddr & 0x3C000)
		                  | ((((unsigned long)speechrom_data[m_speechROMaddr]) << 8) & 0x3FFF);
	}
	else
	{
		m_speechROMaddr = (m_speechROMaddr & 0x3C000);
	}

	m_ROM_bits_count = 8;
}

/*----------------------------------------------------------------------------*/

uint8_t TMS5220::ReadPhrom(int count)
{
	uint8_t val;

	if (m_load_pointer != 0)
	{
		// The first read after load address is ignored.
		m_load_pointer = 0;
		count--;
	}

	if (m_speechROMaddr < m_speechROMlen)
	{
		val = 0;
		int pos = 8 - m_ROM_bits_count;

		int spchbyte = (speechrom_data[m_speechROMaddr] >> pos) & 0xff;

		while (count > 0)
		{
			val <<= 1;

			if ((spchbyte & 0x01) != 0)
			{
				val |= 1;
			}

			spchbyte >>= 1;
			count--;

			if (pos == 7)
			{
				pos = 0;

				m_speechROMaddr = (m_speechROMaddr + 1) & TMS5220_ADDRESS_MASK;

				if (m_speechROMaddr >= m_speechROMlen)
				{
					count = 0;
				}
				else
				{
					spchbyte = speechrom_data[m_speechROMaddr];
				}
			}
			else
			{
				pos++;
			}
		}

		m_ROM_bits_count = (uint8_t)(8 - pos);
	}
	else
	{
		val = 0;
	}

	return val;
}

/*----------------------------------------------------------------------------*/

void TMS5220::LoadState(FILE *SUEF)
{
	UEFReadBuf(m_fifo, sizeof(m_fifo), SUEF);
	m_fifo_head = UEFRead8(SUEF);
	m_fifo_tail = UEFRead8(SUEF);
	m_fifo_count = UEFRead8(SUEF);
	m_fifo_bits_taken = UEFRead8(SUEF);

	m_previous_TALK_STATUS = UEFReadBool(SUEF);
	m_SPEN = UEFReadBool(SUEF);
	m_DDIS = UEFReadBool(SUEF);
	m_TALK = UEFReadBool(SUEF);
	m_TALKD = UEFReadBool(SUEF);
	m_buffer_low = UEFReadBool(SUEF);
	m_buffer_empty = UEFReadBool(SUEF);
	m_irq_pin = UEFReadBool(SUEF);
	m_ready_pin = UEFReadBool(SUEF);

	m_OLDE = UEFReadBool(SUEF);
	m_OLDP = UEFReadBool(SUEF);

	m_new_frame_energy_idx = UEFRead8(SUEF);
	m_new_frame_pitch_idx = UEFRead8(SUEF);
	UEFReadBuf(m_new_frame_k_idx, sizeof(m_new_frame_k_idx), SUEF);

	m_current_energy = (int16_t)UEFRead16(SUEF);
	m_current_pitch = (int16_t)UEFRead16(SUEF);
	UEFReadBuf(m_current_k, sizeof(m_current_k), SUEF);

	m_previous_energy = UEFRead16(SUEF);

	m_subcycle = UEFRead8(SUEF);
	m_subc_reload = UEFRead8(SUEF);
	m_PC = UEFRead8(SUEF);

	m_IP = UEFRead8(SUEF);
	m_inhibit = UEFReadBool(SUEF);
	m_uv_zpar = UEFRead8(SUEF);
	m_pitch_zero = UEFReadBool(SUEF);
	m_pitch_count = UEFRead16(SUEF);

	UEFReadBuf(m_u, sizeof(m_u), SUEF);
	UEFReadBuf(m_x, sizeof(m_x), SUEF);

	m_RNG = UEFRead16(SUEF);
	m_excitation_data = (int16_t)UEFRead16(SUEF);

	m_schedule_dummy_read = UEFReadBool(SUEF);
	m_data_register = UEFRead8(SUEF);
	m_RDB_flag = UEFReadBool(SUEF);

	m_digital_select = UEFReadBool(SUEF);
	m_io_ready = UEFReadBool(SUEF);
	m_ready_count = UEFRead32(SUEF);

	// PHROM state.
	m_speechROMlen = UEFRead32(SUEF);
	m_speechROMaddr = UEFRead32(SUEF);
	m_load_pointer = UEFRead32(SUEF);
	m_ROM_bits_count = UEFRead8(SUEF);
	UEFReadBuf(speechrom_data, sizeof(speechrom_data), SUEF);
}

/*----------------------------------------------------------------------------*/

void TMS5220::SaveState(FILE *SUEF) const
{
	UEFWriteBuf(m_fifo, sizeof(m_fifo), SUEF);
	UEFWrite8(m_fifo_head, SUEF);
	UEFWrite8(m_fifo_tail, SUEF);
	UEFWrite8(m_fifo_count, SUEF);
	UEFWrite8(m_fifo_bits_taken, SUEF);

	UEFWriteBool(m_previous_TALK_STATUS, SUEF);
	UEFWriteBool(m_SPEN, SUEF);
	UEFWriteBool(m_DDIS, SUEF);
	UEFWriteBool(m_TALK, SUEF);
	UEFWriteBool(m_TALKD, SUEF);
	UEFWriteBool(m_buffer_low, SUEF);
	UEFWriteBool(m_buffer_empty, SUEF);
	UEFWriteBool(m_irq_pin, SUEF);
	UEFWriteBool(m_ready_pin, SUEF);

	UEFWriteBool(m_OLDE, SUEF);
	UEFWriteBool(m_OLDP, SUEF);

	UEFWrite8(m_new_frame_energy_idx, SUEF);
	UEFWrite8(m_new_frame_pitch_idx, SUEF);
	UEFWriteBuf(m_new_frame_k_idx, sizeof(m_new_frame_k_idx), SUEF);

	UEFWrite16(m_current_energy, SUEF);
	UEFWrite16(m_current_pitch, SUEF);
	UEFWriteBuf(m_current_k, sizeof(m_current_k), SUEF);

	UEFWrite16(m_previous_energy, SUEF);

	UEFWrite8(m_subcycle, SUEF);
	UEFWrite8(m_subc_reload, SUEF);
	UEFWrite8(m_PC, SUEF);

	UEFWrite8(m_IP, SUEF);
	UEFWriteBool(m_inhibit, SUEF);
	UEFWrite8(m_uv_zpar, SUEF);
	UEFWriteBool(m_pitch_zero, SUEF);
	UEFWrite16(m_pitch_count, SUEF);

	UEFWriteBuf(m_u, sizeof(m_u), SUEF);
	UEFWriteBuf(m_x, sizeof(m_x), SUEF);

	UEFWrite16(m_RNG, SUEF);
	UEFWrite16(m_excitation_data, SUEF);

	UEFWriteBool(m_schedule_dummy_read, SUEF);
	UEFWrite8(m_data_register, SUEF);
	UEFWriteBool(m_RDB_flag, SUEF);

	UEFWriteBool(m_digital_select, SUEF);
	UEFWriteBool(m_io_ready, SUEF);
	UEFWrite32(m_ready_count, SUEF);

	// PHROM state.
	UEFWrite32(m_speechROMlen, SUEF);
	UEFWrite32(m_speechROMaddr, SUEF);
	UEFWrite32(m_load_pointer, SUEF);
	UEFWrite8(m_ROM_bits_count, SUEF);
	UEFWriteBuf(speechrom_data, sizeof(speechrom_data), SUEF);
}

/*----------------------------------------------------------------------------*/

// clock rate = 80 * output sample rate,
// usually 640000 for 8000 Hz sample rate or
// usually 800000 for 10000 Hz sample rate.

TMS5220StreamState::TMS5220StreamState(int Clock) :
	m_last_sample(0),
	m_curr_sample(0),
	m_source_step((int)((double)(Clock / 80) * (double)FRAC_ONE / (double)SoundSampleRate)),
	m_source_pos(0)
{
	chip.Reset();
}

/*----------------------------------------------------------------------------*/

TMS5220StreamState::~TMS5220StreamState()
{
}

/*--------------------------------------------------------------------------*/

// Update the sound chip so that it is in sync with CPU execution.

void TMS5220StreamState::Update(unsigned char* pBuffer, int Length)
{
	int16_t sample_data[MAX_SAMPLE_CHUNK];
	int16_t *curr_data = sample_data;
	int prev = m_last_sample;
	int curr = m_curr_sample;

	// Finish off the current sample.
	if (m_source_pos > 0)
	{
		// Interpolate
		while (Length > 0 && m_source_pos < FRAC_ONE)
		{
			int samp = ((prev * (FRAC_ONE - m_source_pos)) + (curr * m_source_pos)) >> FRAC_BITS;
			*pBuffer++ = ((samp + 32768) >> 8) & 0xff;
			m_source_pos += m_source_step;
			Length--;
		}

		// If we're over, continue; otherwise, we're done.
		if (m_source_pos >= FRAC_ONE)
		{
			m_source_pos -= FRAC_ONE;
		}
		else
		{
			#ifdef DEBUG_SPEECH
			// if (buffer - buff != length)
			//	DebugTrace("Here for some reason - mismatch in num of samples = %d, %d\n", length, buffer - buff);
			#endif

			chip.ProcessSamples(sample_data, 0);
			return;
		}
	}

	// Compute how many new samples we need.
	int final_pos = m_source_pos + Length * m_source_step;
	int new_samples = (final_pos + FRAC_ONE - 1) >> FRAC_BITS;
	if (new_samples > MAX_SAMPLE_CHUNK)
		new_samples = MAX_SAMPLE_CHUNK;

	// Generate them into our buffer.
	chip.ProcessSamples(sample_data, new_samples);

	prev = curr;
	curr = *curr_data++;

	// Then sample-rate convert with linear interpolation.
	while (Length > 0)
	{
		// Interpolate
		while (Length > 0 && m_source_pos < FRAC_ONE)
		{
			int samp = ((prev * (FRAC_ONE - m_source_pos)) + (curr * m_source_pos)) >> FRAC_BITS;
			*pBuffer++ = ((samp + 32768) >> 8) & 0xff;
			m_source_pos += m_source_step;
			Length--;
		}

		// If we're over, grab the next samples.
		if (m_source_pos >= FRAC_ONE)
		{
			m_source_pos -= FRAC_ONE;
			prev = curr;
			curr = *curr_data++;
			if (curr_data - sample_data > MAX_SAMPLE_CHUNK)
				curr_data = sample_data;
		}
	}

	// Remember the last samples.
	m_last_sample = prev;
	m_curr_sample = curr;
}

/*----------------------------------------------------------------------------*/

void TMS5220StreamState::LoadState(FILE *SUEF)
{
	chip.LoadState(SUEF);

	m_last_sample = UEFRead32(SUEF);
	m_curr_sample = UEFRead32(SUEF);
	m_source_step = UEFRead32(SUEF);
	m_source_pos = UEFRead32(SUEF);
}

/*----------------------------------------------------------------------------*/

void TMS5220StreamState::SaveState(FILE *SUEF) const
{
	chip.SaveState(SUEF);

	UEFWrite32(m_last_sample, SUEF);
	UEFWrite32(m_curr_sample, SUEF);
	UEFWrite32(m_source_step, SUEF);
	UEFWrite32(m_source_pos, SUEF);
}

/*----------------------------------------------------------------------------*/

bool SpeechInit()
{
	ZeroMemory(speechrom_data, sizeof(speechrom_data));

	// Read the PHROM files listed in Phroms.cfg.
	char Path[MAX_PATH];
	strcpy(Path, mainWin->GetUserDataPath());
	AppendPath(Path, "Phroms.cfg");

	std::ifstream RomCfg(Path);

	if (!RomCfg)
	{
		mainWin->Report(MessageType::Error, "Cannot open PHROM configuration file:\n  %s", Path);
		return false;
	}

	bool Success = true;

	// Read PHROMS.
	int RomSlot = 15;

	std::string Line;

	while (std::getline(RomCfg, Line))
	{
		Trim(Line);

		// Skip blank lines and comments
		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		if (Line == "EMPTY")
		{
			continue;
		}

		if (Line.size() >= MAX_PATH)
		{
			mainWin->Report(MessageType::Error, "Invalid PHROM configuration file:\n  %s", Path);
			Success = false;
			break;
		}

		char PhromPath[MAX_PATH];
		strcpy(PhromPath, Line.c_str());

		if (IsRelativePath(Line.c_str()))
		{
			strcpy(PhromPath, mainWin->GetUserDataPath());
			AppendPath(PhromPath, "Phroms");
			AppendPath(PhromPath, Line.c_str());
		}

		FILE *InFile = fopen(PhromPath, "rb");

		if (InFile != nullptr)
		{
			size_t BytesRead = fread(&speechrom_data[RomSlot * 16384], 1, 16384, InFile);

			fclose(InFile);

			if (BytesRead == 0 || BytesRead % 1024 != 0)
			{
				mainWin->Report(MessageType::Error,
				                "Invalid PHROM file size:\n  %s",
				                PhromPath);
			}
		}
		else
		{
			mainWin->Report(MessageType::Error, "Cannot open specified PHROM:\n\n%s", PhromPath);
			Success = false;
		}

		if (RomSlot == 0)
		{
			break;
		}
		else
		{
			RomSlot--;
		}
	}

	return Success;
}

/*--------------------------------------------------------------------------*/

// Allocate buffers and reset the TMS5220.

void SpeechStart()
{
	int Clock = 640000; // 640 kHz

	SpeechStop();

	tms5220 = new(std::nothrow) TMS5220StreamState(Clock);

	if (tms5220 == nullptr)
	{
		return;
	}

	SpeechStarted = true;
}

/*--------------------------------------------------------------------------*/

// Free buffers.

void SpeechStop()
{
	if (tms5220 != nullptr)
	{
		delete tms5220;
		tms5220 = nullptr;
	}

	SpeechStarted = false;
}

/*--------------------------------------------------------------------------*/

// Write data to the sound chip.

void SpeechWrite(unsigned char Data)
{
	if (SpeechStarted)
	{
		tms5220->chip.WriteData(Data);
	}
}

/*--------------------------------------------------------------------------*/

void SpeechReadEnable()
{
	if (SpeechStarted)
	{
		tms5220->chip.ReadEnable();
	}
}

/*--------------------------------------------------------------------------*/

// Read status or data from the sound chip.

unsigned char SpeechRead()
{
	if (SpeechStarted)
	{
		unsigned char Value = tms5220->chip.ReadStatus();

		return Value;
	}
	else
	{
		return 0;
	}
}

/*--------------------------------------------------------------------------*/

// Return the ready status from the sound chip
// (false: ready, true: not ready).

bool SpeechReady()
{
	if (SpeechStarted)
	{
		return !tms5220->chip.ReadReady();
	}
	else
	{
		return true;
	}
}

/*--------------------------------------------------------------------------*/

// Return the interrupt status from the sound chip
// (false: interrupt, true: no interrupt).

bool SpeechInterrupt()
{
	if (SpeechStarted)
	{
		return !tms5220->chip.ReadInt();
	}
	else
	{
		return true;
	}
}

/*--------------------------------------------------------------------------*/

void SpeechUpdate(unsigned char* pBuffer, int Length)
{
	if (SpeechStarted)
	{
		tms5220->Update(pBuffer, Length);
	}
}

/*--------------------------------------------------------------------------*/

void SpeechPoll(int Cycles)
{
	if (SpeechStarted)
	{
		tms5220->chip.Poll(Cycles);
	}
}

/*--------------------------------------------------------------------------*/

void LoadSpeechUEF(FILE *SUEF)
{
	if (SpeechStarted)
	{
		tms5220->LoadState(SUEF);
	}
}

/*--------------------------------------------------------------------------*/

void SaveSpeechUEF(FILE *SUEF)
{
	if (SpeechStarted)
	{
		tms5220->SaveState(SUEF);
	}
}

/*--------------------------------------------------------------------------*/
