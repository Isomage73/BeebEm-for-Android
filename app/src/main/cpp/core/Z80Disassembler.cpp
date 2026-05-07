/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator

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
#include <string.h>

#include "Z80mem.h"
#include "Z80.h"
#include "Tube.h"

int Z80Disassemble(int Addr, char* Buffer)
{
	static const char* const reg[8] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
	static const char* const dreg[4] = {"BC", "DE", "HL", "SP"};
	static const char* const cond[8] = {"NZ", "Z", "NC", "C", "PO", "PE", "P", "M"};
	static const char* const arith[8] = {"ADD A,", "ADC A,", "SUB ", "SBC A,", "AND ", "XOR ", "OR ", "CP "};

	char* s = Buffer;
	s += sprintf(s, "%04X ", Addr);

	unsigned char Opcode = ReadZ80Mem(Addr);
	unsigned char d = (Opcode >> 3) & 7;
	unsigned char e = Opcode & 7;

	int size = 1;

	switch (Opcode & 0xC0)
	{
		case 0x00:
			switch (e)
			{
				case 0x00:
					switch (d)
					{
						case 0x00:
							sprintf(s, "%02X           NOP", Opcode);
							break;
						case 0x01:
							sprintf(s, "%02X           EX AF,AF'", Opcode);
							break;
						case 0x02: {
							unsigned char Data = ReadZ80Mem(Addr + 1);
							sprintf(s, "%02X %02X        DJNZ %04X", Opcode, Data, Addr + 2 + (signed char)Data);
							size = 2;
							break;
						}
						case 0x03: {
							unsigned char Data = ReadZ80Mem(Addr + 1);
							sprintf(s, "%02X %02X        JR %04X", Opcode, Data, Addr + 2 + (signed char)Data);
							size = 2;
							break;
						}
						default: {
							unsigned char Data = ReadZ80Mem(Addr + 1);
							sprintf(s, "%02X %02X        JR %s,%04X", Opcode, Data, cond[d & 3], Addr + 2 + (signed char)Data);
							size = 2;
							break;
						}
					}
					break;
				case 0x01:
					if (Opcode & 0x08)
					{
						sprintf(s, "%02X           ADD HL,%s", Opcode, dreg[d >> 1]);
					}
					else
					{
						unsigned char Data1 = ReadZ80Mem(Addr + 1);
						unsigned char Data2 = ReadZ80Mem(Addr + 1);
						int Dest = Data1 | (Data2 << 8);
						sprintf(s, "%02X %02X %02X     LD %s,%04X", Opcode, Data1, Data2, dreg[d >> 1], Dest);
						size = 3;
					}
					break;
				case 0x02:
					switch (d)
					{
						case 0x00:
							sprintf(s, "%02X           LD (BC),A", Opcode);
							break;
						case 0x01:
							sprintf(s, "%02X           LD A,(BC)", Opcode);
							break;
						case 0x02:
							sprintf(s, "%02X           LD (DE),A", Opcode);
							break;
						case 0x03:
							sprintf(s, "%02X           LD A,(DE)", Opcode);
							break;
						case 0x04: {
							unsigned char Data1 = ReadZ80Mem(Addr + 1);
							unsigned char Data2 = ReadZ80Mem(Addr + 2);
							int Dest = Data1 | (Data2 << 8);
							sprintf(s, "%02X %02X %02X     LD (%04X),HL", Opcode, Data1, Data2, Dest);
							size = 3;
							break;
						}
						case 0x05: {
							unsigned char Data1 = ReadZ80Mem(Addr + 1);
							unsigned char Data2 = ReadZ80Mem(Addr + 2);
							int Dest = Data1 | (Data2 << 8);
							sprintf(s, "%02X %02X %02X     LD HL,(%04X)", Opcode, Data1, Data2, Dest);
							size = 3;
							break;
						}
						case 0x06: {
							unsigned char Data1 = ReadZ80Mem(Addr + 1);
							unsigned char Data2 = ReadZ80Mem(Addr + 2);
							int Dest = Data1 | (Data2 << 8);
							sprintf(s, "%02X %02X %02X     LD (%04X),A", Opcode, Data1, Data2, Dest);
							size = 3;
							break;
						}
						case 0x07: {
							unsigned char Data1 = ReadZ80Mem(Addr + 1);
							unsigned char Data2 = ReadZ80Mem(Addr + 2);
							int Dest = Data1 | (Data2 << 8);
							sprintf(s, "%02X %02X %02X     LD A,(%04X)", Opcode, Data1, Data2, Dest);
							size = 3;
							break;
						}
					}
					break;
				case 0x03:
					if (Opcode & 0x08)
					{
						sprintf(s, "%02X           DEC %s", Opcode, dreg[d >> 1]);
					}
					else
					{
						sprintf(s, "%02X           INC %s", Opcode, dreg[d >> 1]);
					}
					break;
				case 0x04:
					sprintf(s, "%02X           INC %s", Opcode, reg[d]);
					break;
				case 0x05:
					sprintf(s, "%02X           DEC %s", Opcode, reg[d]);
					break;
				case 0x06: { // LD d,n
					unsigned char Data = ReadZ80Mem(Addr + 1);
					sprintf(s, "%02X %02X        LD %s,%04X", Opcode, Data, reg[d], Data);
					size = 2;
					break;
				}
				case 0x07: {
					static const char* const str[8] = {
						"RLCA","RRCA","RLA","RRA","DAA","CPL","SCF","CCF"
					};
					sprintf(s, "%02X           %s", Opcode, str[d]);
					break;
				}
			}
			break;
		case 0x40: // LD d,s
			if (d == e)
			{
				sprintf(s, "%02X           HALT", Opcode);
			}
			else
			{
				sprintf(s, "%02X           LD %s,%s", Opcode, reg[d], reg[e]);
			}
			break;
		case 0x80:
			sprintf(s, "%02X           %s,%s", Opcode, arith[d], reg[e]);
			break;
		case 0xC0:
			switch (e)
			{
				case 0x00:
					sprintf(s, "%02X           RET %s", Opcode, cond[d]);
					break;
				case 0x01:
					if (d & 1)
					{
						switch (d >> 1)
						{
							case 0x00:
								sprintf(s, "%02X           RET", Opcode);
								break;
							case 0x01:
								sprintf(s, "%02X           EXX", Opcode);
								break;
							case 0x02:
								sprintf(s, "%02X           JP (HL)", Opcode);
								break;
							case 0x03:
								sprintf(s, "%02X           LD SP,HL", Opcode);
								break;
						}
					}
					else
					{
						if ((d >> 1) == 3)
						{
							sprintf(s, "%02X           POP AF", Opcode);
						}
						else
						{
							sprintf(s, "%02X           POP %s", Opcode, dreg[d >> 1]);
						}
					}
					break;
				case 0x02: {
					unsigned char Data1 = ReadZ80Mem(Addr + 1);
					unsigned char Data2 = ReadZ80Mem(Addr + 2);
					int Dest = Data1 | (Data2 << 8);
					sprintf(s, "%02X %02X %02X     JP %s,%04X", Opcode, Data1, Data2, cond[d], Dest);
					size = 3;
					break;
				}
				case 0x03:
					switch (d)
					{
						case 0x00: {
							unsigned char Data1 = ReadZ80Mem(Addr + 1);
							unsigned char Data2 = ReadZ80Mem(Addr + 2);
							int Dest = Data1 | (Data2 << 8);
							sprintf(s, "%02X %02X %02X     JP %04X", Opcode, Data1, Data2, Dest);
							size = 3;
							break;
						}
						case 0x01: { // 0xCB
							unsigned char ExtOpcode = ReadZ80Mem(++Addr); // Get extension opcode
							d = (ExtOpcode >> 3) & 7;
							e = ExtOpcode & 7;

							switch (ExtOpcode & 0xC0)
							{
								case 0x00: {
									static const char* const str[8] = {
										"RLC","RRC","RL","RR","SLA","SRA","???","SRL"
									};

									sprintf(s, "%02X %02X        %s %s", Opcode, ExtOpcode, str[d], reg[e]);
									break;
								}
								case 0x40:
									sprintf(s, "%02X %02X        BIT %d,%s", Opcode, ExtOpcode, d, reg[e]);
									break;
								case 0x80:
									sprintf(s, "%02X %02X        RES %d,%s", Opcode, ExtOpcode, d, reg[e]);
									break;
								case 0xC0:
									sprintf(s, "%02X %02X        SET %d,%s", Opcode, ExtOpcode, d, reg[e]);
									break;
							}
							size = 2;
							break;
						}
						case 0x02: {
							unsigned char Data = ReadZ80Mem(Addr + 1);
							sprintf(s, "%02X %02X        OUT A,(%02X)", Opcode, Data, Data);
							size = 2;
							break;
						}
						case 0x03: {
							unsigned char Data = ReadZ80Mem(Addr + 1);
							sprintf(s, "%02X %02X        IN A,(%02X)", Opcode, Data, Data);
							size = 2;
							break;
						}
						case 0x04:
							sprintf(s, "%02X           EX (SP),HL", Opcode);
							break;
						case 0x05:
							sprintf(s, "%02X           EX DE,HL", Opcode);
							break;
						case 0x06:
							sprintf(s, "%02X           DI", Opcode);
							break;
						case 0x07:
							sprintf(s, "%02X           EI", Opcode);
							break;
					}
					break;
				case 0x04: {
					unsigned char Data1 = ReadZ80Mem(Addr + 1);
					unsigned char Data2 = ReadZ80Mem(Addr + 2);
					int Dest = Data1 | (Data2 << 8);
					sprintf(s, "%02X %02X %02X     CALL %s,%04X", Opcode, Data1, Data2, cond[d], Dest);
					size = 3;
					break;
				}
				case 0x05:
					if (d & 1)
					{
						switch (d >> 1)
						{
							case 0x00: {
								unsigned char Data1 = ReadZ80Mem(Addr + 1);
								unsigned char Data2 = ReadZ80Mem(Addr + 2);
								int Dest = Data1 | (Data2 << 8);
								sprintf(s, "%02X %02X %02X     CALL %04X", Opcode, Data1, Data2, Dest);
								size = 3;
								break;
							}
							case 0x02: { // 0xED
								unsigned char ExtOpcode = ReadZ80Mem(++Addr); // Get extension opcode
								d = (ExtOpcode >> 3) & 7;
								e = ExtOpcode & 7;
								size = 2;
								switch (ExtOpcode & 0xC0)
								{
									case 0x40:
										switch (e)
										{
											case 0x00:
												sprintf(s, "%02X %02X        IN %s,(C)", Opcode, ExtOpcode, dreg[d]);
												break;
											case 0x01:
												sprintf(s, "%02X %02X        OUT (C),%s", Opcode, ExtOpcode, dreg[d]);
												break;
											case 0x02:
												if (d & 1)
												{
													sprintf(s, "%02X %02X        ADC HL,%s", Opcode, ExtOpcode, dreg[d >> 1]);
												}
												else
												{
													sprintf(s, "%02X %02X        SBC HL,%s", Opcode, ExtOpcode, dreg[d >> 1]);
												}
												break;
											case 0x03: {
												unsigned char Data1 = ReadZ80Mem(Addr + 1);
												unsigned char Data2 = ReadZ80Mem(Addr + 2);
												int Dest = Data1 | (Data2 << 8);

												if (d & 1)
												{
													sprintf(s, "%02X %02X        LD %s,(%04X)", Opcode, ExtOpcode, dreg[d >> 1], Dest);
												}
												else
												{
													sprintf(s, "%02X %02X        LD (%04X),%s", Opcode, ExtOpcode, Dest, dreg[d >> 1]);
												}

												size += 2;
												break;
											}
											case 0x04: {
												static const char* const str[8] = {
													"NEG", "???", "???", "???", "???", "???", "???", "???"
												};
												sprintf(s, "%02X %02X        %s", Opcode, ExtOpcode, str[d]);
												break;
											}
											case 0x05: {
												static const char* const str[8] = {
													"RETN", "RETI", "???", "???", "???", "???", "???", "???"
												};
												sprintf(s, "%02X %02X        %s", Opcode, ExtOpcode, str[d]);
												break;
											}
											case 0x06:
												sprintf(s, "%02X %02X        IM %d", Opcode, ExtOpcode, d - 1);
												break;
											case 0x07: {
												static const char* const str[8] = {
													"LD I,A","???","LD A,I","???","RRD","RLD","???","???"
												};
												sprintf(s, "%02X %02X        %s", Opcode, ExtOpcode, str[d]);
												break;
											}
										}
										break;
									case 0x80: {
										static const char* const str[32] = {
											"LDI", "CPI", "INI", "OUTI", "???", "???", "???", "???",
											"LDD", "CPD", "IND", "OUTD", "???", "???", "???", "???",
											"LDIR", "CPIR", "INIR", "OTIR", "???", "???", "???", "???",
											"LDDR", "CPDR", "INDR", "OTDR", "???", "???", "???", "???"
										};
										sprintf(s, "%02X %02X        %s", Opcode, ExtOpcode, str[ExtOpcode & 0x1F]);
										break;
									}
								}
								break;
							}
							default: { // 0x01 (0xDD) = IX, 0x03 (0xFD) = IY
								const char* ireg = (Opcode & 0x20) ? "IY" : "IX";
								unsigned char ExtOpcode = ReadZ80Mem(++Addr); // Get extension opcode
								size = 2;
								switch (ExtOpcode)
								{
									case 0x09:
										sprintf(s, "%02X %02X        ADD %s, BC", Opcode, ExtOpcode, ireg);
										break;
									case 0x19:
										sprintf(s, "%02X %02X        ADD %s, DE", Opcode, ExtOpcode, ireg);
										break;
									case 0x21: {
										unsigned char Data1 = ReadZ80Mem(Addr + 1);
										unsigned char Data2 = ReadZ80Mem(Addr + 2);
										int Dest = Data1 | (Data2 << 8);
										sprintf(s, "%02X %02X %02X %02X  LD %s,%04x", Opcode, ExtOpcode, Data1, Data2, ireg, Dest);
										size += 2;
										break;
									}
									case 0x22: {
										unsigned char Data1 = ReadZ80Mem(Addr + 1);
										unsigned char Data2 = ReadZ80Mem(Addr + 2);
										int Dest = Data1 | (Data2 << 8);
										sprintf(s, "%02X %02X %02X %02X  LD (%04x),%s", Opcode, ExtOpcode, Data1, Data2, Dest, ireg);
										size += 2;
										break;
									}
									case 0x23:
										sprintf(s, "%02X %02X        INC %s", Opcode, ExtOpcode, ireg);
										break;
									case 0x29:
										sprintf(s, "%02X %02X        ADD %s,%s", Opcode, ExtOpcode, ireg, ireg);
										break;
									case 0x2A: {
										unsigned char Data1 = ReadZ80Mem(Addr + 1);
										unsigned char Data2 = ReadZ80Mem(Addr + 2);
										int Dest = Data1 | (Data2 << 8);
										sprintf(s, "%02X %02X %02X %02X  LD %s,(%04x)", Opcode, ExtOpcode, Data1, Data2, ireg, Dest);
										size += 2;
										break;
									}
									case 0x2B:
										sprintf(s, "%02X %02X        DEC %s", Opcode, ExtOpcode, ireg);
										break;
									case 0x34: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     INC (%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0x35: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     DEC (%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0x36: {
										unsigned char Data1 = ReadZ80Mem(Addr + 1);
										unsigned char Data2 = ReadZ80Mem(Addr + 2);
										sprintf(s, "%02X %02X %02X %02X  LD (%s+%02x),%02X", Opcode, ExtOpcode, Data1, Data2, ireg, Data1, Data2);
										size += 2;
										break;
									}
									case 0x39:
										sprintf(s, "%02X %02X        ADD %s,SP", Opcode, ExtOpcode, ireg);
										break;
									case 0x46:
									case 0x4E:
									case 0x56:
									case 0x5E:
									case 0x66:
									case 0x6E: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     LD %s,(%s+%02x)", Opcode, ExtOpcode, Data, reg[(ExtOpcode >> 3) & 7], ireg, Data);
										size += 1;
										break;
									}
									case 0x70:
									case 0x71:
									case 0x72:
									case 0x73:
									case 0x74:
									case 0x75:
									case 0x77: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     LD (%s+%02x),%s", Opcode, ExtOpcode, Data, ireg, Data, reg[ExtOpcode & 7]);
										size += 1;
										break;
									}
									case 0x7E: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     LA A,(%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0x86: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     ADD A,(%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0x8E: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     ADC A,(%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0x96: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     SUB A,(%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0x9E: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     SBC A,(%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0xA6: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     AND A,(%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0xAE: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     XOR A,(%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0xB6: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     OR A,(%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0xBE: {
										unsigned char Data = ReadZ80Mem(Addr + 1);
										sprintf(s, "%02X %02X %02X     CP A,(%s+%02x)", Opcode, ExtOpcode, Data, ireg, Data);
										size += 1;
										break;
									}
									case 0xE1:
										sprintf(s, "%02X %02X        POP %s", Opcode, ExtOpcode, ireg);
										break;
									case 0xE3:
										sprintf(s, "%02X %02X        EX (SP),%s", Opcode, ExtOpcode, ireg);
										break;
									case 0xE5:
										sprintf(s, "%02X %02X        PUSH %s", Opcode, ExtOpcode, ireg);
										break;
									case 0xE9:
										sprintf(s, "%02X %02X        JP (%s)", Opcode, ExtOpcode, ireg);
										break;
									case 0xF9:
										sprintf(s, "%02X %02X        LD SP,%s", Opcode, ExtOpcode, ireg);
										break;
									case 0xCB: {
										unsigned char SubOpcode = ReadZ80Mem(Addr + 2); // Next sub-opcode
										d = (SubOpcode >> 3) & 7;
										unsigned char Data = ReadZ80Mem(Addr + 1);

										switch (SubOpcode & 0xC0)
										{
											case 0x00: {
												static const char* const str[8] = {
													"RLC", "RRC", "RL", "RR", "SLA", "SRA", "???", "SRL"
												};
												sprintf(s, "%02X %02X %02X     %s (%s+%02X)", Opcode, ExtOpcode, SubOpcode, str[d], ireg, Data);
												break;
											}
											case 0x40:
												sprintf(s, "%02X %02X %02X     BIT %d (%s+%02X)", Opcode, ExtOpcode, SubOpcode, d, ireg, Data);
												break;
											case 0x80:
												sprintf(s, "%02X %02X %02X     RES %d (%s+%02X)", Opcode, ExtOpcode, SubOpcode, d, ireg, Data);
												break;
											case 0xC0:
												sprintf(s, "%02X %02X %02X     SET %d (%s+%02X)", Opcode, ExtOpcode, SubOpcode, d, ireg, Data);
												break;
										}

										size += 2;
										break;
									}
								}
								break;
							}
						}
					}
					else
					{
						if ((d >> 1) == 3)
						{
							sprintf(s, "%02X           PUSH AF", Opcode);
						}
						else
						{
							sprintf(s, "%02X           PUSH %s", Opcode, dreg[d >> 1]);
						}
					}
					break;
				case 0x06: {
					unsigned char Data = ReadZ80Mem(Addr + 1);
					sprintf(s, "%02X %02X        %s %02X", Opcode, Data, arith[d], Data);
					size += 1;
					break;
				}
				case 0x07:
					sprintf(s, "%02X           RST %02X", Opcode, Opcode & 0x38);
					break;
			}
			break;
	}

	return size;
}
