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

#ifndef _RECCPU_H
#define _RECCPU_H
#include "Emitter.h"

void __fastcall recWriteMem(unsigned short location, unsigned short value);

class RecCPU
{
	public:
		RecCPU();
		~RecCPU();
		void AddBlockInstruction();
		void InitRecMem();
		void EnterRecompiledCode(); //Function which holds the ASM to run the dispatch
		unsigned char* RecompileBlock(); //Function which handles generation of the recompiled code
		unsigned char* __fastcall ExecuteBlock(); //This function returns the codebuffer pointer for start of rec code.
		void FlushConstRegisters(bool flush);
		void IncRegisterAge();
		void FlushLiveRegisters(bool flush);
		void ToggleLockRegister(int reg, bool locked);
		void MoveLiveRegister(X86RegisterType toreg, X86RegisterType fromreg, bool swap);
		void SetupMulDivSrcRegister(unsigned short srcReg, unsigned short tarReg, bool isSigned);
		int GetLiveRegister(unsigned short GPRReg, bool isDestOnly);
		int GetLiveRegisterNoAssign(unsigned short GPRReg);
		int GetFreeLiveRegister(unsigned short GPRReg, bool isDestOnly);
		void AssignLiveRegister(unsigned char GPRReg, int x86Reg);
		void FlushLiveRegister(unsigned short GPRReg, bool flush);
		void ReAssignLiveRegister(int x86RegFrom, int x86RegTo);
		void ResetRecMem();

		void recCpuCore(); 
		void recCpuJump(); 
		void recCpuLoad(); 
		void recCpuStore(); 
		void recCpuAdd(); 
		void recADDCheckCarry();
		void recADDCheckOVF(X86RegisterType yReg, X86RegisterType dReg, X86RegisterType xTemp, bool XisY);
		void recSUBCheckCarry();
		void recSUBCheckOVF(X86RegisterType yReg, X86RegisterType dReg, X86RegisterType xTemp, bool XisY);
		void recCpuSub();
		void recTestLogic(X86RegisterType dReg);
		void recCpuAND(); 
		void recCpuOR(); 
		void recCpuXOR(); 
		void recMULCheckCarry();
		void recDIVCheckCarry();
		void recCpuMul(); 
		void recCpuDiv(); 
		void recCpuShift();
		void recCpuPushPop();
		void recCpuPallate();
		void recCpuNOTNEG();
};

#endif
