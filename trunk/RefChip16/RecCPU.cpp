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

#include "CPU.h"
#include "Emitter.h"
#include "RecCPU.h"
#include "D3DApp.h"
using namespace CPU;

//extern CPU *RefChip16CPU;
extern Emitter *RefChip16Emitter;
extern Sprite SpriteSet;
extern unsigned long *j32Ptr[32];
#define CPU_LOG __Log
#define FPS_LOG __Log2
void    *codebuffer; //Memory pointer for allocated stuff we're storing our code in.

#define CONST_PROP 1
#define REG_CACHING

#define REG_Z		GPR[(recOpCode & 0xf)]
#define REG_X		GPR[((recOpCode >> 24) & 0xf)]
#define REG_Y		GPR[((recOpCode >> 28) & 0xf)]
#define Op_X		((recOpCode >> 24) & 0xf)
#define Op_Y		((recOpCode >> 28) & 0xf)
#define Op_Z		(recOpCode & 0xf)
#define IMMEDIATE   ((unsigned short)(recOpCode & 0xFFFF))
#define TestX		0
#define TestZ		1
#define TestTemp	2

unsigned short tempreg;

struct CurrentInst
{
	unsigned char *StartPC;
	unsigned int EndPC;
	unsigned int BlockCycles;
	unsigned int BlockInstructions;
};

struct CurrentBlock
{
	bool GPRIsConst[16];
	unsigned short GPRConstVal[16];
	//Currently only supports what is left in 
	//EAX due to lack of x86 registers! I guess SSE would be better for this.
	unsigned short LiveGPRReg;
};

struct CurrentBlock GPRStatus;
struct CurrentInst PCIndex[0xffff]; //0xFFFF >> 2 (PC increments by 4)	

unsigned short recPC;
unsigned int recOpCode;
int cpubranch = 0;
unsigned short RecMemory[64*1024];
using namespace CPU;

RecCPU::RecCPU()
{
}

RecCPU::~RecCPU()
{
	_aligned_free(codebuffer);
}

void RecCPU::AddBlockInstruction()
{
	PCIndex[recPC].BlockInstructions++;
}
void RecCPU::InitRecMem()
{
	codebuffer = (unsigned char*)_aligned_malloc(0x40000, 4); //Just over 260k (64k in possible opcodes *4 for some room)
	RefChip16Emitter->SetPTR((unsigned char*)codebuffer);
	FlushConstRegisters(false);

	
	if(codebuffer == NULL) 
		CPU_LOG("Error Allocating Rec Memory!\n");
	else
		CPU_LOG("Rec Alloc Success!!\n");
}

void RecCPU::ResetRecMem()
{
	RefChip16Emitter->SetPTR((unsigned char*)codebuffer);
	recPC = 0;

	FlushConstRegisters(false);
	GPRStatus.LiveGPRReg = 0xffff;
	memset(PCIndex, NULL, sizeof(PCIndex));
}


void RecCPU::EnterRecompiledCode()
{
	cpubranch = 0;
	recPC = PC >> 2;
	//CPU_LOG("Entering Recompiler with PC = %x! cycles = %x\n", PC, cycles);
	
	__asm {
		pushad;
        call ExecuteBlock; // Call the getBlock function
        call eax; // The block to jump to is returned in eax
		popad;
    }

	if(cpubranch != 2) PC = PCIndex[recPC].EndPC;

	//CPU_LOG("Recompile Done, PC now = %x EndPC %x cpubranch %x cycles = %x RecMem in use %4.2f KBytes\n", PC, PCIndex[recPC].EndPC, cpubranch, cycles, (float)(RefChip16Emitter->GetPTR() - (unsigned char *)codebuffer) / 1024);

}

unsigned char* __fastcall RecCPU::ExecuteBlock()
{
	if(PCIndex[recPC].StartPC == NULL)
	{
		//CPU_LOG("Not Compiled So Recompiling!\n");
		PCIndex[recPC].StartPC = RecompileBlock();
		
		//CPU_LOG("End PC set to %x\n", PCIndex[recPC].EndPC);
		CPU_LOG("Recompiling Done Instructions in block %d! Execution time!\n", PCIndex[recPC].BlockInstructions);
	}

	return (unsigned char*)PCIndex[recPC].StartPC;
}

void __fastcall recWriteMem(unsigned short location, unsigned short value)
{
	
	//CPU_LOG("rec Writing to %x with value %x startpc %x recmem %x\n", location, value, PCIndex[RecMemory[location]].StartPC, RecMemory[location]);
	PCIndex[RecMemory[location]].StartPC = NULL;
	Memory[location & 0xffff] = value & 0xff;
	Memory[(location+1) & 0xffff] = value>>8;
}


unsigned char* RecCPU::RecompileBlock()
{
	unsigned char* StartPTR = RefChip16Emitter->GetPTR();
	
	while(cpubranch == 0)
	{
		
		recOpCode = ReadMem(PC + 2) | (ReadMem(PC) << 16);
		RecMemory[PC] = recPC;
		PC+=4;
		PCIndex[recPC].BlockCycles++;
		PCIndex[recPC].EndPC = PC;

		switch(recOpCode>>20 & 0xf)
		{
			case 0x0: recCpuCore(); break;
			case 0x1: recCpuJump(); break;
			case 0x2: recCpuLoad(); break;
			case 0x3: recCpuStore(); break;
			case 0x4: recCpuAdd(); break;
			case 0x5: recCpuSub(); break;
			case 0x6: recCpuAND(); break;
			case 0x7: recCpuOR(); break;
			case 0x8: recCpuXOR(); break;
			case 0x9: recCpuMul(); break;
			case 0xA: recCpuDiv(); break;
			case 0xB: recCpuShift(); break;
			case 0xC: recCpuPushPop(); break;
			case 0xD: recCpuPallate(); break;

			default:
				CPU_LOG("Unknown Op\n");
				break;
		}
		if(cycles + PCIndex[recPC].BlockCycles >= (nextvsync + ((1000000/60) * fps))) break;
	}
	ClearLiveRegister(0xffff, true);
	//FPS_LOG("Block Length %x\n", PCIndex[recPC].BlockCycles);
	if(cpubranch != 3) RefChip16Emitter->ADD32ItoM((unsigned int)&cycles, PCIndex[recPC].BlockCycles);
	FlushConstRegisters(true);
	RefChip16Emitter->RET();
	

	return StartPTR;
}

void RecCPU::FlushConstRegisters(bool flush)
{

	int i;
	
	for(i = 0; i < 16; i++)
	{
		if(CONST_PROP && GPRStatus.GPRIsConst[i] == true)
		{
			if(flush == true) RefChip16Emitter->MOV16ItoM((unsigned int)&GPR[i], GPRStatus.GPRConstVal[i]);	
			GPRStatus.GPRIsConst[i] = false;
		}
	}

}

void RecCPU::CheckLiveRegister(unsigned char GPRReg, bool writeback)
{
#ifdef REG_CACHING
	if(CONST_PROP && GPRStatus.GPRIsConst[GPRReg] == true) CPU_LOG("REGCACHE WARNING - Const reg trying to be used as live reg!\n");
	if(GPRStatus.LiveGPRReg == GPRReg) 
	{
		if(writeback == true)
		{
			//CPU_LOG("REGCACHE Writing back Reg %d as it's about to be swapped!\n", GPRReg);
			RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg], EAX);
		}
		//CPU_LOG("REGCACHE using live reg %d\n", GPRStatus.LiveGPRReg);
		return;
	}

	//Live Reg is different from the needed Reg, so we flush what's there.
	if(GPRStatus.LiveGPRReg != 0xffff)
	{
		//CPU_LOG("REGCACHE Flushing live reg %d\n", GPRStatus.LiveGPRReg);
		RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg], EAX);
	}

	RefChip16Emitter->MOV16MtoR(EAX, (unsigned int)&GPR[GPRReg]);
	GPRStatus.LiveGPRReg = GPRReg;
#else
	RefChip16Emitter->MOV16MtoR(EAX, (unsigned int)&GPR[GPRReg]);
#endif
	//CPU_LOG("REGCACHE Live reg now %d\n", GPRStatus.LiveGPRReg);
}

void RecCPU::FlushLiveRegister(unsigned short GPRReg)
{
#ifdef REG_CACHING
	//CPU_LOG("REGCACHE reg %d is live, flushing back to reg, leaving in EAX!\n", GPRStatus.LiveGPRReg);
	RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg], EAX);
#endif
}
void RecCPU::ClearLiveRegister(unsigned short GPRReg, bool flush)
{
#ifdef REG_CACHING
	if(GPRStatus.LiveGPRReg != 0xffff && (GPRStatus.LiveGPRReg == GPRReg || GPRReg == 0xffff))
	{
		if(flush == true)
		{
			//CPU_LOG("REGCACHE reg %d is live, flushing out!\n", GPRStatus.LiveGPRReg);
			RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg], EAX);
		}
		//else //CPU_LOG("REGCACHE reg %d is live, asked not to flush!\n", GPRStatus.LiveGPRReg);
		GPRStatus.LiveGPRReg = 0xffff;
	}	
#endif
}

void RecCPU::MoveLiveRegister(unsigned short GPRReg, X86RegisterType to)
{
#ifdef REG_CACHING
	if(GPRStatus.LiveGPRReg == GPRReg)
	{
		//CPU_LOG("REGCACHE moving live reg %d to different x86 reg\n", GPRStatus.LiveGPRReg);
		RefChip16Emitter->MOV16RtoR(to, EAX);
	}
	else
	{
		//CPU_LOG("REGCACHE non live reg being moved to different x86 Reg\n");
		RefChip16Emitter->MOV16MtoR(to, (unsigned int)&GPR[GPRReg]);
	}
#else
	RefChip16Emitter->MOV16MtoR(to, (unsigned int)&GPR[GPRReg]);
#endif
	
}

void RecCPU::SetLiveRegister(unsigned char GPRReg)
{
#ifdef REG_CACHING
	//if(GPRStatus.LiveGPRReg != 0xffff && GPRStatus.LiveGPRReg != GPRReg)
		//CPU_LOG("REGCACHE Warning - Setting Live EAX Reg to %d, was %d data potentially lost, Make sure previous Op wrote back\n", GPRReg, GPRStatus.LiveGPRReg);
	/*else
		//CPU_LOG("REGCACHE Live EAX Reg set to %d\n", GPRReg);*/

	GPRStatus.LiveGPRReg = GPRReg;
#else
	RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRReg], EAX);
#endif
}
void RecCPU::recCpuCore()
{
	//CPU_LOG("Core Recompiling %x from PC %x\n", recOpCode, PC-4);
	
	switch((recOpCode >> 16) & 0xf)
	{
	case 0x0: //NOP
		//CPU_LOG("Nop\n");
		break;
	case 0x2: //Wait for VBLANK - may as well do this in interpreter, less buggy and fiddly ;p
		//	CPU_LOG("Waiting for VBlank\n");
			//The CPU waits for VSync so we fast forward here (Small Optimization)
			//If we emulate it as a loop properly, this will cause a lot of Rec overhead!
			ClearLiveRegister(0xffff, true);
			RefChip16Emitter->CALL16((int)SkipToVBlank);
			RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
			cpubranch = 3;
		break;
	case 0x3: //Background Colour
		//CPU_LOG("Set BG colour to %x\n", OpCode & 0xf);

		RefChip16Emitter->MOV16ItoM((unsigned int)&SpriteSet.BackgroundColour, recOpCode & 0xf);
		ClearLiveRegister(0xffff, true);

		break;
	case 0x4: // Set Sprite H/W
		RefChip16Emitter->MOV16ItoM((unsigned int)&SpriteSet.Height, (recOpCode >> 8) & 0xFF);
		RefChip16Emitter->MOV16ItoM((unsigned int)&SpriteSet.Width , (recOpCode & 0xFF) * 2);
		//CPU_LOG("Set Sprite H = %x W = %x\n", (recOpCode >> 8) & 0xFF, (recOpCode & 0xFF) * 2);
		break;		
	case 0x7: //Random Number
		//CPU_LOG("Random number generated from %x", IMMEDIATE);
		ClearLiveRegister(0xffff, true);
		RefChip16Emitter->MOV32ItoR(ECX, IMMEDIATE+1);
		RefChip16Emitter->CALL16((int)GenerateRandom);

		SetLiveRegister(Op_X);
		GPRStatus.GPRIsConst[Op_X] = false;

		break;
	case 0x8: //FLIP Sprite Orientation
		//CPU_LOG("Flip V = %s H = %s totalcode %x\n", (recOpCode >> 8) & 0x1 ? "true" : "false", (recOpCode >> 8) & 0x2 ? "true" : "false", (recOpCode >> 8) & 0xf );

		RefChip16Emitter->MOV16ItoM((unsigned int)&SpriteSet.VerticalFlip, (recOpCode >> 8) & 0x1);
		RefChip16Emitter->MOV16ItoM((unsigned int)&SpriteSet.HorizontalFlip , (recOpCode >> 9) & 0x1);
		break;
	default:
		ClearLiveRegister(0xffff, true);
		FlushConstRegisters(true);		
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuCore);
		break;
	}
	
}
void RecCPU::recCpuJump()
{
	//CPU_LOG("Jump Recompiling %x from PC %x\n", recOpCode, PC-4);
	ClearLiveRegister(0xffff, true);
	FlushConstRegisters(true);
	RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
	//Need to write the PC as some Ops here will Store it
	RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
	RefChip16Emitter->CALL(CpuJump);
	cpubranch = 1;
} 
void RecCPU::recCpuLoad()
{
	//CPU_LOG("Load Recompiling %x from PC %x\n", recOpCode, PC-4);

	switch((recOpCode >> 16) & 0xf)
	{
	//Copy Immediate to GPR X
	case 0x0:	
		ClearLiveRegister(Op_X, false); //No need to flush it, its const or being written over
		if(CONST_PROP)
		{
			GPRStatus.GPRConstVal[Op_X] = IMMEDIATE;
			GPRStatus.GPRIsConst[Op_X] = true;
		}
		else
		{
			RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, IMMEDIATE);
		}
		break;
	//Point Stack  Pointer to Address
	case 0x1:
		RefChip16Emitter->MOV16ItoM((unsigned int)&StackPTR, IMMEDIATE);
		break;
	//Load Register with value at imm address
	case 0x2:
		ClearLiveRegister(0xffff, (Op_X == GPRStatus.LiveGPRReg) ? false : true);
		RefChip16Emitter->MOV32ItoR(ECX, IMMEDIATE);
		RefChip16Emitter->CALL16((int)recReadMem);
		GPRStatus.GPRIsConst[Op_X] = false;
		SetLiveRegister(Op_X);
		break;
	//Load X with value from memory using address in Y
	case 0x3:		
		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
			RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
		else
			MoveLiveRegister(Op_Y, ECX);

		ClearLiveRegister(0xffff, (Op_X == GPRStatus.LiveGPRReg) ? false : true);

		RefChip16Emitter->CALL16((int)recReadMem);

		GPRStatus.GPRIsConst[Op_X] = false;
		SetLiveRegister(Op_X);
		break;
	//Load Y in to X
	case 0x4:
		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			ClearLiveRegister(Op_X, false);
			GPRStatus.GPRConstVal[Op_X] = GPRStatus.GPRConstVal[Op_Y];
			GPRStatus.GPRIsConst[Op_X] = true;
		}
		else
		{
			CheckLiveRegister(Op_Y, true);
			ClearLiveRegister(Op_Y, false); //Dont really need to do this here, but it supresses warnings on the swap (when logging).
			SetLiveRegister(Op_X);
			GPRStatus.GPRIsConst[Op_X] = false;
			
		}
		break;
	default:
		ClearLiveRegister(0xffff, true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->CALL(CpuLoad);
		break;
	}
	
} 
void RecCPU::recCpuStore()
{
	//CPU_LOG("Store Recompiling %x from PC %x\n", recOpCode, PC-4);

	switch((recOpCode >> 16) & 0xf)
	{
	//Store X Register value in imm Address 
	case 0x0:		
		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)RefChip16Emitter->MOV16ItoR(EDX, GPRStatus.GPRConstVal[Op_X]);
		else MoveLiveRegister(Op_X, EDX);
		if(IMMEDIATE > 0) RefChip16Emitter->MOV32ItoR(ECX, IMMEDIATE);
		ClearLiveRegister(0xffff, true);
		RefChip16Emitter->CALL16((int)recWriteMem);
		break;
	//Store X Regsiter value in address given by Y Register
	case 0x1:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)RefChip16Emitter->MOV16ItoR(EDX, GPRStatus.GPRConstVal[Op_X]);
		else MoveLiveRegister(Op_X, EDX);
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
		else MoveLiveRegister(Op_Y, ECX);
		ClearLiveRegister(0xffff, true);
		RefChip16Emitter->CALL16((int)recWriteMem);		
		break;
	default:
		ClearLiveRegister(0xffff, true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->CALL(CpuStore);
		break;
	}	
}

void RecCPU::recADDCheckOVF()
{
	
	RefChip16Emitter->XOR16RtoR(ECX, EDX); //Set if either is different
	RefChip16Emitter->SHR16ItoR(ECX, 15);
	RefChip16Emitter->CMP16ItoR(ECX, 0);
	j32Ptr[0] = RefChip16Emitter->JG32(0); //X and Y were different so no overflow

	RefChip16Emitter->XOR16RtoR(EDX, EAX);
	RefChip16Emitter->SHR16ItoR(EDX, 15);
	RefChip16Emitter->CMP16ItoR(EDX, 1);

	j32Ptr[1] = RefChip16Emitter->JNE32(0); //Result was the same for X and result so dont Overflow

	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x40); //Set Overflow Flag

	RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping overflow flag.
	RefChip16Emitter->x86SetJ32( j32Ptr[1] ); 	
	
}

void RecCPU::recADDCheckCarry()
{
	j32Ptr[0] = RefChip16Emitter->JAE32(0); //Jump if it's not carrying CF = 0	
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x2); //Set Carry Flag
	RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping carry flag.
}

void RecCPU::recCpuAdd()
{
	//CPU_LOG("Add Recompiling %x from PC %x\n", recOpCode, PC-4);
	RefChip16Emitter->AND16ItoM((unsigned int)&Flag._u16, ~0xC6);
	
	switch((recOpCode >> 16) & 0xf)
	{
	//X = X + imm [c,z,o,n]
	case 0x0:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{
			int flagsettings = 0;
			int CalcResult = GPRStatus.GPRConstVal[Op_X] + IMMEDIATE;
			//Carry, if not zero
			if(CalcResult > 0xFFFF) flagsettings |= 0x2;

			 CalcResult &= 0xffff;

			if(CalcResult == 0) flagsettings |= 0x4;
			//Negative Overflow
			if((GPRStatus.GPRConstVal[Op_X] & 0x8000) == (IMMEDIATE & 0x8000))
			{
				if(((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (CalcResult & 0x8000)) == 0x8000)
					flagsettings |= 0x40;
			}
			//Negative
			if(CalcResult & 0x8000) flagsettings |= 0x80;

			GPRStatus.GPRConstVal[Op_X] = CalcResult;

			RefChip16Emitter->MOV16ItoM((unsigned int)&Flag._u16, flagsettings);
			return;
		}
		else
		{
	
			CheckLiveRegister(Op_X, false); //Sets it live, no need to change

			//If Immediate is 0, we dont need to add or alter REG
			if(IMMEDIATE != 0)	
			{
				MoveLiveRegister(Op_X, ECX);
				RefChip16Emitter->MOV16ItoR(EDX, IMMEDIATE);
				
				RefChip16Emitter->ADD16RtoR(EAX, EDX);	
				SetLiveRegister(Op_X);
			}
			else 
			{
				RefChip16Emitter->CMP16ItoR(EAX, 0); //See if the result was zero

				j32Ptr[0] = RefChip16Emitter->JNE32(0); //Jump if it's not zero
				//Carry on if it is zero
				RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); //Set the zero flag
	
				RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping zero flag.
				return;
			}
			
		}
		break;
	//X = X + Y [c,z,o,n]
	case 0x1:
		//ClearLiveRegister(0xffff, true);
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flagsettings = 0;
			unsigned int CalcResult = GPRStatus.GPRConstVal[Op_X] + GPRStatus.GPRConstVal[Op_Y];
			//Carry, if not zero

			if(CalcResult > 0xFFFF) flagsettings |= 0x2;

			 CalcResult &= 0xffff;

			if(CalcResult == 0) flagsettings |= 0x4;
			//Negative Overflow
			if((GPRStatus.GPRConstVal[Op_X] & 0x8000) == (GPRStatus.GPRConstVal[Op_Y] & 0x8000))
			{
				if(((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (CalcResult & 0x8000)) == 0x8000)
					flagsettings |= 0x40;
			}
			//Negative
			if(CalcResult & 0x8000) flagsettings |= 0x80;

			GPRStatus.GPRConstVal[Op_X] = CalcResult;

			RefChip16Emitter->MOV16ItoM((unsigned int)&Flag._u16, flagsettings);
			return;
		}
		else
		{
			
			if(Op_X == Op_Y) 
			{
				//If we reached here then it is definately not a const
				CheckLiveRegister(Op_X, false);
				MoveLiveRegister(Op_X, ECX);
				RefChip16Emitter->ADD16RtoR(EAX, EAX);
			}
			else
			{
				if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)RefChip16Emitter->MOV16ItoR(EDX, GPRStatus.GPRConstVal[Op_Y]);
				else MoveLiveRegister(Op_Y, EDX);

				if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					ClearLiveRegister(0xffff, true);
					RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
				}
				else CheckLiveRegister(Op_X, false);				
				MoveLiveRegister(Op_X, ECX);

				RefChip16Emitter->ADD16RtoR(EAX, EDX);
			}
			
			GPRStatus.GPRIsConst[Op_X] = false;
			//Just in case X was a const
			SetLiveRegister(Op_X);
		}
		break;
	//Z = X + Y [c,z,o,n]
	case 0x2:		
		//ClearLiveRegister(0xffff, true);
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flagsettings = 0;
			int CalcResult = GPRStatus.GPRConstVal[Op_X] + GPRStatus.GPRConstVal[Op_Y];
			//Carry, if not zero
			if(CalcResult > 0xFFFF) flagsettings |= 0x2;

			 CalcResult &= 0xffff;

			if(CalcResult == 0) flagsettings |= 0x4;
			//Negative Overflow
			if((GPRStatus.GPRConstVal[Op_X] & 0x8000) == (GPRStatus.GPRConstVal[Op_Y] & 0x8000))
			{
				if(((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (CalcResult & 0x8000)) == 0x8000)
					flagsettings |= 0x40;
			}
			//Negative
			if(CalcResult & 0x8000) flagsettings |= 0x80;

			GPRStatus.GPRConstVal[Op_Z] = CalcResult;
			GPRStatus.GPRIsConst[Op_Z] = true;
			ClearLiveRegister(Op_Z, false);
			RefChip16Emitter->MOV16ItoM((unsigned int)&Flag._u16, flagsettings);
			return;
		}
		else
		{
			if(Op_X == Op_Y) 
			{
				//If we reached here then it is definately not a const
				CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);
				MoveLiveRegister(Op_X, ECX);
				RefChip16Emitter->ADD16RtoR(EAX, EAX);
			}
			else
			{
				if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)RefChip16Emitter->MOV16ItoR(EDX, GPRStatus.GPRConstVal[Op_Y]);
				else MoveLiveRegister(Op_Y, EDX);

				if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					ClearLiveRegister(0xffff, true);
					RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
				}
				else CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);	

				MoveLiveRegister(Op_X, ECX);				
				RefChip16Emitter->ADD16RtoR(EAX, EDX);
			}
			
			SetLiveRegister(Op_Z);
			GPRStatus.GPRIsConst[Op_Z] = false;
		}
		break;
	default:
		ClearLiveRegister(0xffff, true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->CALL(CpuAdd);
		return;
		break;
	}

	recADDCheckCarry();
	recTestLogic();		
	recADDCheckOVF();
} 

void RecCPU::recSUBCheckOVF()
{
	if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)RefChip16Emitter->MOV16ItoR(EDX, GPRStatus.GPRConstVal[Op_X]);
	else RefChip16Emitter->MOV16MtoR(EDX, (unsigned int)&REG_X);
	RefChip16Emitter->XOR16RtoR(ECX, EDX);
	RefChip16Emitter->SHR16ItoR(ECX, 15);
	RefChip16Emitter->CMP16ItoR(ECX, 1);

	j32Ptr[5] = RefChip16Emitter->JNE32(0); //Result was the same for X and Y so dont Overflow
	RefChip16Emitter->XOR16RtoR(EDX, EAX);
	RefChip16Emitter->SHR16ItoR(EDX, 15);
	RefChip16Emitter->CMP16ItoR(EDX, 1);
	j32Ptr[6] = RefChip16Emitter->JNE32(0); //Result was the same for X and Y so dont Overflow

	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x40); //Set Overflow Flag
	
	RefChip16Emitter->x86SetJ32( j32Ptr[5] ); //Return from skipping overflow flag.
	RefChip16Emitter->x86SetJ32( j32Ptr[6] ); 

	
}

void RecCPU::recSUBCheckCarry()
{	
	j32Ptr[0] = RefChip16Emitter->JAE32(0); //Jump if it's not carrying CF = 0	
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x2); //Set Carry Flag
	
	RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping carry flag.
	
}
void RecCPU::recCpuSub()
{
	//CPU_LOG("Sub Recompiling %x from PC %x\n", recOpCode, PC-4);
	RefChip16Emitter->AND16ItoM((unsigned int)&Flag._u16, ~0xC6);
	
	switch((recOpCode >> 16) & 0xf)
	{
	//X = X - imm [c,z,o,n]
	case 0x0:
		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{
			int flagsettings = 0;
			int CalcResult = GPRStatus.GPRConstVal[Op_X] - IMMEDIATE;
			//Carry, if not zero

			if(GPRStatus.GPRConstVal[Op_X] < IMMEDIATE) flagsettings |= 0x2;

			if(CalcResult == 0) flagsettings |= 0x4;
			
			// X ^ IMM bit set if sign is different
			if(((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (IMMEDIATE & 0x8000)) == 0x8000)
			{
				flagsettings |= 0x40; //Overflow
			}
			//Negative
			if(CalcResult & 0x8000) flagsettings |= 0x80;

			GPRStatus.GPRConstVal[Op_X] = CalcResult;

			RefChip16Emitter->MOV16ItoM((unsigned int)&Flag._u16, flagsettings);
			return;
		}
		else
		{
			
			CheckLiveRegister(Op_X, false);
			RefChip16Emitter->MOV16ItoR(ECX, IMMEDIATE);
			
			
			RefChip16Emitter->CMP16RtoR(EAX, ECX);
			recSUBCheckCarry();
			if(IMMEDIATE != 0)
			{
				RefChip16Emitter->SUB16RtoR(EAX, ECX);
				recSUBCheckOVF();
			}

			SetLiveRegister(Op_X);
			recTestLogic();
		}
		break;
	//X = X - Y [c,z,o,n]
	case 0x1:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flagsettings = 0;
			int CalcResult = GPRStatus.GPRConstVal[Op_X] - GPRStatus.GPRConstVal[Op_Y];
			//Carry, if not zero

			if(GPRStatus.GPRConstVal[Op_X] < GPRStatus.GPRConstVal[Op_Y]) flagsettings |= 0x2;

			if(CalcResult == 0) flagsettings |= 0x4;

			// X ^ Y bit set if sign is different
			if(((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (GPRStatus.GPRConstVal[Op_Y] & 0x8000)) == 0x8000) 
			{
				flagsettings |= 0x40; //Overflow
			}
			//Negative
			if(CalcResult & 0x8000) flagsettings |= 0x80;

			GPRStatus.GPRConstVal[Op_X] = CalcResult;

			RefChip16Emitter->MOV16ItoM((unsigned int)&Flag._u16, flagsettings);
			return;
		}
		else
		{
			if(Op_X == Op_Y) 
			{
				RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
				if(CONST_PROP)
				{
						GPRStatus.GPRConstVal[Op_X] = 0;
						GPRStatus.GPRIsConst[Op_X] = true;
						ClearLiveRegister(Op_X, false);
				}
				else
				{
#ifdef REG_CACHING
					ClearLiveRegister(0xffff, (Op_X == GPRStatus.LiveGPRReg) ? false : true);
					RefChip16Emitter->XOR16RtoR(EAX, EAX);
					SetLiveRegister(Op_X);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
				}			
			}
			else
			{
				if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
				else MoveLiveRegister(Op_Y, ECX);

				if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					ClearLiveRegister(0xffff, true);
					RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
				}
				else CheckLiveRegister(Op_X, false);			

				RefChip16Emitter->CMP16RtoR(EAX, ECX);
				recSUBCheckCarry();
				RefChip16Emitter->SUB16RtoR(EAX, ECX);
				recSUBCheckOVF();
			
				SetLiveRegister(Op_X); //In case X was const, we need to make it the live reg

				GPRStatus.GPRIsConst[Op_X] = false;
				recTestLogic();
			}
		}
		break;
	//Z = X - Y [c,z,o,n]
	case 0x2:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flagsettings = 0;
			int CalcResult = GPRStatus.GPRConstVal[Op_X] - GPRStatus.GPRConstVal[Op_Y];
			//Carry, if not zero

			if(GPRStatus.GPRConstVal[Op_X] < GPRStatus.GPRConstVal[Op_Y]) flagsettings |= 0x2;

			if(CalcResult == 0) flagsettings |= 0x4;

			// X ^ Y bit set if sign is different
			if(((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (GPRStatus.GPRConstVal[Op_Y] & 0x8000)) == 0x8000) 
			{
				flagsettings |= 0x40; //Overflow
			}
			//Negative
			if(CalcResult & 0x8000) flagsettings |= 0x80;

			GPRStatus.GPRConstVal[Op_Z] = CalcResult;
			GPRStatus.GPRIsConst[Op_Z] = true;
			ClearLiveRegister(Op_Z, false); //Incase it's different from X and Y and was live
			RefChip16Emitter->MOV16ItoM((unsigned int)&Flag._u16, flagsettings);
			return;
		}
		else
		{
			if(Op_X == Op_Y) 
			{
				RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
				
				if(CONST_PROP)
				{			
					ClearLiveRegister(Op_Z, false); //Incase it's different from X and Y and was live
					GPRStatus.GPRConstVal[Op_Z] = 0;					
					GPRStatus.GPRIsConst[Op_Z] = true;
				}
				else
				{
#ifdef REG_CACHING
					ClearLiveRegister(0xffff, (Op_Z == GPRStatus.LiveGPRReg) ? false : true);
					RefChip16Emitter->XOR16RtoR(EAX, EAX);
					SetLiveRegister(Op_Z);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_Z, 0);
#endif
				}	
				
			}
			else
			{
				if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
				else MoveLiveRegister(Op_Y, ECX);

				if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					ClearLiveRegister(0xffff, true);
					RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
				}
				else  CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);

				RefChip16Emitter->CMP16RtoR(EAX, ECX);
				recSUBCheckCarry();
				RefChip16Emitter->SUB16RtoR(EAX, ECX);
				recSUBCheckOVF();
				RefChip16Emitter->MOV16RtoM((unsigned int)&REG_Z, EAX);
				GPRStatus.GPRIsConst[Op_Z] = false;
				SetLiveRegister(Op_Z);
				recTestLogic();
			}
		}
		break;
	//X - imm no store just flags [c,z,o,n]
	case 0x3:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{
			int flagsettings = 0;
			int CalcResult = GPRStatus.GPRConstVal[Op_X] - IMMEDIATE;
			//Carry, if not zero

			if(GPRStatus.GPRConstVal[Op_X] < IMMEDIATE) flagsettings |= 0x2;

			if(CalcResult == 0) flagsettings |= 0x4;
			
			// X ^ IMM bit set if sign is different
			if(((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (IMMEDIATE & 0x8000)) == 0x8000)
			{
				flagsettings |= 0x40; //Overflow
			}
			//Negative
			if(CalcResult & 0x8000) flagsettings |= 0x80;
			
			RefChip16Emitter->MOV16ItoM((unsigned int)&Flag._u16, flagsettings);
			return;
		}
		else
		{
			CheckLiveRegister(Op_X, true);

			RefChip16Emitter->MOV16ItoR(ECX, IMMEDIATE);
			RefChip16Emitter->CMP16RtoR(EAX, ECX);
			recSUBCheckCarry();
			if(IMMEDIATE != 0)
			{
				ClearLiveRegister(Op_X, false);
				RefChip16Emitter->SUB16RtoR(EAX, ECX);
				recSUBCheckOVF();
				
			}

			recTestLogic();

		}
		break;
	//X - Y no store just flags [c,z,o,n]
	case 0x4:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flagsettings = 0;
			int CalcResult = GPRStatus.GPRConstVal[Op_X] - GPRStatus.GPRConstVal[Op_Y];
			//Carry, if not zero

			if(GPRStatus.GPRConstVal[Op_X] < GPRStatus.GPRConstVal[Op_Y]) flagsettings |= 0x2;

			if(CalcResult == 0) flagsettings |= 0x4;

			// X ^ Y bit set if sign is different
			if(((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (GPRStatus.GPRConstVal[Op_Y] & 0x8000)) == 0x8000) 
			{
				flagsettings |= 0x40; //Overflow
			}
			//Negative
			if(CalcResult & 0x8000) flagsettings |= 0x80;

			RefChip16Emitter->MOV16ItoM((unsigned int)&Flag._u16, flagsettings);
			return;
		}
		else
		{
			if(Op_X == Op_Y) 
			{
				RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
			}
			else
			{
				if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					ClearLiveRegister(0xffff, true);
					RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
				}
				else CheckLiveRegister(Op_X, true);
				
				if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					if(GPRStatus.GPRConstVal[Op_Y] == 0) 
					{
						CPU_LOG("SUB flags Y == 0\n"); //Only works when Y is 0 :(
						RefChip16Emitter->CMP16ItoR(EAX, GPRStatus.GPRConstVal[Op_Y]);
						recSUBCheckCarry();
						recSUBCheckOVF();
						recTestLogic();
						return;
					}
					else RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
				}
				else MoveLiveRegister(Op_Y, ECX);

				ClearLiveRegister(Op_X, false);

				RefChip16Emitter->CMP16RtoR(EAX, ECX);
				recSUBCheckCarry();
				RefChip16Emitter->SUB16RtoR(EAX, ECX);
				recSUBCheckOVF();
				recTestLogic();
			}
		}
		break;
	default:
		ClearLiveRegister(0xffff, true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->CALL(CpuSub);
		break;
	}
	
}

//This function checks the Zero and Negative conditions from the results of the Logic tests (XOR, AND, OR)
void RecCPU::recTestLogic()
{		
	RefChip16Emitter->CMP16ItoR(EAX, 0); //See if the result was zero
	j32Ptr[0] = RefChip16Emitter->JNE32(0); //Jump if it's not zero
	//Carry on if it is zero
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); //Set the zero flag
	
	j32Ptr[1] = RefChip16Emitter->JMP32(0); //Done with Zero, skip to end!

	RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping zero flag.
	
	RefChip16Emitter->CMP16ItoR(EAX, 0); //Do same compare again to find out if it's greater or less than zero
	j32Ptr[2] = RefChip16Emitter->JG32(0); //Jump if it's greater (don't set negative)
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80); //Set Negative Flag
		
	RefChip16Emitter->x86SetJ32( j32Ptr[1] ); //Return from setting zero.
	RefChip16Emitter->x86SetJ32( j32Ptr[2] ); //Return from skipping negative flag.
}

void RecCPU::recCpuAND()
{
	//CPU_LOG("AND Recompiling %x from PC %x\n", recOpCode, PC-4);

	RefChip16Emitter->AND16ItoM((unsigned int)&Flag._u16, ~0x84);
	
	switch((recOpCode >> 16 & 0xf))
	{
		//X = X & imm [z,n]
		case 0x0:
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{

				GPRStatus.GPRConstVal[Op_X] &= IMMEDIATE;
				
				if(GPRStatus.GPRConstVal[Op_X] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(GPRStatus.GPRConstVal[Op_X] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);
			}
			else
			{
				if(IMMEDIATE == 0)
				{
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 

					if(CONST_PROP)
					{
						GPRStatus.GPRIsConst[Op_X] = true;
						GPRStatus.GPRConstVal[Op_X] = 0;
						
						ClearLiveRegister(Op_X, false);
					}
					else
					{
#ifdef REG_CACHING
						ClearLiveRegister(0xffff, (Op_X == GPRStatus.LiveGPRReg) ? false : true);
						RefChip16Emitter->XOR16RtoR(EAX, EAX);
						SetLiveRegister(Op_X);
#else
						RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
					}	
				}
				else
				{
					CheckLiveRegister(Op_X, false);
					RefChip16Emitter->AND16ItoR(EAX, IMMEDIATE);
					SetLiveRegister(Op_X);
					recTestLogic();
				}
				
			}
		break;
		//X = X & Y [z,n]
		case 0x1:
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				GPRStatus.GPRConstVal[Op_X] &= GPRStatus.GPRConstVal[Op_Y];


				if(GPRStatus.GPRConstVal[Op_X] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(GPRStatus.GPRConstVal[Op_X] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);

			}
			else
			{
				if(Op_X == Op_Y) 
				{
					CheckLiveRegister(Op_X, false);
				}
				else
				{
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						CheckLiveRegister(Op_X, false);
						RefChip16Emitter->AND16ItoR(EAX, GPRStatus.GPRConstVal[Op_Y]);
					}
					else //X might be const instead
					{
						if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							CheckLiveRegister(Op_Y, true); // Write Y back if it was already live as we're about to lose it.
							RefChip16Emitter->AND16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
							GPRStatus.GPRIsConst[Op_X] = false;
						}
						else 
						{
							MoveLiveRegister(Op_Y, EDX);
							CheckLiveRegister(Op_X, false); 	
							RefChip16Emitter->AND16RtoR(EAX, EDX);
						}					
					}					
					
					SetLiveRegister(Op_X);
				}
				
				recTestLogic();
				
			}
		break;
		//Z = X & Y [z,n]
		case 0x2:
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				GPRStatus.GPRConstVal[Op_Z] = GPRStatus.GPRConstVal[Op_X] & GPRStatus.GPRConstVal[Op_Y];


				if(GPRStatus.GPRConstVal[Op_Z] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(GPRStatus.GPRConstVal[Op_Z] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);

				GPRStatus.GPRIsConst[Op_Z] = true;
				ClearLiveRegister(Op_Z, false); //Incase it's different from X and Y and was live
			}
			else
			{
				if(Op_X == Op_Y) 
				{
					//If we reached here Op_X isnt const
					CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);
				}
				else
				{
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);
						RefChip16Emitter->AND16ItoR(EAX, GPRStatus.GPRConstVal[Op_Y]);
					}
					else //X might be const instead
					{
						if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							CheckLiveRegister(Op_Y, (Op_Y == Op_Z) ? false : true); // Write Y back if it was already live as we're about to lose it (unless Y == Z).
							RefChip16Emitter->AND16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
							
						}
						else 
						{
							MoveLiveRegister(Op_Y, EDX);
							CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true); 	
							RefChip16Emitter->AND16RtoR(EAX, EDX);
						}					
					}					
					GPRStatus.GPRIsConst[Op_Z] = false;
					SetLiveRegister(Op_Z);
				}
				recTestLogic();				
			}
		break;
		//X & imm discard flags only [z,n]
		case 0x3:
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{
				tempreg = GPRStatus.GPRConstVal[Op_X] & IMMEDIATE;
				
				if(tempreg == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(tempreg & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);
			}
			else
			{
				if(IMMEDIATE == 0)
				{
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				}
				else
				{
					CheckLiveRegister(Op_X, true); //The live instruction is different so copy it in, then we can ignore it.

					RefChip16Emitter->AND16ItoR(EAX, IMMEDIATE);
					ClearLiveRegister(0xffff, false);
					recTestLogic();
				}
			}
		break;
		//X & Y discard flags only [z,n]
		case 0x4:
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				tempreg = GPRStatus.GPRConstVal[Op_X] & GPRStatus.GPRConstVal[Op_Y];
				
				if(tempreg == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(tempreg & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);
			}
			else
			{
				if(Op_X == Op_Y) 
				{ 
					//If we reached here Op_X isnt const
					CheckLiveRegister(Op_X, false);
				}
				else
				{
					CPU_LOG("AND3 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						CheckLiveRegister(Op_X, true);
						RefChip16Emitter->AND16ItoR(EAX, GPRStatus.GPRConstVal[Op_Y]);
					}
					else //X might be const instead
					{
						if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							CheckLiveRegister(Op_Y, true); // Write Y back if it was already live as we're about to lose it.
							RefChip16Emitter->AND16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
							GPRStatus.GPRIsConst[Op_X] = false;
						}
						else 
						{
							MoveLiveRegister(Op_Y, EDX);
							CheckLiveRegister(Op_X, true); 	
							RefChip16Emitter->AND16RtoR(EAX, EDX);
						}					
					}					
				}
				ClearLiveRegister(0xffff, false);
				recTestLogic();
			}
		break;
		default:
			ClearLiveRegister(0xffff, true);
			FlushConstRegisters(true);
			RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
			RefChip16Emitter->CALL(CpuAND);
			break;

	}
}

void RecCPU::recCpuOR()
{
	//CPU_LOG("OR Recompiling %x from PC %x\n", recOpCode, PC-4);
	
	RefChip16Emitter->AND16ItoM((unsigned int)&Flag._u16, ~0x84);
	//ClearLiveRegister(0xffff, true);
	switch((recOpCode >> 16 & 0xf))
	{
		//X = X | imm [z,n]
		case 0x0:
			//if(IMMEDIATE == 0)FPS_LOG("OR1 IMM is 0\n");
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{

				GPRStatus.GPRConstVal[Op_X] |= IMMEDIATE;
				
				if(GPRStatus.GPRConstVal[Op_X] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(GPRStatus.GPRConstVal[Op_X] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);
			}
			else
			{
				if(IMMEDIATE == 0)
				{
					CheckLiveRegister(Op_X, false);
				}
				else
				{
					CheckLiveRegister(Op_X, false);
					RefChip16Emitter->OR16ItoR(EAX, IMMEDIATE);
					SetLiveRegister(Op_X);
				}
							
				recTestLogic();
				
			}
		break;
		//X = X | Y [z,n]
		case 0x1:
			//ClearLiveRegister(0xffff, true);
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				GPRStatus.GPRConstVal[Op_X] |= GPRStatus.GPRConstVal[Op_Y];


				if(GPRStatus.GPRConstVal[Op_X] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(GPRStatus.GPRConstVal[Op_X] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);

			}
			else
			{
				if(Op_X == Op_Y) 
				{
					//If we reached here Op_X isnt const
					//FPS_LOG("OR X=Y is 0 %x\n", recOpCode);
					CheckLiveRegister(Op_X, false);
				}
				else
				{
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						CheckLiveRegister(Op_X, false);
						RefChip16Emitter->OR16ItoR(EAX, GPRStatus.GPRConstVal[Op_Y]);
					}
					else //X might be const instead
					{
						if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							CheckLiveRegister(Op_Y, true); // Write Y back if it was already live as we're about to lose it.
							RefChip16Emitter->OR16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
							GPRStatus.GPRIsConst[Op_X] = false;
						}
						else 
						{
							MoveLiveRegister(Op_Y, EDX);
							CheckLiveRegister(Op_X, false); 	
							RefChip16Emitter->OR16RtoR(EAX, EDX);
						}					
					}					
					
					SetLiveRegister(Op_X);
				}
				
				recTestLogic();
				
			}
		break;
		//Z = X | Y [z,n]
		case 0x2:
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				GPRStatus.GPRConstVal[Op_Z] = GPRStatus.GPRConstVal[Op_X] | GPRStatus.GPRConstVal[Op_Y];


				if(GPRStatus.GPRConstVal[Op_Z] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(GPRStatus.GPRConstVal[Op_Z] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);

				GPRStatus.GPRIsConst[Op_Z] = true;
				ClearLiveRegister(Op_Z, false); //Incase it's different from X and Y and was live
			}
			else
			{
				if(Op_X == Op_Y) 
				{
					//If we reached here neither are const
					CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);
				}
				else
				{
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);
						RefChip16Emitter->OR16ItoR(EAX, GPRStatus.GPRConstVal[Op_Y]);
					}
					else //X might be const instead
					{
						if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							CheckLiveRegister(Op_Y, (Op_Y == Op_Z) ? false : true); // Write Y back if it was already live as we're about to lose it (unless Y == Z).
							RefChip16Emitter->OR16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
							
						}
						else 
						{
							MoveLiveRegister(Op_Y, EDX);
							CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true); 	
							RefChip16Emitter->OR16RtoR(EAX, EDX);
						}					
					}					
					GPRStatus.GPRIsConst[Op_Z] = false;
					SetLiveRegister(Op_Z);
				}
				recTestLogic();				
			}
		break;
		default:
			ClearLiveRegister(0xffff, true);
			FlushConstRegisters(true);
			RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
			RefChip16Emitter->CALL(CpuOR);
			break;

	}
}
void RecCPU::recCpuXOR()
{
	//CPU_LOG("XOR Recompiling %x from PC %x\n", recOpCode, PC-4);
	RefChip16Emitter->AND16ItoM((unsigned int)&Flag._u16, ~0x84);
	//ClearLiveRegister(0xffff, true);
	switch((recOpCode >> 16 & 0xf))
	{
		//X = X ^ imm [z,n]
		case 0x0:
			//if(IMMEDIATE == 0)FPS_LOG("XOR1 IMM is 0\n");
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{

				GPRStatus.GPRConstVal[Op_X] ^= IMMEDIATE;

				if(GPRStatus.GPRConstVal[Op_X] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(GPRStatus.GPRConstVal[Op_X] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);
			}
			else
			{
				if(IMMEDIATE == 0)
				{
					CheckLiveRegister(Op_X, false);
				}
				else
				{
					CheckLiveRegister(Op_X, false);
					RefChip16Emitter->XOR16ItoR(EAX, IMMEDIATE);
					SetLiveRegister(Op_X);
				}
				
				recTestLogic();
			}
		break;
		//X = X ^ Y [z,n]
		case 0x1:
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				GPRStatus.GPRConstVal[Op_X] ^= GPRStatus.GPRConstVal[Op_Y];

				if(GPRStatus.GPRConstVal[Op_X] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(GPRStatus.GPRConstVal[Op_X] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);

			}
			else
			{
				if(Op_X == Op_Y) 
				{
					//FPS_LOG("XOR1 X=Y is 0 setting REG_%d to const %x\n", Op_X, recOpCode);
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 

					if(CONST_PROP)
					{
						GPRStatus.GPRIsConst[Op_X] = true;
						GPRStatus.GPRConstVal[Op_X] = 0;						
						ClearLiveRegister(Op_X, false);
					}
					else
					{
#ifdef REG_CACHING
						ClearLiveRegister(0xffff, (Op_X == GPRStatus.LiveGPRReg) ? false : true);
						RefChip16Emitter->XOR16RtoR(EAX, EAX);
						SetLiveRegister(Op_X);
#else
						RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
					}
				}
				else
				{
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true) 
					{
						CheckLiveRegister(Op_X, false);
						if(GPRStatus.GPRConstVal[Op_Y] != 0) //Only do this if Y isnt 0, else X doesnt change.
							RefChip16Emitter->XOR16ItoR(EAX, GPRStatus.GPRConstVal[Op_Y]);
					}
					else if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
					{
						CheckLiveRegister(Op_Y, true);
						if(GPRStatus.GPRConstVal[Op_X] != 0) //Only do this if X isnt 0, else Y doesnt change.
							RefChip16Emitter->XOR16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
					} 
					else 
					{
						MoveLiveRegister(Op_Y, ECX);
						CheckLiveRegister(Op_X, false);
						RefChip16Emitter->XOR16RtoR(EAX, ECX);
					}

					SetLiveRegister(Op_X);
					recTestLogic();
					GPRStatus.GPRIsConst[Op_X] = false;
				}
			}
		break;
		//Z = X ^ Y [z,n]
		case 0x2:
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				GPRStatus.GPRConstVal[Op_Z] = GPRStatus.GPRConstVal[Op_X] ^ GPRStatus.GPRConstVal[Op_Y];

				if(GPRStatus.GPRConstVal[Op_Z] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(GPRStatus.GPRConstVal[Op_Z] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);

				GPRStatus.GPRIsConst[Op_Z] = true;
			}
			else
			{
				if(Op_X == Op_Y) 
				{
					//FPS_LOG("XOR2 X=Y is 0 setting REG_%d to const %x\n", Op_Z, recOpCode);
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 

					if(CONST_PROP)
					{
						GPRStatus.GPRIsConst[Op_Z] = true;
						GPRStatus.GPRConstVal[Op_Z] = 0;						
						ClearLiveRegister(Op_Z, false);
					}
					else
					{
#ifdef REG_CACHING
						ClearLiveRegister(0xffff, (Op_Z == GPRStatus.LiveGPRReg) ? false : true);
						RefChip16Emitter->XOR16RtoR(EAX, EAX);
						SetLiveRegister(Op_Z);
#else
						RefChip16Emitter->MOV16ItoM((unsigned int)&REG_Z, 0);
#endif
					}
				}
				else
				{
					CPU_LOG("XOR2 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true) 
					{
						CPU_LOG("XOR2 Y Const Opt\n");
						CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);  // Write X back if it was already live as we're about to lose it (unless X == Z).
						if(GPRStatus.GPRConstVal[Op_Y] != 0) //Only do this if Y isnt 0, else X doesnt change.
							RefChip16Emitter->XOR16ItoR(EAX, GPRStatus.GPRConstVal[Op_Y]);
					}
					else if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
					{
						CPU_LOG("XOR2 X Const Opt\n");
						CheckLiveRegister(Op_Y, (Op_Y == Op_Z) ? false : true);  // Write Y back if it was already live as we're about to lose it (unless Y == Z).
						if(GPRStatus.GPRConstVal[Op_X] != 0) //Only do this if X isnt 0, else Y doesnt change.
							RefChip16Emitter->XOR16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
					} 
					else 
					{
						MoveLiveRegister(Op_Y, ECX);
						CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);  // Write X back if it was already live as we're about to lose it (unless X == Z).
						RefChip16Emitter->XOR16RtoR(EAX, ECX);
					}
		
					recTestLogic();
					SetLiveRegister(Op_Z);
					GPRStatus.GPRIsConst[Op_Z] = false;
				}
			}
		break;
		default:
			FlushConstRegisters(true);
			RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
			RefChip16Emitter->CALL(CpuXOR);
			break;

	}
} 

void RecCPU::recMULCheckCarry()
{
	j32Ptr[0] = RefChip16Emitter->JAE32(0); //Jump if it's not carrying CF = 0	
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x2); //Set Carry Flag
	RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping carry flag.
	
}

void RecCPU::recCpuMul()
{

	//CPU_LOG("MUL Recompiling %x from PC %x\n", recOpCode, PC-4);
	
	RefChip16Emitter->AND16ItoM((unsigned int)&Flag._u16, ~0x86); //Clear Carry Flag
	
	switch((recOpCode >> 16) & 0xf)
	{
	//X = X * imm [c,z,n]
	case 0x0:
		//if(IMMEDIATE == 0)FPS_LOG("MUL IMM is 0\n");
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{
			int flags = 0;
			
			if((GPRStatus.GPRConstVal[Op_X] * IMMEDIATE) > 0xFFFF) flags |= 0x2;

			GPRStatus.GPRConstVal[Op_X] *= IMMEDIATE;

			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
		}
		else
		{
			if(IMMEDIATE == 1) CPU_LOG("MUL 1 IMM 1\n");
			if(IMMEDIATE == 0)
			{
				RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 

				if(CONST_PROP)
				{
					GPRStatus.GPRIsConst[Op_X] = true;
					GPRStatus.GPRConstVal[Op_X] = 0;						
					ClearLiveRegister(Op_X, false);
				}
				else
				{
#ifdef REG_CACHING
					ClearLiveRegister(0xffff, (Op_X == GPRStatus.LiveGPRReg) ? false : true);
					RefChip16Emitter->XOR16RtoR(EAX, EAX);
					SetLiveRegister(Op_X);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
				}
			}
			else
			{
				RefChip16Emitter->MOV16ItoR(ECX, IMMEDIATE);
				CheckLiveRegister(Op_X, false);
				RefChip16Emitter->MUL16RtoEAX(ECX);

				recMULCheckCarry();
				SetLiveRegister(Op_X);
				recTestLogic();
			}
		}
		break;
	//X = X * Y [c,z,n]
	case 0x1:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flags = 0;
			
			if((GPRStatus.GPRConstVal[Op_X] * GPRStatus.GPRConstVal[Op_Y]) > 0xFFFF) flags |= 0x2;

			GPRStatus.GPRConstVal[Op_X] *= GPRStatus.GPRConstVal[Op_Y];

			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
		}
		else
		{
			CPU_LOG("MUL1 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true) 
			{
				if(GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("MUL 2 Y 1\n");
					RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);					
			}
			else MoveLiveRegister(Op_Y, ECX);

			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{
				if(GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("MUL 2 X 1\n");
				ClearLiveRegister(0xffff, true);
				RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
			}
			else CheckLiveRegister(Op_X, false);
						
			RefChip16Emitter->MUL16RtoEAX(ECX);
			recMULCheckCarry();
			//RefChip16Emitter->MOV16RtoM((unsigned int)&REG_X, EAX);
			GPRStatus.GPRIsConst[Op_X] = false;
			SetLiveRegister(Op_X);
			recTestLogic();
			
		}
		break;
	//Z = X * Y [c,z,n]
	case 0x2:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flags = 0;
			
			if((GPRStatus.GPRConstVal[Op_X] * GPRStatus.GPRConstVal[Op_Y]) > 0xFFFF) flags |= 0x2;

			GPRStatus.GPRConstVal[Op_Z] = GPRStatus.GPRConstVal[Op_X] * GPRStatus.GPRConstVal[Op_Y];

			if(GPRStatus.GPRConstVal[Op_Z] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_Z] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			GPRStatus.GPRIsConst[Op_Z] = true;
		}
		else
		{
			CPU_LOG("MUL2 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);

			if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true) 
			{
				if(GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("MUL 3 Y 1\n");
					RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);					
			}
			else MoveLiveRegister(Op_Y, ECX);

			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{
				if(GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("MUL 3 X 1\n");
				ClearLiveRegister(0xffff, (GPRStatus.LiveGPRReg == Op_Z) ? false : true);
				RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
			}
			else CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);
			
			RefChip16Emitter->MUL16RtoEAX(ECX);

			recMULCheckCarry();

			SetLiveRegister(Op_Z);
			GPRStatus.GPRIsConst[Op_Z] = false;
			recTestLogic();
			
		}
		break;
	default:
		ClearLiveRegister(0xffff, true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->CALL(CpuMul);
		return;
		break;
	}
}

void RecCPU::recDIVCheckCarry()
{
	RefChip16Emitter->CMP16ItoR(EDX, 0);
	j32Ptr[0] = RefChip16Emitter->JE32(0); //Jump if Remainder(EDX) is 0 (Not Carry)
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x2); //Set Carry Flag

	RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping carry flag set.

	
}

void RecCPU::recCpuDiv()
{
	//CPU_LOG("DIV Recompiling %x from PC %x\n", recOpCode, PC-4);
	
	//DIV uses EDX and EAX, if anything is in EDX, it could screw things up
	RefChip16Emitter->XOR16RtoR(EDX, EDX);
	RefChip16Emitter->AND16ItoM((unsigned int)&Flag._u16, ~0x86); //Clear Carry Flag
	
	switch((recOpCode >> 16) & 0xf)
	{
	//X = X / imm [c,z,n]
	case 0x0:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true || IMMEDIATE == 0)
		{
			int flags = 0;

			if(IMMEDIATE != 0 && GPRStatus.GPRConstVal[Op_X] != 0)
			{
				if((GPRStatus.GPRConstVal[Op_X] % IMMEDIATE) != 0) flags |= 0x2;

				GPRStatus.GPRConstVal[Op_X] /= IMMEDIATE;
			}
			else GPRStatus.GPRConstVal[Op_X] = 0;

			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			GPRStatus.GPRIsConst[Op_X] = true;
		}
		else
		{
			if(IMMEDIATE == 1) CPU_LOG("DIV 1 IMM 1\n");
			CheckLiveRegister(Op_X, false);
			RefChip16Emitter->MOV16ItoR(ECX, IMMEDIATE);
			RefChip16Emitter->DIV16RtoEAX(ECX);		
			
			recDIVCheckCarry();

			SetLiveRegister(Op_X);
			recTestLogic();
		}
		break;
	//X = X / Y [c,z,n]
	case 0x1:		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flags = 0;
			if(GPRStatus.GPRConstVal[Op_Y] != 0 && GPRStatus.GPRConstVal[Op_X] != 0)
			{
			
				if((GPRStatus.GPRConstVal[Op_X] % GPRStatus.GPRConstVal[Op_Y]) != 0) flags |= 0x2;

				GPRStatus.GPRConstVal[Op_X] /= GPRStatus.GPRConstVal[Op_Y];

			} else GPRStatus.GPRConstVal[Op_X] = 0;

			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
		}
		else
		{
			if(Op_X == Op_Y) 
			{
				if(CONST_PROP)
				{
					GPRStatus.GPRIsConst[Op_X] = true;
					GPRStatus.GPRConstVal[Op_X] = 1;						
					ClearLiveRegister(Op_X, false);
				}
				else
				{
#ifdef REG_CACHING
					ClearLiveRegister(0xffff, (Op_X == GPRStatus.LiveGPRReg) ? false : true);
					RefChip16Emitter->MOV16ItoR(EAX, 1);
					SetLiveRegister(Op_X);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 1);
#endif
				}
			}
			else
			{
				CPU_LOG("DIV2 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
				if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					if(GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("DIV 2 Y 1\n");
						RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
				}
				else MoveLiveRegister(Op_Y, ECX);

				if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					if(GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("DIV 2 X 1\n");
					ClearLiveRegister(0xffff, true);
					RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
					SetLiveRegister(Op_X);
				}
				else CheckLiveRegister(Op_X, false);

				RefChip16Emitter->DIV16RtoEAX(ECX);	

				recDIVCheckCarry();
				RefChip16Emitter->MOV16RtoM((unsigned int)&REG_X, EAX);
				GPRStatus.GPRIsConst[Op_X] = false;
				SetLiveRegister(Op_X);
				recTestLogic();
			}			
		}
		break;
	//Z = X / Y [c,z,n]
	case 0x2:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flags = 0;
			if(GPRStatus.GPRConstVal[Op_Y] != 0 && GPRStatus.GPRConstVal[Op_X] != 0)
			{
			
				if((GPRStatus.GPRConstVal[Op_X] % GPRStatus.GPRConstVal[Op_Y]) != 0) flags |= 0x2;

				GPRStatus.GPRConstVal[Op_Z] = GPRStatus.GPRConstVal[Op_X] / GPRStatus.GPRConstVal[Op_Y];

			} else GPRStatus.GPRConstVal[Op_Z] = 0;

			if(GPRStatus.GPRConstVal[Op_Z] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_Z] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			GPRStatus.GPRIsConst[Op_Z] = true;
		}
		else
		{
			if(Op_X == Op_Y) 
			{
				if(CONST_PROP)
				{
					GPRStatus.GPRIsConst[Op_Z] = true;
					GPRStatus.GPRConstVal[Op_Z] = 1;						
					ClearLiveRegister(Op_Z, false);
				}
				else
				{
#ifdef REG_CACHING
					ClearLiveRegister(0xffff, (Op_X == GPRStatus.LiveGPRReg) ? false : true);
					RefChip16Emitter->MOV16ItoR(EAX, 1);
					SetLiveRegister(Op_Z);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_Z, 1);
#endif
				}
			}
			else
			{
				CPU_LOG("DIV3 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
				if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					if(GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("DIV 3 Y 1\n");
					RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
				}
				else MoveLiveRegister(Op_Y, ECX);

				if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					if(GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("DIV 3 X 1\n");
					ClearLiveRegister(0xffff, (GPRStatus.LiveGPRReg == Op_Z) ? false : true);
					RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
					
				}
				else CheckLiveRegister(Op_X, (Op_X == Op_Z) ? false : true);

				RefChip16Emitter->DIV16RtoEAX(ECX);	
				recDIVCheckCarry();

				GPRStatus.GPRIsConst[Op_Z] = false;
				SetLiveRegister(Op_Z);
				recTestLogic();
			}			
		}
		break;
	default:
		ClearLiveRegister(0xffff, true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->CALL(CpuDiv);
		break;
	}
	
} 
void RecCPU::recCpuShift()
{
	//CPU_LOG("SHIFT Recompiling %x from PC %x\n", recOpCode, PC-4);
	
	RefChip16Emitter->AND16ItoM((unsigned int)&Flag._u16, ~0x84);
	
	switch(((recOpCode >> 16) & 0xf))
	{
	//SHL & SAL [z,n]
	case 0x0:		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{

			GPRStatus.GPRConstVal[Op_X] = GPRStatus.GPRConstVal[Op_X] << (recOpCode & 0xf);

			if(GPRStatus.GPRConstVal[Op_X] == 0) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80); 
			return;
		}
		else
		{
			if((recOpCode & 0xf) != 0)
			{
				CheckLiveRegister(Op_X, false);
				RefChip16Emitter->SHL16ItoR(EAX, (recOpCode & 0xf));
				SetLiveRegister(Op_X);
			}
		}
		break;
	//SHR [z,n]
	case 0x1:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{

			GPRStatus.GPRConstVal[Op_X] = GPRStatus.GPRConstVal[Op_X] >> (recOpCode & 0xf);

			if(GPRStatus.GPRConstVal[Op_X] == 0) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80); 
			return;
		}
		else
		{
			if((recOpCode & 0xf) != 0)
			{
				CheckLiveRegister(Op_X, false);
				RefChip16Emitter->SHR16ItoR(EAX, (recOpCode & 0xf));
				SetLiveRegister(Op_X);
			}
		}
		break;
	//SAR (repeat sign) [z,n]
	case 0x2:
		//ClearLiveRegister(0xffff, true);
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{
			//By casting REG_X as short, we are forcing the compiler (not the recompiler mind :p) to do an arithmetic shift.
			GPRStatus.GPRConstVal[Op_X] = (short)GPRStatus.GPRConstVal[Op_X] >> (recOpCode & 0xf);

			if(GPRStatus.GPRConstVal[Op_X] == 0) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80); 
			return;
		}
		else
		{
			if((recOpCode & 0xf) != 0)
			{
				CheckLiveRegister(Op_X, false);
				RefChip16Emitter->SAR16ItoR(EAX, (recOpCode & 0xf));
				SetLiveRegister(Op_X);
			}
		}
		break;
	//SHL & SAL Y Reg [z,n]
	case 0x3:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{

			GPRStatus.GPRConstVal[Op_X] = GPRStatus.GPRConstVal[Op_X] << GPRStatus.GPRConstVal[Op_Y];

			if(GPRStatus.GPRConstVal[Op_X] == 0) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80); 
			return;
		}
		else
		{
			CPU_LOG("SHFT1 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true) 
			{
				RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
			}
			else MoveLiveRegister(Op_Y, ECX);

			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true) 
			{
				ClearLiveRegister(0xffff, true);
				RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
				SetLiveRegister(Op_X);
			}
			else CheckLiveRegister(Op_X, false);			
			
			RefChip16Emitter->SHL16CLtoR(EAX);
			//RefChip16Emitter->MOV16RtoM((unsigned int)&REG_X, EAX);
			SetLiveRegister(Op_X);
			GPRStatus.GPRIsConst[Op_X] = false;
		}
		break;
	//SHR Y Reg [z,n]
	case 0x4:
		//ClearLiveRegister(0xffff, true);
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{

			GPRStatus.GPRConstVal[Op_X] = GPRStatus.GPRConstVal[Op_X] >> GPRStatus.GPRConstVal[Op_Y];

			if(GPRStatus.GPRConstVal[Op_X] == 0) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80); 
			return;
		}
		else
		{
			CPU_LOG("SHFT2 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true) 
			{
				RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
			}
			else MoveLiveRegister(Op_Y, ECX);

			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true) 
			{
				ClearLiveRegister(0xffff, true);
				RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
				SetLiveRegister(Op_X);
			}
			else CheckLiveRegister(Op_X, false);

			RefChip16Emitter->SHR16CLtoR(EAX);

			SetLiveRegister(Op_X);
			GPRStatus.GPRIsConst[Op_X] = false;
		}
		break;
	//SAR Y Reg [z,n]
	case 0x5:
		//ClearLiveRegister(0xffff, true);
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{

			GPRStatus.GPRConstVal[Op_X] = (short)GPRStatus.GPRConstVal[Op_X] >> GPRStatus.GPRConstVal[Op_Y];

			if(GPRStatus.GPRConstVal[Op_X] == 0) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80); 
			return;
		}
		else
		{
			CPU_LOG("SHFT3 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true) 
			{
				RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
			}
			else MoveLiveRegister(Op_Y, ECX);

			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true) 
			{
				ClearLiveRegister(0xffff, true);
				RefChip16Emitter->MOV16ItoR(EAX, GPRStatus.GPRConstVal[Op_X]);
				SetLiveRegister(Op_X);
			}
			else CheckLiveRegister(Op_X, false);

			RefChip16Emitter->SAR16CLtoR(EAX);

			SetLiveRegister(Op_X);
			GPRStatus.GPRIsConst[Op_X] = false;
		}		
		break;
	default:
		ClearLiveRegister(0xffff, true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->CALL(CpuShift);
		return;
		break;
	}
	recTestLogic();
}
void RecCPU::recCpuPushPop()
{
	//CPU_LOG("PUSHPOP Recompiling %x from PC %x\n", recOpCode, PC-4);
	
	switch((recOpCode >> 16) & 0xf)
	{
	//Store Register X on stack SP + 2 (X is actually in the first nibble)
	case 0x0:
		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true) 
		{
			ClearLiveRegister(0xffff, true);
			RefChip16Emitter->MOV16ItoR(EDX, GPRStatus.GPRConstVal[Op_X]);
		}
		else 
		{
			MoveLiveRegister(Op_X, EDX);
			ClearLiveRegister(0xffff, true);
		}
		RefChip16Emitter->MOV32MtoR(ECX, (unsigned int)&StackPTR);
		RefChip16Emitter->CALL16((int)recWriteMem);
		RefChip16Emitter->ADD32ItoM((unsigned int)&StackPTR, 2);
		
		break;
	//Decrease Stack Pointer and load value in to Reg X 
	case 0x1:
		ClearLiveRegister(0xffff, (Op_X == GPRStatus.LiveGPRReg) ? false : true);
		RefChip16Emitter->SUB32ItoM((unsigned int)&StackPTR, 2);
		RefChip16Emitter->MOV16MtoR(ECX, (unsigned int)&StackPTR);
		RefChip16Emitter->CALL16((int)recReadMem);
		RefChip16Emitter->MOV16RtoM((unsigned int)&REG_X, EAX);
		GPRStatus.GPRIsConst[Op_X] = false;
		SetLiveRegister(Op_X);
		break;
	//Store all GPR registers in the stack, increase SP by 32 (16 x 2)
	case 0x2:
		ClearLiveRegister(0xffff, true);
		for(int i = 0; i < 16; i++)
		{
			if(CONST_PROP && GPRStatus.GPRIsConst[i] == true) RefChip16Emitter->MOV16ItoR(EDX, GPRStatus.GPRConstVal[i]);
			else RefChip16Emitter->MOV16MtoR(EDX, (unsigned int)&GPR[i]);
			RefChip16Emitter->MOV32MtoR(ECX, (unsigned int)&StackPTR);
			RefChip16Emitter->CALL16((int)recWriteMem);
			RefChip16Emitter->ADD32ItoM((unsigned int)&StackPTR, 2);
		}
		break;
	//Decrease SP by 32 and POP all GPR registers
	case 0x3:
		//CPU_LOG("Restore All Registers on stack PC = %x\n", PC);
		ClearLiveRegister(0xffff, false);
		FlushConstRegisters(false);

		for(int i = 15; i >= 0; i--)
		{
			RefChip16Emitter->SUB32ItoM((unsigned int)&StackPTR, 2);
			RefChip16Emitter->MOV16MtoR(ECX, (unsigned int)&StackPTR);
			RefChip16Emitter->CALL16((int)recReadMem);
			RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[i], EAX);		
		}
		SetLiveRegister(0);
		break;
	//Store flags register on stack, increase SP by 2
	case 0x4:
		ClearLiveRegister(0xffff, true);
		RefChip16Emitter->MOV16MtoR(EDX, (unsigned int)&Flag._u16);
		RefChip16Emitter->MOV32MtoR(ECX, (unsigned int)&StackPTR);
		RefChip16Emitter->CALL16((int)recWriteMem);
		RefChip16Emitter->ADD32ItoM((unsigned int)&StackPTR, 2);
		break;
	//Decrease SP by 2, restore flags register
	case 0x5:
		ClearLiveRegister(0xffff, true);
		RefChip16Emitter->SUB32ItoM((unsigned int)&StackPTR, 2);
		RefChip16Emitter->MOV16MtoR(ECX, (unsigned int)&StackPTR);
		RefChip16Emitter->CALL16((int)recReadMem);
		RefChip16Emitter->MOV16RtoM((unsigned int)&Flag._u16, EAX);
		
		break;
	default:
		ClearLiveRegister(0xffff, true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->CALL(CpuPushPop);
		break;
	}
	
}

void RecCPU::recCpuPallate()
{
	switch((recOpCode >> 16) & 0xf)
	{
	case 0x0:
	case 0x1:
	default:
		ClearLiveRegister(0xffff, true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->CALL(CpuPallate);
		break;
	}
}