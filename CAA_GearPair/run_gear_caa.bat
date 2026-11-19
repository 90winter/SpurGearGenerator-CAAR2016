@echo off
setlocal
set "APP_DIR=%~dp0"
set "BIN_DIR=%APP_DIR%win_b64\code\bin"
set "WORKER=%BIN_DIR%\GearPairWorker.exe"
set "SEED=%BIN_DIR%\GearSeed.CATPart"
set "CATSTART=C:\Program Files\Dassault Systemes\B26\win_b64\code\bin\CATSTART.exe"
set "CATENV=CATIA_P3.V5-6R2016.B26"
set "CATDIRENV=C:\ProgramData\DassaultSystemes\CATEnv"

if not exist "%WORKER%" (
    echo [ERROR] GearPairWorker.exe not found: "%WORKER%"
    exit /b 1
)
if not exist "%SEED%" (
    echo [ERROR] GearSeed.CATPart not found: "%SEED%"
    exit /b 1
)

for %%I in ("%APP_DIR%..\Output_CAA") do set "OUT_DIR=%%~fI"
if "%~1"=="" goto :run_worker

set "OUT_DIR=%~1"
set "M_VAL=%~2"
set "Z1_VAL=%~3"
set "Z2_VAL=%~4"
set "B1_VAL=%~5"
set "B2_VAL=%~6"
set "A_VAL=%~7"
set "X1_VAL=%~8"
set "X2_VAL=%~9"
if "%X2_VAL%"=="" set "X2_VAL=0"
if "%M_VAL%"=="" set "M_VAL=1.0"
if "%Z1_VAL%"=="" set "Z1_VAL=34"
if "%Z2_VAL%"=="" set "Z2_VAL=60"
if "%B1_VAL%"=="" set "B1_VAL=6.0"
if "%B2_VAL%"=="" set "B2_VAL=6.0"
if "%A_VAL%"=="" set "A_VAL=20.0"
if "%X1_VAL%"=="" set "X1_VAL=0"

set "GEAR_JOB_FILE=%TEMP%\caa_gear_job_%RANDOM%.txt"
powershell -NoProfile -Command "$lines=@('seed=%SEED%','outDir=%OUT_DIR%','m=%M_VAL%','z1=%Z1_VAL%','z2=%Z2_VAL%','b1=%B1_VAL%','b2=%B2_VAL%','alpha=%A_VAL%','x1=%X1_VAL%','x2=%X2_VAL%','backlash=0'); [System.IO.File]::WriteAllLines('%GEAR_JOB_FILE%',$lines,(New-Object System.Text.UTF8Encoding($false)))"
"%CATSTART%" -env "%CATENV%" -direnv "%CATDIRENV%" -nowindow -run "%WORKER%"
set "RC=%ERRORLEVEL%"
del /q "%GEAR_JOB_FILE%" >nul 2>&1
exit /b %RC%

:run_worker
set "GEAR_JOB_FILE=%TEMP%\caa_gear_job_%RANDOM%.txt"
powershell -NoProfile -Command "$lines=@('seed=%SEED%','outDir=%OUT_DIR%','m=1.0','z1=34','z2=60','b1=6.0','b2=6.0','alpha=20.0','x1=0','x2=0','backlash=0'); [System.IO.File]::WriteAllLines('%GEAR_JOB_FILE%',$lines,(New-Object System.Text.UTF8Encoding($false)))"
"%CATSTART%" -env "%CATENV%" -direnv "%CATDIRENV%" -nowindow -run "%WORKER%"
set "RC=%ERRORLEVEL%"
del /q "%GEAR_JOB_FILE%" >nul 2>&1
exit /b %RC%
