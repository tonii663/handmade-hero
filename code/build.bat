@echo off

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM XInput libraries - xinput.lib xinput9_1_0.lib

cl -std:c++17 -FAsc -Zi ..\code\Win32_Handmade.cpp user32.lib gdi32.lib 

popd
	
