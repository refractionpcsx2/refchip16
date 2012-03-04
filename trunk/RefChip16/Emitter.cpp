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

#include "RecCPU.h"
#include "Emitter.h"
#include <iostream>
//Based a lot on the old PCSX2 emitter code, it's a little easier to navigate that the current one
//albiet less clever :)

signed char   *x86Ptr;
unsigned char *j8Ptr[32];
unsigned long *j32Ptr[32];
extern RecCPU *RefChip16RecCPU;

Emitter::Emitter()
{
}

Emitter::~Emitter()
{
	x86Ptr = NULL;
}
unsigned char* Emitter::GetPTR()
{
	return x86Ptr;
}

void Emitter::SetPTR(unsigned char* PTR)
{
	x86Ptr = PTR;
}

void Emitter::ModRM(unsigned char mod, unsigned char reg, unsigned char rm)
{
	*(unsigned char*)x86Ptr++ = ((mod << 6) | (reg << 3 ) | (rm));
}

/*Adds*/
void Emitter::ADD32ItoM(unsigned int dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
	ModRM(0, 0, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
	*(unsigned long*)x86Ptr = imm;
	x86Ptr+= 4;

}
void Emitter::ADD16MtoR(X86RegisterType dest, unsigned int src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x03; //The MOV from Mem to Reg opcode
	ModRM(0, dest, DISP32);
	*(unsigned long*)x86Ptr = src;
	x86Ptr+= 4;

}

void Emitter::ADD32MtoR(X86RegisterType dest, unsigned int src)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x03; //The MOV from Mem to Reg opcode
	ModRM(0, dest, DISP32);
	*(unsigned long*)x86Ptr = src;
	x86Ptr+= 4;

}

void Emitter::ADD16ItoR(X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	if( dest == EAX )
	{
		*(unsigned char*)x86Ptr++ = 0x05; //The MOV from Mem to Reg opcode
	}
	else
	{
		*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
		ModRM(3, 0, dest);
	}
	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;

}

void Emitter::ADD32ItoR(X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
	ModRM(3, 0, dest);
	*(unsigned long*)x86Ptr = imm;
	x86Ptr+= 4;

}

void Emitter::ADD16RtoR(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x03; //The MOV from Mem to Reg opcode
	ModRM(3, dest, src);
}

void Emitter::ADD32RtoR(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x03; //The MOV from Mem to Reg opcode
	ModRM(3, dest, src);
}

void Emitter::SUB32ItoM(unsigned int dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
	ModRM(0, 5, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
	*(unsigned long*)x86Ptr = imm;
	x86Ptr+= 4;

}


void Emitter::SUB16ItoR(X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
	ModRM(3, 5, dest);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;
}

void Emitter::SUB16RtoR(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x29; //The MOV from Mem to Reg opcode
	ModRM(3, src, dest);
}

/*Compares*/
void Emitter::CMP16ItoM(unsigned int dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
	ModRM( 0, 7, DISP32 );
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;

}

void Emitter::CMP16ItoR(X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)

	if(dest == EAX)
	{
		*(unsigned char*)x86Ptr++ = 0x3D; 
	}
	else
	{
		*(unsigned char*)x86Ptr++ = 0x81; 
		ModRM( 3, 7, dest );
	}
	
	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;
}

void Emitter::CMP32ItoR(X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)


		*(unsigned char*)x86Ptr++ = 0x81; 
		ModRM( 3, 7, dest );
	
	
	*(unsigned long*)x86Ptr = imm;
	x86Ptr+= 4;
}

void Emitter::CMP16RtoR(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x39; 
	ModRM( 3, src, dest );
}

/*ANDs*/
void Emitter::AND16RtoM(unsigned int dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x21; //The MOV to Mem from Reg opcode
	ModRM(0, src, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
}

void Emitter::AND16RtoR(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x21; //The MOV to Mem from Reg opcode
	ModRM(3, src, dest);
}

void Emitter::AND16MtoR( X86RegisterType dest, unsigned int src ) 
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x23; //The MOV to Mem from Reg opcode
	ModRM(0, dest, DISP32);
	*(unsigned long*)x86Ptr = src;
	x86Ptr+= 4;
}

void Emitter::MUL16MtoEAX(unsigned int src ) 
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xF7; //The MOV to Mem from Reg opcode
	ModRM(0, 4, DISP32);
	*(unsigned long*)x86Ptr = src;
	x86Ptr+= 4;
}

void Emitter::MUL16RtoEAX( X86RegisterType src ) 
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xF7; //The MOV to Mem from Reg opcode
	ModRM(3, 4, src);
}

/*DIVs - Make sure EDX is cleared!!*/
void Emitter::DIV16MtoEAX(unsigned int src ) 
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xF7; //The MOV to Mem from Reg opcode
	ModRM(0, 6, DISP32);
	*(unsigned long*)x86Ptr = src;
	x86Ptr+= 4;
}

void Emitter::DIV16RtoEAX( X86RegisterType src ) 
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xF7; //The MOV to Mem from Reg opcode
	ModRM(3, 6, src);
}

void Emitter::AND16ItoR( X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	if(dest == EAX)
	{
		*(unsigned char*)x86Ptr++ = 0x25; 
	}
	else
	{
		*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
		ModRM(3, 4, dest);
	}

	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;

}

void Emitter::AND32ItoR( X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	if(dest == EAX)
	{
		*(unsigned char*)x86Ptr++ = 0x25; 
	}
	else
	{
		*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
		ModRM(3, 4, dest);
	}

	*(unsigned long*)x86Ptr = imm;
	x86Ptr+= 4;

}


void Emitter::AND16ItoM(unsigned int dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
	ModRM(0, 4, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;

}

/*ORs*/
void Emitter::OR16RtoM(unsigned int dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x09; //The MOV to Mem from Reg opcode
	ModRM(0, src, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
}

void Emitter::OR16RtoR(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x09; //The MOV to Mem from Reg opcode
	ModRM(3, src, dest);
}

void Emitter::OR16MtoR( X86RegisterType dest, unsigned int src ) 
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x0B; //The MOV to Mem from Reg opcode
	ModRM(0, dest, DISP32);
	*(unsigned long*)x86Ptr = src;
	x86Ptr+= 4;
}

void Emitter::OR16ItoR( X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	if(dest == EAX)
	{
		*(unsigned char*)x86Ptr++ = 0x0D; 
	}
	else
	{
		*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
		ModRM(3, 1, dest);
	}

	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;

}

void Emitter::OR16ItoM(unsigned int dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
	ModRM(0, 1, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;

}

/*XORs*/
void Emitter::XOR16RtoM(unsigned int dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x31; //The MOV to Mem from Reg opcode
	ModRM(0, src, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
}

void Emitter::XOR16MtoR( X86RegisterType dest, unsigned int src ) 
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x33; //The MOV to Mem from Reg opcode
	ModRM(0, dest, DISP32);
	*(unsigned long*)x86Ptr = src;
	x86Ptr+= 4;
}

void Emitter::XOR16ItoR( X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	if(dest == EAX)
	{
		*(unsigned char*)x86Ptr++ = 0x35; 
	}
	else
	{
		*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
		ModRM(3, 6, dest);
	}

	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;

}

void Emitter::XOR16ItoM(unsigned int dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x81; //The MOV from Mem to Reg opcode
	ModRM(0, 6, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;

}

void Emitter::XOR16RtoR(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x31; //The MOV from Mem to Reg opcode
	ModRM(3, src, dest);
}

void Emitter::XOR32RtoR(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x31; //The MOV from Mem to Reg opcode
	ModRM(3, src, dest);
}


/*Shifts*/
void Emitter::SHL16ItoR( X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	if(imm == 1)
	{
		*(unsigned char*)x86Ptr++ = 0xD1; 
		*(unsigned char*)x86Ptr++ = 0xE0 + dest; 
	}
	else
	{
		*(unsigned char*)x86Ptr++ = 0xC1; //The MOV from Mem to Reg opcode
		ModRM(3, 4, dest);
		*(unsigned char*)x86Ptr++ = imm;
	}
}

//There is no R to R so we use CL (ECX)
void Emitter::SHL16CLtoR( X86RegisterType dest)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xD3; //The MOV from Mem to Reg opcode
	ModRM(3, 4, dest);
}

void Emitter::SHR16ItoR( X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	if(imm == 1)
	{
		*(unsigned char*)x86Ptr++ = 0xD1; 
		*(unsigned char*)x86Ptr++ = 0xE8 + dest; 
	}
	else
	{
		*(unsigned char*)x86Ptr++ = 0xC1; //The MOV from Mem to Reg opcode
		ModRM(3, 5, dest);
		*(unsigned char*)x86Ptr++ = imm;
	}
}

//There is no R to R so we use CL (ECX)
void Emitter::SHR16CLtoR( X86RegisterType dest)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xD3; //The MOV from Mem to Reg opcode
	ModRM(3, 5, dest);
}

void Emitter::SAR16ItoR( X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	if(imm == 1)
	{
		*(unsigned char*)x86Ptr++ = 0xD1; 
		ModRM(3, 7, dest);
	}
	else
	{
		*(unsigned char*)x86Ptr++ = 0xC1; //The MOV from Mem to Reg opcode
		ModRM(3, 7, dest);
		*(unsigned char*)x86Ptr++ = imm;
	}
}

//There is no R to R so we use CL (ECX)
void Emitter::SAR16CLtoR( X86RegisterType dest)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xD3; //The MOV from Mem to Reg opcode
	ModRM(3, 7, dest);
}
/*Moves*/

void Emitter::MOV32ItoM(unsigned int dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xC7; //The MOV from Mem to Reg opcode
	ModRM(0, EAX, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;
	*(unsigned long*)x86Ptr = imm;
	x86Ptr+= 4;

}
//0x8B 05 src
void Emitter::MOV32MtoR(X86RegisterType dest, unsigned int src)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x8B; //The MOV from Mem to Reg opcode
	ModRM(0, dest, DISP32);
	*(unsigned long*)x86Ptr = src;
	x86Ptr+= 4;

}

void Emitter::MOV32RtoM(unsigned int dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x89; //The MOV to Mem from Reg opcode
	ModRM(0, src, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;

}

void Emitter::MOV16ItoR(X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xB8 + dest; //The MOV from Mem to Reg opcode
	//ModRM(0, 0, dest);

	*(unsigned short*)x86Ptr = imm;
	x86Ptr+= 2;

}
void Emitter::MOV32ItoR(X86RegisterType dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	//*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xB8 + dest; //The MOV from Mem to Reg opcode
	//ModRM(0, 0, dest);

	*(unsigned long*)x86Ptr = imm;
	x86Ptr+= 4;

}

void Emitter::MOV16ItoM(unsigned int dest, int imm)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0xC7; //The MOV from Mem to Reg opcode
	ModRM(0, 0, DISP32);

	*(unsigned long*)x86Ptr = (unsigned long)dest;
	x86Ptr+= 4;
	*(unsigned short*)x86Ptr = (unsigned short)imm;
	x86Ptr+= 2;

}

void Emitter::MOV16RmtoR(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x8B; //The MOV from Mem to Reg opcode
	ModRM(0, dest, src);
}

void Emitter::MOV16RtoRm(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x89; //The MOV from Mem to Reg opcode
	ModRM(0, src, dest);
}

void Emitter::MOV16MtoR(X86RegisterType dest, unsigned int src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x8B; //The MOV from Mem to Reg opcode
	ModRM(0, dest, DISP32);
	*(unsigned long*)x86Ptr = src;
	x86Ptr+= 4;

}

void Emitter::MOV16RtoR(X86RegisterType dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x8B; //The MOV from Mem to Reg opcode
	ModRM(3, dest, src);

}

void Emitter::MOV16RtoM(unsigned int dest, X86RegisterType src)
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x66; //Prefix not needed for native 32bit (0x66 for 16bit)
	*(unsigned char*)x86Ptr++ = 0x89; //The MOV from Mem to Reg opcode
	ModRM(0, src, DISP32);
	*(unsigned long*)x86Ptr = dest;
	x86Ptr+= 4;

}


/*Jumps*/

unsigned long* Emitter::J32Rel( int cc, int to )
{
	RefChip16RecCPU->AddBlockInstruction();
  *(unsigned char*)x86Ptr++ = 0x0F;
  *(unsigned char*)x86Ptr++ = cc;
  *(unsigned long*)x86Ptr = to;
  x86Ptr+= 4;

   return  (unsigned long*)( x86Ptr - 4 );
}

unsigned long* Emitter::JNE32( unsigned long to ) 
{ 
	RefChip16RecCPU->AddBlockInstruction();
	return J32Rel( 0x85, to ); 
}

unsigned long* Emitter::JNS32( unsigned long to ) 
{ 
	RefChip16RecCPU->AddBlockInstruction();
	return J32Rel( 0x89, to ); 
}


unsigned long* Emitter::JE32( unsigned long to ) 
{ 
	RefChip16RecCPU->AddBlockInstruction();
	return J32Rel( 0x84, to ); 
}

unsigned long* Emitter::JG32( unsigned long to ) 
{ 
	RefChip16RecCPU->AddBlockInstruction();
	return J32Rel( 0x8F, to ); 
}

unsigned long* Emitter::JL32( unsigned long to ) 
{ 
	RefChip16RecCPU->AddBlockInstruction();
	return J32Rel( 0x8C, to ); 
}

unsigned long* Emitter::JLE32( unsigned long to ) 
{ 
	RefChip16RecCPU->AddBlockInstruction();
	return J32Rel( 0x8E, to ); 
}

//Jump if OF = 0 - used for MUL operations
unsigned long* Emitter::JNO32( unsigned long to ) 
{ 
	RefChip16RecCPU->AddBlockInstruction();
	return J32Rel( 0x81, to ); 
}

//Jump if OF = 0 - used for SUB operations
unsigned long* Emitter::JO32( unsigned long to ) 
{ 
	RefChip16RecCPU->AddBlockInstruction();
	return J32Rel( 0x80, to ); 
}

//Jump if CF = 0 - used for MUL operations
unsigned long* Emitter::JAE32( unsigned long to ) 
{ 
	RefChip16RecCPU->AddBlockInstruction();
	return J32Rel( 0x83, to ); 
}

unsigned long* Emitter::JMP32( unsigned long to ) 
{ 
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0xE9;
	*(unsigned long*)x86Ptr = to;
	x86Ptr+= 4;

	return  (unsigned long*)( x86Ptr - 4 );
}

void Emitter::x86SetJ32( unsigned long* j32 )
{
	RefChip16RecCPU->AddBlockInstruction();
	*j32 = ( x86Ptr - (unsigned char*)j32 ) - 4;
}

void Emitter::PUSH32R( X86RegisterType src ) 
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x50 | src; 
}

void Emitter::POP32R( X86RegisterType src ) 
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0x58 | src; 
}


void Emitter::RET()
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned long*)x86Ptr++ = 0xC3; //RET Op, needs no mod or anything.
}

void Emitter::RDTSC()
{
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned long*)x86Ptr++ = 0x0F;
	*(unsigned long*)x86Ptr++ = 0x31;
}

void Emitter::CALL( void (*func)() )
{
	int dest = (int)func - ((int)GetPTR() + 5);
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0xe8;
	*(unsigned long*)x86Ptr = dest;
	x86Ptr += 4;


}

void Emitter::CALL16( unsigned long func)
{
	int dest = (signed long)func - ((int)GetPTR() + 5);
	RefChip16RecCPU->AddBlockInstruction();
	*(unsigned char*)x86Ptr++ = 0xe8;
	*(unsigned long*)x86Ptr = dest;
	x86Ptr += 4;
}