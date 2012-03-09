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

// include the basic windows header files and the Direct3D header file

#include "CPU.h"
#include "D3DApp.h"
#include "Sound.h"
#include "Emitter.h"

extern SoundDevice *RefChip16Sound;
extern Emitter *RefChip16Emitter;
extern unsigned char ScreenBuffer[320][240];
extern Sprite SpriteSet;
extern unsigned long pixelcolours[16];

unsigned char* pixels; //Pointer for pixel colours

using namespace CPU;

#define REG_Z		GPR[OpCode & 0xf]
#define REG_X		GPR[((OpCode >> 24) & 0xf)]
#define REG_Y		GPR[((OpCode >> 28) & 0xf)]
#define Op_X		((OpCode >> 24) & 0xf)
#define Op_Y		((OpCode >> 28) & 0xf)
#define Op_Z		(OpCode & 0xf)
#define IMMEDIATE   ((short)(OpCode & 0xFFFF))
#define CPU_LOG __Log
#define FPS_LOG __Log2
//#define LOGGINGENABLED

extern int cpubranch;
namespace CPU
{

unsigned short PC;
unsigned long OpCode;
unsigned short VBlank;
FlagRegister Flag;
unsigned long cycles;
unsigned long nextvsync;
bool Running = false;
char Recompiler = 1;
bool drawing = false;
FILE * LogFile; 
unsigned short StackPTR;
unsigned short GPR[16];
unsigned char Memory[64*1024];
unsigned char ROMHeader[16];

unsigned short __fastcall recReadMem(unsigned short memloc){
	return ((Memory[(memloc+1) & 0xffff] << 8) | (Memory[(memloc) & 0xffff]));
}

void __fastcall recWriteMem(unsigned short location, unsigned short value)
{
	//CPU_LOG("rec Writing to %x with value %x", location, value);
	Memory[location & 0xffff] = value & 0xff;
	Memory[(location+1) & 0xffff] = value>>8;
}


unsigned short ReadMem(unsigned long location){
	return((Memory[(location+1) & 0xffff] << 8) | (Memory[(location) & 0xffff]));
}

unsigned short ReadMemReverse(unsigned long location){
	return((Memory[(location) & 0xffff] << 8) | (Memory[(location+1) & 0xffff]));
}

unsigned char ReadMem8(unsigned long location){
	//CPU_LOG("8bit read rom %x with %x\n", location, Memory[location & 0xffff]);
	return Memory[location & 0xffff];
}

void WriteMem(unsigned short location, unsigned short value){
	//CPU_LOG("16bit write to %x with %x", location, value);
	Memory[location & 0xffff] = value & 0xff;
	Memory[(location+1) & 0xffff] = value>>8;
}

void WriteMem8(unsigned short location, unsigned char value){
	//CPU_LOG("8bit write to %x with %x", location, value);
	Memory[location & 0xffff] = value;
}


unsigned short __fastcall GenerateRandom()
{
	return rand();
}
//This function handles the Conditions for the CPU, it is represented by a number, so this is the best way we're gonna do it!

int Condition(int Cond)
{
	//CPU_LOG("Flags z=%x n=%x c=%x o=%x\n", Flag.Zero, Flag.Negative, Flag.CarryBorrow, Flag.Overflow);
	switch(Cond)
	{
		case 0x0: case 0x1:  // [z==1] & [z==0] Equal
			//CPU_LOG("Zero cond %x\n", (Cond+1) & 0x1);
			if(Flag.Zero != (Cond & 0x1)) 
				return 1;
			break;
		case 0x2: case 0x3:  // [n==1] & [n==0] Negative
			//CPU_LOG("Negative cond %x\n", (Cond+1) & 0x1);
			if(Flag.Negative != (Cond & 0x1))
				return 1;
			break;
		case 0x4:			// [n==0 && z==0] Positive (Not zero or negative)
			//CPU_LOG("Negative & Zero are 0\n");
			if(Flag.Negative == 0 && Flag.Zero == 0) 
				return 1;
			break;
		case 0x5: case 0x6: // [o==1] & [o==0] Overflow
			//CPU_LOG("Overflow\n");
			if(Flag.Overflow == (Cond & 0x1))
				return 1;
			break;
		case 0x7:			// [c==0 && z==0] Above Zero (Unsigned Greater Than)	
			//CPU_LOG("Carry and Zero 0\n");
			if(Flag.CarryBorrow == 0 && Flag.Zero == 0)
				return 1;
			break;
		case 0x8: case 0x9:	// [c==0] & [c==1] Less Than Zero
			//CPU_LOG("Carry cond %x\n", (Cond) & 0x1);
			if(Flag.CarryBorrow == (Cond & 0x1))
				return 1;
			break;
		case 0xA:			// [c==1 || z==1] Less Than or Equal to Zero
			//CPU_LOG("Carry or Zero set\n");
			if(Flag.CarryBorrow == 1 || Flag.Zero == 1)
				return 1;
			break;
		case 0xB:			// [o==n && z==0] Signed Greater than Zero
			//CPU_LOG("signed greater (O=N and Z = 0)\n");
			if(Flag.Overflow == Flag.Negative && Flag.Zero == 0)
				return 1;
			break;
		case 0xC:			// [o==n] Signed Greater than Zero or Equal
			//CPU_LOG("Signed Greater or Zero (O = N)\n");
			if(Flag.Overflow == Flag.Negative)
				return 1;
			break;
		case 0xD:			// [o!=n] Signed Less than Zero
			//CPU_LOG("Signed Less Than (O != N)\n");
			if(Flag.Overflow != Flag.Negative)
				return 1;
			break;
		case 0xE:			// [o!=n || z==1] Signed Less than Zero or Equal
			//CPU_LOG("Signed Less Than or Equal to Zero (O!=N or Z= 1)\n");
			if(Flag.Overflow != Flag.Negative || Flag.Zero == 1)
				return 1;
			break;
		default:
			//OxF is reserved
			break;
	}
	return 0;
}

void CpuCore()
{
	int MemAddr;
	int X, Y;	
	unsigned short randval;
	//CPU_LOG("Core OP %x\n", (OpCode >> 16 & 0xf));
	switch((OpCode >> 16 & 0xf))
	{
	case 0x0: //NOP
		//CPU_LOG("Nop\n");
		break;
	case 0x1: //CLS
		//CPU_LOG("Clear Screen\n");
		
	 // Clear back buffer and depth buffer
		
		memset(ScreenBuffer, 0, sizeof(ScreenBuffer));

		SpriteSet.BackgroundColour = 0;
		ClearRenderTarget();
		//RedrawLastScreen();
		nextvsync = cycles + (1000000 / 60);
		break;
	case 0x2: //Wait for VBLANK
		//CPU_LOG("Waiting for VBlank\n");
		if(!VBlank) //PC -= 4;
		{
			cycles = nextvsync;
		}
		break;
	case 0x3: //Background Colour
		//CPU_LOG("Set BG colour to %x\n", OpCode & 0xf);
		if(SpriteSet.BackgroundColour == (OpCode & 0xf)) 
		{
			//Redraw is really slow and inefficient (fixed pipe crap :P) so we 
			//need to avoid it whenever possible. (Pacman for checking)
			//FPS_LOG("Skipping Background Colour Change");
			return;
		}
		SpriteSet.BackgroundColour = OpCode & 0xf;

		ClearRenderTarget();
		StartDrawing();
		RedrawLastScreen();
		
		break;
	case 0x4: // Set Sprite H/W
		SpriteSet.Height = (OpCode >> 8) & 0xFF;
		SpriteSet.Width =  (OpCode & 0xFF) * 2;
		//CPU_LOG("Set Sprite H = %x W = %x\n", SpriteSet.Height, SpriteSet.Width);
		break;
	case 0x5: //Draw Sprite from Mem addr
		X = REG_X;
		Y = REG_Y;
		MemAddr = IMMEDIATE;
		StartDrawing();
		//CPU_LOG("Draw Sprite at Cords X = %d Y = %d, Mem = %x\n", X, Y, MemAddr);
		DrawSprite(MemAddr, X, Y);
		break;
	case 0x6: //Draw Sprite from Register addr
		X = REG_X;
		Y = REG_Y;
		MemAddr = REG_Z;
		StartDrawing();
		//CPU_LOG("Draw Sprite at Cords from reg %x X = %d Y = %d, Mem = %x\n", (OpCode >> 8) & 0xf, X, Y, MemAddr);
		DrawSprite(MemAddr, X, Y);
		break;
	case 0x7: //Random Number
		randval = rand() % ((unsigned short)IMMEDIATE+1); //Apparently if IMMEDIATE is 1, it always generates 0 O_o
		//Need to make sure result isnt 0 and is in range.
		if(randval > IMMEDIATE) randval -= 1;
		REG_X = randval;
		//CPU_LOG("Random  number generated %x max %x\n", REG_X, IMMEDIATE);
		break;
	case 0x8: //FLIP Sprite Orientation
		//Do Nothing for the minute!
		//CPU_LOG("Flip V = %s H = %s totalcode %x\n", (OpCode >> 8) & 0x1 ? "true" : "false", (OpCode >> 8) & 0x2 ? "true" : "false", (OpCode >> 8) & 0xf );
		SpriteSet.VerticalFlip = ((OpCode >> 8) & 0x1) ? true : false;
		SpriteSet.HorizontalFlip = ((OpCode >> 8) & 0x2) ? true : false;
		break;
	case 0x9: //Stop playing any sounds
		//Do Nothing :D
		RefChip16Sound->StopVoice();
		//CPU_LOG("Not implemented %x\n", PC);
		break;
	case 0xA: //Generate 500hz sound
		//Do Nothing :D
		RefChip16Sound->GenerateHz(500, IMMEDIATE);
		//RefChip16Sound->StopVoice();
		//CPU_LOG("500hz sound for %d milliseconds PC %x\n", IMMEDIATE, PC);
		break;
	case 0xB:  //Generate 1000hz sound
		//Do Nothing :D
		RefChip16Sound->GenerateHz(1000, IMMEDIATE);
		//RefChip16Sound->StopVoice();
		//CPU_LOG("1000hz sound for %d milliseconds PC %x\n", IMMEDIATE, PC);
		break;
	case 0xC: //Generate 1500hz sound
		RefChip16Sound->GenerateHz(1500, IMMEDIATE);
		//RefChip16Sound->StopVoice();
		//CPU_LOG("1500hz sound for %d milliseconds PC %x\n", IMMEDIATE, PC);
		break;
	case 0xD: //Play tone specified in X for IMMEDIATE ms
		CPU_LOG("%dhz sound for %d milliseconds PC %x\n", REG_X, IMMEDIATE, PC);
		RefChip16Sound->GenerateHz(REG_X, IMMEDIATE);
		break;
	case 0xE: //Set ADSR
		RefChip16Sound->SetADSR(Op_Y, Op_X, ((OpCode >> 4) & 0xf), Op_Z, ((OpCode >> 12) & 0xf), ((OpCode >> 8) & 0xf));
		//RefChip16Sound->StopVoice();
		//CPU_LOG("1500hz sound for %d milliseconds PC %x\n", IMMEDIATE, PC);
		break;
	default:
		//CPU_LOG("Bad Core Op %x\n", PC);
		break;
	}
}

void CpuJump()
{
	int X, Y;
	//CPU_LOG("Jump Op %x\n", PC);
	switch((OpCode >> 16 & 0xf))
	{
	/*Jump Commands*/
	//Jump
	case 0x0:
		//CPU_LOG("Jump to %x\n", IMMEDIATE);
		PC = IMMEDIATE;
		cpubranch = 2;
		break;
	//Jump if carry raised is true (Obsolete! in here for compat sake)
	case 0x1:
		//CPU_LOG("Obsolete Jump to %x if Carry is true\n", IMMEDIATE);
		if(Condition(0x9))
		{
			PC = IMMEDIATE;
			cpubranch = 2;
		}
		break;
	//Jump if condition X is true
	case 0x2:
		//CPU_LOG("Jump to %x if x is true\n", IMMEDIATE);
		if(Condition(Op_X))
		{
			PC = IMMEDIATE;
			cpubranch = 2;
		}
		break;
	//Jump if X = Y
	case 0x3:
		Y = REG_Y;
		X = REG_X;
		//CPU_LOG("Jump to %x if %x = %x\n", IMMEDIATE, X, Y);
		if(X == Y)
		{
			PC = IMMEDIATE;
			cpubranch = 2;
		}
		break;
	//Jump to addres in X (Indirect jump!)
	case 0x6:
		X = REG_X;
		//CPU_LOG("Jump to %x Indirect\n", X);
		PC = X;
		cpubranch = 2;
		break;
		/*Branch/Call Commands*/
	//Call Subroutine at Address
	case 0x4:
		//CPU_LOG("Subroutine %x\n", IMMEDIATE);
		WriteMem(StackPTR, PC);
		StackPTR += 2;
		PC = IMMEDIATE;
		cpubranch = 2;
		break;
	//Return from subroutine (POP PC from stack)
	case 0x5:
		//CPU_LOG("Return from sub\n");
		StackPTR -= 2;
		PC = ReadMem(StackPTR);
		cpubranch = 2;
		break;
	//Call Subroutine if condition x is true
	case 0x7:
		//CPU_LOG("Subroutine %x if x is true\n", IMMEDIATE);
		if(Condition(Op_X))
		{
			WriteMem(StackPTR, PC);
			StackPTR += 2;
			PC = IMMEDIATE;
			cpubranch = 2;
		}
		break;
	//Call Subroutine in Regsiter X
	case 0x8:
		X = REG_X;
		//CPU_LOG("Subroutine %x from X reg\n", X);
		WriteMem(StackPTR, PC);
		StackPTR += 2;
		PC = X;
		cpubranch = 2;
		break;
	default:
		//CPU_LOG("Bad Jump Op %x\n", PC);
		break;
	}
}
 
void CpuLoad()
{
	////CPU_LOG("Load Op %x\n", PC);
	switch((OpCode >> 16 & 0xf))
	{
	//Copy Immediate to GPR X
	case 0x0:
		//CPU_LOG("Load Imm to X(%x), imm = %x, PC %x\n", Op_X, IMMEDIATE, PC);
		REG_X = IMMEDIATE;		
		break;
	//Point Stack  Pointer to Address
	case 0x1:
		//CPU_LOG("Load Imm to StackPTR, imm = %x, PC %x\n", IMMEDIATE, PC);
		StackPTR = IMMEDIATE;
		break;
	//Load Register with value at imm address
	case 0x2:
		//CPU_LOG("Load imm addr to X(%x), immaddr data = %x, PC %x\n", Op_X, ReadMem(IMMEDIATE), PC);
		REG_X = ReadMem(IMMEDIATE);
		break;
	//Load X with value from memory using address in Y
	case 0x3:
		//CPU_LOG("Load Y(%x) addr to X(%x), Yaddr = %x, Yaddr data = %x, PC %x OpCode %x\n", Op_Y, Op_X, REG_Y, ReadMem(REG_Y), PC, OpCode);
		REG_X = ReadMem(REG_Y);
		break;
	//Load Y in to X
	case 0x4:
		//CPU_LOG("Load Y(%x) to X(%x), Y = %x, PC %x\n", Op_X, Op_Y, REG_Y, PC);
		REG_X = REG_Y;
		break;
	default:
		//CPU_LOG("Bad Load Op %x\n", PC);
		break;
	}

	
}
 
void CpuStore()
{
	
	switch((OpCode >> 16 & 0xf))
	{
	//Store X Register value in imm Address 
	case 0x0:
		//CPU_LOG("Store X in mem imm address, X = %x, imm addr = %x, PC %x\n", REG_X, IMMEDIATE, PC);
		WriteMem(IMMEDIATE, REG_X);
		break;
	//Store X Regsiter value in address given by Y Register
	case 0x1:
		//CPU_LOG("Store X(%x) in mem Y(%x) address, X = %x, Y addr = %x, PC %x\n", Op_X, Op_Y, REG_X, REG_Y, PC);
		WriteMem(GPR[((OpCode >> 28) & 0xf)], REG_X);
		break;
	default:
		//CPU_LOG("Bad Store Op %x\n", PC);
		break;
	}
}

int AddSetFlags(int CalcResult, int signcalc)
{
	if(CalcResult > 0xFFFF) 
	{
			Flag.CarryBorrow = 1;
	}
	else Flag.CarryBorrow = 0;

	CalcResult &= 0xffff;

	if(CalcResult == 0) Flag.Zero = 1;
	else Flag.Zero = 0;

	Flag.Overflow = 0;
	if(signcalc) 
	{
		if(((REG_X & 0x8000) && !(CalcResult & 0x8000)) || (!(REG_X & 0x8000) && (CalcResult & 0x8000))) Flag.Overflow = 1;
	}

	if(CalcResult & 0x8000) Flag.Negative = 1;
	else Flag.Negative = 0;

	return (signed short)CalcResult;
}

void CpuAdd()
{
	
	switch((OpCode >> 16 & 0xf))
	{
	//X = X + Imm [c,z,o,n]
	case 0x0:		
		//CPU_LOG("Add X = X(%x) + imm, X = %x, imm = %x, Result = %x, Op %x\n", Op_X, REG_X, IMMEDIATE, REG_X + IMMEDIATE, PC);
		REG_X = AddSetFlags(REG_X + IMMEDIATE, ((REG_X & 0x8000) && (IMMEDIATE & 0x8000)) || (!(REG_X & 0x8000) && !(IMMEDIATE & 0x8000)));
		break;
	//X = X + Y [c,z,o,n]
	case 0x1:
		//CPU_LOG("Add X = X + Y, X = %x, Y = %x, Result = %x, Op %x\n", REG_X, REG_Y, REG_X + REG_Y, PC);
		REG_X = AddSetFlags(REG_X + REG_Y, ((REG_X & 0x8000) && (REG_Y & 0x8000)) || (!(REG_X & 0x8000) && !(REG_Y & 0x8000)));
		break;
	//Z = X + Y [c,z,o,n]
	case 0x2:
		//CPU_LOG("Add Z = X + Y, X = %x, imm = %x, Result = %x, Op %x\n", REG_X, REG_Y, REG_X + REG_Y, PC);
		REG_Z = AddSetFlags(REG_X + REG_Y, ((REG_X & 0x8000) && (REG_Y & 0x8000)) || (!(REG_X & 0x8000) && !(REG_Y & 0x8000)));
		break;
	default:
		//CPU_LOG("Bad Add Op %x\n", PC);
		break;
	}
}

int SubSetFlags(unsigned short Val1, unsigned short Val2)
{

	unsigned short CalcResult = (Val1 - Val2);
	//Calculate Borrow
	if(Val1 < Val2) Flag.CarryBorrow = 1;
	else Flag.CarryBorrow = 0;

	
	//Calculate Overflow Flag
	if(((Val1 & 0x8000) == 0x8000 && (Val2 & 0x8000) == 0 && (CalcResult & 0x8000) == 0) 
		|| ((Val1 & 0x8000) == 0 && (Val2 & 0x8000) == 0x8000 && ( CalcResult & 0x8000) == 0x8000)) 
	{
			Flag.Overflow = 1;
	}
	else 
	{
			Flag.Overflow = 0;	
	}

	//Calculate if zero result
	if(CalcResult == 0) 
	{
			Flag.Zero = 1;
			Flag.Negative = 0;
			return (signed short)CalcResult; 
	}
	else Flag.Zero = 0;

	//Calculate if result is negative
	if(CalcResult & 0x8000) Flag.Negative = 1;
	else Flag.Negative = 0;

	return (signed short)CalcResult;
}
 
void CpuSub()
{
	
	switch((OpCode >> 16 & 0xf))
	{
	//X = X - imm [c,z,o,n]
	case 0x0:
		//CPU_LOG("Sub Op X = X - imm, X = %x, imm = %x, Result = %x, PC %x\n", REG_X, IMMEDIATE, REG_X - IMMEDIATE,  PC);
		REG_X = SubSetFlags(REG_X, IMMEDIATE);
		break;
	//X = X - Y [c,z,o,n]
	case 0x1:
		//CPU_LOG("Sub Op X = X - Y, X = %x, Y = %x, Result = %x, PC %x\n", REG_X, REG_Y, REG_X - REG_Y,  PC);
		REG_X = SubSetFlags(REG_X, REG_Y);
		break;
	//Z = X - Y [c,z,o,n]
	case 0x2:
		//CPU_LOG("Sub Op Z = X - Y, X = %x, Y = %x, Result = %x, PC %x\n", REG_X, REG_Y, REG_X - REG_Y,  PC);
		REG_Z = SubSetFlags(REG_X, REG_Y);
		break;
	//X - imm no store just flags [c,z,o,n]
	case 0x3:
		//CPU_LOG("Sub Op X - imm just flags, X = %x, imm = %x, Result = %x, PC %x\n", REG_X, IMMEDIATE, REG_X - IMMEDIATE,  PC);
		SubSetFlags(REG_X, IMMEDIATE);
		break;
	//X - Y no store just flags [c,z,o,n]
	case 0x4:
		//CPU_LOG("Sub Op X - Y just flags, X = %x, Y = %x, Result = %x, PC %x\n", REG_X, REG_Y, REG_X - REG_Y,  PC);
		SubSetFlags(REG_X, REG_Y);
		break;
	default:
		//CPU_LOG("Bad Sub Op %x\n", PC);
		break;
	}
}

void LogicCMP(unsigned short Result)
{
	if(Result == 0) 
	{
			Flag.Negative = 0;
			Flag.Zero = 1;
	}
	else 
	{
			Flag.Zero = 0;
			if(Result & 0x8000) Flag.Negative = 1;
			else Flag.Negative = 0;
	}
}

void CpuAND()
{
	
	switch((OpCode >> 16 & 0xf))
	{
	//X = X & imm [z,n]
	case 0x0:
		//CPU_LOG("AND X = X & IMM, X = %x, Imm = %x, Result = %x PC %x OpCode %x\n", REG_X, IMMEDIATE, REG_X & IMMEDIATE, PC, OpCode);
		REG_X = REG_X & IMMEDIATE;
		LogicCMP(REG_X);
		break;
	//X = X & Y [z,n]
	case 0x1:
		//CPU_LOG("AND X = X(%x) & Y(%x), X = %x, Y = %x, Result = %x PC %x\n", Op_X, Op_Y, REG_X, REG_Y, REG_X & REG_Y, PC);
		REG_X = REG_X & REG_Y;
		LogicCMP(REG_X);
		break;
	//Z = X & Y [z,n]
	case 0x2:
		//CPU_LOG("AND Z = X & Y, X = %x, Y = %x, Result = %x PC %x\n", REG_X, IMMEDIATE, REG_X & REG_Y, PC);
		REG_Z = REG_X & REG_Y;
		LogicCMP(REG_Z);
		break;
	//X & imm discard flags only [z,n]
	case 0x3:
		//CPU_LOG("AND X & IMM flags only, X = %x, Imm = %x, Result = %x PC %x\n", REG_X, IMMEDIATE, REG_X & IMMEDIATE, PC);
		LogicCMP(REG_X & IMMEDIATE);
		break;
	//X & Y discard flags only [z,n]
	case 0x4:
		//CPU_LOG("AND X & Y flags only, X = %x, Y = %x, Result = %x PC %x\n", REG_X, IMMEDIATE, REG_X & REG_Y, PC);
		LogicCMP(REG_X & REG_Y);
		break;
	default:
		//CPU_LOG("Bad AND Op %x\n", PC);
		break;
	}
}
 
void CpuOR()
{
	switch((OpCode >> 16 & 0xf))
	{
	//X = X | imm [z,n]
	case 0x0:
		//CPU_LOG("OR X = X(%x) | IMM, X = %x, Imm = %x, Result = %x PC %x\n", Op_X,  REG_X, IMMEDIATE, REG_X | IMMEDIATE, PC);
		REG_X = REG_X | IMMEDIATE;
		LogicCMP(REG_X);
		break;
	//X = X | Y [z,n]
	case 0x1:
		//CPU_LOG("OR X = X(%x) | Y(%x), X = %x, Y = %x, Result = %x PC %x\n", Op_X, Op_Y, REG_X, REG_Y, REG_X | REG_Y, PC);
		REG_X = REG_X | REG_Y;
		LogicCMP(REG_X);
		break;
	//Z = X | Y [z,n]
	case 0x2:
		//CPU_LOG("OR Z = X | Y, X = %x, Y = %x, Result = %x PC %x\n", REG_X, REG_Y, REG_X | REG_Y, PC);
		REG_Z = REG_X | REG_Y;
		LogicCMP(REG_Z);
		break;
	default:
		//CPU_LOG("Bad OR Op %x\n", PC);
		break;
	}
}
  
void CpuXOR()
{
	//CPU_LOG("XOR Op %x\n", PC);
	switch((OpCode >> 16 & 0xf))
	{
	//X = X ^ imm [z,n]
	case 0x0:
		//CPU_LOG("XOR X = X | IMM, X = %x, Imm = %x, Result = %x PC %x\n", REG_X, IMMEDIATE, REG_X ^ IMMEDIATE, PC);
		REG_X = REG_X ^ IMMEDIATE;
		LogicCMP(REG_X);
		break;
	//X = X ^ Y [z,n]
	case 0x1:
		//CPU_LOG("XOR X = X | IMM, X = %x, Y = %x, Result = %x PC %x\n", REG_X, REG_Y, REG_X ^ REG_Y, PC);
		REG_X = REG_X ^ REG_Y;
		LogicCMP(REG_X);
		break;
	//Z = X ^ Y [z,n]
	case 0x2:
		//CPU_LOG("XOR Z = X | IMM, X = %x, Y = %x, Result = %x PC %x\n", REG_X, REG_Y, REG_X ^ REG_Y, PC);
		REG_Z = REG_X ^ REG_Y;
		LogicCMP(REG_Z);
		break;
	default:
		//CPU_LOG("Bad XOR Op %x\n", PC);
		break;
	}
}

int MulSetFlags(int CalcResult)
{

	//Calculate Borrow
	if(CalcResult > 0xFFFF) Flag.CarryBorrow = 1;
	else Flag.CarryBorrow = 0;
	
	//Calculate if zero result
	if(CalcResult == 0) Flag.Zero = 1;
	else Flag.Zero = 0;

	//Calculate if result is negative
	if(CalcResult & 0x8000) Flag.Negative = 1;
	else Flag.Negative = 0;

	return (signed short)CalcResult;
}

void CpuMul()
{
	
	switch((OpCode >> 16 & 0xf))
	{
	//X = X * imm [c,z,n]
	case 0x0:
		//CPU_LOG("MUL X = X * Imm, X = %x, Imm = %x, result = %x, PC %x\n", REG_X, IMMEDIATE, REG_X * IMMEDIATE, PC);
		REG_X = MulSetFlags(REG_X * IMMEDIATE);
		break;
	//X = X * Y [c,z,n]
	case 0x1:
		//CPU_LOG("MUL X = X * Y, X = %x, Y = %x, result = %x, PC %x\n", REG_X, REG_Y, REG_X * REG_Y, PC);
		REG_X = MulSetFlags(REG_X * REG_Y);
		break;
	//Z = X * Y [c,z,n]
	case 0x2:
		//CPU_LOG("MUL Z = X * Y, X = %x, Y = %x, result = %x, PC %x\n", REG_X, REG_Y, REG_X * REG_Y, PC);
		REG_Z = MulSetFlags(REG_X * REG_Y);
		break;
	default:
		//CPU_LOG("Bad MUL Op %x\n", PC);
		break;
	}
}

int DivSetFlags(unsigned short CalcResult, int Remainder)
{

	//Calculate Borrow
	if(Remainder != 0) Flag.CarryBorrow = 1;
	else Flag.CarryBorrow = 0;
	
	//Calculate if zero result
	if(CalcResult == 0) Flag.Zero = 1;
	else Flag.Zero = 0;

	//Calculate if result is negative
	if(CalcResult & 0x8000) Flag.Negative = 1;
	else Flag.Negative = 0;

	return (signed short)CalcResult;
}
 
void CpuDiv()
{
	//CPU_LOG("DIV Op %x\n", PC);
	switch((OpCode >> 16 & 0xf))
	{
	//X = X / imm [c,z,n]
	case 0x0:
		//CPU_LOG("DIV X = X / Imm, X = %x, Imm = %x, result = %x, PC %x\n", REG_X, IMMEDIATE, REG_X / IMMEDIATE, PC);
		REG_X = DivSetFlags(REG_X / IMMEDIATE, REG_X % IMMEDIATE);
		break;
	//X = X / Y [c,z,n]
	case 0x1:
		//CPU_LOG("DIV X = X / Y, X = %x, Y = %x, result = %x, PC %x\n", REG_X, REG_Y, REG_X / REG_Y, PC);
		REG_X = DivSetFlags(REG_X / REG_Y, REG_X % REG_Y);
		break;
	//Z = X / Y [c,z,n]
	case 0x2:
		//CPU_LOG("DIV Z = X / Y, X = %x, Y = %x, result = %x, PC %x\n", REG_X, REG_Y, REG_X / REG_Y, PC);
		REG_Z = DivSetFlags(REG_X / REG_Y, REG_X % REG_Y);
		break;
	default:
		//CPU_LOG("Bad DIV Op %x\n", PC);
		break;
	}
}

void CpuShift()
{
	
	
	switch((OpCode >> 16 & 0xf))
	{
	//SHL & SAL [z,n]
	case 0x0:
		//CPU_LOG("SHL & SAL, REG_X(%x) << N, REG_X = %x, N = %x, Result = %x, Op %x\n", Op_X, REG_X, OpCode & 0xf, REG_X << (OpCode & 0xf), OpCode);
		REG_X = REG_X << (OpCode & 0xf);
		break;
	//SHR [z,n]
	case 0x1:
		//CPU_LOG("SHR, REG_X >> N, REG_X = %x, N = %x, Result = %x, Op %x\n", REG_X, OpCode & 0xf, REG_X >> (OpCode & 0xf), OpCode);
		REG_X = REG_X >> (OpCode & 0xf);
		break;
	//SAR (repeat sign) [z,n]
	case 0x2:
		//CPU_LOG("SAR (repeat sign), REG_X >> N, REG_X = %x, N = %x, Result = %x, Op %x\n", REG_X, OpCode & 0xf, REG_X >> (OpCode & 0xf), OpCode);
		REG_X = (short)REG_X >> (OpCode & 0xf);
		break;
	//SHL & SAL Y Reg [z,n]
	case 0x3:
		//CPU_LOG("SHL/SAL, REG_X << REG_Y & 0xF, REG_X = %x, REG_Y = %x, Result = %x, Op %x\n", REG_X, REG_Y & 0xF, REG_X << (REG_Y & 0xf), OpCode);
		REG_X = REG_X << (REG_Y & 0xF);
		break;
	//SHR Y Reg [z,n]
	case 0x4:
		//CPU_LOG("SHR, REG_X >> REG_Y & 0xF, REG_X = %x, REG_Y = %x, Result = %x, Op %x\n", REG_X, REG_Y & 0xF, REG_X >> (REG_Y & 0xf), OpCode);
		REG_X = REG_X >> (REG_Y & 0xF);

		break;
	//SAR Y Reg [z,n]
	case 0x5:
		//CPU_LOG("SAR, REG_X >> REG_Y & 0xF, REG_X = %x, REG_Y = %x, Result = %x, Op %x\n", REG_X, REG_Y & 0xF, REG_X >> (REG_Y & 0xf), OpCode);
		REG_X = (short)REG_X >> (REG_Y & 0xF);
		break;
	default:
		//CPU_LOG("Bad Shift Op %x\n", PC);
		break;
	}
	if(REG_X & 0x8000) Flag.Negative = 1;
	else  Flag.Negative = 0;
	if(REG_X == 0) Flag.Zero = 1;
	else  Flag.Zero = 0; 
}

void CpuPushPop()
{
	
	switch((OpCode >> 16 & 0xf))
	{
	//Store Register X on stack SP + 2 (X is actually in the first nibble)
	case 0x0:
		//CPU_LOG("Store Register X(%x) value %x on stack PC = %x\n", (OpCode >> 16) & 0xff, REG_X, PC);
		WriteMem(StackPTR, REG_X);
		StackPTR += 2;
		break;
	//Decrease Stack Pointer and load value in to Reg X (X is actually in the first nibble)
	case 0x1:
		StackPTR -= 2;
		//CPU_LOG("Store Register X(%x) value %x from stack PC = %x\n", (OpCode >> 16) & 0xff, ReadMem(StackPTR), PC);
		REG_X = ReadMem(StackPTR);
		break;
	//Store all GPR registers in the stack, increase SP by 32 (16 x 2)
	case 0x2:
		//CPU_LOG("Store All Registers on stack PC = %x\n", PC);
		for(int i = 0; i < 16; i++)
		{
			WriteMem(StackPTR, GPR[i]);
			StackPTR += 2;
		}
		break;
	//Decrease SP by 32 and POP all GPR registers
	case 0x3:
		//CPU_LOG("Restore All Registers on stack PC = %x\n", PC);
		for(int i = 15; i >= 0; i--)
		{
			StackPTR -= 2;
			GPR[i] = ReadMem(StackPTR);			
		}
		break;
	//Store flags register on stack, increase SP by 2
	case 0x4:
		//CPU_LOG("Store Flags (%x) on stack PC = %x\n", Flag._u16, PC);
		WriteMem(StackPTR, Flag._u16);
		StackPTR += 2;
		break;
	//Decrease SP by 2, restore flags register
	case 0x5:
		StackPTR -= 2;
		//CPU_LOG("Restore Flags (%x) from stack PC = %x\n", ReadMem(StackPTR), PC);
		Flag._u16 = ReadMem(StackPTR);
		break;
	default:
		//CPU_LOG("Bad PUSHPOP Op %x\n", PC);
		break;
	}
}


void CpuPallate()
{
	int j = 0;
	//CPU_LOG("PAL %x IMM %x\n", (OpCode >> 16 & 0xf), IMMEDIATE);
	switch((OpCode >> 16 & 0xf))
	{
		//PAL HHLL - Load the palette starting at address HHLL, 16*3 bytes, RGB format; used for all drawing since last vblank.
		case 0x0: 
			
			pixels = (unsigned char*)pixelcolours;
			
			for(int i = 0; j < 48; i+=4)
			{
				pixels[i] = ReadMem8(IMMEDIATE + j++);	
				pixels[i+1] = ReadMem8(IMMEDIATE + j++);	
				pixels[i+2] = ReadMem8(IMMEDIATE + j++);
				if(i > 0) pixels[i+3] = 0xFF;
				else pixels[i+3] = 0x0;
				
			}
			GenerateVertexList();
			RedrawLastScreen();
			
		break;
		//PAL Rx - Load the palette starting at the address pointed to by Register X, 16*3 bytes, RGB format; used for all drawing since last vblank.
		case 0x1:

			pixels = (unsigned char*)pixelcolours;

			for(int i = 0; j < 48; i+=4)
			{
				pixels[i] = ReadMem8(REG_X + j++);	
				pixels[i+1] = ReadMem8(REG_X + j++);	
				pixels[i+2] = ReadMem8(REG_X + j++);
				if(i > 0) pixels[i+3] = 0xFF;
				else pixels[i+3] = 0x0;
				
			}
			GenerateVertexList();
			RedrawLastScreen();					
		break;
	}
}
void OpenLog()
{
	
	fopen_s(&LogFile, ".\\c16Log.txt","w"); 
//	setbuf( LogFile, NULL );

}

void Reset()
{
	//OpenLog();
	Flag._u16 = 0;
	PC = 0;
	StackPTR = 0xFDF0;
	cycles = 0;
	nextvsync = (1000000 / 60);
	//Running = false;
	for(int i = 0; i < 16; i++) 
		GPR[i] = 0;
	memset(Memory, 0, sizeof(Memory));

	D3DReset();
	ClearRenderTarget();
	//StartDrawing();
	RefChip16Sound->StopVoice();
	RefChip16Sound->SetADSR(0, 0, 15, 0, 15, TRIANGLE);
	
}

void FetchOpCode()
{
	
	OpCode = ReadMem(PC + 2) | (ReadMem(PC) << 16);
	PC+=4;
	////CPU_LOG("Loading PC %x OpCode %x\n", PC, OpCode);
}

void ExecuteOp()
{
	
	//CPU_LOG("%x OPcode %x\n", PC, (OpCode >> 16 & 0xff));
	switch(OpCode>>20 & 0xf)
	{
		case 0x0: CpuCore(); break;
		case 0x1: CpuJump(); break;
		case 0x2: CpuLoad(); break;
		case 0x3: CpuStore(); break;
		case 0x4: CpuAdd(); break;
		case 0x5: CpuSub(); break;
		case 0x6: CpuAND(); break;
		case 0x7: CpuOR(); break;
		case 0x8: CpuXOR(); break;
		case 0x9: CpuMul(); break;
		case 0xA: CpuDiv(); break;
		case 0xB: CpuShift(); break;
		case 0xC: CpuPushPop(); break;
		case 0xD: CpuPallate(); break;
		default:
			//CPU_LOG("Unknown Op\n");
			break;
	}
//	CALL(CpuCore);
}
void CPULoop()
{

		FetchOpCode();
		ExecuteOp();
}

int LoadRom(const char *Filename){
	FILE * pFile;  //Create File Pointer
	long lSize;   //Need a variable to tell us the size
	
	unsigned int magicnumber;

	fopen_s(&pFile, Filename,"rb");     //Open the file, args r = read, b = binary

	if (pFile!=NULL)  //If the file exists
	{
		Reset();
		fseek ( pFile , 0 , SEEK_END); //point it at the end
		lSize = ftell (pFile) - 16; //Save the filesize
		rewind (pFile); //Start the file at the beginning again
		fread (&ROMHeader,1,16,pFile); //Read in the file
		magicnumber = (ROMHeader[0] + (ROMHeader[1] << 8) + (ROMHeader[2] << 16) + (ROMHeader[3] << 24));

		if(magicnumber != 0x36314843)
		{
			CPU_LOG("Header not on ROM file attempting to load without MagicNumber = %x", magicnumber);
			rewind (pFile); //Start the file at the beginning again
			fread (Memory,1,lSize+16,pFile); //Read in the file
			fclose (pFile); //Close the file
		}
		else
		{
			CPU_LOG("Rom Spec Version %d.%d\n", (ROMHeader[5] >> 4) & 0xF, ROMHeader[5] & 0xF);
			//Check the spec isnt newer than what is currently implemented!
			if(ROMHeader[5] > 0x11) 
			{
				return 2;
			}
			fseek ( pFile , 16 , SEEK_SET); //Point it past the header
			fread (Memory,1,lSize,pFile); //Read in the file
			fclose (pFile); //Close the file
			PC = ROMHeader[0xA] + (ROMHeader[0xB] << 8); //Read off start address.
			//CPU_LOG("PC = %x", PC);
			//RefChip16D3D->StartDrawing();
		}
		return 0;
	} 
	else
	{
		//User cancelled, either way, do nothing.
		return 1;
	}	
}

void __Log(char *fmt, ...) {
#ifdef LOGGINGENABLED
	va_list list;


	if (LogFile == NULL) return;
	
	va_start(list, fmt);
	vfprintf_s(LogFile, fmt, list);
	va_end(list);
#endif
}

void __Log2(char *fmt, ...) {

	va_list list;


	if (LogFile == NULL) return;
	
	va_start(list, fmt);
	vfprintf_s(LogFile, fmt, list);
	va_end(list);
}
}