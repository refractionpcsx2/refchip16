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
#define REG_COUNT 8

#define REG_Z		GPR[(recOpCode & 0xf)]
#define REG_X		GPR[((recOpCode >> 24) & 0xf)]
#define REG_Y		GPR[((recOpCode >> 28) & 0xf)]
#define Op_X		((recOpCode >> 24) & 0xf)
#define Op_Y		((recOpCode >> 28) & 0xf)
#define Op_Z		(recOpCode & 0xf)
#define IMMEDIATE   ((unsigned short)(recOpCode & 0xFFFF))

unsigned short tempreg;

struct CurrentInst
{
	unsigned char *StartPC;
	unsigned int EndPC;
	unsigned int BlockCycles;
	unsigned int BlockInstructions;
};

struct LiveReg
{
	bool inuse; //Is the register live with some contents
	unsigned short gprreg; //GPR register number that is in the X86 register
	unsigned int age; //How many instructions it's been since this reg was last used
	bool isdirty; //We use "Dirty" to describe if the contents are going to be different from the real register. Clean means it is a read
	bool islocked; //Used for regs we never want to write to (like ESI which is used as a stack pointer)
};

struct CurrentBlock
{
	bool GPRIsConst[16];
	unsigned short GPRConstVal[16];
	//Currently only supports what is left in 
	//Using 4 registers for now, will try more later if i can avoid the stack pointer.
	LiveReg LiveGPRReg[REG_COUNT];
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
	memset(codebuffer, NULL, sizeof(codebuffer));
	FlushConstRegisters(false);
	FlushLiveRegisters(false);
	for (int i = 0; i < REG_COUNT; i++)
	{
		GPRStatus.LiveGPRReg[i].age = 0;
		GPRStatus.LiveGPRReg[i].inuse = false;
		GPRStatus.LiveGPRReg[i].isdirty = false;
		GPRStatus.LiveGPRReg[i].gprreg = 0xffff;
	}	
	GPRStatus.LiveGPRReg[4].islocked = true; //Don't touch the Stack Pointer, this would be bad :P
	memset(RecMemory, NULL, sizeof(RecMemory));
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

void __fastcall recPrintValue(unsigned short value)
{
	CPU_LOG("Result is %x", value);
}


unsigned char* RecCPU::RecompileBlock()
{
	unsigned char* StartPTR = RefChip16Emitter->GetPTR();
	
	while(cpubranch == 0)
	{
		
		recOpCode = ReadMem(PC + 2) | (ReadMem(PC) << 16);
		RecMemory[PC] = recPC;
		RecMemory[PC+2] = recPC;
		PC+=4;
		PCIndex[recPC].BlockCycles++;
		
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
			case 0xE: recCpuNOTNEG(); break;

			default:
				CPU_LOG("Unknown Op\n");
				break;
		}
		IncRegisterAge();
		if(cycles + PCIndex[recPC].BlockCycles >= (nextsecond + (1000000.0f / 60.0f * fps))) break;
	}
	PCIndex[recPC].EndPC = PC;
	//ClearLiveRegister(0xffff, true);
	
	//FPS_LOG("Block Length %x\n", PCIndex[recPC].BlockCycles);
	if(cpubranch != 3) RefChip16Emitter->ADD32ItoM((unsigned int)&cycles, PCIndex[recPC].BlockCycles);
	FlushLiveRegisters(true);
	FlushConstRegisters(true);
	RefChip16Emitter->RET();
	

	return StartPTR;
}

//Flush all constant registers if modified back to the associated GPR
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


/********************/
/*RegCache Handliers*/
/********************/

//Increase the age since a register was last used (for freeing it up for another register)
void RecCPU::IncRegisterAge() {
	for (int i = 0; i < REG_COUNT; i++)
	{
		if (GPRStatus.LiveGPRReg[i].inuse == true) {
			GPRStatus.LiveGPRReg[i].age++;
		}
	}
}

//Move one register to another, or swap two over which is quicker than flushing the existing one.
void RecCPU::MoveLiveRegister(X86RegisterType toreg, X86RegisterType fromreg, bool swap = false)
{
	if (fromreg == toreg) //Already in there, no need to move.
		return;

	if (GPRStatus.LiveGPRReg[fromreg].inuse == false)
	{
		CPU_LOG("Swap of %d to %d faied! to reg is empty.", fromreg, toreg);
		return;
	}
	
	if (swap == false || GPRStatus.LiveGPRReg[toreg].inuse == false)
	{
		if (GPRStatus.LiveGPRReg[toreg].inuse == true)
		{
			if (GPRStatus.LiveGPRReg[toreg].isdirty == true)
			{
				RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg[toreg].gprreg], toreg);
			}
		}
		RefChip16Emitter->MOV16RtoR(toreg, fromreg);
		memcpy(&GPRStatus.LiveGPRReg[toreg], &GPRStatus.LiveGPRReg[fromreg], sizeof(LiveReg));
		GPRStatus.LiveGPRReg[fromreg].age = 0;
		GPRStatus.LiveGPRReg[fromreg].inuse = false;
		GPRStatus.LiveGPRReg[fromreg].isdirty = false;
		GPRStatus.LiveGPRReg[fromreg].gprreg = 0xffff;
	}
	else //If the other reg is live with something.
	{
		LiveReg tempreginfo;
		//CPU_LOG("Moving from %d to %d with swap", fromreg, toreg);
		memcpy(&tempreginfo, &GPRStatus.LiveGPRReg[toreg], sizeof(LiveReg));
		RefChip16Emitter->PUSH32R(toreg);
		//RefChip16Emitter->PUSH32R(fromreg);
		RefChip16Emitter->MOV16RtoR(toreg, fromreg);
		//RefChip16Emitter->POP32R(toreg);
		RefChip16Emitter->POP32R(fromreg);
		memcpy(&GPRStatus.LiveGPRReg[toreg], &GPRStatus.LiveGPRReg[fromreg], sizeof(LiveReg));
		memcpy(&GPRStatus.LiveGPRReg[fromreg], &tempreginfo, sizeof(LiveReg));
	}
}

//Sets a lock on a certain x86 reg to signal that we don't want its contents to change
//and if a reg needs to be forcefully free'd we don't want it taking this one.
void RecCPU::ToggleLockRegister(int reg, bool locked) {
	GPRStatus.LiveGPRReg[reg].islocked = locked;
}

//Search for a free x86 register or free the least recently used one
int RecCPU::GetFreeLiveRegister(unsigned short GPRReg, bool isDestOnly) {
	int oldestReadReg = -1;
	unsigned int Readage = 0;
	int oldestWrittenReg = -1;
	unsigned int Writtenage = 0;

	//First loop through to see if any are currently unused noting the least recently used ones which are in use, just in case there isnt any free.
	for (int i = 0; i < REG_COUNT; i++)
	{
		if (GPRStatus.LiveGPRReg[i].islocked == false)
		{
			if (GPRStatus.LiveGPRReg[i].inuse == false)
			{
				//Horray we found a spare register!
				//Only copy the data to it if it isn't the destination only, there's no point else :P
				if ((GPRReg & 0xfff0) != 0xfff0)
				{
					if (CONST_PROP && GPRStatus.GPRIsConst[GPRReg] == true)
					{
						if (isDestOnly == false)
						{
							RefChip16Emitter->MOV16ItoR((X86RegisterType)i, GPRStatus.GPRConstVal[GPRReg]);
							
						}
						GPRStatus.LiveGPRReg[i].isdirty = true; //Set to true as const data is dirty data
						GPRStatus.GPRIsConst[GPRReg] = false;
					}
					else
					{
						if (isDestOnly == false)
						{
							RefChip16Emitter->MOV16MtoR((X86RegisterType)i, (unsigned int)&GPR[GPRReg]);
							GPRStatus.LiveGPRReg[i].isdirty = false; //Set to false for now as it should be the same as the current data.
						}
						else GPRStatus.LiveGPRReg[i].isdirty = true;
					}
				}
				//Remember to set the "Dirty" flag if you write to this register.	
				GPRStatus.LiveGPRReg[i].inuse = true;
				GPRStatus.LiveGPRReg[i].gprreg = GPRReg;
				GPRStatus.LiveGPRReg[i].age = 0;
				
				return i;
			}
			if (GPRStatus.LiveGPRReg[i].isdirty == false)
			{
				if (GPRStatus.LiveGPRReg[i].age > Readage) {
					Readage = GPRStatus.LiveGPRReg[i].age;
					oldestReadReg = i;
				}
			}
			else
			{
				if (GPRStatus.LiveGPRReg[i].age > Writtenage) {
					Writtenage = GPRStatus.LiveGPRReg[i].age;
					oldestWrittenReg = i;
				}
			}
		}
	}
	//First loop found nothing, but we made note of the oldest regs that were read only and written only.
	//Written to regs are more likely to be used sooner (assumption, it depends!) so lets try getting rid of the read ones first.
	//Simply because we don't have to write these back, so it will be quicker to use these regs up.
	if (Readage > 0)
	{
		//CPU_LOG("Flushing least recently read dirty = %d", GPRStatus.LiveGPRReg[oldestReadReg].isdirty);
		if(GPRStatus.LiveGPRReg[oldestReadReg].isdirty)
			RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg[oldestReadReg].gprreg], (X86RegisterType)oldestReadReg);

		if (!(GPRReg & 0xfff0))
		{
			if (CONST_PROP && GPRStatus.GPRIsConst[GPRReg] == true)
			{
				if (isDestOnly == false)
				{
					RefChip16Emitter->MOV16ItoR((X86RegisterType)oldestReadReg, GPRStatus.GPRConstVal[GPRReg]);
				}
				GPRStatus.LiveGPRReg[oldestReadReg].isdirty = true;
				GPRStatus.GPRIsConst[GPRReg] = false;
			}
			else
			{
				if (isDestOnly == false)
				{
					RefChip16Emitter->MOV16MtoR((X86RegisterType)oldestReadReg, (unsigned int)&GPR[GPRReg]);
					GPRStatus.LiveGPRReg[oldestReadReg].isdirty = false; //Set to false for now as it should be the same as the current data.
				}
				else GPRStatus.LiveGPRReg[oldestReadReg].isdirty = true;
			}
		}

		GPRStatus.LiveGPRReg[oldestReadReg].gprreg = GPRReg;
		GPRStatus.LiveGPRReg[oldestReadReg].age = 0;
		
		return oldestReadReg;
	}
	else
	{
		//CPU_LOG("Flushing least recently read dirty = %d", GPRStatus.LiveGPRReg[oldestWrittenReg].isdirty);
		//First write back our dirty data

		RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg[oldestWrittenReg].gprreg], (X86RegisterType)oldestWrittenReg);

		if (!(GPRReg & 0xfff0))
		{
			if (CONST_PROP && GPRStatus.GPRIsConst[GPRReg] == true)
			{
				if (isDestOnly == false)
				{
					RefChip16Emitter->MOV16ItoR((X86RegisterType)oldestWrittenReg, GPRStatus.GPRConstVal[GPRReg]);
					
				}
				GPRStatus.LiveGPRReg[oldestWrittenReg].isdirty = true; //Set to true as const data is dirty data
				GPRStatus.GPRIsConst[GPRReg] = false;
			}
			else
			{
				if (isDestOnly == false)
				{
					RefChip16Emitter->MOV16MtoR((X86RegisterType)oldestWrittenReg, (unsigned int)&GPR[GPRReg]);
				}
				else GPRStatus.LiveGPRReg[oldestWrittenReg].isdirty = true; //Set to false for now as it should be the same as the current data.
			}
		}
		GPRStatus.LiveGPRReg[oldestWrittenReg].gprreg = GPRReg;
		GPRStatus.LiveGPRReg[oldestWrittenReg].age = 0;		
		return oldestWrittenReg;
	}
	CPU_LOG("Error finding register to allocate!");
	return -1;
}

//Search for the GPRReg in our live registers, if it finds it, return it, if not, make a new entry
int RecCPU::GetLiveRegister(unsigned short GPRReg, bool isDestOnly = false) {

	for (int i = 0; i < REG_COUNT; i++) //First loop through seeing if the register is actually live already
	{
		if (GPRStatus.LiveGPRReg[i].inuse == true)
		{
			if (GPRStatus.GPRIsConst[GPRStatus.LiveGPRReg[i].gprreg] == true) {
				CPU_LOG("WARNING GPR %x Cached & Const!", GPRStatus.LiveGPRReg[i].gprreg);
			}
		}
		//Could probably just check the reg number here, but just in case we forgot to unset it somewhere
		if (GPRStatus.LiveGPRReg[i].gprreg == GPRReg && GPRStatus.LiveGPRReg[i].inuse == true)
		{
			GPRStatus.LiveGPRReg[i].age = 0;
			return i;
		}
		
	}
	//If we got here, the register is not live so we need to check for a free register.
	return GetFreeLiveRegister(GPRReg, isDestOnly);

}

int RecCPU::GetLiveRegisterNoAssign(unsigned short GPRReg) {

	for (int i = 0; i < REG_COUNT; i++) //First loop through seeing if the register is actually live already
	{
		if (GPRStatus.LiveGPRReg[i].inuse == true)
		{
			if (GPRStatus.GPRIsConst[GPRStatus.LiveGPRReg[i].gprreg] == true) {
				CPU_LOG("WARNING GPR %x Cached & Const!", GPRStatus.LiveGPRReg[i].gprreg);
			}
		}
		//Could probably just check the reg number here, but just in case we forgot to unset it somewhere
		if (GPRStatus.LiveGPRReg[i].gprreg == GPRReg && GPRStatus.LiveGPRReg[i].inuse == true)
		{
			GPRStatus.LiveGPRReg[i].age = 0;
			return i;
		}
	}
	//If we got here, the register is not live.
	return -1;

}

//livereg is the x86 register you are taking over
//newGPR is the GPR register you want it to represent
//EG you move a value from Y to X, to save a copy you can do ReAssignLiveRegister(yReg, X_Op)
void RecCPU::ReAssignLiveRegister(int livereg, int newGPR)
{
	int newReg = GetLiveRegisterNoAssign(newGPR);
	//Flush the old data first, if it's dirty
	if (newReg == livereg)
		return;

	if (GPRStatus.LiveGPRReg[livereg].isdirty == true && GPRStatus.LiveGPRReg[livereg].inuse == true)
	{
		CPU_LOG("Live reg %d is dirty", livereg);
		RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg[livereg].gprreg], (X86RegisterType)livereg);
	}
	
	if (newReg != -1) { //A register already exists for the new GPR we want to use, that data is now invalid.
		CPU_LOG("Flushing existing register for %d", newGPR);
		
		GPRStatus.LiveGPRReg[newReg].inuse = false;
		GPRStatus.LiveGPRReg[newReg].gprreg = 0xffff;
		GPRStatus.LiveGPRReg[newReg].age = 0;
		GPRStatus.LiveGPRReg[newReg].isdirty = false;
	}
	if (GPRStatus.GPRIsConst[newGPR] == true) {
		CPU_LOG("WARNING Reassigned GPR %x Cached & Const!", newGPR);
	}

	GPRStatus.LiveGPRReg[livereg].inuse = true;
	GPRStatus.LiveGPRReg[livereg].gprreg = newGPR;
	GPRStatus.LiveGPRReg[livereg].age = 0;
	GPRStatus.LiveGPRReg[livereg].isdirty = true;
}

//Assigns a recently written to register the contents of a current register. (an instruction that writes to a reg directly.
//DOES NOT FLUSH
void RecCPU::AssignLiveRegister(unsigned char GPRReg, int x86Reg)
{
	if (GPRStatus.GPRIsConst[GPRReg] == true) {
		CPU_LOG("WARNING Assigned GPR %x Cached & Const!", GPRReg);
	}
	FlushLiveRegister(GPRReg, false);  //Double check it wasn't assigned elsewhere
	GPRStatus.LiveGPRReg[x86Reg].inuse = true;
	GPRStatus.LiveGPRReg[x86Reg].gprreg = GPRReg;
	GPRStatus.LiveGPRReg[x86Reg].age = 0;
	GPRStatus.LiveGPRReg[x86Reg].isdirty = true; //Set to false for now as it should be the same as the current data.
}
//Flush all used cached regs, writing back any "dirty" written data
void RecCPU::FlushLiveRegisters(bool flush) {

	//Flush any used registers that have been written and reset all the register information
	for (int i = 0; i < REG_COUNT; i++)
	{
		if (flush && GPRStatus.LiveGPRReg[i].inuse == true && GPRStatus.LiveGPRReg[i].isdirty == true) {
			RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg[i].gprreg], (X86RegisterType)i);
		}
		GPRStatus.LiveGPRReg[i].inuse = false;
		GPRStatus.LiveGPRReg[i].gprreg = 0xffff;
		GPRStatus.LiveGPRReg[i].isdirty = false; //Don't forget to copy its dirty status! Important!
		GPRStatus.LiveGPRReg[i].age = 0;
	}
}
//Flush a single x86 Register
void RecCPU::FlushLiveRegister(unsigned short GPRReg, bool flush)
{
	int x86reg = GetLiveRegisterNoAssign(GPRReg);
	if (x86reg == -1) //Not found, so we don't care!
		return;
	//CPU_LOG("Flushing %x", x86reg);
#ifdef REG_CACHING
	//CPU_LOG("REGCACHE reg %d is live, flushing back to reg, leaving in EAX!\n", GPRStatus.LiveGPRReg);
	if (GPRStatus.LiveGPRReg[x86reg].isdirty == true && flush == true)
	{
		RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg[x86reg].gprreg], (X86RegisterType)x86reg);
		//RefChip16Emitter->XOR16RtoR((X86RegisterType)x86reg, (X86RegisterType)x86reg);
	}
	
	GPRStatus.LiveGPRReg[x86reg].inuse = false;
	GPRStatus.LiveGPRReg[x86reg].gprreg = 0xffff;
	GPRStatus.LiveGPRReg[x86reg].isdirty = false; //Don't forget to copy its dirty status! Important!
	GPRStatus.LiveGPRReg[x86reg].age = 0;
#endif
}

//Special handling for MUL/DIV instructions due to how x86 works
//Could be done in those instructions but it felt better to bring it all into one function
void RecCPU::SetupMulDivSrcRegister(unsigned short srcReg, unsigned short tarReg, bool isSigned) {
	//DIV and MUL instructions are a pain in the ass and you can only write results to EAX with EDX being the remainder,
	// so we need to salvage these if they have data in them.
	// Additionally EDX is used as a sign extension of EAX for IDIV (which we use)
	int curSrc = GetLiveRegister(srcReg);

	if (curSrc == -1)
	{
		CPU_LOG("ERROR ALLOCATING Source in SetupMulDivSrcRegister Pos 1");
	}
	else if (curSrc != 0) //Source not yet in EAX so swap it with whatever is
	{
		MoveLiveRegister(EAX, (X86RegisterType)curSrc, true);
		curSrc = 0;
	}

	ToggleLockRegister(curSrc, true);

	if (srcReg != tarReg) //They are different so we better check if the target is in ECX
	{
		int curTar = GetLiveRegister(tarReg); //See if Target is in the registers, if not move it in.

		if (curTar >= 0)
		{
			if (curTar != ECX) {
				bool doSwap = true;
				if (curTar == EDX) //Div is going to use EDX, so we may as well get rid of the other live reg
				{
					doSwap = false;
				}
				MoveLiveRegister(ECX, (X86RegisterType)curTar, doSwap); //Swap them, in case the other reg is needed	
			}

		}
		else //tarReg is not currently in Memory, so we failed to get a reg, damn.
		{
			CPU_LOG("ERROR ALLOCATING tarReg in SetupMulDivSrcRegister Pos 1!");
		}
	}

	ToggleLockRegister(curSrc, false);


	if (GPRStatus.LiveGPRReg[EDX].inuse == true)
	{
		if (GPRStatus.LiveGPRReg[EDX].isdirty)
		{
			//If it's dirty, copy it back, if not, just pretend it wasn't there.
			RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[GPRStatus.LiveGPRReg[EDX].gprreg], EDX);
		}
		GPRStatus.LiveGPRReg[EDX].inuse = false;
		GPRStatus.LiveGPRReg[EDX].gprreg = 0xffff;
		GPRStatus.LiveGPRReg[EDX].age = 0;
		GPRStatus.LiveGPRReg[EDX].isdirty = false;

	}
	ToggleLockRegister(EDX, true); //Lock out EDX from being allocated.
	if (isSigned)
	{
		RefChip16Emitter->CDQ16(); //EDX is the sign extension of EAX
	}
	else
	{
		RefChip16Emitter->XOR16RtoR(EDX, EDX);
	}
	
}


/*****************************/
/* Start of CPU Instructions */
/*****************************/

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
			FlushLiveRegisters(true);
			RefChip16Emitter->CALL16((int)SkipToVBlank);
			//RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
			cpubranch = 3;
		break;
	case 0x3: //Background Colour
		//CPU_LOG("Set BG colour to %x\n", OpCode & 0xf);
		RefChip16Emitter->MOV16ItoM((unsigned int)&SpriteSet.BackgroundColour, recOpCode & 0xf);
		break;
	case 0x4: // Set Sprite H/W
		RefChip16Emitter->MOV16ItoM((unsigned int)&SpriteSet.Height, (recOpCode >> 8) & 0xFF);
		RefChip16Emitter->MOV16ItoM((unsigned int)&SpriteSet.Width , (recOpCode & 0xFF) * 2);
		//CPU_LOG("Set Sprite H = %x W = %x\n", (recOpCode >> 8) & 0xFF, (recOpCode & 0xFF) * 2);
		break;		
	case 0x7: //Random Number
		//CPU_LOG("Random number generated from %x", IMMEDIATE);
		FlushLiveRegisters(true);
		RefChip16Emitter->MOV32ItoR(ECX, IMMEDIATE+1);
		RefChip16Emitter->CALL16((int)GenerateRandom);
		AssignLiveRegister(Op_X, EAX);
		//SetLiveRegister(Op_X);
		GPRStatus.GPRIsConst[Op_X] = false;

		break;
	case 0x8: //FLIP Sprite Orientation
		//CPU_LOG("Flip V = %s H = %s totalcode %x\n", (recOpCode >> 8) & 0x1 ? "true" : "false", (recOpCode >> 8) & 0x2 ? "true" : "false", (recOpCode >> 8) & 0xf );

		RefChip16Emitter->MOV16ItoM((unsigned int)&SpriteSet.VerticalFlip, (recOpCode >> 8) & 0x1);
		RefChip16Emitter->MOV16ItoM((unsigned int)&SpriteSet.HorizontalFlip , (recOpCode >> 9) & 0x1);
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);		
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuCore);
		break;
	}
	
}

//TODO - Bit more complicated than everything else
void RecCPU::recCpuJump()
{
	//CPU_LOG("Jump Recompiling %x from PC %x\n", recOpCode, PC-4);
	FlushLiveRegisters(true);
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
	int regSrc = -1;
	switch((recOpCode >> 16) & 0xf)
	{
	//Copy Immediate to GPR X
	case 0x0:
		if(CONST_PROP)
		{
			FlushLiveRegister(Op_X, false); //No need to flush it, its const or being written over
			
			GPRStatus.GPRConstVal[Op_X] = IMMEDIATE;
			GPRStatus.GPRIsConst[Op_X] = true;
			//FlushConstRegisters(true);
		}
		else
		{
			regSrc = GetLiveRegister(Op_X, true);
			//CPU_LOG("RegSrc assigned to reg %d\n", regSrc);
			if (regSrc >= 0)
			{
				RefChip16Emitter->MOV16ItoR((X86RegisterType)regSrc, IMMEDIATE);
				GPRStatus.LiveGPRReg[regSrc].isdirty = true;
				//FlushLiveRegister(Op_X, true);
			}
			else
			{
				FlushLiveRegister(Op_X, false);
				RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, IMMEDIATE);
			}
		}
		break;
	//Point Stack  Pointer to Address
	case 0x1:
		RefChip16Emitter->MOV16ItoM((unsigned int)&StackPTR, IMMEDIATE);
		break;
	//Load Register with value at imm address, shouldn't need to flush reg unless its in ECX
	case 0x2:
		FlushLiveRegisters(true);
		RefChip16Emitter->MOV32ItoR(ECX, IMMEDIATE);
		RefChip16Emitter->CALL16((int)recReadMem);
		GPRStatus.GPRIsConst[Op_X] = false;
		AssignLiveRegister(Op_X, EAX);
		break;
	//Load X with value from memory using address in Y
	case 0x3:
		
		FlushLiveRegisters(true);

		if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
			RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
		else
			RefChip16Emitter->MOV16MtoR(ECX, (unsigned int)&REG_Y);

		RefChip16Emitter->CALL16((int)recReadMem);

		GPRStatus.GPRIsConst[Op_X] = false;
		AssignLiveRegister(Op_X, EAX);
		break;
	//Load Y in to X
	case 0x4:
		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			FlushLiveRegister(Op_X, false);
			GPRStatus.GPRConstVal[Op_X] = GPRStatus.GPRConstVal[Op_Y];
			GPRStatus.GPRIsConst[Op_X] = true;
		}
		else
		{
			if (Op_X == Op_Y) return;

			int yReg = GetLiveRegister(Op_Y);
			if(Op_Y != Op_X)
				FlushLiveRegister(Op_X, true);

			ReAssignLiveRegister(yReg, Op_X);
			GPRStatus.GPRIsConst[Op_X] = false;
			
		}
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
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
		FlushLiveRegisters(true);
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)RefChip16Emitter->MOV16ItoR(EDX, GPRStatus.GPRConstVal[Op_X]);
		else {
			int xReg = GetLiveRegister(Op_X);
			if (xReg >= 0)
			{
				MoveLiveRegister(EDX, (X86RegisterType)xReg);
				FlushLiveRegister(Op_X, true);
			}
			else
				RefChip16Emitter->MOV16MtoR(EDX, (unsigned int)&REG_X);
		}

		if(IMMEDIATE > 0) RefChip16Emitter->MOV32ItoR(ECX, IMMEDIATE);
		else RefChip16Emitter->XOR16RtoR(ECX, ECX);
		FlushLiveRegisters(false);
		RefChip16Emitter->CALL16((int)recWriteMem);
		break;
	//Store X Register value in address given by Y Register
	case 0x1:
		FlushLiveRegisters(true);
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)RefChip16Emitter->MOV16ItoR(EDX, GPRStatus.GPRConstVal[Op_X]);
		else {
			int xReg = GetLiveRegister(Op_X);
			if (xReg >= 0)
			{
				MoveLiveRegister(EDX, (X86RegisterType)xReg);
				FlushLiveRegister(Op_X, true); //Just clear the rec info
			}
			else
				RefChip16Emitter->MOV16MtoR(EDX, (unsigned int)&REG_X);
		}
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)RefChip16Emitter->MOV16ItoR(ECX, GPRStatus.GPRConstVal[Op_Y]);
		else {
			int yReg = GetLiveRegister(Op_Y);
			if (yReg >= 0)
			{
				MoveLiveRegister(ECX, (X86RegisterType)yReg);
				FlushLiveRegister(Op_Y, true); //Just clear the rec info
			}
			else
				RefChip16Emitter->MOV16MtoR(ECX, (unsigned int)&REG_Y);
		}

		RefChip16Emitter->CALL16((int)recWriteMem);		
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuStore);
		break;
	}	
}

//This function checks the Zero and Negative conditions from the results
void RecCPU::recTestLogic(X86RegisterType dReg)
{
	RefChip16Emitter->CMP16ItoR(dReg, 0); //See if the result was zero
	j32Ptr[0] = RefChip16Emitter->JNE32(0); //Jump if it's not zero
											//Carry on if it is zero
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); //Set the zero flag

	j32Ptr[1] = RefChip16Emitter->JMP32(0); //Done with Zero, skip to end!

	RefChip16Emitter->x86SetJ32(j32Ptr[0]); //Return from skipping zero flag.

	RefChip16Emitter->CMP16ItoR(dReg, 0); //Do same compare again to find out if it's greater or less than zero
	j32Ptr[2] = RefChip16Emitter->JG32(0); //Jump if it's greater (don't set negative)
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80); //Set Negative Flag

	RefChip16Emitter->x86SetJ32(j32Ptr[1]); //Return from setting zero.
	RefChip16Emitter->x86SetJ32(j32Ptr[2]); //Return from skipping negative flag.
}

//EAX = Result ECX = Copy of Op_X //EDX = IMM or Op_Y
void RecCPU::recADDCheckOVF(X86RegisterType yReg, X86RegisterType dReg, X86RegisterType xTemp, bool XisY)
{
	if (XisY == false)
	{
		RefChip16Emitter->XOR16RtoR(xTemp, yReg); //Set if either is different
		RefChip16Emitter->SHR16ItoR(xTemp, 15);
		RefChip16Emitter->CMP16ItoR(xTemp, 0);
		j32Ptr[0] = RefChip16Emitter->JG32(0); //X and Y were different so no overflow

		RefChip16Emitter->MOV16RtoR(xTemp, yReg);
		RefChip16Emitter->XOR16RtoR(xTemp, dReg);
		RefChip16Emitter->SHR16ItoR(xTemp, 15);
		RefChip16Emitter->CMP16ItoR(xTemp, 1);
	}
	else //If X == Y then they are going to be the same, so the first jump would be true anyway.
	{
		RefChip16Emitter->MOV16RtoR(xTemp, yReg);
		RefChip16Emitter->XOR16RtoR(xTemp, dReg);
		RefChip16Emitter->SHR16ItoR(xTemp, 15);
		RefChip16Emitter->CMP16ItoR(xTemp, 1);
	}
	j32Ptr[1] = RefChip16Emitter->JNE32(0); //Result was the same for X and result so dont Overflow

	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x40); //Set Overflow Flag

	RefChip16Emitter->x86SetJ32(j32Ptr[0]); //Return from skipping overflow flag.
	RefChip16Emitter->x86SetJ32(j32Ptr[1]);

}

void RecCPU::recADDCheckCarry()
{
	j32Ptr[0] = RefChip16Emitter->JAE32(0); //Jump if it's not carrying CF = 0	
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x2); //Set Carry Flag
	RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping carry flag.
}

void RecCPU::recCpuAdd()
{
	int xReg = -1; 
	int yReg = -1;
	int xTemp = -1;
	int dReg = -1;
	bool yIsTemp = false;
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
			CPU_LOG("X = X + imm, X = %d, imm = %d Result = %d\n", GPRStatus.GPRConstVal[Op_X], IMMEDIATE, CalcResult);
			//Carry, if not zero
			if(CalcResult > 0xFFFF) flagsettings |= 0x2;
			CPU_LOG("Flag = %d\n", flagsettings);
			// CalcResult &= 0xffff;

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
			
			xReg = GetLiveRegister(Op_X);
			
			dReg = xReg;
			//If Immediate is 0, we dont need to add or alter REG, yReg represents IMM here
			if(IMMEDIATE != 0)	
			{	
				xTemp = GetFreeLiveRegister(0xfffe, true); //Should come back with a number, one of 4 regs must be usable.
				if (xTemp == -1) CPU_LOG("ALLOC ERROR ADD X = X + imm");
				RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later 

				yReg = GetFreeLiveRegister(0xfffd, true);
				yIsTemp = true;
				RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, IMMEDIATE);

				RefChip16Emitter->ADD16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
			}
			else 
			{
				RefChip16Emitter->CMP16ItoR((X86RegisterType)xReg, 0); //See if the result was zero

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
			if (Op_X == Op_Y) //If we reached here, neither is const
			{
				xReg = GetLiveRegister(Op_X); //Sets it live, no need to change				
				dReg = xReg;

				xTemp = GetFreeLiveRegister(0xfffe, true); //Should come back with a number, one of 4 regs must be usable.
				if (xTemp == -1) CPU_LOG("ALLOC ERROR ADD X = X + imm");
				
				RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later 
				yReg = xTemp; //This is going to be used for the flag calculation, best get it right.

				RefChip16Emitter->ADD16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
			}
			else
			{
				if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					yReg = GetFreeLiveRegister(0xfffd, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);

				if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					xReg = GetLiveRegister(Op_X, true);
					
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
				}
				else xReg = GetLiveRegister(Op_X);
				dReg = xReg;
				xTemp = GetFreeLiveRegister(0xfffe, true); //Should come back with a number, one of 4 regs must be usable.
				if (xTemp == -1) CPU_LOG("ALLOC ERROR ADD X = X + imm");
				RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later 

				RefChip16Emitter->ADD16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
				GPRStatus.GPRIsConst[Op_X] = false;
			}
			
			
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
			FlushLiveRegister(Op_Z, false);
			RefChip16Emitter->MOV16ItoM((unsigned int)&Flag._u16, flagsettings);
			return;
		}
		else
		{
			if(Op_X == Op_Y) 
			{
				//If we reached here then both are definately not a const
				xReg = GetLiveRegister(Op_X);
				yReg = xReg;
				if (Op_X == Op_Z)
				{
					dReg = xReg;
				}
				else
				{
					dReg = GetLiveRegister(Op_Z, true);
					RefChip16Emitter->MOV16RtoR((X86RegisterType)dReg, (X86RegisterType)xReg);
				}
				xTemp = GetFreeLiveRegister(0xfffe, true); //Should come back with a number, one of 4 regs must be usable.
				if (xTemp == -1) CPU_LOG("ALLOC ERROR ADD X = X + imm");
				RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later 
							
				RefChip16Emitter->ADD16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
			}
			else
			{
				xTemp = GetFreeLiveRegister(0xfffe, true); //Should come back with a number, one of 4 regs must be usable.
				if (xTemp == -1) CPU_LOG("ALLOC ERROR ADD X = X + imm");

				if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
					GPRStatus.LiveGPRReg[xReg].isdirty = true;
					if (Op_X != Op_Z) {
						GPRStatus.GPRIsConst[Op_X] = false;
					}
				}
				else xReg = GetLiveRegister(Op_X);

				if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					
					if (Op_Y != Op_Z)
					{
						yReg = GetLiveRegister(0xfffd, true);
						yIsTemp = true;						
					}
					else
					{
						yReg = GetLiveRegister(Op_Y);
						GPRStatus.LiveGPRReg[yReg].isdirty = true;
						GPRStatus.GPRIsConst[Op_Y] = false;
					}
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);				 

				if (Op_X == Op_Z)
				{
					dReg = xReg;
					RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later
					RefChip16Emitter->ADD16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);					
				}
				else if(Op_Y == Op_Z)
				{
					dReg = yReg;
					yReg = GetLiveRegister(0xfffd, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16RtoR((X86RegisterType)yReg, (X86RegisterType)dReg); //Make a copy for later
					RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)yReg); //Make a copy for later
					RefChip16Emitter->ADD16RtoR((X86RegisterType)dReg, (X86RegisterType)xReg);
				}
				else
				{
					dReg = GetLiveRegisterNoAssign(Op_Z);
					if (dReg >= 0) //There is a live reg for it)
					{
						FlushLiveRegister(Op_Z, false);
					}
					dReg = xReg;
					ReAssignLiveRegister(dReg, Op_Z); //reassign to different GPR as we're overwriting x
					RefChip16Emitter->ADD16RtoR((X86RegisterType)xReg, (X86RegisterType)yReg);									
				}				
			}
			
			GPRStatus.GPRIsConst[Op_Z] = false;
		}
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuAdd);
		return;
		break;
	}
	RefChip16Emitter->CMP16RtoR((X86RegisterType)xReg, (X86RegisterType)yReg);
	recADDCheckCarry();
	recTestLogic((X86RegisterType)dReg);
	recADDCheckOVF((X86RegisterType)yReg, (X86RegisterType)dReg, (X86RegisterType)xTemp, (xReg == yReg));
	
	//Clear out temporary registers
	if (yIsTemp == true)
	{
		FlushLiveRegister(0xfffd, false);
	}
	FlushLiveRegister(0xfffe, false);
	GPRStatus.LiveGPRReg[dReg].isdirty = true;
	
} 

void RecCPU::recSUBCheckOVF(X86RegisterType yReg, X86RegisterType dReg, X86RegisterType xTemp, bool XisY)
{
	if (XisY == false)
	{
		if(GPRStatus.LiveGPRReg[yReg].gprreg != 0xfffd)
			FlushLiveRegister(Op_Y, true);

		RefChip16Emitter->XOR16RtoR(yReg, xTemp); //Set if either is different
		RefChip16Emitter->SHR16ItoR(yReg, 15);
		RefChip16Emitter->CMP16ItoR(yReg, 1);
		j32Ptr[0] = RefChip16Emitter->JNE32(0); //Result was the same for X and Y so dont Overflow

		//RefChip16Emitter->MOV16RtoR(xTemp, yReg);
		RefChip16Emitter->XOR16RtoR(xTemp, dReg);
		RefChip16Emitter->SHR16ItoR(xTemp, 15);
		RefChip16Emitter->CMP16ItoR(xTemp, 1);
		j32Ptr[1] = RefChip16Emitter->JNE32(0); //Result was the same for X and result so dont Overflow
		RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x40); //Set Overflow Flag

		RefChip16Emitter->x86SetJ32(j32Ptr[0]); //Return from skipping overflow flag.
		RefChip16Emitter->x86SetJ32(j32Ptr[1]);
	}
	//else //If X == Y then they are going to be the same, it will never overflow	
}

void RecCPU::recSUBCheckCarry()
{	
	j32Ptr[0] = RefChip16Emitter->JAE32(0); //Jump if it's not carrying CF = 0	
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x2); //Set Carry Flag
	
	RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping carry flag.
	
}
void RecCPU::recCpuSub()
{
	int xReg = -1;
	int yReg = -1;
	int xTemp = -1;
	int dReg = -1;
	bool yIsTemp = false;
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
			GPRStatus.GPRIsConst[Op_X] = false;
			xReg = GetLiveRegister(Op_X);
			yReg = GetLiveRegister(0xfffd, true);
			yIsTemp = true;
			dReg = xReg;

			RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, IMMEDIATE);
				

			if(IMMEDIATE != 0)
			{
				xTemp = GetFreeLiveRegister(0xfffe, true); //Should come back with a number, one of 4 regs must be usable.
				if (xTemp == -1) CPU_LOG("ALLOC ERROR ADD X = X + imm");
				RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later 

				RefChip16Emitter->SUB16RtoR((X86RegisterType)xReg, (X86RegisterType)yReg);

				recSUBCheckCarry();
				recSUBCheckOVF((X86RegisterType)yReg, (X86RegisterType)dReg, (X86RegisterType)xTemp, false);
			}
			
			recTestLogic((X86RegisterType)dReg);
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
						FlushLiveRegister(Op_X, false);
				}
				else
				{
#ifdef REG_CACHING
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->XOR16RtoR((X86RegisterType)xReg, (X86RegisterType)xReg);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
				}			
			}
			else
			{
				if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					yReg = GetLiveRegister(0xfffd, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);

				if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
				}
				else xReg = GetLiveRegister(Op_X);

				dReg = xReg;
				xTemp = GetFreeLiveRegister(0xfffe, true); //Should come back with a number, one of 4 regs must be usable.
				if (xTemp == -1) CPU_LOG("ALLOC ERROR ADD X = X + imm");
				RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later 

				RefChip16Emitter->SUB16RtoR((X86RegisterType)xReg, (X86RegisterType)yReg);
				recSUBCheckCarry();
				recSUBCheckOVF((X86RegisterType)yReg, (X86RegisterType)dReg, (X86RegisterType)xTemp, false);
			

				GPRStatus.GPRIsConst[Op_X] = false;
				recTestLogic((X86RegisterType)dReg);
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
			FlushLiveRegister(Op_Z, false); //Incase it's different from X and Y and was live
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
					FlushLiveRegister(Op_Z, false); //Incase it's different from X and Y and was live
					GPRStatus.GPRConstVal[Op_Z] = 0;					
					GPRStatus.GPRIsConst[Op_Z] = true;
				}
				else
				{
#ifdef REG_CACHING
					dReg = GetLiveRegister(Op_Z, true);
					RefChip16Emitter->XOR16RtoR((X86RegisterType)dReg, (X86RegisterType)dReg);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_Z, 0);
#endif
				}	
				
			}
			else
			{
				xTemp = GetFreeLiveRegister(0xfffe, true); //Should come back with a number, one of 4 regs must be usable.
				if (xTemp == -1) CPU_LOG("ALLOC ERROR ADD X = X + imm");

				if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
					if (Op_X != Op_Z) {
						GPRStatus.GPRIsConst[Op_X] = false;
						GPRStatus.LiveGPRReg[xReg].isdirty = true;
					}
				}
				else xReg = GetLiveRegister(Op_X);

				if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					yReg = GetLiveRegister(0xfffd, true);
					if (Op_Y != Op_Z)
					{
						yIsTemp = true;
					}
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);


				if (Op_X == Op_Z)
				{
					dReg = xReg;
					RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later
					
					RefChip16Emitter->SUB16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
					recSUBCheckCarry();
					recSUBCheckOVF((X86RegisterType)yReg, (X86RegisterType)dReg, (X86RegisterType)xTemp, false);
					GPRStatus.GPRIsConst[Op_Z] = false;
					recTestLogic((X86RegisterType)dReg);
				}
				else if (Op_Y == Op_Z) //This is a pain as you have to subtract in the right order, will need all 4 regs
				{
					dReg = yReg;
					yReg = GetFreeLiveRegister(0xfffd, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later
					RefChip16Emitter->MOV16RtoR((X86RegisterType)yReg, (X86RegisterType)dReg);
					RefChip16Emitter->MOV16RtoR((X86RegisterType)dReg, (X86RegisterType)xReg);
					
					RefChip16Emitter->SUB16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
					recSUBCheckCarry();
					recSUBCheckOVF((X86RegisterType)yReg, (X86RegisterType)dReg, (X86RegisterType)xTemp, false);
					GPRStatus.GPRIsConst[Op_Z] = false;
					recTestLogic((X86RegisterType)dReg);

				}
				else
				{
					dReg = GetLiveRegister(Op_Z);
					//if (dReg >= 0) //There is a live reg for it)
					//{ 
					//	FlushLiveRegister(Op_Z, false);
					//}
					//dReg = xReg;
					RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg);
					RefChip16Emitter->MOV16RtoR((X86RegisterType)dReg, (X86RegisterType)xReg);
					
					//ReAssignLiveRegister(dReg, Op_Z);
					RefChip16Emitter->SUB16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
					recSUBCheckCarry();
					recSUBCheckOVF((X86RegisterType)yReg, (X86RegisterType)dReg, (X86RegisterType)xTemp, false);
					GPRStatus.GPRIsConst[Op_Z] = false;
					recTestLogic((X86RegisterType)dReg);
					
				}
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
			GPRStatus.GPRIsConst[Op_X] = false;
			xReg = GetLiveRegister(Op_X);
			yReg = GetLiveRegister(0xfffd, true);
			yIsTemp = true;
			dReg = xReg;

			RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, IMMEDIATE);


			if (IMMEDIATE != 0)
			{
				xTemp = GetFreeLiveRegister(0xfffe, true); //Should come back with a number, one of 4 regs must be usable.
				if (xTemp == -1) CPU_LOG("ALLOC ERROR ADD X = X + imm");
				RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later 
				FlushLiveRegister(Op_X, true);
				RefChip16Emitter->SUB16RtoR((X86RegisterType)xReg, (X86RegisterType)yReg);
				recSUBCheckCarry();
				recSUBCheckOVF((X86RegisterType)yReg, (X86RegisterType)dReg, (X86RegisterType)xTemp, false);
			}

			recTestLogic((X86RegisterType)dReg);

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
				if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					yReg = GetLiveRegister(0xfffd, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);

				if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
				}
				else xReg = GetLiveRegister(Op_X);

				dReg = xReg;
				xTemp = GetFreeLiveRegister(0xfffe, true); //Should come back with a number, one of 4 regs must be usable.
				if (xTemp == -1) CPU_LOG("ALLOC ERROR ADD X = X + imm");
				RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg); //Make a copy for later 

				
				FlushLiveRegister(Op_X, (GPRStatus.GPRIsConst[Op_X] == true) ? false : true);
				RefChip16Emitter->SUB16RtoR((X86RegisterType)xReg, (X86RegisterType)yReg);
				recSUBCheckCarry();

				recSUBCheckOVF((X86RegisterType)yReg, (X86RegisterType)dReg, (X86RegisterType)xTemp, false);
				
				recTestLogic((X86RegisterType)dReg);
				
			}
		}
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuSub);
		return;
		break;
	}

	if (yIsTemp == true)
	{
		FlushLiveRegister(0xfffd, false);
	}
	FlushLiveRegister(0xfffe, false);
	GPRStatus.LiveGPRReg[dReg].isdirty = true;
	
}

void RecCPU::recCpuAND()
{
	int xReg = -1;
	int yReg = -1;
	int xTemp = -1;
	int dReg = -1;
	bool yIsTemp = false;
	//CPU_LOG("AND Recompiling %x from PC %x\n", recOpCode, PC-4);

	RefChip16Emitter->AND16ItoM((unsigned int)&Flag._u16, ~0x84);

	switch ((recOpCode >> 16 & 0xf))
	{
		//X = X & imm [z,n]
	case 0x0:
		if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{

			GPRStatus.GPRConstVal[Op_X] &= IMMEDIATE;

			if (GPRStatus.GPRConstVal[Op_X] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
			else if (GPRStatus.GPRConstVal[Op_X] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);
			return;
		}
		else
		{
			if (IMMEDIATE == 0)
			{
				RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);

				if (CONST_PROP)
				{
					GPRStatus.GPRIsConst[Op_X] = true;
					GPRStatus.GPRConstVal[Op_X] = 0;

					FlushLiveRegister(Op_X, false);
				}
				else
				{
#ifdef REG_CACHING
					xReg = GetLiveRegister(Op_X);
					dReg = xReg;
					RefChip16Emitter->XOR16RtoR((X86RegisterType)dReg, (X86RegisterType)dReg);
					GPRStatus.LiveGPRReg[dReg].isdirty = true;
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
				}
				return;
			}
			else
			{
				dReg = GetLiveRegister(Op_X);
				RefChip16Emitter->AND16ItoR((X86RegisterType)dReg, IMMEDIATE);

			}

		}
		break;
		//X = X & Y [z,n]
	case 0x1:
		if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			GPRStatus.GPRConstVal[Op_X] &= GPRStatus.GPRConstVal[Op_Y];


			if (GPRStatus.GPRConstVal[Op_X] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
			else if (GPRStatus.GPRConstVal[Op_X] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);
			return;
		}
		else
		{
			if (Op_X == Op_Y)
			{
				dReg = GetLiveRegister(Op_X);
				//Nothing to do, just check if the register is zero or negative
			}
			else
			{
				if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					//If we hit here, X wasnt const
					dReg = GetLiveRegister(Op_X);
					RefChip16Emitter->AND16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else //X might be const instead
				{
					if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
					{
						yReg = GetLiveRegister(Op_Y);
						dReg = GetLiveRegister(Op_X);  //This will copy the const reg in for us
						RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);
						RefChip16Emitter->AND16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
						GPRStatus.GPRIsConst[Op_X] = false; //should do this too
					}
					else
					{
						yReg = GetLiveRegister(Op_Y);
						dReg = GetLiveRegister(Op_X);  //This will copy the const reg in for us	
						RefChip16Emitter->AND16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
						GPRStatus.LiveGPRReg[dReg].isdirty = true;
					}
				}
			}

			recTestLogic((X86RegisterType)dReg);
			return;

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
				FlushLiveRegister(Op_Z, false); //Incase it's different from X and Y and was live
				return;
			}
			else
			{
				if(Op_X == Op_Y)
				{
					//If we reached here Op_X isnt const
					dReg = GetLiveRegister(Op_X);
					if (Op_Z != Op_X)
						ReAssignLiveRegister(dReg, Op_Z);
				}
				else
				{
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						dReg = GetLiveRegister(Op_X);
						ReAssignLiveRegister(dReg, Op_Z);

						RefChip16Emitter->AND16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_Y]);
						
					}
					else //X might be const instead
					{
						if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							dReg = GetLiveRegister(Op_Y);

							ReAssignLiveRegister(dReg, Op_Z);
							RefChip16Emitter->AND16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);

						}
						else
						{
							dReg = GetLiveRegister(Op_X);
							yReg = GetLiveRegister(Op_Y);
							ReAssignLiveRegister(dReg, Op_Z);
							RefChip16Emitter->AND16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
						}
					}
					GPRStatus.GPRIsConst[Op_Z] = false;
				}
			}
		break;
		//X & imm discard flags only [z,n]
		case 0x3:
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{
				tempreg = GPRStatus.GPRConstVal[Op_X] & IMMEDIATE;

				if(tempreg == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
				else if(tempreg & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);
				return;
			}
			else
			{
				if(IMMEDIATE == 0)
				{
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
					return;
				}
				else
				{
					dReg = GetLiveRegister(Op_X); //The live instruction is different so copy it in, then we can ignore it.
					xTemp = GetLiveRegister(0xfffe, true);
					RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)dReg);
					RefChip16Emitter->AND16ItoR((X86RegisterType)xTemp, IMMEDIATE);
					dReg = xTemp;
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
				return;
			}
			else
			{
				if(Op_X == Op_Y)
				{
					//If we reached here Op_X isnt const
					//We can call this reg as it won't be modified anyway
					dReg = GetLiveRegister(Op_X);
				}
				else
				{
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						xReg = GetLiveRegister(Op_X);
						xTemp = GetLiveRegister(0xfffe, true);
						dReg = xTemp;
						RefChip16Emitter->MOV16RtoR((X86RegisterType)dReg, (X86RegisterType)xReg);
						RefChip16Emitter->AND16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_Y]);
					}
					else //X might be const instead
					{
						if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							yReg = GetLiveRegister(Op_Y);
							xTemp = GetLiveRegister(0xfffe, true);
							dReg = xTemp;
							RefChip16Emitter->MOV16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
							RefChip16Emitter->AND16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);
						}
						else
						{
							xReg = GetLiveRegister(Op_X);
							yReg = GetLiveRegister(Op_Y);
							xTemp = GetLiveRegister(0xfffe, true);
							dReg = xTemp;
							RefChip16Emitter->MOV16RtoR((X86RegisterType)dReg, (X86RegisterType)xReg);
							RefChip16Emitter->AND16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
						}
					}
				}
			}
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuAND);
		return;
		break;

	}
	recTestLogic((X86RegisterType)dReg);
	if (yIsTemp == true)
	{
		FlushLiveRegister(0xfffd, false);
	}
	if (xTemp >= 0)
		FlushLiveRegister(0xfffe, false);

	GPRStatus.LiveGPRReg[dReg].isdirty = true;
	
}

void RecCPU::recCpuOR()
{
	//CPU_LOG("OR Recompiling %x from PC %x\n", recOpCode, PC-4);
	int xReg = -1;
	int yReg = -1;
	int xTemp = -1;
	int dReg = -1;
	bool yIsTemp = false;

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
				return;
			}
			else
			{
				if(IMMEDIATE == 0)
				{
					dReg = GetLiveRegister(Op_X); //Nothing to change, just load it in for checking
				}
				else
				{
					dReg = GetLiveRegister(Op_X);
					RefChip16Emitter->OR16ItoR((X86RegisterType)dReg, IMMEDIATE);
				}				
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
				return;
			}
			else
			{
				if(Op_X == Op_Y) 
				{
					//If we reached here Op_X isnt const
					dReg = GetLiveRegister(Op_X); //Nothing to change, just load it in for checking
				}
				else
				{
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						dReg = GetLiveRegister(Op_X);
						RefChip16Emitter->OR16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_Y]);
					}
					else //X might be const instead
					{
						if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							dReg = GetLiveRegister(Op_Y); // Write Y back if it was already live as we're about to lose it.
							ReAssignLiveRegister(dReg, Op_X);
							RefChip16Emitter->OR16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);
							GPRStatus.GPRIsConst[Op_X] = false;
						}
						else 
						{
							yReg = GetLiveRegister(Op_Y); // Write Y back if it was already live as we're about to lose it.
							dReg = GetLiveRegister(Op_X);
							RefChip16Emitter->OR16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
						}					
					}					
				}	
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
				FlushLiveRegister(Op_Z, false); //Incase it's different from X and Y and was live
				return;
			}
			else
			{
				if(Op_X == Op_Y) 
				{
					//If we reached here neither are const
					dReg = GetLiveRegister(Op_X);
					ReAssignLiveRegister(dReg, Op_Z);
				}
				else
				{
					if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						dReg = GetLiveRegister(Op_X);
						ReAssignLiveRegister(dReg, Op_Z);
						RefChip16Emitter->OR16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_Y]);
					}
					else //X might be const instead
					{

						if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							dReg = GetLiveRegister(Op_Y);
							ReAssignLiveRegister(dReg, Op_Z);
							RefChip16Emitter->OR16ItoR((X86RegisterType)dReg,  GPRStatus.GPRConstVal[Op_X]);
							
						}
						else 
						{
							dReg = GetLiveRegister(Op_X);
							yReg = GetLiveRegister(Op_Y);
							ReAssignLiveRegister(dReg, Op_Z);
							RefChip16Emitter->OR16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
						}					
					}										
				}
				GPRStatus.GPRIsConst[Op_Z] = false;
			}
		break;
		default:
			FlushLiveRegisters(true);
			FlushConstRegisters(true);
			RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
			RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
			RefChip16Emitter->CALL(CpuOR);
			return;
			break;

	}
	recTestLogic((X86RegisterType)dReg);
	if (yIsTemp == true)
	{
		FlushLiveRegister(0xfffd, false);
	}
	if (xTemp >= 0)
		FlushLiveRegister(0xfffe, false);

	GPRStatus.LiveGPRReg[dReg].isdirty = true;
}
void RecCPU::recCpuXOR()
{
	int xReg = -1;
	int yReg = -1;
	int xTemp = -1;
	int dReg = -1;
	bool yIsTemp = false;
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
				return;
			}
			else
			{
				if (IMMEDIATE == 0)
				{
					dReg = GetLiveRegister(Op_X); //Nothing to change, just load it in for checking
				}
				else
				{
					dReg = GetLiveRegister(Op_X);
					RefChip16Emitter->XOR16ItoR((X86RegisterType)dReg, IMMEDIATE);
				}
			}
		break;
		//X = X ^ Y [z,n]
		case 0x1:
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				GPRStatus.GPRConstVal[Op_X] ^= GPRStatus.GPRConstVal[Op_Y];

				if(GPRStatus.GPRConstVal[Op_X] == 0)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4); 
				else if(GPRStatus.GPRConstVal[Op_X] & 0x8000)RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x80);
				return;
			}
			else
			{
				if (Op_X == Op_Y)
				{
					//If we reached here Op_X isnt const
					FlushLiveRegister(Op_X, false); //XORing itself will zero the register, so we can make it a const.
					GPRStatus.GPRIsConst[Op_X] = true;
					GPRStatus.GPRConstVal[Op_X] = 0;
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
					return;
				}
				else
				{
					if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						dReg = GetLiveRegister(Op_X);
						RefChip16Emitter->XOR16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_Y]);
					}
					else //X might be const instead
					{
						if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							dReg = GetLiveRegister(Op_Y); // Write Y back if it was already live as we're about to lose it.
							ReAssignLiveRegister(dReg, Op_X);
							RefChip16Emitter->XOR16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);
							GPRStatus.GPRIsConst[Op_X] = false;
						}
						else
						{
							yReg = GetLiveRegister(Op_Y); // Write Y back if it was already live as we're about to lose it.
							dReg = GetLiveRegister(Op_X);
							RefChip16Emitter->XOR16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
						}
					}
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
				return;
			}
			else
			{
				if (Op_X == Op_Y)
				{
					//If we reached here neither are const
					FlushLiveRegister(Op_Z, false); //XORing itself will zero the register, so we can make it a const.
					GPRStatus.GPRIsConst[Op_Z] = true;
					GPRStatus.GPRConstVal[Op_Z] = 0;
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
					return;
				}
				else
				{
					if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
					{
						//If we hit here, X wasnt const
						dReg = GetLiveRegister(Op_X);
						ReAssignLiveRegister(dReg, Op_Z);
						RefChip16Emitter->XOR16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_Y]);
					}
					else //X might be const instead
					{

						if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
						{
							dReg = GetLiveRegister(Op_Y);
							ReAssignLiveRegister(dReg, Op_Z);
							RefChip16Emitter->XOR16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);

						}
							else
							{
								dReg = GetLiveRegister(Op_X);
								yReg = GetLiveRegister(Op_Y);
								ReAssignLiveRegister(dReg, Op_Z);
								RefChip16Emitter->XOR16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
							}
					}
				}
				GPRStatus.GPRIsConst[Op_Z] = false;
			}
		break;
		default:
			FlushLiveRegisters(true);
			FlushConstRegisters(true);
			RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
			RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
			RefChip16Emitter->CALL(CpuXOR);
			return;
			break;

	}
	recTestLogic((X86RegisterType)dReg);
	if (yIsTemp == true)
	{
		FlushLiveRegister(0xfffd, false);
	}
	if (xTemp >= 0)
		FlushLiveRegister(0xfffe, false);

	GPRStatus.LiveGPRReg[dReg].isdirty = true;
} 

void RecCPU::recMULCheckCarry()
{
	j32Ptr[0] = RefChip16Emitter->JAE32(0); //Jump if it's not carrying CF = 0	
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x2); //Set Carry Flag
	RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping carry flag.
	
}
//Make sure you clear needed register
void RecCPU::recCpuMul()
{
	int xReg = -1;
	int yReg = -1;
	int xTemp = -1;
	int dReg = -1;
	bool yIsTemp = false;
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
			return;
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
					FlushLiveRegister(Op_X, false);					
				}
				else
				{
#ifdef REG_CACHING
					dReg = GetLiveRegister(Op_X);
					RefChip16Emitter->XOR16RtoR((X86RegisterType)dReg, (X86RegisterType)dReg);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
				}
				return;
			}
			else
			{
				yReg = GetLiveRegister(0xfffd, true);
				SetupMulDivSrcRegister(Op_X, 0xfffd, false);
				yReg = GetLiveRegister(0xfffd, true); //Do this again in case it moved.
				dReg = GetLiveRegister(Op_X);  //Should return EAX, in theory.
				yIsTemp = true;
				RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, IMMEDIATE);
				RefChip16Emitter->MUL16RtoEAX((X86RegisterType)yReg);

				recMULCheckCarry();
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
			return;
		}
		else
		{
			CPU_LOG("MUL1 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
			if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true) 
			{
				if(GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("MUL 2 Y 1\n");
				if (GPRStatus.GPRConstVal[Op_Y] == 0) {
					FlushLiveRegister(Op_X, false);
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
					GPRStatus.GPRConstVal[Op_X] = 0;
					GPRStatus.GPRIsConst[Op_X] = true;
					return;
				}
				yReg = GetLiveRegister(Op_Y, true);
				yIsTemp = true;
				RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
			}
			else yReg = GetLiveRegister(Op_Y);

			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{
				if(GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("MUL 2 X 1\n");
				if (GPRStatus.GPRConstVal[Op_X] == 0) {
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
					GPRStatus.GPRConstVal[Op_X] = 0;
					GPRStatus.GPRIsConst[Op_X] = true;
					return;
				}
				dReg = GetLiveRegister(Op_X, true);
				RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);
			}
			else dReg = GetLiveRegister(Op_X);
						
			SetupMulDivSrcRegister(Op_X, Op_Y, false);
			yReg = GetLiveRegister(Op_Y); //Make sure we're pointing at the right registers again
			dReg = GetLiveRegister(Op_X);
			RefChip16Emitter->MUL16RtoEAX((X86RegisterType)yReg);
			recMULCheckCarry();
			GPRStatus.GPRIsConst[Op_X] = false;			
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
			return;
		}
		else
		{
			CPU_LOG("MUL2 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);

			if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				if (GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("MUL 3 Y 1\n");
				if (GPRStatus.GPRConstVal[Op_Y] == 0) {
					FlushLiveRegister(Op_Z, false);
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
					GPRStatus.GPRConstVal[Op_Z] = 0;
					GPRStatus.GPRIsConst[Op_Z] = true;
					return;
				}
				yReg = GetLiveRegister(Op_Y, true);
				yIsTemp = true;
				RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
			}
			else yReg = GetLiveRegister(Op_Y);

			if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{
				if (GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("MUL 3 X 1\n");
				if (GPRStatus.GPRConstVal[Op_X] == 0) {
					FlushLiveRegister(Op_Z, false);
					RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
					GPRStatus.GPRConstVal[Op_Z] = 0;
					GPRStatus.GPRIsConst[Op_Z] = true;
					return;
				}				
				dReg = GetLiveRegister(Op_X, true);
				RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);
			}
			else dReg = GetLiveRegister(Op_X);
			
			SetupMulDivSrcRegister(Op_X, Op_Y, false);
			
			yReg = GetLiveRegister(Op_Y); //Make sure we're pointing at the right registers again
			dReg = GetLiveRegister(Op_X);
			if (Op_Y == Op_Z)
			{
				MoveLiveRegister((X86RegisterType)dReg, (X86RegisterType)yReg, true);
				yIsTemp = false;
			}
			AssignLiveRegister(Op_Z, dReg);
			RefChip16Emitter->MUL16RtoEAX((X86RegisterType)yReg);
			recMULCheckCarry();
			GPRStatus.GPRIsConst[Op_Z] = false;
			
		}
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuMul);
		return;
		break;
	}
	recTestLogic((X86RegisterType)dReg);
	if (yIsTemp == true)
	{
		FlushLiveRegister(GPRStatus.LiveGPRReg[yReg].gprreg, false);
	}
	if (xTemp >= 0)
		FlushLiveRegister(0xfffe, false);

	ToggleLockRegister(EDX, false);
	GPRStatus.LiveGPRReg[dReg].isdirty = true;
}

void RecCPU::recDIVCheckCarry()
{
	RefChip16Emitter->CMP16ItoR(EDX, 0);
	j32Ptr[0] = RefChip16Emitter->JE32(0); //Jump if Remainder(EDX) is 0 (Not Carry)
	RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x2); //Set Carry Flag

	RefChip16Emitter->x86SetJ32( j32Ptr[0] ); //Return from skipping carry flag set.

	
}

//Make sure you clear needed registers
void RecCPU::recCpuDiv()
{
	int xReg = -1;
	int yReg = -1;
	int xTemp = -1;
	int dReg = -1;
	bool yIsTemp = false;
	//CPU_LOG("DIV Recompiling %x from PC %x\n", recOpCode, PC-4);
	
	//DIV uses EDX and EAX, if anything is in EDX, it could screw things up
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
			return;
		}
		else
		{
			if(IMMEDIATE == 1) CPU_LOG("DIV 1 IMM 1\n");
			yReg = GetLiveRegister(0xfffe, true);
			yIsTemp = true;
			RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, IMMEDIATE);
			SetupMulDivSrcRegister(Op_X, 0xfffe, false);
			yReg = ECX;
			dReg = EAX;

			
			RefChip16Emitter->DIV16RtoEAX((X86RegisterType)yReg);
			
			recDIVCheckCarry();
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
			return;
		}
		else
		{
			if(Op_X == Op_Y) 
			{
				if(CONST_PROP)
				{
					GPRStatus.GPRIsConst[Op_X] = true;
					GPRStatus.GPRConstVal[Op_X] = 1;						
					FlushLiveRegister(Op_X, false);					
				}
				else
				{
#ifdef REG_CACHING
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, 1);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 1);
#endif
				}
				return;
			}
			else
			{
				CPU_LOG("DIV2 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
				if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					if(GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("DIV 2 Y 1\n");
					yReg = GetLiveRegister(Op_Y, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);

				if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					if(GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("DIV 2 X 1\n");
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
				}
				else xReg = GetLiveRegister(Op_X);

				SetupMulDivSrcRegister(Op_X, Op_Y, false);
				yReg = ECX;
				xReg = EAX;
				dReg = xReg;

				RefChip16Emitter->DIV16RtoEAX((X86RegisterType)yReg);

				recDIVCheckCarry();
				GPRStatus.GPRIsConst[Op_X] = false;
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
			return;
		}
		else
		{
			if(Op_X == Op_Y) 
			{
				if(CONST_PROP)
				{
					GPRStatus.GPRIsConst[Op_Z] = true;
					GPRStatus.GPRConstVal[Op_Z] = 1;						
					FlushLiveRegister(Op_Z, false);					
				}
				else
				{
#ifdef REG_CACHING
					dReg = GetLiveRegister(Op_Z, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, 1);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_Z, 1);
#endif
				}
				return;
			}
			else
			{
				CPU_LOG("DIV2 X %d, Y %d X = %x Y = %x\n", GPRStatus.GPRIsConst[Op_X], GPRStatus.GPRIsConst[Op_Y], GPRStatus.GPRConstVal[Op_X], GPRStatus.GPRConstVal[Op_Y]);
				if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					if(GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("DIV 2 Y 1\n");
					yReg = GetLiveRegister(Op_Y, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);

				if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					if(GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("DIV 2 X 1\n");
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
				}
				else xReg = GetLiveRegister(Op_X);

				SetupMulDivSrcRegister(Op_X, Op_Y, false);
				yReg = ECX;
				xReg = EAX;

				FlushLiveRegister(Op_X, GPRStatus.GPRIsConst[Op_X] ? false : true);
				RefChip16Emitter->DIV16RtoEAX((X86RegisterType)yReg);

				recDIVCheckCarry();
				ReAssignLiveRegister(EAX, Op_Z);
				GPRStatus.GPRIsConst[Op_Z] = false;
				dReg = EAX;
			}			
		}
		break;
		//X = X MOD IMM [z,n]
	case 0x3:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{
			int flags = 0;
			short TempResult = 0;

			TempResult = (short)GPRStatus.GPRConstVal[Op_X] % (short)IMMEDIATE;
			
			if((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (IMMEDIATE & 0x8000)) GPRStatus.GPRConstVal[Op_X] = TempResult + IMMEDIATE;
			else GPRStatus.GPRConstVal[Op_X] = TempResult;

			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			GPRStatus.GPRIsConst[Op_X] = true;
			return;
		}
		else
		{
			yReg = GetLiveRegister(0xfffe, true);
			yIsTemp = true;
			SetupMulDivSrcRegister(Op_X, 0xfffe, true);
			yReg = ECX;
			dReg = EAX;

			RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, IMMEDIATE);
			RefChip16Emitter->IDIV16RtoEAX((X86RegisterType)yReg);

			RefChip16Emitter->XOR16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);
			RefChip16Emitter->AND16ItoR((X86RegisterType)dReg, 0x8000);

			RefChip16Emitter->CMP16ItoR((X86RegisterType)dReg, 0x8000);
			j32Ptr[0] = RefChip16Emitter->JNE32(0);

			RefChip16Emitter->ADD16RtoR(EDX, (X86RegisterType)yReg);

			RefChip16Emitter->x86SetJ32( j32Ptr[0] );

			RefChip16Emitter->MOV16RtoR((X86RegisterType)dReg, EDX);
		}
		break;
	//X = X MOD Y [z,n]
	case 0x4:		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flags = 0;
			short TempResult = 0;

			TempResult = (short)GPRStatus.GPRConstVal[Op_X] % (short)GPRStatus.GPRConstVal[Op_Y];
			
			if((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (GPRStatus.GPRConstVal[Op_Y] & 0x8000)) GPRStatus.GPRConstVal[Op_X] = TempResult + (short)GPRStatus.GPRConstVal[Op_Y];
			else GPRStatus.GPRConstVal[Op_X] = TempResult;

			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			return;
		}
		else
		{
			if(Op_X == Op_Y) 
			{
				if(CONST_PROP)
				{
					GPRStatus.GPRIsConst[Op_X] = true;
					GPRStatus.GPRConstVal[Op_X] = 0;						
					FlushLiveRegister(Op_X, false);
				}
				else
				{
#ifdef REG_CACHING
					dReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, 0);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
				}
				RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
				return;
			}
			else
			{
				if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					if (GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("DIV 2 Y 1\n");
					yReg = GetLiveRegister(Op_Y, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);

				if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					if (GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("DIV 2 X 1\n");
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
				}
				else xReg = GetLiveRegister(Op_X);

				SetupMulDivSrcRegister(Op_X, Op_Y, true);
				yReg = GetLiveRegister(Op_Y);
				xReg = GetLiveRegister(Op_X);
				dReg = xReg;
				xTemp = GetLiveRegister(0xfffe, true);

				RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg);
				RefChip16Emitter->XOR16RtoR((X86RegisterType)xTemp, (X86RegisterType)yReg);
				RefChip16Emitter->AND16ItoR((X86RegisterType)xTemp, 0x8000);

				RefChip16Emitter->IDIV16RtoEAX((X86RegisterType)yReg);
				
				
				RefChip16Emitter->CMP16ItoR((X86RegisterType)xTemp, 0x8000);
				j32Ptr[0] = RefChip16Emitter->JNE32(0);
				
				RefChip16Emitter->ADD16RtoR(EDX, (X86RegisterType)yReg);

				RefChip16Emitter->x86SetJ32( j32Ptr[0] );
				
				RefChip16Emitter->MOV16RtoR((X86RegisterType)dReg, EDX);
				GPRStatus.GPRIsConst[Op_X] = false;
			}			
		}
		break;
	//Z = X MOD Y [z,n]
	case 0x5:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flags = 0;
			short TempResult = 0;

			TempResult = (short)GPRStatus.GPRConstVal[Op_X] % (short)GPRStatus.GPRConstVal[Op_Y];
			
			if((GPRStatus.GPRConstVal[Op_X] & 0x8000) ^ (GPRStatus.GPRConstVal[Op_Y] & 0x8000)) GPRStatus.GPRConstVal[Op_Z] = TempResult + (short)GPRStatus.GPRConstVal[Op_Y];
			else GPRStatus.GPRConstVal[Op_Z] = TempResult;

			if(GPRStatus.GPRConstVal[Op_Z] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_Z] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			GPRStatus.GPRIsConst[Op_Z] = true;
			return;
		}
		else
		{
			if (Op_X == Op_Y)
			{
				if (CONST_PROP)
				{
					GPRStatus.GPRIsConst[Op_X] = true;
					GPRStatus.GPRConstVal[Op_X] = 0;
					FlushLiveRegister(Op_X, false);
				}
				else
				{
#ifdef REG_CACHING
					dReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, 0);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
				}
				RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
				return;
			}
			else
			{
				if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					if (GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("DIV 2 Y 1\n");
					yReg = GetLiveRegister(Op_Y, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);

				if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					if (GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("DIV 2 X 1\n");
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
				}
				else xReg = GetLiveRegister(Op_X);

				SetupMulDivSrcRegister(Op_X, Op_Y, true);
				yReg = GetLiveRegister(Op_Y);
				xReg = GetLiveRegister(Op_X);
				dReg = xReg;
				xTemp = GetLiveRegister(0xfffe, true);

				RefChip16Emitter->MOV16RtoR((X86RegisterType)xTemp, (X86RegisterType)xReg);
				RefChip16Emitter->XOR16RtoR((X86RegisterType)xTemp, (X86RegisterType)yReg);
				RefChip16Emitter->AND16ItoR((X86RegisterType)xTemp, 0x8000);
				FlushLiveRegister(xReg, GPRStatus.GPRIsConst[Op_X] ? false : true);
				RefChip16Emitter->IDIV16RtoEAX((X86RegisterType)yReg);


				RefChip16Emitter->CMP16ItoR((X86RegisterType)xTemp, 0x8000);
				j32Ptr[0] = RefChip16Emitter->JNE32(0);

				RefChip16Emitter->ADD16RtoR(EDX, (X86RegisterType)yReg);

				RefChip16Emitter->x86SetJ32(j32Ptr[0]);

				RefChip16Emitter->MOV16RtoR((X86RegisterType)dReg, EDX);
				ReAssignLiveRegister(EAX, Op_Z);
				GPRStatus.GPRIsConst[Op_Z] = false;
			}
		}
		break;
	//X = X % IMM [z,n]
	case 0x6:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{
			int flags = 0;
			CPU_LOG("X = X REM IMM CONST\n");
			GPRStatus.GPRConstVal[Op_X] = (short)GPRStatus.GPRConstVal[Op_X] % IMMEDIATE;
			CPU_LOG("Result = %d\n", (short)GPRStatus.GPRConstVal[Op_X]);
			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			return;
		}
		else
		{
			CPU_LOG("X = X REM IMM");
			//FlushLiveRegisters(true);
			xTemp = GetLiveRegister(0xfffe, true);
			SetupMulDivSrcRegister(Op_X, 0xfffe, true);
			xTemp = ECX;

			RefChip16Emitter->MOV16ItoR((X86RegisterType)xTemp, IMMEDIATE);

			RefChip16Emitter->x86SetJ32(j32Ptr[1]);

			RefChip16Emitter->IDIV16RtoEAX((X86RegisterType)xTemp);
			GPRStatus.GPRIsConst[Op_X] = false;

			ReAssignLiveRegister(EDX, Op_X);
			dReg = EDX;
		}
		break;
	//X = X % Y [z,n]
	case 0x7:		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flags = 0;

			GPRStatus.GPRConstVal[Op_X] = (short)GPRStatus.GPRConstVal[Op_X] % (short)GPRStatus.GPRConstVal[Op_Y];

			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			return;
		}
		else {
			if (Op_X == Op_Y)
			{
				if (CONST_PROP)
				{
					GPRStatus.GPRIsConst[Op_X] = true;
					GPRStatus.GPRConstVal[Op_X] = 0;
					FlushLiveRegister(Op_X, false);
				}
				else
				{
#ifdef REG_CACHING
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, 0);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
				}
				RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
				return;
			}
			else
			{
				if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					if (GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("DIV 2 Y 1\n");
					yReg = GetLiveRegister(Op_Y, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);

				if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					if (GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("DIV 2 X 1\n");
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
				}
				else xReg = GetLiveRegister(Op_X);

				SetupMulDivSrcRegister(Op_X, Op_Y, true);
				yReg = ECX;
				xReg = EAX;

				RefChip16Emitter->IDIV16RtoEAX((X86RegisterType)yReg);

				GPRStatus.GPRIsConst[Op_X] = false;
				FlushLiveRegister(Op_X, false);
				ReAssignLiveRegister(EDX, Op_X);
				dReg = EDX;
			}
		}
		break;
	//Z = X % Y [z,n]
	case 0x8:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flags = 0;

			GPRStatus.GPRConstVal[Op_Z] = (short)GPRStatus.GPRConstVal[Op_X] % (short)GPRStatus.GPRConstVal[Op_Y];

			if(GPRStatus.GPRConstVal[Op_Z] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_Z] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			GPRStatus.GPRIsConst[Op_Z] = true;
			return;
		}
		else
		{
			if (Op_X == Op_Y)
			{
				if (CONST_PROP)
				{
					GPRStatus.GPRIsConst[Op_X] = true;
					GPRStatus.GPRConstVal[Op_X] = 0;
					FlushLiveRegister(Op_X, false);
				}
				else
				{
#ifdef REG_CACHING
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, 0);
#else
					RefChip16Emitter->MOV16ItoM((unsigned int)&REG_X, 0);
#endif
				}
				RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, 0x4);
				return;
			}
			else
			{
				if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
				{
					if (GPRStatus.GPRConstVal[Op_Y] == 1) CPU_LOG("DIV 2 Y 1\n");
					yReg = GetLiveRegister(Op_Y, true);
					yIsTemp = true;
					RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
				}
				else yReg = GetLiveRegister(Op_Y);

				if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
				{
					if (GPRStatus.GPRConstVal[Op_X] == 1) CPU_LOG("DIV 2 X 1\n");
					xReg = GetLiveRegister(Op_X, true);
					RefChip16Emitter->MOV16ItoR((X86RegisterType)xReg, GPRStatus.GPRConstVal[Op_X]);
				}
				else xReg = GetLiveRegister(Op_X);

				SetupMulDivSrcRegister(Op_X, Op_Y, true);
				yReg = ECX;
				xReg = EAX;
				FlushLiveRegister(Op_X, GPRStatus.GPRIsConst[Op_X] ? false : true);
				RefChip16Emitter->IDIV16RtoEAX((X86RegisterType)yReg);

				GPRStatus.GPRIsConst[Op_X] = false;
				ReAssignLiveRegister(EDX, Op_Z);
				dReg = EDX;
			}
		}
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuDiv);
		return;
		break;
	}
	recTestLogic((X86RegisterType)dReg);
	if (yIsTemp == true)
	{
		FlushLiveRegister(GPRStatus.LiveGPRReg[yReg].gprreg, false);
	}
	if (xTemp >= 0)
		FlushLiveRegister(0xfffe, false);

	ToggleLockRegister(EDX, false);
	GPRStatus.LiveGPRReg[dReg].isdirty = true;
} 
void RecCPU::recCpuShift()
{
	//CPU_LOG("SHIFT Recompiling %x from PC %x\n", recOpCode, PC-4);
	int dReg = -1;
	int xReg = -1;
	int yReg = -1;
	bool yIsTemp = false;
	int xTemp = -1;

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
				dReg = GetLiveRegister(Op_X);
				RefChip16Emitter->SHL16ItoR((X86RegisterType)dReg, (recOpCode & 0xf));
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
				dReg = GetLiveRegister(Op_X);
				RefChip16Emitter->SHR16ItoR((X86RegisterType)dReg, (recOpCode & 0xf));
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
				dReg = GetLiveRegister(Op_X);
				RefChip16Emitter->SAR16ItoR((X86RegisterType)dReg, (recOpCode & 0xf));
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
				yReg = GetLiveRegister(0xfffd, true);
				yIsTemp = true;
				RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
			}
			else yReg = GetLiveRegister(Op_Y);

			MoveLiveRegister(ECX, (X86RegisterType)yReg, true);

			yReg = GetLiveRegister(yIsTemp ? 0xfffd : Op_Y);

			if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true) 
			{
				dReg = GetLiveRegister(Op_X, true);
				RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);
			}
			else dReg = GetLiveRegister(Op_X);

			MoveLiveRegister(EAX, (X86RegisterType)dReg, true);

			dReg = GetLiveRegister(Op_X);

			RefChip16Emitter->SHL16CLtoR((X86RegisterType)dReg);
			//RefChip16Emitter->MOV16RtoM((unsigned int)&REG_X, EAX);
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
			if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				yReg = GetLiveRegister(0xfffd, true);
				yIsTemp = true;
				RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
			}
			else yReg = GetLiveRegister(Op_Y);

			MoveLiveRegister(ECX, (X86RegisterType)yReg, true);

			yReg = GetLiveRegister(yIsTemp ? 0xfffd : Op_Y);

			if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{
				dReg = GetLiveRegister(Op_X, true);
				RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);
			}
			else dReg = GetLiveRegister(Op_X);

			MoveLiveRegister(EAX, (X86RegisterType)dReg, true);

			dReg = GetLiveRegister(Op_X);

			RefChip16Emitter->SHR16CLtoR((X86RegisterType)dReg);
			//RefChip16Emitter->MOV16RtoM((unsigned int)&REG_X, EAX);
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
			if (CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
			{
				yReg = GetLiveRegister(0xfffd, true);
				yIsTemp = true;
				RefChip16Emitter->MOV16ItoR((X86RegisterType)yReg, GPRStatus.GPRConstVal[Op_Y]);
			}
			else yReg = GetLiveRegister(Op_Y);

			MoveLiveRegister(ECX, (X86RegisterType)yReg, true);

			yReg = GetLiveRegister(yIsTemp ? 0xfffd : Op_Y);

			if (CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
			{
				dReg = GetLiveRegister(Op_X, true);
				RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, GPRStatus.GPRConstVal[Op_X]);
			}
			else dReg = GetLiveRegister(Op_X);

			MoveLiveRegister(EAX, (X86RegisterType)dReg, true);

			dReg = GetLiveRegister(Op_X);

			RefChip16Emitter->SAR16CLtoR((X86RegisterType)dReg);
			//RefChip16Emitter->MOV16RtoM((unsigned int)&REG_X, EAX);
			GPRStatus.GPRIsConst[Op_X] = false;
		}		
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuShift);
		return;
		break;
	}
	recTestLogic((X86RegisterType)dReg);

	if (xTemp >= 0)
		FlushLiveRegister(0xfffe, false);

	if (yIsTemp == true)
	{
		FlushLiveRegister(GPRStatus.LiveGPRReg[yReg].gprreg, false);
	}

	GPRStatus.LiveGPRReg[dReg].isdirty = true;
}
void RecCPU::recCpuPushPop()
{
	int xReg = -1;
	//CPU_LOG("PUSHPOP Recompiling %x from PC %x\n", recOpCode, PC-4);
	
	switch((recOpCode >> 16) & 0xf)
	{
	//Store Register X on stack SP + 2 (X is actually in the first nibble)
	case 0x0:
		
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true) 
		{
			FlushLiveRegisters(true);
			RefChip16Emitter->MOV16ItoR(EDX, GPRStatus.GPRConstVal[Op_X]);
		}
		else 
		{
			xReg = GetLiveRegister(Op_X);
			MoveLiveRegister(EDX, (X86RegisterType)xReg, true);
			FlushLiveRegisters(true);
		}
		RefChip16Emitter->MOV32MtoR(ECX, (unsigned int)&StackPTR);
		RefChip16Emitter->CALL16((int)recWriteMem);
		RefChip16Emitter->ADD32ItoM((unsigned int)&StackPTR, 2);
		
		break;
	//Decrease Stack Pointer and load value in to Reg X 
	case 0x1:
		FlushLiveRegisters(true);
		RefChip16Emitter->SUB32ItoM((unsigned int)&StackPTR, 2);
		RefChip16Emitter->MOV16MtoR(ECX, (unsigned int)&StackPTR);
		RefChip16Emitter->CALL16((int)recReadMem);
		AssignLiveRegister(Op_X, EAX);
		GPRStatus.GPRIsConst[Op_X] = false;
		break;
	//Store all GPR registers in the stack, increase SP by 32 (16 x 2)
	case 0x2:
		FlushLiveRegisters(true);
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
		FlushLiveRegisters(false);
		FlushConstRegisters(false);

		for(int i = 15; i >= 0; i--)
		{
			RefChip16Emitter->SUB32ItoM((unsigned int)&StackPTR, 2);
			RefChip16Emitter->MOV16MtoR(ECX, (unsigned int)&StackPTR);
			RefChip16Emitter->CALL16((int)recReadMem);
			RefChip16Emitter->MOV16RtoM((unsigned int)&GPR[i], EAX);		
		}
		AssignLiveRegister(0, EAX);
		break;
	//Store flags register on stack, increase SP by 2
	case 0x4:
		FlushLiveRegisters(true);
		RefChip16Emitter->MOV16MtoR(EDX, (unsigned int)&Flag._u16);
		RefChip16Emitter->MOV32MtoR(ECX, (unsigned int)&StackPTR);
		RefChip16Emitter->CALL16((int)recWriteMem);
		RefChip16Emitter->ADD32ItoM((unsigned int)&StackPTR, 2);
		break;
	//Decrease SP by 2, restore flags register
	case 0x5:
		FlushLiveRegisters(true);
		RefChip16Emitter->SUB32ItoM((unsigned int)&StackPTR, 2);
		RefChip16Emitter->MOV16MtoR(ECX, (unsigned int)&StackPTR);
		RefChip16Emitter->CALL16((int)recReadMem);
		RefChip16Emitter->MOV16RtoM((unsigned int)&Flag._u16, EAX);
		
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuPushPop);
		return;
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
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuPallate);
		return;
		break;
	}
}

void RecCPU::recCpuNOTNEG()
{
	int xReg = -1;
	int yReg = -1;
	int xTemp = -1;
	int dReg = -1;
	bool yIsTemp = false;

	RefChip16Emitter->AND16ItoM((unsigned int)&Flag._u16, ~0x84);
	switch((recOpCode >> 16) & 0xf)
	{
	case 0x0:
		if(CONST_PROP)
		{
			int flags = 0;

			GPRStatus.GPRConstVal[Op_X] = ~IMMEDIATE;
			GPRStatus.GPRIsConst[Op_X] = true;

			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			return;
		}
		else 
		{
			dReg = GetLiveRegister(Op_X, true);
		
			RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, ~IMMEDIATE);
		}
		break;
	case 0x1:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{
			int flags = 0;

			GPRStatus.GPRConstVal[Op_X] = ~GPRStatus.GPRConstVal[Op_X];
				
			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			return;
		}
		else 
		{
			dReg = GetLiveRegister(Op_X);		
			RefChip16Emitter->NOT16R((X86RegisterType)dReg);
		}
		break;
	case 0x2:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flags = 0;

			GPRStatus.GPRConstVal[Op_X] = ~GPRStatus.GPRConstVal[Op_Y];
			GPRStatus.GPRIsConst[Op_X] = true;

			FlushLiveRegister(Op_X, false);

			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			return;
		}
		else 
		{
			dReg = GetLiveRegister(Op_Y);
			if (Op_X != Op_Y)
			{
				FlushLiveRegister(dReg, true);
				AssignLiveRegister((X86RegisterType)dReg, Op_X);
			}
			RefChip16Emitter->NOT16R((X86RegisterType)dReg);
			
			GPRStatus.GPRIsConst[Op_X] = false;
		}
		break;
	case 0x3:
		if(CONST_PROP)
		{
			int flags = 0;

			GPRStatus.GPRConstVal[Op_X] = -IMMEDIATE;
			GPRStatus.GPRIsConst[Op_X] = true;
			FlushLiveRegister(Op_X, false);

			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			return;
		}
		else 
		{
			dReg = GetLiveRegister(Op_X);
			RefChip16Emitter->MOV16ItoR((X86RegisterType)dReg, -IMMEDIATE);
		}
		break;
	case 0x4:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_X] == true)
		{
			int flags = 0;

			GPRStatus.GPRConstVal[Op_X] = -GPRStatus.GPRConstVal[Op_X];
				
			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			return;
		}
		else 
		{
			dReg = GetLiveRegister(Op_X);
			RefChip16Emitter->NEG16R((X86RegisterType)dReg);
		}
		break;
	case 0x5:
		if(CONST_PROP && GPRStatus.GPRIsConst[Op_Y] == true)
		{
			int flags = 0;

			GPRStatus.GPRConstVal[Op_X] = -GPRStatus.GPRConstVal[Op_Y];
			GPRStatus.GPRIsConst[Op_X] = true;
			if(GPRStatus.GPRConstVal[Op_X] == 0) flags |= 0x4;
			else if(GPRStatus.GPRConstVal[Op_X] & 0x8000) flags |= 0x80;

			FlushLiveRegister(Op_X, false);

			RefChip16Emitter->OR16ItoM((unsigned int)&Flag._u16, flags);
			return;
		}
		else 
		{			
			yReg = GetLiveRegister(Op_Y);
			dReg = GetLiveRegister(Op_X);

			if (Op_X != Op_Y)
				RefChip16Emitter->MOV16RtoR((X86RegisterType)dReg, (X86RegisterType)yReg);

			RefChip16Emitter->NEG16R((X86RegisterType)dReg);
			GPRStatus.GPRIsConst[Op_X] = false;
		}
		break;
	default:
		FlushLiveRegisters(true);
		FlushConstRegisters(true);
		RefChip16Emitter->MOV32ItoM((unsigned int)&OpCode, recOpCode);
		RefChip16Emitter->MOV32ItoM((unsigned int)&PC, PC);
		RefChip16Emitter->CALL(CpuNOTNEG);
		return;
		break;
	}
	recTestLogic((X86RegisterType)dReg);
	if (yIsTemp == true)
	{
		FlushLiveRegister(GPRStatus.LiveGPRReg[yReg].gprreg, false);
	}
	if (xTemp >= 0)
		FlushLiveRegister(0xfffe, false);

	ToggleLockRegister(EDX, false);
	GPRStatus.LiveGPRReg[dReg].isdirty = true;
}