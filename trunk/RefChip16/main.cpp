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
/*#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>*/
// Create our new Chip16 app
FILE * iniFile;  //Create Ini File Pointer
InputDevice *RefChip16Input;
SoundDevice *RefChip16Sound;
RecCPU *RefChip16RecCPU;
Emitter *RefChip16Emitter;
extern LPDIRECT3D9 d3d;
extern LPDIRECT3DDEVICE9 d3ddev;
extern LPDIRECT3DVERTEXBUFFER9 v_buffer;
extern LPDIRECT3DINDEXBUFFER9 i_buffer;

// The WindowProc function prototype
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
HMENU                   hMenu, hSubMenu, hSubMenu2;
OPENFILENAME ofn;

char szFileName[MAX_PATH] = "";
int LoadSuccess = 0;
__int64 vsyncstart, vsyncend;
unsigned char framenumber;
int fps = 0;	
char headingstr [128];
char inisettings[4];
char MenuScale = 1;

int SCREEN_WIDTH = 320;
int SCREEN_HEIGHT = 240;
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

	//Clean up DirectX and COM
	if(v_buffer)
		v_buffer->Release();    // close and release the vertex buffer

	if(i_buffer)
		i_buffer->Release();    // close and release the index buffer

	if(d3ddev)
		d3ddev->Release();      // close and release the device
	
	if(d3d)
		d3d->Release();			//close and release directx
}

int SaveIni(){
	
	//fopen_s(&iniFile, "./refchip16.ini","w+");     //Open the file, args r = read, b = binary

	if (iniFile!=NULL)  //If the file exists
	{
		inisettings[0] = Recompiler;
		inisettings[1] = MenuVSync;
		inisettings[2] = MenuScale;
		
		rewind (iniFile);
		
		CPU_LOG("Saving Ini %x and %x and %x pos %d\n", Recompiler, MenuVSync, MenuScale, ftell(iniFile));
		fwrite(&inisettings,1,4,iniFile); //Read in the file
		CPU_LOG("pos %d\n", Recompiler, MenuVSync, MenuScale, ftell(iniFile));
		fclose(iniFile); //Close the file
		fclose(LogFile); 
		return 0;
	} 
	else
	{
		CPU_LOG("Error Saving Ini\n");
		//User cancelled, either way, do nothing.
		fclose(LogFile); 
		return 1;
	}	
}

int LoadIni(){

	fopen_s(&iniFile, "./refchip16.ini","rb");     //Open the file, args r+ = read, b = binary

	if (iniFile!=NULL)  //If the file exists
	{
		
		fread (&inisettings,1,4,iniFile); //Read in the file
		//fclose (iniFile); //Close the file
		if(ftell (iniFile) > 0) // Identify if the inifile has just been created
		{
			Recompiler = inisettings[0];
			MenuVSync = inisettings[1];
			MenuScale = inisettings[2];
			switch(MenuScale)
			{
			case 1:
				SCREEN_WIDTH = 320;
				SCREEN_HEIGHT = 250;
				break;
			case 2:
				SCREEN_WIDTH = 640;
				SCREEN_HEIGHT = 490;
				break;
			case 3:
				SCREEN_WIDTH = 960;
				SCREEN_HEIGHT = 730;
				break;
			}
		}
		else
		{
			//Defaults
			Recompiler = 1;
			MenuVSync = 1;
			MenuScale = 1;
			SCREEN_WIDTH = 320;
			SCREEN_HEIGHT = 240;
			CPU_LOG("Defaults loaded, new ini\n");
		}
		
		CPU_LOG("Loading Ini %x and %x and %x\n", Recompiler, MenuVSync, MenuScale);
		fclose(iniFile); //Close the file
		fopen_s(&iniFile, "./refchip16.ini","wb+");     //Open the file, args r+ = read, b = binary
		return 0;
	} 
	else
	{
		CPU_LOG("Error Loading Ini\n");
		fopen_s(&iniFile, "./refchip16.ini","wb+");     //Open the file, args r+ = read, b = binary
		//User cancelled, either way, do nothing.
		return 1;
	}	
}

void UpdateTitleBar(HWND hWnd)
{
	sprintf_s(headingstr, "RefChip16 V1.2 FPS: %d Recompiler %s", fps, Recompiler ? "Enabled" : "Disabled");
	SetWindowText(hWnd, headingstr);
}
// The entry point for any Windows program
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HWND hWnd;
	WNDCLASSEX wc;
	time_t counter;
	LPSTR RomName;
	

	//_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	ZeroMemory(&wc, sizeof(WNDCLASSEX));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = "WindowClass";

	RegisterClassEx(&wc);
	OpenLog();
	LoadIni();
	// calculate the size of the client area
    RECT wr = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};    // set the size, but not the position
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);    // adjust the size
	hWnd = CreateWindowEx(NULL, "WindowClass", NULL,
	WS_CAPTION|WS_MINIMIZE|WS_SYSMENU, 100, 100, wr.right - wr.left, wr.bottom - wr.top,
	NULL, NULL, hInstance, NULL);
	UpdateTitleBar(hWnd); //Set the title stuffs
	ShowWindow(hWnd, nCmdShow);


	SCREEN_HEIGHT = wr.bottom - wr.top;
	SCREEN_WIDTH = wr.right - wr.left;
	RefChip16Input = new InputDevice(hInstance, hWnd);
	RefChip16Sound = new SoundDevice();
	RefChip16Emitter = new Emitter();
	RefChip16RecCPU = new RecCPU();
		
	InitDisplay(320, 240, hWnd);
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
		}		
	}

	// enter the main loop:
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	counter = time(NULL);
	
	while(msg.message != WM_QUIT)
	{
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if(Running == true)
		{
		
			if(cycles >= nextvsync) //We have a VBlank!
			{
				//QueryPerformanceCounter((LARGE_INTEGER *)&vsyncend);
				//CPU_LOG("VBlank");
				//framenumber++;
				//CPU_LOG("Time for frame %d to render %d cycles\n", framenumber, vsyncend - vsyncstart);
				VBlank = 1;
				//RedrawLastScreen();
				EndDrawing();
				nextvsync += (1000000 / 60);

				//Ignore controls if the user isnt pointing at this window
				if(GetActiveWindow() == hWnd) 
					RefChip16Input->UpdateControls();

				fps+=1;
				if(counter < time(NULL))
				{
					UpdateTitleBar(hWnd);
					UpdateWindow(hWnd);
					fps = 0;
					counter = time(NULL);		
				}	
				StartDrawing();
				//QueryPerformanceCounter((LARGE_INTEGER *)&vsyncstart);
				
			}
			else
			{
				VBlank = 0;
			}

			if(Recompiler == 1)
			{
				RefChip16RecCPU->EnterRecompiledCode();
			}
			else
			{
				CPULoop();
				cycles += 1;
			}
		}		
	}
	CleanupRoutine();
	return (int)msg.wParam;
}

#define     ID_OPEN        1000
#define     ID_EXIT        1002
#define		ID_ABOUT	   1003
#define		ID_INTERPRETER 1004
#define		ID_RECOMPILER  1005
#define		ID_VSYNC	   1006
#define     ID_WINDOWX1    1007
#define     ID_WINDOWX2    1008
#define     ID_WINDOWX3    1009

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
		SCREEN_HEIGHT = 250;
		break;
	case 2:
		SCREEN_WIDTH = 640;
		SCREEN_HEIGHT = 490;
		break;
	case 3:
		SCREEN_WIDTH = 960;
		SCREEN_HEIGHT = 730;
		break;
	}

	RECT wr = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};    // set the size, but not the position
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);    // adjust the size

	SetWindowPos(hWnd,0,100,100,wr.right - wr.left,wr.bottom - wr.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
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
}

// this is the main message handler for the program
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch(message)
	{
		case WM_CREATE:
		  hMenu = CreateMenu();
		  SetMenu(hWnd, hMenu);

		  hSubMenu = CreatePopupMenu();
		  hSubMenu2 = CreatePopupMenu();
		  AppendMenu(hSubMenu, MF_STRING, ID_OPEN, "&Open");
		  AppendMenu(hSubMenu, MF_STRING, ID_EXIT, "E&xit");
		  CPU_LOG("MenuScale %x", MenuScale);
		  AppendMenu(hSubMenu2, MF_STRING| (Recompiler == 0 ? MF_CHECKED : 0), ID_INTERPRETER, "Enable &Interpreter");
		  AppendMenu(hSubMenu2, MF_STRING| (Recompiler == 1 ? MF_CHECKED : 0), ID_RECOMPILER, "Enable &Recompiler");
		  AppendMenu(hSubMenu2, MF_STRING| (MenuVSync == 1 ? MF_CHECKED : 0), ID_VSYNC, "&Vertical Sync");
		  AppendMenu(hSubMenu2, MF_STRING| (MenuScale == 1 ? MF_CHECKED : 0), ID_WINDOWX1, "WindowScale 320x240 (x&1)");
		  AppendMenu(hSubMenu2, MF_STRING| (MenuScale == 2 ? MF_CHECKED : 0), ID_WINDOWX2, "WindowScale 640x480 (x&2)");
		  AppendMenu(hSubMenu2, MF_STRING| (MenuScale == 3 ? MF_CHECKED : 0), ID_WINDOWX3, "WindowScale 960x720 (x&3)");
		  InsertMenu(hMenu, 0, MF_POPUP|MF_BYPOSITION, (UINT_PTR)hSubMenu, "File");
		  InsertMenu(hMenu, 1, MF_POPUP|MF_BYPOSITION, (UINT_PTR)hSubMenu2, "Settings");
		  InsertMenu(hMenu, 2, MF_STRING, ID_ABOUT, "&About");
		  DrawMenuBar(hWnd);
		  break;
		break;
		case WM_COMMAND :
			
		  switch(LOWORD(wParam))
		  {
		  case ID_OPEN :
			ZeroMemory( &ofn , sizeof( ofn));
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
					RefChip16RecCPU->ResetRecMem();
					Running = true;
				}
			}			
			break;
		  case ID_INTERPRETER :
			if(Recompiler == 1)
			{			
				ToggleRecompilerState(hWnd);
				Recompiler = 0;
				UpdateTitleBar(hWnd); //Set the title stuffs
			 }
			 break;
		  case ID_RECOMPILER :
			 
			 if(Recompiler == 0)
			 {
				 ToggleRecompilerState(hWnd);
				 Recompiler = 1;
				 UpdateTitleBar(hWnd); //Set the title stuffs
			 }
			 break;
		  case ID_VSYNC :
			  ToggleVSync(hWnd);
			  ResetDevice(hWnd);
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
		  case ID_ABOUT :
				 MessageBox(hWnd, "RefChip16 V1.2 Written by Refraction - Big thanks to the Chip16 devs for this :)", "RefChip16", 0);			 
			 break;
		  case ID_EXIT :
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
					DestroyWindow(hWnd);
					return 0;
				}
				break;
				case VK_HOME:
				{
					SCREEN_HEIGHT++;
					SetWindowPos(hWnd,0,100,100,SCREEN_WIDTH,SCREEN_HEIGHT,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
					return 0;
				}
				case VK_END:
				{
					SCREEN_WIDTH++;
					SetWindowPos(hWnd,0,100,100,SCREEN_WIDTH,SCREEN_HEIGHT,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
					return 0;
				}
								
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

	return DefWindowProc (hWnd, message, wParam, lParam);
}

