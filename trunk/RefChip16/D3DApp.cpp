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
using namespace CPU;
// include the Direct3D Library files
#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")
//extern InputDevice *RefChip16Input;
//extern CPU *RefChip16CPU;
typedef void (*ON_DEVICE_LOST)();
typedef void (*ON_DEVICE_RESET)();
ON_DEVICE_LOST onDeviceLost;
ON_DEVICE_RESET onDeviceReset;

LPDIRECT3D9 d3d;
LPDIRECT3DDEVICE9 d3ddev;
LPDIRECT3DVERTEXBUFFER9 v_buffer;
LPDIRECT3DINDEXBUFFER9 i_buffer;
LPD3DXFONT m_font;
D3DCOLOR StatusFontColor;
D3DPRESENT_PARAMETERS d3dpp;
RECT m_statusbox;

int width;
int height;
char MenuVSync = 1;
char ActualVSync = 1;


unsigned char ScreenBuffer[320][240];
Sprite SpriteSet;
bool drawing = false;
struct CHIP16VERTEX {FLOAT X, Y, Z; DWORD COLOR;};
#define CUSTOMFVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)
#define CPU_LOG __Log
#define FPS_LOG __Log2

/*Color indexes (0xRRGGBB):
------------------------
0x0 - 0x000000 (Black, Transparent in foreground layer)
0x1 - 0x000000 (Black)
0x2 - 0x888888 (Gray)
0x3 - 0xBF3932 (Red)
0x4 - 0xDE7AAE (Pink)
0x5 - 0x4C3D21 (Dark brown)
0x6 - 0x905F25 (Brown)
0x7 - 0xE49452 (Orange)
0x8 - 0xEAD979 (Yellow)
0x9 - 0x537A3B (Green)
0xA - 0xABD54A (Light green)
0xB - 0x252E38 (Dark blue)
0xC - 0x00467F (Blue)
0xD - 0x68ABCC (Light blue)
0xE - 0xBCDEE4 (Sky blue)
0xF - 0xFFFFFF (White)*/
									//  0		    1			2			3			4			5			6			7
unsigned long pixelcolours[16] = { 0x00000000, 0xFF000000, 0xFF888888, 0xFFBF3932, 0xFFDE7AAE, 0xFF4C3D21, 0xFF905F25, 0xFFE49452, 
								    //  8			9			A			B			C			D			E			F
								   0xFFEAD979, 0xFF537A3B, 0xFFABD54A, 0xFF252E38, 0xFF00467F, 0xFF68ABCC, 0xFFBCDEE4, 0xFFFFFFFF };

CHIP16VERTEX pixel[4*16]; //Buffer to store all the pixel verticles
short indices[6*16]; // create the indices 6 indices per square

void D3DReset()
{
	SpriteSet.Height = 0;
	SpriteSet.Width = 0;
	SpriteSet.BackgroundColour = 0;
	SpriteSet.HorizontalFlip = SpriteSet.VerticalFlip = false;
	memset(ScreenBuffer, 0, sizeof(ScreenBuffer)); 
	ClearRenderTarget();

}

void ClearRenderTarget()
{
	if(drawing == true) d3ddev->EndScene(); //Dont do a present, it hates vsync ;p
		//EndDrawing()
	drawing = false;
	d3ddev->Clear(0, NULL, D3DCLEAR_TARGET,pixelcolours[SpriteSet.BackgroundColour] , 1.0f, 0);
	d3ddev->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);

	StartDrawing();
}

void StartDrawing()
{
	//CPU_LOG("Start Scene");
	if(drawing == true) return;
	
	drawing = true;
	d3ddev->BeginScene();
	d3ddev->SetFVF(CUSTOMFVF);

	// select the vertex buffer to display
    d3ddev->SetStreamSource(0, v_buffer, 0, sizeof(CHIP16VERTEX));
	// select the vertex and index buffers to use
	d3ddev->SetIndices(i_buffer);

}
void GenerateVertexList()
{	
	int i;
	int j = 0;

		
	//Generate the verticies for each of the colours
	for(i = 0; i < 16; i++)
	{
		pixel[i*4].X       = 0.0f;	pixel[i*4].Y	   = 0.0f;  pixel[i*4].Z       = 10.0f;  pixel[i*4].COLOR  	 = pixelcolours[i];
		pixel[1 + (i*4)].X = 0.0f;  pixel[1 + (i*4)].Y = 1.0f;  pixel[1 + (i*4)].Z = 10.0f;  pixel[1 + (i*4)].COLOR = pixelcolours[i];
		pixel[2 + (i*4)].X = 1.0f;  pixel[2 + (i*4)].Y = 0.0f;  pixel[2 + (i*4)].Z = 10.0f;  pixel[2 + (i*4)].COLOR = pixelcolours[i];
		pixel[3 + (i*4)].X = 1.0f;  pixel[3 + (i*4)].Y = 1.0f;  pixel[3 + (i*4)].Z = 10.0f; pixel[3 + (i*4)].COLOR = pixelcolours[i];
		
	}


	//Generate the index
	for(i = 0; i < 16; i++)
	{
		indices[j++] = i++;
		indices[j++] = i++;
		indices[j++] = i++;
		indices[j++] = i++;
	}

	// create a vertex buffer interface called v_buffer
    d3ddev->CreateVertexBuffer(sizeof(pixel),
                               0,
                               CUSTOMFVF,
                               D3DPOOL_MANAGED,
                               &v_buffer,
                               NULL);

	

    VOID* pVoid;    // a void pointer
	
    // lock v_buffer and load the vertices into it
    v_buffer->Lock(0, 0, (void**)&pVoid, 0);
    memcpy(pVoid, pixel, sizeof(pixel));
    v_buffer->Unlock();

	// create a index buffer interface called i_buffer
    d3ddev->CreateIndexBuffer(sizeof(indices),
                              0,
                              D3DFMT_INDEX16,
                              D3DPOOL_MANAGED,
                              &i_buffer,
                              NULL);

    // lock i_buffer and load the indices into it
    i_buffer->Lock(0, 0, (void**)&pVoid, 0);
    memcpy(pVoid, indices, sizeof(indices));
    i_buffer->Unlock(); 
}


void InitDisplay(int width, int height, HWND hWnd)
{
	d3d = NULL;
	d3ddev = NULL;
	v_buffer = NULL;
	i_buffer = NULL;

	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hWnd;
	d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.BackBufferWidth = 320;
    d3dpp.BackBufferHeight = 240;
   // d3dpp.EnableAutoDepthStencil = TRUE;
   // d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

	if(!MenuVSync) d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	else d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    d3d->CreateDevice(D3DADAPTER_DEFAULT,
                      D3DDEVTYPE_HAL,
                      hWnd,
                      D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                      &d3dpp,
                      &d3ddev);

//
	GenerateVertexList();

	d3ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); // for some reason this culls by default?! wtf
    d3ddev->SetRenderState(D3DRS_LIGHTING, FALSE);    // turn off the 3D lighting
    d3ddev->SetRenderState(D3DRS_ZENABLE, FALSE);    // turn on the z-buffer
	d3ddev->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_3WEIGHTS);
	D3DXMATRIX Ortho2D;	
    D3DXMATRIX Identity;
    
    D3DXMatrixOrthoLH(&Ortho2D, 320.0f, -240.0f, 1.0f, 100.0f);
    D3DXMatrixIdentity(&Identity);

    d3ddev->SetTransform(D3DTS_PROJECTION, &Ortho2D);
    d3ddev->SetTransform(D3DTS_WORLD, &Identity);
    d3ddev->SetTransform(D3DTS_VIEW, &Identity);
}

void DrawSprite(unsigned short MemAddr, int X, int Y)
{
	bool shifted = false;	
	int yinc = 1;
	int xinc = 1;
	int xstart, xend;
	int ystart, yend;
	int MemPos = 0;
	int StartMemSkip = 0;
	int EndMemSkip = 0;
	int j;
	//__int64 count1, count2;
	
	unsigned char curpixel;

	//QueryPerformanceCounter((LARGE_INTEGER *)&count1);
	//CPU_LOG("Draw MemAddr %x, X %d Y %d", MemAddr, X, Y);
	//This draws the Sprite located at MemAddr on the screen, top left corner Y x X, for the size of the sprite in SpriteSet.
	if(SpriteSet.HorizontalFlip)
	{
		xstart = X+SpriteSet.Width;
		xend = X;
		xinc = -1;

		if(xend >= xstart) return;
		if(xend >= 320) return;
		if(xstart < 0) return;

		if(xstart >= 320)
		{
			//CPU_LOG("X start out of bounds start X=%d xinc=%d\n", xstart, xinc);
			StartMemSkip = (xstart - 319);
			xstart -= StartMemSkip & ~0x1;
			//CPU_LOG("X start skipping %d memory before each line adding %d to j xend = %d\n", StartMemSkip / 2, StartMemSkip & 0x1, xend);
		}
		if(xend < 0)
		{
			//Untested! probably wrong.
			//CPU_LOG("X end out of bounds end x=%d\n", xend);
			EndMemSkip = ((0 - xend) - 1) / 2;
			xend = 0;
			//CPU_LOG("xend now x=%d\n", xend);
		}
	}
	else
	{		
		xstart = X;
		xend = X+SpriteSet.Width;
		if(xend <= xstart || xstart >= 320)return;

		if(xend > 320)
		{
			//CPU_LOG("X end out of bounds end x=%d\n", xend);
			EndMemSkip = (xend - 319) / 2 ; //Mem inc is done just before the end skip so we do 320 - 1
			xend = 320;
			//CPU_LOG("xend now x=%d\n", xend);
		}
		else if(xstart < 0)
		{
			//CPU_LOG("X start out of bounds start X=%d xinc=%d\n", xstart, xinc);
			StartMemSkip = (0 - xstart);
			xstart += StartMemSkip & ~0x1;
			//CPU_LOG("X start skipping %d memory before each line adding %d to j xend = %d\n", StartMemSkip / 2, StartMemSkip & 0x1, xend);
		} 
	}

	if(SpriteSet.VerticalFlip)
	{
		ystart = Y+SpriteSet.Height;
		yend = Y;
		yinc = -1;

		if(yend >= ystart)return;
		if(yend >= 240) return;
		if(ystart < 0)	return;
	}
	else
	{
		ystart = Y;
		yend = Y+SpriteSet.Height;

		if(yend <= ystart || ystart >= 240)return;

		if(yend > 240)
		{
			//CPU_LOG("Y end out of bounds end y=%d\n", yend);
			yend = 240;
			//CPU_LOG("yend now y=%d\n", yend);
		}
		else if(ystart < 0)
		{
			//CPU_LOG("Y low start out of bounds start y=%d\n", ystart);
			MemAddr += ((xend - xstart) / 2) * abs(ystart);
			ystart = 0;
			//CPU_LOG("y low start now y=%d\n", ystart);
		}
	}	

	D3DXMATRIX matTranslate;

	CPU_LOG("Starting Sprite draw\n");
	//CPU_LOG("\nAt X=%x X+S=%x Y=%x to Y+S=%x\n", X, X+SpriteSet.Width, Y, Y + SpriteSet.Height);

	CPU::Flag.CarryBorrow = 0;

	for(int i = ystart; i != yend;){
		MemAddr += StartMemSkip >> 1;
		CPU_LOG("\n");
		j = xstart + (StartMemSkip & 0x1);
		for(; j != xend;)
		{	
			if(xstart > xend) MemPos = (j - xstart) & 0x1;
			else MemPos = (xstart - j) & 0x1;

			curpixel = CPU::ReadMem8(MemAddr & 0xffff);
			curpixel = curpixel >> ((~MemPos & 0X1) << 2) & 0xf;

							
			CPU_LOG("%x", curpixel);
			if(curpixel > 0)
			{
				if(ScreenBuffer[j][i] != 0) CPU::Flag.CarryBorrow = 1;	//Check collision

				D3DXMatrixTranslation(&matTranslate, (float)j-160.0f, (float)i-120.0f, 1.0f);
				d3ddev->SetTransform(D3DTS_WORLD, &matTranslate);
				d3ddev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, curpixel*4, 0, 4, 0, 2); 
			}
			ScreenBuffer[j][i] = curpixel;	

			j+=xinc;
			MemAddr += MemPos;
		}

		MemAddr += EndMemSkip;
		i += yinc;
	}
	//QueryPerformanceCounter((LARGE_INTEGER *)&count2);

//	FPS_LOG("Cycles to render %d x %d Sprite %d\n", SpriteSet.Height, SpriteSet.Width, count2 - count1);
//	CPU_LOG("End Sprite draw\n");
}


void RedrawLastScreen()
{
	
	D3DXMATRIX matTranslate;
	
	if(drawing == false) return;
	FPS_LOG("Starting redraw");
	
	for(int i = 239; i != 0; --i){
		for(int j = 319; j != 0; --j){	
			if(ScreenBuffer[j][i] != 0)
			{
				D3DXMatrixTranslation(&matTranslate, (float)j-160.0f, (float)i-120.0f, 1.0f);
				d3ddev->SetTransform(D3DTS_WORLD, &matTranslate);
				d3ddev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, (ScreenBuffer[j][i])*4, 0, 4, 0, 2); 
			}
		}				
	}
	//FPS_LOG("End redraw");
}

void EndDrawing()
{
	if(drawing == false) return;

    d3ddev->EndScene(); 

	// Flip!
    d3ddev->Present(NULL, NULL, NULL, NULL);
	//CPU_LOG("End Scene");
	drawing = false;
}

void ResetDevice(HWND hWnd)
{
	if(drawing == true) EndDrawing();

	if (onDeviceLost) onDeviceLost();
	
	HRESULT result = d3ddev->Reset(&d3dpp);

	if(result == 0)
	{
		if (onDeviceReset) onDeviceReset();
	
		if(v_buffer)
			v_buffer->Release();    // close and release the vertex buffer

		if(i_buffer)
			i_buffer->Release();    // close and release the index buffer

		if(d3ddev)
			d3ddev->Release();      // close and release the device
	
		if(d3d)
			d3d->Release();			//close and release directx


		InitDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, hWnd);
	//d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);	
	}
	else MessageBox(hWnd, "VSync change Failed", "Vsync Error", 0);

	ActualVSync = MenuVSync;
	StartDrawing();
	RedrawLastScreen();
}
