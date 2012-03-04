#ifndef _COMMON_H
#define _COMMON_H

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

//Standard C libraries
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <windowsx.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

//Customer Headers
#include "D3DApp.h"

#include "Sound.h"
#include "RecCPU.h"
#include "Emitter.h"
#include "InputDevice.h"



#endif