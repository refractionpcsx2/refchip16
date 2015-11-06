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

#ifndef _EMITTER_H
#define _EMITTER_H

typedef enum
		{
			EAX = 0,
			EBX = 3,
			ECX = 1,
			EDX = 2,
			ESI = 6,
			EDI = 7,
			EBP = 5,
			ESP = 4 
		} X86RegisterType;

		enum ASSEMBLER_HELP_VALUES
		{
			SIB    = 4,  ///< SIB Value Passed To SibSB
			DISP32 = 5   ///< DISP32 Value Passed To ModRM
		};

class Emitter
{
	public:
		
		Emitter();
		~Emitter();
		
		unsigned char* GetPTR();
		void SetPTR(unsigned char* PTR);

		void ModRM(unsigned char mod, unsigned char reg, unsigned char rm);
		void ADD32ItoM(unsigned int dest, int imm);
		void ADD16MtoR(X86RegisterType dest, unsigned int src);
		void ADD32MtoR(X86RegisterType dest, unsigned int src);
		void ADD16ItoR(X86RegisterType dest, int imm);
		void ADD16RtoR(X86RegisterType dest, X86RegisterType src);
		void ADD32RtoR(X86RegisterType dest, X86RegisterType src);
		void ADD32ItoR(X86RegisterType dest, int imm);
		void SUB16ItoR(X86RegisterType dest, int imm);
		void SUB16RtoR(X86RegisterType dest, X86RegisterType src);
		void SUB32ItoM(unsigned int dest, int imm);
		void MUL16MtoEAX(unsigned int src );
		void MUL16RtoEAX( X86RegisterType src );
		void CDQ16();
		void DIV16MtoEAX(unsigned int src );
		void DIV16RtoEAX( X86RegisterType src );
		void IDIV16RtoEAX(X86RegisterType src);
		void AND16ItoM(unsigned int dest, int imm);
		void AND16ItoR( X86RegisterType dest, int imm);
		void AND32ItoR( X86RegisterType dest, int imm);
		void AND16RtoM(unsigned int dest, X86RegisterType src);
		void AND16MtoR( X86RegisterType dest, unsigned int src );
		void AND16RtoR(X86RegisterType dest, X86RegisterType src);
		void OR16ItoM(unsigned int dest, int imm);
		void OR16ItoR( X86RegisterType dest, int imm);
		void OR16RtoM(unsigned int dest, X86RegisterType src);
		void OR16MtoR( X86RegisterType dest, unsigned int src );
		void OR16RtoR(X86RegisterType dest, X86RegisterType src);
		void XOR16ItoM(unsigned int dest, int imm);
		void XOR16ItoR( X86RegisterType dest, int imm);
		void XOR16RtoM(unsigned int dest, X86RegisterType src);
		void XOR16MtoR( X86RegisterType dest, unsigned int src );
		void XOR16RtoR(X86RegisterType dest, X86RegisterType src);
		void XOR32RtoR(X86RegisterType dest, X86RegisterType src);
		void NOT16R(X86RegisterType src);
		void NEG16R(X86RegisterType src);
		void SHL16ItoR( X86RegisterType dest, int imm);
		void SHL16CLtoR( X86RegisterType dest);
		void SHR16ItoR( X86RegisterType dest, int imm);
		void SHR16CLtoR( X86RegisterType dest);
		void SAR16ItoR( X86RegisterType dest, int imm);
		void SAR16CLtoR( X86RegisterType dest);
		void CMP16ItoM(unsigned int dest, int imm);
		void CMP16ItoR(X86RegisterType dest, int imm);
		void CMP32ItoR(X86RegisterType dest, int imm);
		void CMP16RtoR(X86RegisterType dest, X86RegisterType src);
		void MOV32MtoR(X86RegisterType dest, unsigned int src);
		void MOV32RtoM(unsigned int dest, X86RegisterType src);
		void MOV32ItoM(unsigned int dest, int imm);
		void MOV16MtoR(X86RegisterType dest, unsigned int src);
		void MOV16RtoM(unsigned int dest, X86RegisterType src);
		void MOV16ItoR(X86RegisterType dest, int imm);
		void MOV32ItoR(X86RegisterType dest, int imm);
		void MOV16ItoM(unsigned int dest, int imm);
		void MOV16RtoR(X86RegisterType dest, X86RegisterType src);
		void MOV16RmtoR(X86RegisterType dest, X86RegisterType src);
		void MOV16RtoRm(X86RegisterType dest, X86RegisterType src);
		unsigned long* J32Rel( int cc, int to );
		unsigned long* JNE32( unsigned long to );
		unsigned long* JE32( unsigned long to );
		unsigned long* JG32( unsigned long to );
		unsigned long* JL32( unsigned long to );
		unsigned long* JLE32( unsigned long to );
		unsigned long* JMP32( unsigned long to );
		unsigned long* JNO32( unsigned long to );
		unsigned long* JO32( unsigned long to );
		unsigned long* JAE32( unsigned long to );
		unsigned long* JNS32( unsigned long to );
		void x86SetJ32( unsigned long* j32 );
		void PUSH32R( X86RegisterType src );
		void POP32R( X86RegisterType src );
		void RDTSC();
		void RET();
		void CALL( void (*func)() );
		void CALL16( unsigned long func );
		

		unsigned char* x86Ptr; //Pointer for writing out the code buffer

	private:
		
};
#endif