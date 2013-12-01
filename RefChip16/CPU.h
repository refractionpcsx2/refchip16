/*
	RefChip16 Copyright 2011-2012

	This file is part of RefChip16.

    RefChip16 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    RefChip16 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with RefChip16.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _CPU_H
#define _CPU_H

#include <iostream>
namespace CPU
{
/*
Flag register:
---------------
Bit[0] - Reserved
Bit[1] - c (Unsigned Carry and Unsigned Borrow flag)
Bit[2] - z (Zero flag)
Bit[3] - Reserved
Bit[4] - Reserved
Bit[5] - Reserved
Bit[6] - o (Signed Overflow Flag)
Bit[7] - n (Negative Flag; aka Sign Flag)
*/

union FlagRegister {
	unsigned short _u16;
	struct {
		unsigned short Reserved1 : 1;
		unsigned short CarryBorrow : 1;
		unsigned short Zero : 1;
		unsigned short Reserved : 3;
		unsigned short Overflow : 1;
		unsigned short Negative : 1;
		unsigned short Reserved2 : 8;
	};
};
/*
class CPU
{
	public:
		CPU();
		~CPU();*/
		void CPULoop();
		int LoadRom(const char *Filename);
		void OpenLog();
		void Reset();
		void __Log(char *fmt, ...);
		void __Log2(char *fmt, ...);

		unsigned short __fastcall recReadMem(unsigned short memloc);
		
		unsigned short ReadMem(unsigned long location);
		unsigned short ReadMemReverse(unsigned long location);
		unsigned char ReadMem8(unsigned long location);
		void WriteMem(unsigned short location, unsigned short value);
		void WriteMem8(unsigned short location, unsigned char value);
		int Condition(int Cond);
		int AddSetFlags(int CalcResult, int signcalc);
		int SubSetFlags(unsigned short Val1, unsigned short Val2);
		int MulSetFlags(int CalcResult);
		int DivSetFlags(unsigned short CalcResult, int Remainder);
		void LogicCMP(unsigned short Result);
		void OutputXY();
		void Yay();
		extern unsigned short PC;
		extern unsigned long OpCode;
		extern unsigned short VBlank;
		extern FlagRegister Flag;
		extern unsigned int cycles;
		extern unsigned int nextvsync;
		extern unsigned int fps;
		extern bool Running;
		extern char Recompiler;
		extern bool drawing;

		unsigned short __fastcall GenerateRandom(unsigned long immediate);
		unsigned short __fastcall SkipToVBlank();
		void FetchOpCode();
		void ExecuteOp();
		void CpuCore(); 
		void CpuJump(); 
		void CpuLoad(); 
		void CpuStore(); 
		void CpuAdd(); 
		void CpuSub();
		void CpuAND(); 
		void CpuOR(); 
		void CpuXOR(); 
		void CpuMul(); 
		void CpuDiv(); 
		void CpuShift();
		void CpuPushPop();
		void CpuPallate();
		void CpuNOTNEG();

		extern FILE * LogFile; 
		
/*
Memory:
--------
64 KB (65536 bytes).

0x0000 - Start of ROM.
0xFDF0 - Start of stack (32 levels).
0xFFF0 - IO ports.
*/
		extern unsigned short StackPTR;
		extern unsigned short GPR[16];
		extern unsigned char Memory[64*1024];
		

}
#endif