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

#ifndef _SOUND_H
#define _SOUND_H

#include <iostream>
#include <xaudio2.h>

#define TRIANGLE 0
#define SAWTOOTH 1 
#define PULSE    2 //Square for now
#define NOISE    3

struct DSP
{
	int WaveformType; // 0 = Triangle, 1 = Sawtooth, 2 = Pulse, 3 = Noise
	int Frequency; //In Hz
	double MasterVolume; // 1 = 100%, -1 is -100% but used in square wave generation.
	int Amplitude; //0 - 0x7ff8
	int SampleNo;
	int Length; // Length in ms of playtime
	short AmpStep;
	double Angle;
	short TempSample;
	int CurAttack;
	int CurDecay;
	int CurSustain;
	int CurRelease;
	int Attack[16]; //Time from key on it takes to reach full amplitude.
	int Decay[16]; // Time it takes to drop from full amplitude to the value of sustain
	double Sustain[16]; //The volume at which a majority of the tone is played
	int Release[16]; //After key off, how long the sample is continued to play for as amplitude drops
};


class SoundDevice
{
	public:
		SoundDevice(HWND hWnd);
		~SoundDevice();
		int InitXAudio(HWND hWnd);
		void GenerateHz(int Rate, int Period);		
		void StopVoice();		
		void SetADSR(int Attack, int Decay, int Sustain, int Release, int Volume, int Type);
	private:
		BYTE* pbWaveData;
		IXAudio2* pXAudio2;
		IXAudio2MasteringVoice* pMasterVoice;
		IXAudio2SourceVoice* pSourceVoice;
		WAVEFORMATEX waveformat;
		XAUDIO2_VOICE_STATE pVoiceState;
		short GenerateTriangleSample();
		short GenerateSawtoothSample();
		short GeneratePulseSample();
		short GenerateNoiseSample();

};

#endif