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

SDL_Surface*    SDL_Display;
SDL_Renderer*	renderer = NULL;
SDL_Texture *texture = NULL;
SDL_Window *screen = NULL;
char MenuVSync = 1;
char Smoothing = 0;
extern HWND hwndSDL;
unsigned char ScreenBuffer[320][240];
Sprite SpriteSet;
struct CHIP16VERTEX {FLOAT X, Y, Z; DWORD COLOR;};
#define CUSTOMFVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)
CHIP16VERTEX pixel[4*16]; //Buffer to store all the pixel verticles

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

void D3DReset()
{
	SpriteSet.Height = 0;
	SpriteSet.Width = 0;
	SpriteSet.BackgroundColour = 0;
	SpriteSet.HorizontalFlip = SpriteSet.VerticalFlip = false;
	memset(ScreenBuffer, 0, sizeof(ScreenBuffer)); 

	// Need to reset colours in case the palate has been changed.
	pixelcolours[0] = 0xFF000000; 	pixelcolours[1] = 0xFF000000;	pixelcolours[2] = 0xFF888888;	pixelcolours[3] = 0xFFBF3932;	pixelcolours[4] = 0xFFDE7AAE;
	pixelcolours[5] = 0xFF4C3D21;	pixelcolours[6] = 0xFF905F25;	pixelcolours[7] = 0xFFE49452;	pixelcolours[8] =  0xFFEAD979;	pixelcolours[9] = 0xFF537A3B;
	pixelcolours[10] = 0xFFABD54A;	pixelcolours[11] = 0xFF252E38;	pixelcolours[12] =  0xFF00467F;	pixelcolours[13] = 0xFF68ABCC;	pixelcolours[14] =  0xFFBCDEE4;
	pixelcolours[15] =  0xFFFFFFFF;

	CPU_LOG("Video Reset");

}
extern int prev_v_cycle;

void DestroyDisplay() {
	SDL_DestroyTexture(texture);
	SDL_FreeSurface(SDL_Display);
	SDL_DestroyRenderer(renderer);
}
void InitDisplay(int width, int height, HWND hWnd)
{
	int flags;
	if(SDL_Init(SDL_INIT_VIDEO) < 0) 
	{
       MessageBox(hWnd, "Failed to INIT SDL", "Error", MB_OK);
	}

	screen = SDL_CreateWindow("SDL Window",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		width, height,
		SDL_WINDOW_MAXIMIZED | SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL);
	
	struct SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);

	if(-1 == SDL_GetWindowWMInfo(screen, &wmInfo))
		 MessageBox(hWnd, "Failed to get WMInfo SDL", "Error", MB_OK);

	hwndSDL = wmInfo.info.win.window;
	flags = SDL_RENDERER_ACCELERATED;
	if (MenuVSync)
	{
		flags |= SDL_RENDERER_PRESENTVSYNC;
	}

	renderer = SDL_CreateRenderer(screen, -1, flags);
	

	SDL_Display = SDL_CreateRGBSurface(0, width, height, 32, 0xff0000, 0xff00, 0xff, 0xff000000);
	
	SetParent(hwndSDL, hWnd);
	SetWindowPos(hwndSDL, HWND_TOP , 0, 0, width, height, NULL);
	prev_v_cycle = SDL_GetTicks();


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

		if(xstart > 319)
		{
			StartMemSkip = ((xstart - 319));
			xstart -= ((xstart - 319)) - (StartMemSkip & 0x1);
		}
		if(xend < 0)
		{
			EndMemSkip = ((0 - xend)) / 2;
			xend = -1;
		}
	}
	else
	{		
		xstart = X;
		xend = X+SpriteSet.Width;
		if(xend <= xstart || xstart >= 320 || xend < 0) return;

		if(xend > 320)
		{
			EndMemSkip = (xend - 319) / 2 ; //Mem inc is done just before the end skip so we do 320 - 1
			xend = 320;
		}
		else if(xstart < 0)
		{
			StartMemSkip = (0 - xstart);
			xstart = 0 - (StartMemSkip & 0x1);
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

		if(ystart > 239)
		{
			MemAddr += (((X+SpriteSet.Width) - X) / 2) * abs(240 - ystart);
			ystart = 239;
		}

		if(yend < 0) yend = -1;
	}
	else
	{
		ystart = Y;
		yend = Y+SpriteSet.Height;

		if(yend <= ystart || ystart >= 240 || yend < 0)	return;

		if(yend > 240)
		{
			yend = 240;
		}
		else if(ystart < 0)
		{
			MemAddr += (((X+SpriteSet.Width) - X) / 2) * abs(ystart);
			ystart = 0;
		}
	}	

	CPU::Flag.CarryBorrow = 0;
	
	for(int i = ystart; i != yend;){
		MemAddr += StartMemSkip >> 1;
		
		j = xstart;
		if((StartMemSkip & 0x1)) j+=xinc;

		for(; j != xend;)
		{	
			MemPos = ((xstart - j) & 0x1);

			curpixel = CPU::ReadMem8(MemAddr & 0xffff);
			curpixel = curpixel >> ((~MemPos & 0X1) << 2) & 0xf;

							
			//Never do anything with transparent textures! causes oddities in snafu
			if(curpixel > 0)
			{
				if(ScreenBuffer[j][i] != 0) CPU::Flag.CarryBorrow = 1;	//Check collision
							
				ScreenBuffer[j][i] = curpixel;	
			}
			
			//CPU_LOG("%x", curpixel);
			j+=xinc;
			MemAddr += MemPos;
		}

		MemAddr += EndMemSkip;
		i += yinc;
	}
	//FPS_LOG("Cycles to render %d x %d Sprite %d\n", SpriteSet.Height, SpriteSet.Width, count2 - count1);
}


void RedrawLastScreen()
{
	//FPS_LOG("Starting redraw");
	unsigned short scale = 1; //Used for when the screen is bigger :P

	scale = SCREEN_WIDTH / 320;
	SDL_Rect rect = {0,0,SCREEN_WIDTH,SCREEN_HEIGHT};
	SDL_FillRect(SDL_Display, &rect, pixelcolours[SpriteSet.BackgroundColour]);
	unsigned short u_Scale = (unsigned short)(scale - 1);

	for(unsigned short i = 0; i < 240; i++){
		
		for(unsigned short j = 0; j < 320; j++){	
		
			if(ScreenBuffer[j][i] != 0)
			{
				rect = {j*scale,i*scale,scale,scale};
				SDL_FillRect(SDL_Display, &rect, pixelcolours[ScreenBuffer[j][i]]);
				
				//Time for some dodgy filtering!
				if(Smoothing == 1 && ScreenBuffer[j][i] > 1)
				{
					int alpha = 0x5a000000;
					//upper left
					if(ScreenBuffer[j-1][i] == ScreenBuffer[j][i] && ScreenBuffer[j][i-1] == ScreenBuffer[j][i]/* && ScreenBuffer[j - 1][i - 1] == SpriteSet.BackgroundColour*/)
					{						
						if (ScreenBuffer[j - 1][i - 1] == SpriteSet.BackgroundColour) {
							alpha = 0x5a000000;
						}
						else {
							alpha = 0;
						}
						SDL_Rect trect = {((j-1)*scale)+u_Scale,((i-1)*scale)+ u_Scale,1,1 };
						SDL_FillRect(SDL_Display, &trect, pixelcolours[ScreenBuffer[j][i]] - alpha);
						if (u_Scale > 1) {
							SDL_Rect trect2 = { ((j - 1)*scale) + u_Scale,((i - 1)*scale) + 1,1,1 };
							SDL_FillRect(SDL_Display, &trect2, pixelcolours[ScreenBuffer[j][i]] - alpha);
							SDL_Rect trect3 = { ((j - 1)*scale)+1,((i - 1)*scale) + u_Scale,1,1 };
							SDL_FillRect(SDL_Display, &trect3, pixelcolours[ScreenBuffer[j][i]] - alpha);
						}
					}
					
					//lower left
					if(ScreenBuffer[j-1][i] == ScreenBuffer[j][i] && ScreenBuffer[j][i+1] == ScreenBuffer[j][i]/* && ScreenBuffer[j - 1][i + 1] == SpriteSet.BackgroundColour*/)
					{
						if (ScreenBuffer[j - 1][i + 1] == SpriteSet.BackgroundColour) {
							alpha = 0x5a000000;
						}
						else {
							alpha = 0;
						}
						SDL_Rect trect = {((j-1)*scale)+u_Scale,((i+1)*scale),1,1 };
						SDL_FillRect(SDL_Display, &trect, pixelcolours[ScreenBuffer[j][i]] - alpha);
						if (u_Scale > 1) {
							SDL_Rect trect2 = { ((j - 1)*scale) + 1,((i + 1)*scale),1,1 };
							SDL_FillRect(SDL_Display, &trect2, pixelcolours[ScreenBuffer[j][i]] - alpha);
							SDL_Rect trect3 = { ((j - 1)*scale) + u_Scale,((i + 1)*scale)+1,1,1 };
							SDL_FillRect(SDL_Display, &trect3, pixelcolours[ScreenBuffer[j][i]] - alpha);
							
						}
					}
					
					
					//upper right
					if(ScreenBuffer[j][i-1] == ScreenBuffer[j][i] && ScreenBuffer[j+1][i] == ScreenBuffer[j][i]/* && ScreenBuffer[j+1][i-1] == SpriteSet.BackgroundColour*/)
					{
						if (ScreenBuffer[j + 1][i - 1] == SpriteSet.BackgroundColour) {
							alpha = 0x5a000000;
						}
						else {
							alpha = 0;
						}
						SDL_Rect trect = {(j+1)*scale,((i-1)*scale)+u_Scale,1,1 };
						SDL_FillRect(SDL_Display, &trect, pixelcolours[ScreenBuffer[j][i]] - alpha);
						if (u_Scale > 1) {
							SDL_Rect trect2 = { ((j + 1)*scale),((i - 1)*scale)+1,1,1 };
							SDL_FillRect(SDL_Display, &trect2, pixelcolours[ScreenBuffer[j][i]] - alpha);
							SDL_Rect trect3 = { ((j + 1)*scale) + 1,((i - 1)*scale) + u_Scale,1,1 };
							SDL_FillRect(SDL_Display, &trect3, pixelcolours[ScreenBuffer[j][i]] - alpha);

						}
					}
					
					//lower right
					if (ScreenBuffer[j + 1][i] == ScreenBuffer[j][i] && ScreenBuffer[j][i + 1] == ScreenBuffer[j][i]/* && ScreenBuffer[j + 1][i + 1] == SpriteSet.BackgroundColour*/)
					{
						if (ScreenBuffer[j + 1][i + 1] == SpriteSet.BackgroundColour) {
							alpha = 0x5a000000;
						}
						else {
							alpha = 0;
						}
						SDL_Rect trect = { (j + 1)*scale,(i + 1)*scale,1,1 };
						SDL_FillRect(SDL_Display, &trect, pixelcolours[ScreenBuffer[j][i]] - alpha);
						if (u_Scale > 1) {
							SDL_Rect trect2 = { ((j + 1)*scale)+1,((i + 1)*scale),1,1 };
							SDL_FillRect(SDL_Display, &trect2, pixelcolours[ScreenBuffer[j][i]] - alpha);
							SDL_Rect trect3 = { ((j + 1)*scale),((i + 1)*scale) + 1,1,1 };
							SDL_FillRect(SDL_Display, &trect3, pixelcolours[ScreenBuffer[j][i]] - alpha);

						}
					}
				}
			}
		}				
	}
	texture = SDL_CreateTextureFromSurface(renderer, SDL_Display);
	//FPS_LOG("End redraw");
}

void EndDrawing()
{
	//CPU_LOG("End Scene");

	drawing = false;
		
	if( SDL_MUSTLOCK( SDL_Display ) ) { 
		SDL_UnlockSurface(SDL_Display);
	}
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	
	SDL_RenderPresent(renderer);
	SDL_DestroyTexture(texture);
}

void StartDrawing()
{
	//CPU_LOG("Start Scene");
	if( SDL_MUSTLOCK( SDL_Display ) ) 
	{ 
		SDL_LockSurface( SDL_Display ); 
	}
}
