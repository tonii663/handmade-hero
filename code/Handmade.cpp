#include "Handmade.h"

inline uint32
SafeTruncateU64(uint64 value)
{
	ASSERT(value <= 0xFFFFFFFF);
	uint32 result = (uint32)value;
	return result;
}

internal void
RenderWeirdGradient(GameOffScreenBuffer* buffer, int xOffset, int yOffset)
{
	uint8* row = (uint8*)buffer->Memory;

	for(int y = 0; y < buffer->Height; y++)
	{
		uint32* pixel = (uint32*)row;
		for (int x = 0; x < buffer->Width; x++)
		{
			// NOTE(afb) :: Changed from gradient because my screen
			// equal big gayyy.
			int32 red = 232 << 16;
			int32 green = (xOffset + 228) << 8;
			int32 blue = (yOffset + 155);
			*pixel = green | blue | red;
			pixel++;
		}
		row += buffer->Pitch;
	}
}

internal void
OutputSound(SoundBuffer* soundBuffer, int hz)
{
	local_persist int count = 0;
	local_persist f32 tSine = 0.0f;
	int32 toneVolume = 4000;
	int32 toneHz = 256;
	
	int wavePeriod = (soundBuffer->SamplesPerSecond/toneHz);
	
	int16* sampleOut = soundBuffer->Data;
	for(int32 sampleIndex = 0; sampleIndex < soundBuffer->SampleCount; sampleIndex++)
	{
		f32 sineValue = sinf(tSine);
		int16 sampleValue = (int16)(toneVolume * sineValue);
		
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;
		
		tSine += 2 * PI32 * 1.0f / (f32)wavePeriod;	   
	}
	count++;
}

internal void
UpdateAndRender(GameMemory* gameMemory, Input* input, GameOffScreenBuffer* buffer,
				SoundBuffer* soundBuffer)
{
	
	ASSERT(ARRAY_COUNT(input->Controllers[0].Buttons) ==
		   ((int)(&input->Controllers[0].Back - &input->Controllers[0].MoveUp)+1));
	ASSERT(gameMemory->PersistentMemorySize >= sizeof(GameState));
	
	DebugFile file = {};
	GameState* state = (GameState*)gameMemory->PersistentMemory;
	if(!gameMemory->IsInitialized)
	{
		char* fileName = __FILE__;
		file = DEBUGPlatformReadFile(fileName);
		if(file.Data)
		{
			DEBUGPlatformWriteFile("test.out", file.Size, file.Data);
			DEBUGPlatformFreeFileMemory(&file);
		}
		
		state->ToneHz = 256;
		gameMemory->IsInitialized = true;
	}

	for(int controllerIndex = 0;
		controllerIndex < ARRAY_COUNT(input->Controllers);
		controllerIndex++)
	{
		ControllerState* controllerInput = GetController(input, controllerIndex);
		if(controllerInput->IsAnalog)
		{
			// NOTE(afb) :: Analog input tuning
			state->ToneHz = 256 + (int)
				(128.0f * controllerInput->LeftStickAverageY);
			state->YOffset += (int)
				(4.0f * controllerInput->LeftStickAverageX);
		}
		else
		{
			// NOTE(afb) :: Digital input tuning
			if(controllerInput->MoveLeft.EndedDown)
			{
				state->YOffset--;
			}
			else if(controllerInput->MoveRight.EndedDown)
			{
				state->YOffset++;
			}
		}

		//Input.AButtonalfTransitionCount;
		if(controllerInput->ActionDown.EndedDown)
		{
			state->XOffset++;
		}
		else if(controllerInput->ActionUp.EndedDown)
		{
			state->XOffset--;
		}
	}
	OutputSound(soundBuffer, state->ToneHz);
	RenderWeirdGradient(buffer, state->XOffset, state->YOffset);
}
