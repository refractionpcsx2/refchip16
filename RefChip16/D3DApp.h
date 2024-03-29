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

#ifndef _D3DAPP_H
#define _D3DAPP_H

#include <SDL.h>
#include <SDL_syswm.h>

extern unsigned short SCREEN_WIDTH;
extern unsigned short SCREEN_HEIGHT;

extern char MenuVSync;
extern char Smoothing;
struct Sprite
{
	int Width;
	int Height;
	bool VerticalFlip;
	bool HorizontalFlip;
	int BackgroundColour;
};

void DestroyDisplay();
void InitDisplay(int width, int height, HWND hWnd);
void DrawSprite(unsigned short MemAddr, int X, int Y);
void EndDrawing();
void StartDrawing();
void D3DReset();
void RedrawLastScreen();
		


#endif
