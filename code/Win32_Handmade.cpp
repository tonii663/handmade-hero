#include <math.h>
#include <stdint.h>

#define local_persist	 static
#define global_variable	 static
#define internal         static

typedef uint8_t  uint8;
typedef uint16_t uint16p;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float 	f32;
typedef double 	f64;

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

#define PI32 3.141592653589793238462f

#include "Handmade.cpp"

#include <malloc.h>
#include <windows.h>
#include <xinput.h>
#include <dsound.h>

#if HANDMADE_INTERNAL
#include <stdio.h>
#endif

#include "Win32_Handmade.h"

#if HANDMADE_DEBUG
#if HANDMADE_WIN32
#undef ASSERT
#define ASSERT(exp)\
	if(!(exp)){DebugBreak();}
#else
#define ASSERT(exp)
#endif
#else
#define ASSERT(exp)
#endif

//====================================================================================
// NOTE(afb) :: XInputGetState Loading
#define X_INPUT_GET_STATE(name) \
	DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)

X_INPUT_GET_STATE(XinputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
typedef X_INPUT_GET_STATE(x_input_get_state);
global_variable x_input_get_state* XInputGetState_ = XinputGetStateStub;

// NOTE(afb) :: XInputSetState Loading
#define X_INPUT_SET_STATE(name)\
	DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)

X_INPUT_SET_STATE(XinputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
typedef X_INPUT_SET_STATE(x_input_set_state);
global_variable x_input_set_state* XInputSetState_ = XinputSetStateStub;

#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_


#define DIRECT_SOUND_CREATE(name)\
	HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter)

DIRECT_SOUND_CREATE(DirectSoundCreateStub)
{
	return(DSERR_NODRIVER);
}
typedef DIRECT_SOUND_CREATE(direct_sound_create);
global_variable direct_sound_create* DirectSoundCreate_ = DirectSoundCreateStub;
#define DirectSoundCreate DirectSoundCreate_
//====================================================================================


global_variable Win32DIBBuffer GlobalBackBuffer; // Video
global_variable	LPDIRECTSOUNDBUFFER GlobalSecondarySoundBuffer; // Audio
global_variable bool GameRunning;
global_variable int64 GlobalPerformaceFrequency;


internal DebugFile
DEBUGPlatformReadFile(char* fileName)
{
	DebugFile result = {};
	HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ,
									NULL, OPEN_EXISTING, NULL, NULL);
	
	if(fileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER fileSize = {};
		if(GetFileSizeEx(fileHandle, &fileSize))
		{
			uint32 fileSize32 = SafeTruncateU64(fileSize.QuadPart);
			DWORD bytesRead;
			result.Data = VirtualAlloc(0, fileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

			if(ReadFile(fileHandle, result.Data, fileSize32, &bytesRead, 0) &&
			   (fileSize32 == bytesRead))
			{
				result.Size = fileSize32;
			}
			else
			{
				DEBUGPlatformFreeFileMemory(&result);
			}
		}
		else
		{
			//NOTE(afb) :: Failure
		}
		CloseHandle(fileHandle);
	}
	else
	{
		//NOTE(afb) :: Failure
	}
	return (result);
}

internal void
DEBUGPlatformFreeFileMemory(DebugFile* file)
{
	if(file->Data)
	{
		VirtualFree(file->Data, 0, MEM_RELEASE);
		file->Data = NULL;
		file->Size = 0;
	}
}

internal bool
DEBUGPlatformWriteFile(char* fileName, uint32 memorySize, void* data)
{
	bool result = false;
	HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, FILE_SHARE_READ,
									NULL, CREATE_ALWAYS, NULL, NULL);
	
	if(fileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD bytesWritten;
		
		if(WriteFile(fileHandle, data, memorySize, &bytesWritten, NULL))
		{
			result = bytesWritten == memorySize;
		}
		else
		{
			// NOTE(afb) :: Failure
		}
		
		CloseHandle(fileHandle);
	}
	else
	{
		//NOTE(afb) :: Failure
	}
	return result;
}


internal void
Win32InitDirectSound(HWND windowHandle, int32 samplesPerSecond, int32 bufferSize)
{
	// NOTE(afb) :: Load DirectSound Library
	HINSTANCE dSoundLibrary = LoadLibraryA("dsound.dll");
	if(dSoundLibrary)
	{
		DirectSoundCreate = (direct_sound_create*)GetProcAddress(dSoundLibrary, "DirectSoundCreate");

		LPDIRECTSOUND directSound = {};
		if(DirectSoundCreate && (SUCCEEDED(DirectSoundCreate(0, &directSound, 0))))
		{
			WAVEFORMATEX waveFormat = {};
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nChannels = 2;
			waveFormat.nSamplesPerSec = samplesPerSecond;
			waveFormat.wBitsPerSample = 16;
			waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample)/8;
			waveFormat.nAvgBytesPerSec = samplesPerSecond *
												waveFormat.nBlockAlign;

			// NOTE(afb) :: Create Direct Sound object.
			if(SUCCEEDED(directSound->SetCooperativeLevel(windowHandle, DSSCL_PRIORITY)))
			{
				// NOTE(afb) :: Create primary buffer


				DSBUFFERDESC bufferDescription = {};
				bufferDescription.dwSize = sizeof(DSBUFFERDESC);
				bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				LPDIRECTSOUNDBUFFER primaryBuffer;

				if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0)))
				{
					if(SUCCEEDED(primaryBuffer->SetFormat(&waveFormat)))
					{
						// NOTE(afb) :: Format set
						OutputDebugStringA("Primary buffer created\n");
					}
					else
					{
						// TODO(afb) :: Diagnostics
					}
				}
				else
				{
				// TODO(afb) :: Diagnostics

				}
			}
			else
			{
				// TODO(afb) :: Diagnostics
			}

			DSBUFFERDESC bufferDescription = {};
			bufferDescription.dwSize = sizeof(DSBUFFERDESC);
			bufferDescription.dwBufferBytes = bufferSize;
			bufferDescription.dwFlags = 0;
			bufferDescription.lpwfxFormat = &waveFormat;

			if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &GlobalSecondarySoundBuffer, 0)))
			{
				OutputDebugStringA("Secondary buffer created\n");
			}
			else
			{
				// TODO(afb) :: Diagnostics

			}
		}
		else
		{
			// TODO(afb) :: Diagnostic
		}
	}
	else
	{
			// TODO(afb) :: Diagnostic
	}
}

internal void
Win32LoadXInputLibrary(void)
{
	HMODULE xInputLibrary = LoadLibraryA("xinput1_3.dll");
	if(xInputLibrary)
	{
		XInputGetState_ = (x_input_get_state*)GetProcAddress(xInputLibrary, "XInputGetState");
		if(!XInputGetState_) XInputGetState_ = XinputGetStateStub;

		XInputSetState_ = (x_input_set_state*)GetProcAddress(xInputLibrary, "XInputSetState");
		if(!XInputSetState_) XInputSetState_ = XinputSetStateStub;

		if(!XInputGetState_ || !XInputSetState_)
		{
			// TODO(afb) :: Diagnostic
			OutputDebugStringA("Failed to load xinput");
		}
	}
	else
	{
			// TODO(afb) :: Diagnostic
	}
}

internal Win32WindowDimension
Win32GetWindowDimension(HWND windowHandle)
{
	RECT clientRect = {};
	Win32WindowDimension dim = {};
	GetClientRect(windowHandle, &clientRect);
	dim.Width = clientRect.right - clientRect.left;
	dim.Height = clientRect.bottom - clientRect.top;
	return dim;
}


internal void
Win32ResizeDIB(Win32DIBBuffer* buffer, int width, int height)
{
	if(buffer->Memory)
	{
		VirtualFree(buffer->Memory, 0, MEM_RELEASE);
	}

	buffer->Width = width;
	buffer->Height = height;
	buffer->BytesPerPixel = 4; // R G B and 1 extra for alignment
	buffer->Pitch = width * buffer->BytesPerPixel; // Pitch is width in bytes

	// NOTE(afb) :: If the height is negative then bitmp is a
	// top-down DIB and its origin(0, 0) is the top left corner.
	buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader);
	buffer->Info.bmiHeader.biWidth = width;
	buffer->Info.bmiHeader.biHeight = -height;
	buffer->Info.bmiHeader.biPlanes = 1;
	buffer->Info.bmiHeader.biBitCount = 32;
	buffer->Info.bmiHeader.biCompression = BI_RGB;

	int32 bitmapMemorySize = buffer->Pitch * buffer->Height;
	buffer->Memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32DisplayBufferToWindow(HDC deviceContext, const Win32DIBBuffer* buffer,
						   int x, int y, int width, int height)
{
	// NOTE(afb) :: Play with stretch modes.kissm
	StretchDIBits(deviceContext,
				  0, 0, width, height,  // Destination
				  0, 0, buffer->Width, buffer->Height, // Source
				  buffer->Memory,
				  &buffer->Info,
				  DIB_RGB_COLORS, SRCCOPY);
}


LRESULT CALLBACK
Win32MessgaeProcedure(HWND windowHandle,
					  UINT message,
					  WPARAM wParam,
					  LPARAM lParam)
{
	LRESULT result = 0;

	switch(message)
	{
		// Window is activated or deactiated
		case (WM_ACTIVATEAPP):
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		}break;

		// Window is resized (minimize or maximize)
		case (WM_SIZE):
		{
			// Getting the drawable area of our window and calculating
			// the width and height.
			if(wParam != 1)
			{
				OutputDebugStringA("Resize not minimize.\n");
				Win32WindowDimension dim = Win32GetWindowDimension(windowHandle);
				Win32ResizeDIB(&GlobalBackBuffer, dim.Width, dim.Height);
			}
		}break;

		case (WM_KEYDOWN):
		case (WM_KEYUP):
		case (WM_SYSKEYDOWN):
		case (WM_SYSKEYUP):
		{
		}break;

		case (WM_PAINT):
		{
			PAINTSTRUCT paint;
			HDC deviceContext = BeginPaint(windowHandle, &paint);

			int x = paint.rcPaint.left;
			int y = paint.rcPaint.top;
			int width = paint.rcPaint.right - x;
			int height = paint.rcPaint.bottom - y;

			// Updating the window
			Win32DisplayBufferToWindow(deviceContext, &GlobalBackBuffer,
									   x, y, width, height);

			EndPaint(windowHandle, &paint);
		}break;

		//Window is closed
		case (WM_CLOSE):
		{
			// TODO(afb) : Handle with a message to the user?
			GameRunning = false;
			OutputDebugStringA("WM_CLOSE\n");
			//PostQuitMessage(0);
		}break;

		//Window is being destroyed
		case (WM_DESTROY):
		{
			// TODO(afb) : Handle as a error?
			GameRunning = false;
			OutputDebugStringA("WM_DESTROY\n");
		}break;

		default:
		{
			// OutputDebugStringA("default\n");
			result = DefWindowProc(windowHandle, message, wParam, lParam);
		}break;
	}
	return(result);
}



internal void
Win32FillSoundBuffer(Win32SoundOutput* destBuffer, DWORD bytesToWrite,
					 DWORD byteToLock, SoundBuffer* sourceBuffer)
{
	VOID* region1;
	VOID* region2;
	DWORD region1Size;
	DWORD region2Size;
	
	if(SUCCEEDED(GlobalSecondarySoundBuffer->Lock(byteToLock, bytesToWrite,
												  &region1, &region1Size,
												  &region2, &region2Size,
												  0)))
	{
		int16* sourceSample = sourceBuffer->Data;

		int16* destSample = (int16*)region1;
		DWORD region1SampleCount = region1Size / destBuffer->BytesPerSample;
		for(DWORD sampleIndex = 0; sampleIndex < region1SampleCount; sampleIndex++)
		{
			*destSample++ = *sourceSample++;
			*destSample++ = *sourceSample++;
			destBuffer->RunningSampleIndex++;
		}

		destSample = (int16*)region2;
		DWORD region2SampleCount = region2Size / destBuffer->BytesPerSample;
		for(DWORD sampleIndex = 0; sampleIndex < region2SampleCount; sampleIndex++)
		{
			*destSample++ = *sourceSample++;
			*destSample++ = *sourceSample++;
			destBuffer->RunningSampleIndex++;
		}
		
		GlobalSecondarySoundBuffer->Unlock(region1, region1Size, region2, region2Size);
	}	
}

internal void
Win32ClearSoundBuffer(Win32SoundOutput* soundData)
{
	
	VOID* region1;
	VOID* region2;
	DWORD region1Size;
	DWORD region2Size;
	
	if(SUCCEEDED(GlobalSecondarySoundBuffer->Lock(0, soundData->BufferSize,
												  &region1, &region1Size,
												  &region2, &region2Size,
												  0)))
	{
		int32* destSample  = (int32*)region1;
		for(DWORD sampleIndex = 0; sampleIndex < region1Size; sampleIndex++)
		{
			*destSample = 0;
		}

		destSample  = (int32*)region2;
		for(DWORD sampleIndex = 0; sampleIndex < region2Size; sampleIndex++)
		{
			*destSample = 0;
		}
		
		GlobalSecondarySoundBuffer->Unlock(region1, region1Size, region2, region2Size);
	}// Lock
}



internal void
Win32ProcessKeyboardMessages(ButtonState* newState, bool isDown)
{
	ASSERT(newState->EndedDown != isDown);
	newState->EndedDown = isDown;
	newState->HalfTransitionCount++;
}

internal void
Win32ProcessXInputDigitalButton(DWORD xInputButtonState, ButtonState* oldState,
								DWORD buttonBit, ButtonState* newState)
{
	newState->EndedDown = (xInputButtonState & buttonBit) == buttonBit;
	newState->HalfTransitionCount = (oldState->EndedDown != newState->EndedDown ) ? 1 : 0;
}

internal void
Win32ProcessPendingMessages(ControllerState* keyboardInput)
{
	MSG message = {};
	while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		switch(message.message)
		{
			case (WM_QUIT):
			{
				GameRunning = false;
			}break;
			
			case (WM_KEYDOWN):
			case (WM_KEYUP):
			case (WM_SYSKEYDOWN):
			case (WM_SYSKEYUP):
			{
				uint32 vkCode = (uint32)message.wParam;
				bool wasDown = ((message.lParam & (1 << 30)) != 0);
				bool isDown = ((message.lParam & (1 << 31)) == 0);

				/* TODO(afb) ::
				 * Try using a dictionary or hash table for the keycode
				 * instead of doing a bunch of if statements.
				 */
				if(wasDown != isDown)
				{
					if(vkCode == 'W')
					{
						Win32ProcessKeyboardMessages(&(keyboardInput->MoveUp), isDown);
					}
					else if(vkCode == 'S')
					{
						Win32ProcessKeyboardMessages(&(keyboardInput->MoveDown), isDown);
					}
					else if(vkCode == 'A')
					{
						Win32ProcessKeyboardMessages(&(keyboardInput->MoveLeft), isDown);
					}
					else if(vkCode == 'D')
					{
						Win32ProcessKeyboardMessages(&(keyboardInput->MoveRight), isDown);
					}
					else if(vkCode == 'Q')
					{
						Win32ProcessKeyboardMessages(&(keyboardInput->LeftShoulder), isDown);
					}
					else if(vkCode == 'E')
					{
						Win32ProcessKeyboardMessages(&(keyboardInput->RightShoulder), isDown);
					}
					else if(vkCode == VK_UP)
					{
						Win32ProcessKeyboardMessages(&(keyboardInput->ActionUp), isDown);
					}
					else if(vkCode == VK_DOWN)
					{
						Win32ProcessKeyboardMessages(&(keyboardInput->ActionDown), isDown);
					}
					else if(vkCode == VK_LEFT)
					{
						Win32ProcessKeyboardMessages(&(keyboardInput->ActionLeft), isDown);
					}
					else if(vkCode == VK_RIGHT)
					{
						Win32ProcessKeyboardMessages(&(keyboardInput->ActionRight), isDown);
					}
					else if(vkCode == VK_SPACE)
					{
					}
					else if(vkCode == VK_ESCAPE)
					{
						GameRunning = false;
					}
				}
			}break;
							
			default:
			{
				TranslateMessage(&message);
				DispatchMessage(&message);
			}break;
		}
						
	} // While peek message
					
}

internal f32
Win32ProcessXInputStick(SHORT value, SHORT deadZone)
{
	f32 result = 0;
	if(value < -deadZone)
	{
		result = (f32)((value + deadZone) / (32768.0f - deadZone));
	}
	else if(value > deadZone)
	{
		result = (f32)((value - deadZone) / (32767.0f - deadZone));
	}
	return result;
}							

inline LARGE_INTEGER
Win32GetWallClock()
{
	LARGE_INTEGER result = {};
	/*
	 * QueryPerformanceCounter - counts
	 * QueryPerformanceFrequency - counts/second
	 */
	QueryPerformanceCounter(&result);
	return result;
}

inline f32
Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
	/*
	 * counts / (count/second) = seconds
	 */
	f32 result = ((f32)(end.QuadPart - start.QuadPart) / (f32)GlobalPerformaceFrequency);
	return result;
}

internal void
Win32DebugDrawVerticalLine(Win32DIBBuffer* globalBackBuffer, int x,
						   int top, int bottom, uint32 color)
{
	uint8* pixel = ((uint8*)globalBackBuffer->Memory +
					x * globalBackBuffer->BytesPerPixel +
					top * globalBackBuffer->Pitch);

	for(int y = top; y < bottom; y++)
	{
		*(uint32*)pixel = color;
		pixel += globalBackBuffer->Pitch;
	}
}

internal void
Win32DebugDrawPlayMarker(Win32DIBBuffer* backBuffer, f32 ratio,
						 int padX, int top, int bottom,
						 int value, uint32 color)
						 
{
	int xPos = padX + (int)(ratio * (f32)value);
	Win32DebugDrawVerticalLine(backBuffer, xPos, top,
							   bottom, color);

}

internal void
Win32DebugSyncDisplay(Win32DIBBuffer* backBuffer, Win32DebugSoundMarker* playMarkers,
				 int playMarkerCount, Win32SoundOutput* soundData, f32 desiredSecondsElapsedPerFrame)
{
	int padX = 16;
	int padY = 16;

	int top = padY;
	int bottom = backBuffer->Height - padY;
	
	f32 ratio = ((f32)(backBuffer->Width - 2*padX) /
				 (f32)soundData->BufferSize);
	
	for(int playMarkerIndex = 0;
		playMarkerIndex < playMarkerCount;
		playMarkerIndex++)
	{
		//ASSERT(lastPlayCursors[lastPlayCursorIndex] < (DWORD)soundData->BufferSize);
		Win32DebugSoundMarker* marker = &playMarkers[playMarkerIndex];

		Win32DebugDrawPlayMarker(backBuffer, ratio,
								 padX, top, bottom,
								 marker->PlayCursor, 0xffffff);

		Win32DebugDrawPlayMarker(backBuffer, ratio,
								 padX, top, bottom,
								 marker->WriteCursor, 0xff0f00);
		
	}
}

int CALLBACK
WinMain(HINSTANCE instance,
		HINSTANCE prevInstance,
		LPSTR commandLine,
		int nShowCmd)
{
	LARGE_INTEGER perfFreqCounter = {};
	QueryPerformanceFrequency(&perfFreqCounter);
	GlobalPerformaceFrequency = perfFreqCounter.QuadPart;

	// Should aim to hit constant frame rate.
	UINT desiredSchedulerMS = 1;
	bool sleepIsGranular = (timeBeginPeriod(desiredSchedulerMS) == TIMERR_NOERROR);
	
	Win32LoadXInputLibrary();

	//Window data
	WNDCLASSA windowClass = {};

	// Resizing DIB used for drawing
	Win32ResizeDIB(&GlobalBackBuffer, 1280, 720);

	windowClass.style = CS_OWNDC;//|CS_HREDRAW|CS_VREDRAW;
	windowClass.lpfnWndProc = Win32MessgaeProcedure;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = "HandmadeHeroClass";

	// TODO(afb):: Need a way to reliably get this.
	const int32 framesOfAudioLatency = 4;
	const int32 monitorRefreshRate = 60;
	const int32 gameUpdateHz = monitorRefreshRate / 3;
	const f32 desiredSecondsElapsedPerFrame = 1.0f / gameUpdateHz;
	
	
	//Register window
	if(RegisterClass(&windowClass))
	{
		//Create window
		HWND windowHandle = CreateWindowExA(0,
											windowClass.lpszClassName,
											"HandmaderHero",
											WS_OVERLAPPEDWINDOW|WS_VISIBLE,
											CW_USEDEFAULT,
											CW_USEDEFAULT,
											CW_USEDEFAULT,
											CW_USEDEFAULT,
											0,
											0,
											instance,
											0);

		if(windowHandle)
		{
			// BEGIN Sound Output
			// TODO(afb) :: Make the buffer a minute long?
			Win32SoundOutput soundOutput = {};
			soundOutput.SamplesPerSecond = 48000;
			soundOutput.RunningSampleIndex = 0;
			soundOutput.BytesPerSample = sizeof(int16) * 2;
			soundOutput.BufferSize = soundOutput.SamplesPerSecond * soundOutput.BytesPerSample;
			soundOutput.LatencySampleCount = framesOfAudioLatency * (soundOutput.SamplesPerSecond / (f32)gameUpdateHz);
			
			Win32InitDirectSound(windowHandle, soundOutput.SamplesPerSecond, soundOutput.BufferSize);
			Win32ClearSoundBuffer(&soundOutput);

			int16* samples = (int16*)VirtualAlloc(0, soundOutput.BufferSize,
												  MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

			GlobalSecondarySoundBuffer->Play(0, 0, DSBPLAY_LOOPING);			
			bool soundIsPlaying = true;
			// End Sount Output
			
			// NOTE(afb) :: Initializing game memory
#if HANDMADE_DEBUG || HANDMADE_INTERNAL
			LPVOID baseAddress = (LPVOID)TERABYTE((uint64)2);
#else
			LPVOID baseAddress = 0;
#endif
			GameMemory gameMemory = {};
			gameMemory.PersistentMemorySize = MEGABYTE(64);
			gameMemory.TransientMemorySize = GIGABYTE((uint64)1);
			uint64 totalSize = gameMemory.PersistentMemorySize + gameMemory.TransientMemorySize;
			gameMemory.PersistentMemory = VirtualAlloc(baseAddress, (size_t)totalSize,
													   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			gameMemory.TransientMemory = (uint8*)gameMemory.PersistentMemory + gameMemory.PersistentMemorySize;
			
			uint64 previousCycle = __rdtsc();
			LARGE_INTEGER previousCounter = {};
			QueryPerformanceCounter(&previousCounter);

			HDC deviceContext = GetDC(windowHandle);
			
			if(samples && gameMemory.PersistentMemory && gameMemory.TransientMemory)
			{


				Input input[2] = {};
				Input* oldInput = &input[0];
				Input* newInput = &input[1];

				int DebugSoundMarkerIndex = 0;
				Win32DebugSoundMarker DebugSoundMarkers[gameUpdateHz / 2] = {0};
				
				DWORD lastPlayCursor = 0;
				DWORD lastWriteCursor = 0;
				bool soundIsValid = false;

				DWORD audioLatencyInBytes = 0;
				f32 audioLatencyInSeconds = 0;
						
				GameRunning = true;
				
				while(GameRunning)
				{
					//previousCycle = __rdtsc();

					ControllerState* oldKeyboardInput = GetController(oldInput, 0);
					ControllerState* newKeyboardInput = GetController(newInput, 0);
					*newKeyboardInput = {};

					for(int buttonIndex = 0;
						buttonIndex < ARRAY_COUNT(newKeyboardInput->Buttons);
						buttonIndex++)
					{
						newKeyboardInput->Buttons[buttonIndex].EndedDown = oldKeyboardInput->Buttons[buttonIndex].EndedDown;
					}
					
					// Retrieve messages from queue and dispatch.
					Win32ProcessPendingMessages(newKeyboardInput);
					
					// TODO(afb) :: We may need to poll controller more frequently than
					// every frame.
					DWORD maxControllerCount = XUSER_MAX_COUNT;
					if(maxControllerCount > ARRAY_COUNT(input->Controllers))
					{
						maxControllerCount = ARRAY_COUNT(input->Controllers);
					}

					// NOTE(afb) :: Keyboard is first controll and the
					// rest are gamepads.
					for (DWORD controllerIndex = 1;
						 controllerIndex < maxControllerCount+1;
						 controllerIndex++ )
					{
						XINPUT_STATE state = {};
						
						if(XInputGetState(controllerIndex-1, &state) == ERROR_SUCCESS)
						{
							//NOTE(afb):: Controller connected.
							XINPUT_GAMEPAD* pad = &state.Gamepad;
							
							ControllerState* oldController = GetController(oldInput, controllerIndex);
							ControllerState* newController = GetController(newInput, controllerIndex);
							newController->IsConnected = true;
							
							newController->LeftStickAverageX = Win32ProcessXInputStick(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
							newController->LeftStickAverageY = Win32ProcessXInputStick(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

							if((newController->LeftStickAverageX != 0.0f) || (
								   newController->LeftStickAverageY != 0.0f))
							{
								newController->IsAnalog = true;
							}
							
							if(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
							{
								newController->LeftStickAverageY = 1.0f;
								newController->IsAnalog = false;
							}
							else if(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
							{
								newController->LeftStickAverageY = -1.0f;
								newController->IsAnalog = false;
							}

							if(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
							{
								newController->LeftStickAverageX = 1.0f;
								newController->IsAnalog = false;
							}
							else if(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
							{
								newController->LeftStickAverageX = -1.0f;
								newController->IsAnalog = false;
							}
							
							// NOTE(afb) :: Mapping Left Stick on to the move
							// D-Pad for movement
							f32 threshold = 0.5f;
							Win32ProcessXInputDigitalButton(1,
															&oldController->MoveRight,
															(newController->LeftStickAverageX > threshold) ? 1 : 0,
															&newController->MoveRight);
							Win32ProcessXInputDigitalButton(1,
															&oldController->MoveLeft,
															(newController->LeftStickAverageX < -threshold) ? 1 : 0,
															&newController->MoveLeft);
							Win32ProcessXInputDigitalButton(1,
															&oldController->MoveUp,
															(newController->LeftStickAverageY > threshold) ? 1 : 0,
															&newController->MoveUp);
							Win32ProcessXInputDigitalButton(1,
															&oldController->MoveDown,
															(newController->LeftStickAverageY < -threshold) ? 1 : 0,
															&newController->MoveDown);
							// TODO(afb) :: Min Max macros

							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->Start,
															XINPUT_GAMEPAD_START,
															&newController->Start);
							
							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->Back,
															XINPUT_GAMEPAD_BACK,
															&newController->Back);
							
							
							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->ActionDown,
															XINPUT_GAMEPAD_A,
															&newController->ActionDown);
							
							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->ActionRight,
															XINPUT_GAMEPAD_B,
															&newController->ActionRight);
							
							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->ActionLeft,
															XINPUT_GAMEPAD_X,
															&newController->ActionLeft);
							
							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->ActionUp,
															XINPUT_GAMEPAD_Y,
															&newController->ActionUp);
							

							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->LeftShoulder,
															XINPUT_GAMEPAD_LEFT_SHOULDER,
															&newController->LeftShoulder);
							
							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->RightShoulder,
															XINPUT_GAMEPAD_RIGHT_SHOULDER,
															&newController->RightShoulder);
							
						}
						else
						{
							//NOTE(afb) :: Controller not connected.
							GetController(newInput, controllerIndex)->IsConnected = false;
						}// Xnput controller connected if
					} // Xinput For loop
					
					
					// NOTE(afb) :: Direct Sound Output Test
					// TODO(afb) :: Sound is wrong now since not synced
					// with frame rate.
					DWORD byteToLock = 0;   // Current Position we want to start writng
					DWORD bytesToWrite = 0; // Amount we want to write.
					DWORD targetCursor = 0; // Position we want to write up to
					if(soundIsValid)
					{
						byteToLock = (soundOutput.RunningSampleIndex *
									  soundOutput.BytesPerSample) % soundOutput.BufferSize;
						
						targetCursor = (int32)(lastPlayCursor + soundOutput.LatencySampleCount *
											   soundOutput.BytesPerSample) % soundOutput.BufferSize;

						//NOTE(afb) :: Check
						if(byteToLock > targetCursor)
						{
							bytesToWrite = soundOutput.BufferSize - byteToLock;// + lastPlayCursor;
							bytesToWrite += targetCursor;
						}
						else
						{
							bytesToWrite = targetCursor - byteToLock;
						}

						
					}
				
					//NOTE(afb) :: Sound
					//TODO(afb) :: Does this need to be recreated every frame
					//could just create it before since only sample count changes
					SoundBuffer soundBuffer = {};
					soundBuffer.SamplesPerSecond = soundOutput.SamplesPerSecond;
					soundBuffer.SampleCount = bytesToWrite / soundOutput.BytesPerSample;
					soundBuffer.Data = samples;
				
					//NOTE(afb) :: Rendering
					GameOffScreenBuffer screenBuffer = {};
					screenBuffer.Memory = GlobalBackBuffer.Memory;
					screenBuffer.Width = GlobalBackBuffer.Width;
					screenBuffer.Height = GlobalBackBuffer.Height;
					screenBuffer.Pitch = GlobalBackBuffer.Pitch;
					screenBuffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;

					// Updating game
					UpdateAndRender(&gameMemory, newInput, &screenBuffer, &soundBuffer);
				
					// Write sound to sound card
					if(soundIsValid)
					{
#if HANDMADE_INTERNAL					 
						DWORD playCursor = 0;
						DWORD writeCursor = 0;
						GlobalSecondarySoundBuffer->GetCurrentPosition(&playCursor, &writeCursor);

						DWORD unwrappedWriteCursor = writeCursor;
						if(unwrappedWriteCursor < playCursor)
						{
							unwrappedWriteCursor += soundOutput.BufferSize;
						}
						audioLatencyInBytes = unwrappedWriteCursor - playCursor;
						audioLatencyInSeconds =
							((f32)audioLatencyInBytes / (f32)soundOutput.BytesPerSample) /
							(f32)soundOutput.SamplesPerSecond;
						
						char textBuffer[256];
						_snprintf_s(textBuffer, sizeof(textBuffer), 
								  "PC: %d, TC: %d, BTW: %d, BTL:%d LS:%fs\n",
								  lastPlayCursor, targetCursor, bytesToWrite, byteToLock, 
								  audioLatencyInSeconds);

						OutputDebugStringA(textBuffer);
#endif
						Win32FillSoundBuffer(&soundOutput, bytesToWrite, byteToLock, &soundBuffer);
					} // soundIsValid

					
					// NOTE(afb) :: Frame Counter Calculation
					uint64 endCycle = __rdtsc();
					int64 cyclesElapsed = endCycle - previousCycle;
					previousCycle = endCycle;
					int32 megaCyclesPerFrame = (int32)cyclesElapsed / (1000 * 1000);
					
					
					LARGE_INTEGER endCounter = Win32GetWallClock();
					f32 secondsElapsed = Win32GetSecondsElapsed(previousCounter, endCounter);

					//ASSERT(secondsElapsed < desiredSecondsElapsedPerFrame);
					if(secondsElapsed < desiredSecondsElapsedPerFrame)
					{
						if(sleepIsGranular)
						{
							DWORD sleepTime = (int32)(1000.0f * (desiredSecondsElapsedPerFrame - secondsElapsed));
							if(sleepTime > 0)
							{
								Sleep(sleepTime);
							}
						}
						
						secondsElapsed = Win32GetSecondsElapsed(previousCounter,
																Win32GetWallClock());
						
						float testSecondsElapsed = Win32GetSecondsElapsed(previousCounter,
																		  Win32GetWallClock());
						
						if(testSecondsElapsed < desiredSecondsElapsedPerFrame)
						{
							//TODO(afb) :: Log frame miss
						}
						
					}
					else
					{
						// NOTE(afb) :: Missed frame
						// TODO(afb) :: Logging
					}

					endCounter = Win32GetWallClock();
					int32 milliSecondsElapsedPerFrame = (int32)(1000 * secondsElapsed);
					previousCounter = endCounter;
					
					
					
#if HANDMADE_INTERNAL
					// Drawing sound markers
					Win32DebugSyncDisplay(&GlobalBackBuffer,
										  DebugSoundMarkers,
										  ARRAY_COUNT(DebugSoundMarkers),
										  &soundOutput,
										  desiredSecondsElapsedPerFrame);
#endif
					// Render to the screen
					Win32WindowDimension dim = Win32GetWindowDimension(windowHandle);
					Win32DisplayBufferToWindow(deviceContext, &GlobalBackBuffer, 0, 0, dim.Width, dim.Height);

					
					//NOTE(afb) :: If there was some audio problem
					// recalculate where next audio write should be 
					DWORD playCursor;
					DWORD writeCursor;
					if(GlobalSecondarySoundBuffer->GetCurrentPosition
					   (&playCursor, &writeCursor) == DS_OK)
					{
						lastPlayCursor = playCursor;

						lastWriteCursor = writeCursor;
						if(!soundIsValid)
						{
							// NOTE(afb) :: If sound wasn't valid and then
							// it became valid then write from the writecursor
							// and since we are tracking our right position
							// from running sample index then we map the writecursor
							// into running sample index . 
							soundOutput.RunningSampleIndex =
								writeCursor / soundOutput.BytesPerSample;
						}

						soundIsValid = true;
					}
					else
					{
						soundIsValid = false;
					}
		
					
#if HANDMADE_INTERNAL
					// Debug audio markers
					{
						ASSERT(DebugSoundMarkerIndex < ARRAY_COUNT(DebugSoundMarkers));
						Win32DebugSoundMarker* marker = &DebugSoundMarkers[DebugSoundMarkerIndex++];

						marker->PlayCursor = playCursor;
						marker->WriteCursor = writeCursor;
						
						if(DebugSoundMarkerIndex >= ARRAY_COUNT(DebugSoundMarkers))
						{
							DebugSoundMarkerIndex = 0;
						}
					}
#endif
					
#if 1
					// Display Frames per second an related info
					{
						int32 fps = (int32)(1/secondsElapsed);
						char buffer[256];
						wsprintfA(buffer, "%dms, %dfps, %dMc/f\n", milliSecondsElapsedPerFrame, fps, megaCyclesPerFrame);
						OutputDebugStringA(buffer);
					}
#endif			

					Input* temp = oldInput;
					oldInput = newInput;
					newInput = temp;
					
				}// While Running Loop
			}// If memory initialized
			else
			{
				// TODO(afb) :: Error Logging
			}
			ReleaseDC(windowHandle ,deviceContext);
			VirtualFree(samples, 0, MEM_RELEASE);
		}
		else
		{
			// TODO(afb) :: Error Logging
		}
	}
	else
	{
		// TODO(afb) :: Error Logging
	}
	
	return(0);
}
