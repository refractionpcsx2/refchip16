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

#ifndef _INPUTDEVICE_H
#define _INPUTDEVICE_H

#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>




class InputDevice
{
	public:
		InputDevice(HINSTANCE hInstance, HWND hWnd);
		~InputDevice();
		void UpdateControls();

	private:
		LPDIRECTINPUT8  m_directinputObj;
		LPDIRECTINPUTDEVICE8 m_directinputKeyboard;
};

#endif