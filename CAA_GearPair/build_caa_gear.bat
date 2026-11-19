@echo off
setlocal
pushd "%~dp0"
set "WS_DIR=%CD%"

echo === 1. Setting up VS 2012 64-bit environment ===
call "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\vcvarsall.bat" amd64
if errorlevel 1 exit /b 1

echo === 2. Setting up TCK and CAA RADE environment ===
call "C:\Program Files\Dassault Systemes\B26\win_b64\code\command\tck_init.bat"
call tck_profile V5_6R2016_B26
if errorlevel 1 exit /b 1

echo === 3. Initializing prerequisites ===
call mkGetPreq -p "C:\Program Files\Dassault Systemes\B26" -W "%WS_DIR%"

echo === 4. Building workspace with mkmk ===
call mkmk -a -u -w
set "MKRC=%ERRORLEVEL%"
if %MKRC% NEQ 0 (
    echo [ERROR] mkmk failed with code %MKRC%
    popd
    exit /b %MKRC%
)
if not exist "%WS_DIR%\win_b64\code\bin\GearPairWorker.exe" (
    echo [ERROR] GearPairWorker.exe missing after mkmk
    popd
    exit /b 1
)

echo === 5. Updating runtime view ===
call mkCreateRuntimeView -W "%WS_DIR%"

if exist "C:\Program Files\Dassault Systemes\B26\win_b64\code\bin\netapi32.dll" (
    copy /y "C:\Program Files\Dassault Systemes\B26\win_b64\code\bin\netapi32.dll" "%WS_DIR%\win_b64\code\bin\netapi32.dll" >nul
)

echo === Build Finished with code %MKRC% ===
popd
exit /b %MKRC%