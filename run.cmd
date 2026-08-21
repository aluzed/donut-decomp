@echo off
rem Launch the donut game from the project root on Windows (MSYS2 / MinGW-w64 build).
rem
rem The binary links against the UCRT64 runtime DLLs (SDL2, OpenAL, Bullet, fmt,
rem zlib, libstdc++), so C:\msys64\ucrt64\bin has to be on PATH. Assets are loaded
rem relative to the working directory, hence the pushd into the project root.
rem
rem Usage: run.cmd [game args...]
setlocal
set "MSYS_BIN=C:\msys64\ucrt64\bin"
pushd "%~dp0"

if not exist "build\bin\donut.exe" (
	echo error: build\bin\donut.exe not found - build first: cmake --build build -j
	popd
	exit /b 1
)

set "PATH=%MSYS_BIN%;%PATH%"
build\bin\donut.exe %*
popd
