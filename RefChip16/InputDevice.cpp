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
#include <iostream>
#include "InputDevice.h"

using namespace std;
#pragma comment (lib, "dinput8.lib")
#pragma comment (lib, "dxguid.lib")

//extern D3DApp *RefChip16D3D;
//extern CPU *RefChip16CPU;

InputDevice::InputDevice(HINSTANCE hInstance, HWND hWnd)
{
	//Create Direct Input Object
	DirectInput8Create(hInstance, DIRECTINPUT_VERSION,
        IID_IDirectInput8, (void**)&m_directinputObj, NULL);

	//Register a keyboard device
	m_directinputObj->CreateDevice(GUID_SysKeyboard, &m_directinputKeyboard, NULL); 
	//Set data format to Keyboard (O_o)
	m_directinputKeyboard->SetDataFormat( &c_dfDIKeyboard );
	//Tell it we want full foreground control
	m_directinputKeyboard->SetCooperativeLevel(hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE); 

}

InputDevice::~InputDevice()
{
	if(m_directinputKeyboard)
	{
		m_directinputKeyboard->Unacquire();
		m_directinputKeyboard->Release();
	}
	
	if(m_directinputObj)
		m_directinputObj->Release();

}
void InputDevice::UpdateControls()
{
	BYTE    keys[256];
	unsigned short KeysPressed = 0;
	m_directinputKeyboard->Acquire();

	// Get the input's device state, and put the state in keys - zero first
	ZeroMemory(keys, sizeof(keys) );
	m_directinputKeyboard->GetDeviceState( sizeof(keys), keys );
	

	/*Player One Keys*/
	
	if(keys[DIK_UP]) KeysPressed |= 1; //Up
	else KeysPressed &= ~1;
	if(keys[DIK_DOWN]) KeysPressed |= 1<<1; //Down
	else KeysPressed &= ~(1<<1);
	if(keys[DIK_LEFT]) KeysPressed |= 1<<2; //Left
	else KeysPressed &= ~(1<<2);
	if(keys[DIK_RIGHT]) KeysPressed |= 1<<3; //Right
	else KeysPressed &= ~(1<<3);
	if(keys[DIK_RSHIFT]) KeysPressed |= 1<<4; //Select
	else KeysPressed &= ~(1<<4);
	if(keys[DIK_RETURN]) KeysPressed |= 1<<5; //Start
	else KeysPressed &= ~(1<<5);
	if(keys[DIK_Z]) KeysPressed |= 1<<6; //A Button
	else KeysPressed &= ~(1<<6);
	if(keys[DIK_X]) KeysPressed |= 1<<7; //B Button
	else KeysPressed &= ~(1<<7);
	CPU::WriteMem(0xFFF0, KeysPressed);

	KeysPressed = 0;

	/*Player Two Keys*/
	if(keys[DIK_W]) KeysPressed |= 1; //Up
	else KeysPressed &= ~1;
	if(keys[DIK_S]) KeysPressed |= 1<<1; //Down
	else KeysPressed &= ~(1<<1);
	if(keys[DIK_A]) KeysPressed |= 1<<2; //Left
	else KeysPressed &= ~(1<<2); 
	if(keys[DIK_D]) KeysPressed |= 1<<3; //Right
	else KeysPressed &= ~(1<<3);
	if(keys[DIK_LCONTROL]) KeysPressed |= 1<<4; //Select
	else KeysPressed &= ~(1<<4);
	if(keys[DIK_LSHIFT]) KeysPressed |= 1<<5; //Start
	else KeysPressed &= ~(1<<5);
	if(keys[DIK_R]) KeysPressed |= 1<<6; //A Button
	else KeysPressed &= ~(1<<6);
	if(keys[DIK_T]) KeysPressed |= 1<<7; //B Button
	else KeysPressed &= ~(1<<7);

	CPU::WriteMem(0xFFF2, KeysPressed);
}