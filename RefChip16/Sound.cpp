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
#include "Sound.h"
using namespace CPU;
#define CPU_LOG __Log
//extern CPU *RefChip16CPU;
const double MAX_VOLUME = 32760;
float PI = atan(1.0f)*4; //PI yummy
bool SoundDisabled = false;

struct DSP DSPSettings = { 
		TRIANGLE, //Waveform
		0,		//Frequency
		0.5f,	//Master Amplitude - 50% by default
		0x3FFC, //Amplitude (for sample generation) at 50% by default.
		0,      //SampleNo
		0,		//Length
		0,		//Amplitude Step
		1.0f,	//Angle (for Squarewave)
		0,		//TempSample
		0,		//CurAttack
		0,		//CurDecay
		15,		//CurSustain (volume)
		0,		//CurRelease
		//ADSR Filter
		//0       1       2        3      4        5       6        7      8        9       10       11     12       13      14       15
		{2      , 8     , 16     , 24   , 38     , 56    , 68     , 80   , 100    , 250   , 500    , 800  , 1000   , 3000  , 5000   , 8000}, //Attack ms
		{6      , 24    , 48     , 72   , 114    , 168   , 204    , 240  , 300    , 750   , 1500   , 2400 , 3000   , 9000  , 15000  , 24000}, //Decay ms
		{0.0625f, 0.125f, 0.1875f, 0.25f, 0.3125f, 0.375f, 0.4375f, 0.50f, 0.5625f, 0.625f, 0.6875f, 0.75f, 0.8125f, 0.875f, 0.9375f, 1.0f}, //Sustain volume
		{6      , 24    , 48     , 72   , 114    , 168   , 204    , 240  , 300    , 750   , 1500   , 2400 , 3000   , 9000  , 15000  , 24000}  //Release ms
	};

SoundDevice::SoundDevice(HWND hWnd)
{
	pbWaveData = new BYTE[ 9600000 ]; // room for at least a couple of minutes:)
	//pbWaveData = (unsigned char*)wavedata;
	InitXAudio(hWnd);
}
SoundDevice::~SoundDevice()
{
//	pSourceVoice->Stop( 0 );
	delete pbWaveData;

	if(pSourceVoice && !SoundDisabled)
		pSourceVoice->DestroyVoice();
	if(pMasterVoice && !SoundDisabled)
		pMasterVoice->DestroyVoice();
	if(pXAudio2)
		pXAudio2->Release();
}

int SoundDevice::InitXAudio(HWND hWnd)
{
	HRESULT hr;

	if ( FAILED(hr = CoInitializeEx( 0, COINIT_APARTMENTTHREADED )))
	{
		CPU_LOG("Coinit Failed");
		SoundDisabled = true;
	}

	if ( FAILED(hr = XAudio2Create( &pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) )
	{
		CPU_LOG("Audio Processor Failed");
		SoundDisabled = true;
		return hr;
	}

	
	if ( FAILED(hr = pXAudio2->CreateMasteringVoice( &pMasterVoice ) ) )
	{
		CPU_LOG("Master voice Failed");
		SoundDisabled = true;
		return hr;
	}
	
	waveformat.wBitsPerSample= 16;
	waveformat.nSamplesPerSec = 44100;
	waveformat.wFormatTag = WAVE_FORMAT_PCM;
	waveformat.nChannels = 1;
	waveformat.cbSize = 0;	 //Ignored by Wave PCM format
	waveformat.nBlockAlign = waveformat.nChannels * (waveformat.wBitsPerSample / 8); //Total number of bytes per sample for all channels (16 bits / 8 = 2 bytes * 1 channel = 2 bytes a block)
	waveformat.nAvgBytesPerSec = waveformat.nBlockAlign * waveformat.nSamplesPerSec;  //Bytes per second is blocksize * sample rate. here we have 22050 lots of 2 bytes per second (44100 bytes)
	
	if( FAILED(hr = pXAudio2->CreateSourceVoice( &pSourceVoice, (WAVEFORMATEX*)&waveformat ) ) ) 
	{
		CPU_LOG("Source voice Failed");
		SoundDisabled = true;
			return hr;
	}	
	
	return 0;
}

void SoundDevice::StopVoice()
{
	if(SoundDisabled) return;

	pSourceVoice->GetState(&pVoiceState);
	if(pVoiceState.BuffersQueued > 0) 
	{
		CPU_LOG("Stopping %d voice(s)!\n", pVoiceState.BuffersQueued);
		pSourceVoice->Stop( 0 );//
		pSourceVoice->FlushSourceBuffers();		
	} else CPU_LOG("Stop voice called, nothing playing though\n");
}

void SoundDevice::SetADSR(int Attack, int Decay, int Sustain, int Release, int Volume, int Type)
{
	DSPSettings.CurAttack = Attack;
	DSPSettings.CurDecay = Decay;
	DSPSettings.CurSustain = Sustain;
	DSPSettings.CurRelease = Release;
	DSPSettings.Amplitude = (int)(DSPSettings.Sustain[Volume] * MAX_VOLUME);
	DSPSettings.WaveformType = Type;
	CPU_LOG("ADSR Set A %d ms, D %d ms, S %d, R %d ms, Volume %d, Wavetype %d(%d)\n", DSPSettings.Attack[Attack], DSPSettings.Decay[Decay], (int)DSPSettings.Sustain[Sustain], DSPSettings.Release[Release], DSPSettings.Amplitude, DSPSettings.WaveformType, Type);
}
short SoundDevice::GenerateTriangleSample()
{
	// Negate ampstep whenever it hits the amplitude boundary

    if (DSPSettings.SampleNo >= DSPSettings.Frequency)
	{
		DSPSettings.SampleNo = 0;
        DSPSettings.AmpStep = -DSPSettings.AmpStep;
	}

	DSPSettings.TempSample += DSPSettings.AmpStep;
	DSPSettings.SampleNo++;
    return DSPSettings.TempSample;

}
                    
short SoundDevice::GenerateSawtoothSample()
{
	
	if (DSPSettings.SampleNo >= DSPSettings.Frequency)
	{
		DSPSettings.SampleNo = 0;
		DSPSettings.TempSample = (short)-DSPSettings.Amplitude;
	}
	
	DSPSettings.SampleNo++;
	DSPSettings.TempSample += DSPSettings.AmpStep;
	//CPU_LOG("Frequency %d", DSPSettings.TempSample);
	return DSPSettings.TempSample;
}

short SoundDevice::GeneratePulseSample()
{
	//Im no audio expert, but this returns a really wobbly wave, maybe thats just a shortfall of PCM...
	if(DSPSettings.Amplitude * sin(DSPSettings.SampleNo * DSPSettings.Angle) >= 0.0f)
		return (short)DSPSettings.Amplitude;
	else 
		return (short)-DSPSettings.Amplitude; 
}


//Noise Generator - We're just using random values here for white noise, but! we can control the pitch by making it only do 1 sample per hz :)
short SoundDevice::GenerateNoiseSample()
{
	DSPSettings.SampleNo++;
	
	if(DSPSettings.SampleNo == DSPSettings.Frequency)
	{
		DSPSettings.TempSample = (rand() % (DSPSettings.Amplitude * 2)) - DSPSettings.Amplitude;
		DSPSettings.SampleNo = 0;
	}
	
	return DSPSettings.TempSample;
}
void SoundDevice::GenerateHz(int Rate, int Period)
{
	short *WaveData = (short*)pbWaveData;
	int CurSample = 0;
	double VolumeModifier = 1.0f;
	double SampleLength = (double)(44100 / 1000);  //44100 samples per second / 1000 ms = 44.1 samples every ms * Period to play for
	int Volume = 0;
	int SamplePeriod = (int)(Period * SampleLength);
	int AttackSamples = (int)(SampleLength * DSPSettings.Attack[DSPSettings.CurAttack]);
	int DecaySamples = (int)(SampleLength * DSPSettings.Decay[DSPSettings.CurDecay]);
	int ReleaseSamples = (int)(SampleLength * DSPSettings.Release[DSPSettings.CurRelease]);
	
	DSPSettings.Length = 0;

	if(SoundDisabled) return;

	StopVoice();


	//Make sure we have a rate
	if(Rate == 0) return;
	
	//Setup! - We need to set some of the DSP options before we can process the sound

	switch(DSPSettings.WaveformType)
	{		
		case SAWTOOTH:	
			DSPSettings.Frequency = waveformat.nSamplesPerSec / Rate;
			DSPSettings.AmpStep = (short)((DSPSettings.Amplitude *2) / DSPSettings.Frequency);
			DSPSettings.TempSample = (short)-DSPSettings.Amplitude;
		break;

		case PULSE:
			DSPSettings.Angle = (double)((PI * 2 * Rate) / waveformat.nSamplesPerSec); // Works out the increment speed for the SIN over the sample length
		break;

		case NOISE:
			DSPSettings.TempSample = 0;
			DSPSettings.SampleNo = 0;
			DSPSettings.Frequency = waveformat.nSamplesPerSec / Rate; //Keep our samples per Hz for further calculation
		break;

		case TRIANGLE:			
		default:
			DSPSettings.Frequency = (waveformat.nSamplesPerSec / Rate)/2; //Brings the octive in line with the others,  not sure why its lower, twice as long i guess..
			DSPSettings.AmpStep = (short)((DSPSettings.Amplitude *2) / DSPSettings.Frequency); //Work out the increase in volume (or decrease) per sample.
			DSPSettings.TempSample = 0;
			DSPSettings.SampleNo = DSPSettings.Frequency / 2;
		break;
	}

	CPU_LOG("Processing Sound, A %dms D %dms S %d R %dms Period %dms Rate %dhz Type %d\n", DSPSettings.Attack[DSPSettings.CurAttack], DSPSettings.Decay[DSPSettings.CurDecay], (int)(DSPSettings.Amplitude * DSPSettings.Sustain[DSPSettings.CurSustain]), DSPSettings.Release[DSPSettings.CurRelease], Period, Rate, DSPSettings.WaveformType);
	
	//Attack first
	
	Volume = 0;

	for(CurSample = 0; CurSample < AttackSamples && CurSample < SamplePeriod; CurSample++)
	{		
		switch(DSPSettings.WaveformType)
		{		
			case SAWTOOTH:	
				WaveData[CurSample] = GenerateSawtoothSample();
			break;

			case PULSE:
				DSPSettings.SampleNo = CurSample;
				WaveData[DSPSettings.SampleNo] = GeneratePulseSample();
			break;

			case NOISE:
				WaveData[CurSample] = GenerateNoiseSample();
			break;

			case TRIANGLE:			
			default:
				WaveData[CurSample] = GenerateTriangleSample();
			break;
		}
		
		//This is a bit of a pig, but at some frequencies, we lose steps
		VolumeModifier = (double)((1.0f / (double)AttackSamples ) * (double)CurSample);
		Volume = (int)(DSPSettings.Amplitude * VolumeModifier);

		WaveData[CurSample] = (short)(WaveData[CurSample] * ((double)Volume / (double)DSPSettings.Amplitude));		
		//CPU_LOG("Attack Vol %x Sample %x\n", Volume, WaveData[CurSample]);
	}
	DSPSettings.Length += CurSample; //Log this for the start of the next part of the ADSR check

	//Process Decay	
	double VolumeDecay = (1.0f / Volume) * (Volume - (int)(DSPSettings.Amplitude * DSPSettings.Sustain[DSPSettings.CurSustain]));
			
	for(CurSample = 0; CurSample < DecaySamples && (CurSample + DSPSettings.Length) < SamplePeriod; CurSample++)
	{
		switch(DSPSettings.WaveformType)
		{		
			case SAWTOOTH:	
				WaveData[CurSample + DSPSettings.Length] = GenerateSawtoothSample();
			break;

			case PULSE:
				DSPSettings.SampleNo = CurSample + DSPSettings.Length;
				WaveData[DSPSettings.SampleNo] = GeneratePulseSample();
			break;

			case NOISE:
				WaveData[CurSample + DSPSettings.Length] = GenerateNoiseSample();
			break;

			case TRIANGLE:			
			default:
				WaveData[CurSample + DSPSettings.Length] = GenerateTriangleSample();
			break;
		}
		VolumeModifier = 1.0f - (double)((VolumeDecay / (double)DecaySamples ) * (double)CurSample);
		Volume = (int)(DSPSettings.Amplitude * VolumeModifier);
		
		WaveData[CurSample + DSPSettings.Length] = (short)(WaveData[CurSample + DSPSettings.Length] * ((double)Volume / (double)DSPSettings.Amplitude));	
		//CPU_LOG("Decay Vol %x Sample %x\n", Volume, WaveData[CurSample]);
	}

	//Process Sustain
	for(CurSample = 0; CurSample + DSPSettings.Length < SamplePeriod; CurSample++)
	{
		

		switch(DSPSettings.WaveformType)
		{		
			case SAWTOOTH:	
				WaveData[CurSample + DSPSettings.Length] = GenerateSawtoothSample();
			break;

			case PULSE:
				DSPSettings.SampleNo = CurSample + DSPSettings.Length;
				WaveData[DSPSettings.SampleNo] = GeneratePulseSample();
			break;

			case NOISE:
				WaveData[CurSample + DSPSettings.Length] = GenerateNoiseSample();
			break;

			case TRIANGLE:			
			default:
				WaveData[CurSample + DSPSettings.Length] = GenerateTriangleSample();
			break;
		}
		
		WaveData[CurSample + DSPSettings.Length] = (short)(WaveData[CurSample + DSPSettings.Length] * DSPSettings.Sustain[DSPSettings.CurSustain]);

		//CPU_LOG("Sustain Vol %x Sample %x\n", Volume, WaveData[CurSample]);
	}

	DSPSettings.Length += CurSample; //Log this for the start of the next part of the ADSR check

	//Process Release
	for(CurSample = 0; CurSample < ReleaseSamples; CurSample++) //Ignoring the sample length as Key-Off has happened
	{
		switch(DSPSettings.WaveformType)
		{		
			case SAWTOOTH:	
				WaveData[CurSample + DSPSettings.Length] = GenerateSawtoothSample();
			break;

			case PULSE:
				DSPSettings.SampleNo = CurSample + DSPSettings.Length;
				WaveData[DSPSettings.SampleNo] = GeneratePulseSample();
			break;

			case NOISE:
				WaveData[CurSample + DSPSettings.Length] = GenerateNoiseSample();
			break;

			case TRIANGLE:			
			default:
				WaveData[CurSample + DSPSettings.Length] = GenerateTriangleSample();
			break;
		}

		VolumeModifier = 1.0f - (double)((1.0f / (double)ReleaseSamples ) * (double)CurSample);
		Volume = (int)((DSPSettings.Amplitude * DSPSettings.Sustain[DSPSettings.CurSustain]) * VolumeModifier);
		
		WaveData[CurSample + DSPSettings.Length] = (short)(WaveData[CurSample + DSPSettings.Length]  * ((double)Volume / (double)DSPSettings.Amplitude));
		//CPU_LOG("Release Vol %x Sample %x\n", Volume, WaveData[CurSample]);
	}

	DSPSettings.Length += CurSample; //Log this for the start of the next part of the ADSR check

	XAUDIO2_BUFFER buffer={0};
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.pAudioData = pbWaveData;
	buffer.AudioBytes = DSPSettings.Length*2;
	buffer.PlayBegin = 0;	
	//We dont want to loop so lets set our buffer to no loop
	buffer.LoopBegin = 0;
	buffer.LoopLength = 0;
	buffer.LoopCount = 0;
	
	
	pSourceVoice->SubmitSourceBuffer( &buffer );
	pSourceVoice->Start( 0 );
}