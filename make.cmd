@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "MSVC_PATH=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"

REM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
REM Setup environment
REM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

if not exist "%MSVC_PATH%\vcvarsall.bat" (
	echo VisualC++ could not be found. Please check MSVC_PATH and try again ^^!^^!^^!
	pause
	goto:BuildHasFailed
)

call "%MSVC_PATH%\vcvarsall.bat" x86

REM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
REM Clean-up
REM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

for %%i in (bin,obj) do (
	rmdir /Q /S "%~dp0.\%%i" 2> NUL
	if exist "%~dp0.\%%i" (
		echo Failed to delete existing "%~dp0.\%%i" directory!
		pause
		goto:BuildHasFailed
	)
)

REM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
REM Build!
REM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

set "MSBuildUseNoSolutionCache=1"

for %%p in (Win32,x64) do (
	for %%c in (Release,Debug) do (
		MSBuild /property:Platform=%%p /property:Configuration=%%c /target:Clean   "%~dp0\mktemp.sln"
		if not "!ERRORLEVEL!"=="0" goto:BuildHasFailed
		MSBuild /property:Platform=%%p /property:Configuration=%%c /target:Rebuild "%~dp0\mktemp.sln"
		MSBuild /property:Platform=%%p /property:Configuration=%%c /target:Build   "%~dp0\mktemp.sln"
		if not "!ERRORLEVEL!"=="0" goto:BuildHasFailed
	)
)

echo.
echo BUILD COMPLETED.
echo.

if not "%MAKE_NONINTERACTIVE%"=="1" pause
exit /B 0

REM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
REM Failed
REM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:BuildHasFailed

echo.
echo BUILD HAS FAILED ^^!^^!^^!
echo.

if not "%MAKE_NONINTERACTIVE%"=="1" pause
exit /B 1
