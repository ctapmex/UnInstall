@echo off

if exist makefile set MAKEFILE=makefile

if exist "%VS100COMNTOOLS%..\..\VC\BIN\vcvars32.bat" (
  call "%VS100COMNTOOLS%..\..\VC\BIN\vcvars32.bat"
) else (
  call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"
)

set WIDE=1
set IA64=
set AMD64=
set CPU=
set PLATFORM=
set DEBUG=
set RELEASE=1
set FAR3=1

nmake /A /B /F %MAKEFILE%
rem nmake /F %MAKEFILE%
if errorlevel 1 goto end

if exist "%VS100COMNTOOLS%..\..\VC\BIN\x86_amd64\vcvarsx86_amd64.bat" (
  call "%VS100COMNTOOLS%..\..\VC\BIN\x86_amd64\vcvarsx86_amd64.bat"
) else (
  call "c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\x86_amd64\vcvarsx86_amd64.bat"
)
set AMD64=1
set CPU=AMD64
set PLATFORM=x64
nmake /A /B /F %MAKEFILE%
rem nmake /F %MAKEFILE%

:end
