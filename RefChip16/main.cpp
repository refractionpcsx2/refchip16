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
#include "D3DApp.h"
#include "Sound.h"
#include "RecCPU.h"
#include "Emitter.h"
#include "InputDevice.h"
#include <time.h>
#include <tchar.h>
#include <windows.h>
#include "resource.h"
/*#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>*/
// Create our new Chip16 app
FILE * iniFile;  //Create Ini File Pointer
InputDevice *RefChip16Input;
SoundDevice *RefChip16Sound;
RecCPU *RefChip16RecCPU;
Emitter *RefChip16Emitter;
// The WindowProc function prototype
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
HMENU                   hMenu, hSubMenu, hSubMenu2;
OPENFILENAME ofn;
HBITMAP LogoJPG = NULL;
HWND hwndSDL;

char szFileName[MAX_PATH] = "";
char CurFilename[MAX_PATH] = "";
int LoadSuccess = 0;
__int64 vsyncstart, vsyncend;
unsigned char framenumber;
int fps2 = 0;
char headingstr [128];
char inisettings[5];
char MenuScale = 1;
char LoggingEnable = 0;
int prev_v_cycle = 0;
unsigned int v_cycle = prev_v_cycle;
time_t counter;

unsigned short SCREEN_WIDTH = 320;
unsigned short SCREEN_HEIGHT = 240;
RECT        rc;

using namespace CPU;
#define FPS_LOG __Log2
#define CPU_LOG __Log

void CleanupRoutine()
{
	//Clean up XAudio2
	delete RefChip16RecCPU;
	delete RefChip16Sound;	
	delete RefChip16Input;	
	delete RefChip16Emitter;
}

int SaveIni(){
	
	fopen_s(&iniFile, "./refchip16.ini", "w+b");     //Open the file, args r = read, b = binary
	

	if (iniFile!=NULL)  //If the file exists
	{
		inisettings[0] = Recompiler;
		inisettings[1] = MenuVSync;
		inisettings[2] = MenuScale;
		inisettings[3] = LoggingEnable;
		inisettings[4] = Smoothing;
		
		rewind (iniFile);
		
		///CPU_LOG("Saving Ini %x and %x and %x pos %d\n", Recompiler, MenuVSync, MenuScale, ftell(iniFile));
		fwrite(&inisettings,1,5,iniFile); //Read in the file
		//CPU_LOG("Rec %d, Vsync %d, Scale %d, pos %d\n", Recompiler, MenuVSync, MenuScale, ftell(iniFile));
		fclose(iniFile); //Close the file

		if(LoggingEnable)
			fclose(LogFile); 

		return 0;
	} 
	else
	{
		CPU_LOG("Error Saving Ini\n");
		//User cancelled, either way, do nothing.
		if(LoggingEnable)
			fclose(LogFile); 

		return 1;
	}	
}

int LoadIni(){

	fopen_s(&iniFile, "./refchip16.ini","rb");     //Open the file, args r+ = read, b = binary

	if (iniFile!=NULL)  //If the file exists
	{
		
		fread (&inisettings,1,5,iniFile); //Read in the file
		//fclose (iniFile); //Close the file
		if(ftell (iniFile) > 0) // Identify if the inifile has just been created
		{
			Recompiler = inisettings[0];
			MenuVSync = inisettings[1];
			MenuScale = inisettings[2];
			LoggingEnable = inisettings[3];
			Smoothing = inisettings[4];

			switch(MenuScale)
			{
			case 1:
				SCREEN_WIDTH = 320;
				SCREEN_HEIGHT = 240;
				break;
			case 2:
				SCREEN_WIDTH = 640;
				SCREEN_HEIGHT = 480;
				break;
			case 3:
				SCREEN_WIDTH = 960;
				SCREEN_HEIGHT = 720;
				break;
			}
		}
		else
		{
			//Defaults
			Recompiler = 1;
			MenuVSync = 1;
			MenuScale = 1;
			Smoothing = 0;
			LoggingEnable = 0;
			SCREEN_WIDTH = 320;
			SCREEN_HEIGHT = 240;
			CPU_LOG("Defaults loaded, new ini\n");
		}
		
		CPU_LOG("Loading Ini %x and %x and %x\n", Recompiler, MenuVSync, MenuScale);
		fclose(iniFile); //Close the file
		//fopen_s(&iniFile, "./refchip16.ini","wb+");     //Open the file, args r+ = read, b = binary
		return 0;
	} 
	else
	{
		CPU_LOG("Error Loading Ini\n");
		fopen_s(&iniFile, "./refchip16.ini","r+b");     //Open the file, args r+ = read, b = binary
		//User cancelled, either way, do nothing.
		return 1;
	}	
}

void UpdateTitleBar(HWND hWnd)
{
	sprintf_s(headingstr, "RefChip16 V1.7 FPS: %d Recompiler %s", fps2, Recompiler ? "Enabled" : "Disabled");
	SetWindowText(hWnd, headingstr);
}
// The entry point for any Windows program
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HWND hWnd;
	WNDCLASSEX wc;
	
	LPSTR RomName;
	

	//_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	ZeroMemory(&wc, sizeof(WNDCLASSEX));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszClassName = "WindowClass";
	
	RegisterClassEx(&wc);

	LoadIni();
	if(LoggingEnable)
			OpenLog();
	// calculate the size of the client area
    RECT wr = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};    // set the size, but not the position
    AdjustWindowRect(&wr, WS_CAPTION|WS_MINIMIZE|WS_SYSMENU, TRUE);    // adjust the size
	hWnd = CreateWindowEx(NULL, "WindowClass", NULL,
	WS_CAPTION|WS_MINIMIZE|WS_SYSMENU, 100, 100, wr.right - wr.left, wr.bottom - wr.top,
	NULL, NULL, hInstance, NULL);
	UpdateTitleBar(hWnd); //Set the title stuffs
	ShowWindow(hWnd, nCmdShow);

	RefChip16Input = new InputDevice(hInstance, hWnd);
	RefChip16Sound = new SoundDevice(hWnd);
	RefChip16Emitter = new Emitter();
	RefChip16RecCPU = new RecCPU();
		
	
	RefChip16RecCPU->InitRecMem();
	
	
	if(strstr(lpCmdLine, "-r"))
	{
		RomName = strstr(lpCmdLine, "-r") + 3;
		
		LoadSuccess = LoadRom(RomName);
		if(LoadSuccess == 1) MessageBox(hWnd, "Error Loading Game", "Error!", 0);
		else if(LoadSuccess == 2) MessageBox(hWnd, "Error Loading Game - Spec too new, please check for update", "Error!",0);
		else 
		{
			RefChip16RecCPU->ResetRecMem();
			Running = true;
			InitDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, hWnd);
		}		
	}

	// enter the main loop:
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	
	
	while(msg.message != WM_QUIT)
	{		
				while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				if(Running == true)
				{

					if(cycles >= (nextsecond + (1000000.0f/60.0f * fps))) //We have a VBlank!
					{
						
						VBlank = 1;
						
						//Ignore controls if the user isnt pointing at this window
						if(GetActiveWindow() == hWnd || GetActiveWindow() == hwndSDL) 
							RefChip16Input->UpdateControls();

						fps++; 
						fps2++;

						StartDrawing();
						RedrawLastScreen();

						
						/*if(MenuVSync == 1)
						{
							unsigned int value = prev_v_cycle+(int)((float)(1000.0f / 60.0f) * fps);

							v_cycle = SDL_GetTicks();

							while (v_cycle < value) { v_cycle = SDL_GetTicks(); }

							if(fps == 60)
							{						
								prev_v_cycle += 1000;
								fps = 0;	
								nextsecond += 1000000;
							}	
						}*/
						if ((int)(cycles - nextsecond) < 0)
						{
							nextsecond += 1000000;
							fps = 0;
						}
							EndDrawing();
					}
					else
					{
						VBlank = 0;
					}

					if(counter < time(NULL))
						{
							UpdateTitleBar(hWnd);
							UpdateWindow(hWnd);
							counter = time(NULL);
							fps2 = 0;
							/*if(!MenuVSync)
							{
								//nextsecond += (int)((float)(1000000.0f / 60.0f) * fps);
								fps = 0;								
							}*/
						}

					if(Recompiler == 1)
					{
						if(StackPTR < 0xFDF0) CPU_LOG("Stack Underflow! Recompiler StackPTR = %x", StackPTR);
						if(StackPTR > 0xFFF0) CPU_LOG("Stack Overflow! Recompiler StackPTR = %x", StackPTR);
						RefChip16RecCPU->EnterRecompiledCode();
					}
					else
					{
						cycles += 1;
						CPULoop();
						
					}
				}	
				else Sleep(10);
		
	
	}
	CleanupRoutine();
	return (int)msg.wParam;
}

#define     ID_OPEN        1000
#define     ID_RESET	   1001
#define     ID_EXIT        1002
#define		ID_ABOUT	   1003
#define		ID_INTERPRETER 1004
#define		ID_RECOMPILER  1005
#define		ID_VSYNC	   1006
#define     ID_WINDOWX1    1007
#define     ID_WINDOWX2    1008
#define     ID_WINDOWX3    1009
#define     ID_LOGGING	   1010
#define     ID_SMOOTHING   1011

void ToggleSmoothing(HWND hWnd)
{
	HMENU hmenuBar = GetMenu(hWnd); 
	MENUITEMINFO mii; 

	memset( &mii, 0, sizeof( MENUITEMINFO ) );
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_STATE;    // information to get 
	//Grab filtering state
	GetMenuItemInfo(hSubMenu2, ID_SMOOTHING, FALSE, &mii);
	// Toggle the checked state. 
	Smoothing = !Smoothing;
	mii.fState ^= MFS_CHECKED; 
	// Write the new state to the smoothing flag.
	SetMenuItemInfo(hSubMenu2, ID_SMOOTHING, FALSE, &mii); 

}

void ToggleRecompilerState(HWND hWnd)
{
	HMENU hmenuBar = GetMenu(hWnd); 
	MENUITEMINFO mii; 

	memset( &mii, 0, sizeof( MENUITEMINFO ) );
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_STATE;    // information to get 
	//Grab Recompiler state
	GetMenuItemInfo(hSubMenu2, ID_RECOMPILER, FALSE, &mii);
	// Move this state to the Interpreter flag
	SetMenuItemInfo(hSubMenu2, ID_INTERPRETER, FALSE, &mii);
	// Toggle the checked state. 
	mii.fState ^= MFS_CHECKED; 
	// Move this state to the Recompiler flag
	SetMenuItemInfo(hSubMenu2, ID_RECOMPILER, FALSE, &mii); 

}

void ChangeScale(HWND hWnd, int ID)
{
	HMENU hmenuBar = GetMenu(hWnd); 
	MENUITEMINFO mii; 

	memset( &mii, 0, sizeof( MENUITEMINFO ) );
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_STATE;    // information to get 
	//Grab Recompiler state
	GetMenuItemInfo(hSubMenu2, 1006 + MenuScale, FALSE, &mii);
	// Move this state to the Interpreter flag
	SetMenuItemInfo(hSubMenu2, ID, FALSE, &mii);
	// Toggle the checked state. 
	mii.fState ^= MFS_CHECKED; 
	// Move this state to the Recompiler flag
	SetMenuItemInfo(hSubMenu2, 1006 + MenuScale, FALSE, &mii); 
	MenuScale = ID - 1006;

	switch(MenuScale)
	{
	case 1:
		SCREEN_WIDTH = 320;
		SCREEN_HEIGHT = 240;
		break;
	case 2:
		SCREEN_WIDTH = 640;
		SCREEN_HEIGHT = 480;
		break;
	case 3:
		SCREEN_WIDTH = 960;
		SCREEN_HEIGHT = 720;
		break;
	}

	RECT wr = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};    // set the size, but not the position
   AdjustWindowRect(&wr, WS_CAPTION|WS_MINIMIZE|WS_SYSMENU, TRUE);    // adjust the size

	SetWindowPos(hWnd,0,100,100,wr.right - wr.left,wr.bottom - wr.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
	
	if (Running == true)
	{
		DestroyDisplay();
		InitDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, hWnd);
	}
}

void ToggleVSync(HWND hWnd)
{
	HMENU hmenuBar = GetMenu(hWnd); 
	MENUITEMINFO mii; 

	memset( &mii, 0, sizeof( MENUITEMINFO ) );
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_STATE;    // information to get 
	//Grab Vsync state
	GetMenuItemInfo(hSubMenu2, ID_VSYNC, FALSE, &mii);
	// Toggle the checked state. 
	MenuVSync = !MenuVSync;
	mii.fState ^= MFS_CHECKED; 
	// Write the new state to the VSync flag.
	SetMenuItemInfo(hSubMenu2, ID_VSYNC, FALSE, &mii); 

	if (Running == true)
	{
		DestroyDisplay();
		InitDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, hWnd);
	}
}

void ToggleLogging(HWND hWnd)
{
	HMENU hmenuBar = GetMenu(hWnd); 
	MENUITEMINFO mii; 

	memset( &mii, 0, sizeof( MENUITEMINFO ) );
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_STATE;    // information to get 
	//Grab Logging state
	GetMenuItemInfo(hSubMenu2, ID_LOGGING, FALSE, &mii);
	// Toggle the checked state. 
	LoggingEnable = !LoggingEnable;
	mii.fState ^= MFS_CHECKED; 
	// Write the new state to the Logging flag.
	SetMenuItemInfo(hSubMenu2, ID_LOGGING, FALSE, &mii); 

	if(LoggingEnable == 1)
		OpenLog();
	else
		fclose(LogFile); 
}

// this is the main message handler for the program
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch(message)
	{
		case WM_CREATE:
		  hMenu = CreateMenu();
		  SetMenu(hWnd, hMenu);
		  LogoJPG = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_LOGO));
		  
		  hSubMenu = CreatePopupMenu();
		  hSubMenu2 = CreatePopupMenu();
		  AppendMenu(hSubMenu, MF_STRING, ID_OPEN, "&Open");
		  AppendMenu(hSubMenu, MF_STRING, ID_RESET, "R&eset");
		  AppendMenu(hSubMenu, MF_STRING, ID_EXIT, "E&xit");
		  
		  AppendMenu(hSubMenu2, MF_STRING| (Recompiler == 0 ? MF_CHECKED : 0), ID_INTERPRETER, "Enable &Interpreter");
		  AppendMenu(hSubMenu2, MF_STRING| (Recompiler == 1 ? MF_CHECKED : 0), ID_RECOMPILER, "Enable &Recompiler");
		  AppendMenu(hSubMenu2, MF_STRING| (Smoothing == 1 ? MF_CHECKED : 0), ID_SMOOTHING, "Graphics &Filtering");
		  AppendMenu(hSubMenu2, MF_STRING| (MenuVSync == 1 ? MF_CHECKED : 0), ID_VSYNC, "&Vertical Sync");
		  AppendMenu(hSubMenu2, MF_STRING| (MenuScale == 1 ? MF_CHECKED : 0), ID_WINDOWX1, "WindowScale 320x240 (x&1)");
		  AppendMenu(hSubMenu2, MF_STRING| (MenuScale == 2 ? MF_CHECKED : 0), ID_WINDOWX2, "WindowScale 640x480 (x&2)");
		  AppendMenu(hSubMenu2, MF_STRING| (MenuScale == 3 ? MF_CHECKED : 0), ID_WINDOWX3, "WindowScale 960x720 (x&3)");
		  AppendMenu(hSubMenu2, MF_STRING| (LoggingEnable == 1 ? MF_CHECKED : 0), ID_LOGGING, "Enable Logging");
		  InsertMenu(hMenu, 0, MF_POPUP|MF_BYPOSITION, (UINT_PTR)hSubMenu, "File");
		  InsertMenu(hMenu, 1, MF_POPUP|MF_BYPOSITION, (UINT_PTR)hSubMenu2, "Settings");
		  InsertMenu(hMenu, 2, MF_STRING, ID_ABOUT, "&About");
		  DrawMenuBar(hWnd);
		  break;
		break;
		case WM_PAINT:
			{
			BITMAP bm;
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint(hWnd, &ps);

			HDC hdcMem = CreateCompatibleDC(hdc);
			HGDIOBJ hbmOld = SelectObject(hdcMem, LogoJPG);

			GetObject(LogoJPG, sizeof(bm), &bm);
			StretchBlt(hdc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
			
			SelectObject(hdcMem, hbmOld);
			DeleteDC(hdcMem);

			EndPaint(hWnd, &ps);
			}
		break;
		case WM_COMMAND:
			
		  switch(LOWORD(wParam))
		  {
		  case ID_OPEN:
			ZeroMemory( &ofn , sizeof( ofn ));
			ofn.lStructSize = sizeof ( ofn );
			ofn.hwndOwner = NULL ;
			ofn.lpstrFile = szFileName ;
			ofn.lpstrFile[0] = '\0';
			ofn.nMaxFile = sizeof( szFileName );
			ofn.lpstrFilter = "c16 Rom\0*.c16\0";
			ofn.nFilterIndex =1;
			ofn.lpstrFileTitle = NULL ;
			ofn.nMaxFileTitle = 0 ;
			ofn.lpstrInitialDir=NULL ;
			ofn.Flags = OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST ;
			
			if(GetOpenFileName(&ofn)) 
			{
				LoadSuccess = LoadRom(ofn.lpstrFile);
				if(LoadSuccess == 1) MessageBox(hWnd, "Error Loading Game", "Error!", 0);
				else if(LoadSuccess == 2) MessageBox(hWnd, "Error Loading Game - Spec too new, please check for update", "Error!",0);
				else 
				{
					strcpy_s(CurFilename, szFileName);
					RefChip16RecCPU->ResetRecMem();
					Running = true;
					InitDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, hWnd);	
					
					v_cycle = SDL_GetTicks();
					prev_v_cycle = v_cycle;
					counter = time(NULL);
				}
			}			
			break;
		  case ID_RESET:
			 if(Running == true)
			 {
				 LoadSuccess = LoadRom(CurFilename);
				 if(LoadSuccess == 1) MessageBox(hWnd, "Error Loading Game", "Error!", 0);
				 else if(LoadSuccess == 2) MessageBox(hWnd, "Error Loading Game - Spec too new, please check for update", "Error!",0);
				 else 
				 {
					 RefChip16RecCPU->ResetRecMem();
					 InitDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, hWnd);	
				 	
					 v_cycle = SDL_GetTicks();
					 prev_v_cycle = v_cycle;
					 counter = time(NULL);
				 }
			 }
			 break;
		  case ID_INTERPRETER:
			if(Recompiler == 1)
			{			
				ToggleRecompilerState(hWnd);
				Recompiler = 0;
				UpdateTitleBar(hWnd); //Set the title stuffs
			 }
			 break;
		  case ID_RECOMPILER:
			 
			 if(Recompiler == 0)
			 {
				 ToggleRecompilerState(hWnd);
				 Recompiler = 1;
				 UpdateTitleBar(hWnd); //Set the title stuffs
			 }
			 break;
		  case ID_VSYNC:
			  ToggleVSync(hWnd);
			 break;
		  case ID_SMOOTHING:
			  ToggleSmoothing(hWnd);
			  break;
		  case ID_WINDOWX1:
			  ChangeScale(hWnd, ID_WINDOWX1);
			  break;
		  case ID_WINDOWX2:
			  ChangeScale(hWnd, ID_WINDOWX2);
			  break;
		  case ID_WINDOWX3:
			  ChangeScale(hWnd, ID_WINDOWX3);
			  break;
		  case ID_LOGGING:
			  ToggleLogging(hWnd);
			  break;
		  case ID_ABOUT:
				 MessageBox(hWnd, "RefChip16 V1.7 Written by Refraction - Big thanks to the Chip16 devs for this :)", "RefChip16", 0);			 
			 break;
		  case ID_EXIT:
			  DestroyDisplay();
			 DestroyWindow(hWnd);
			 return 0;
			 break;
		  }
		case WM_KEYDOWN:
		{
			switch(wParam)
			{		
				case VK_ESCAPE:
				{
					DestroyDisplay();
					DestroyWindow(hWnd);
					return 0;
				}
				break;								
				default:
					return 0;
			}
		}

		case WM_DESTROY:
		{
			PostQuitMessage(0);
			SaveIni();
			return 0;
		} 
		break;
	}
	prev_v_cycle = SDL_GetTicks() - (int)((float)(1000.0f / 60.0f) * fps);
	return DefWindowProc (hWnd, message, wParam, lParam);
}

