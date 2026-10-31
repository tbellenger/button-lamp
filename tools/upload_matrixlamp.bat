@echo off
setlocal EnableExtensions
REM Compile and upload sketches\MatrixLamp_R4_Wifi to an Arduino Uno R4 WiFi.
REM Double-click this file, or from a command prompt:
REM   tools\upload_matrixlamp.bat
REM   tools\upload_matrixlamp.bat COM5
REM   tools\upload_matrixlamp.bat COM5 --monitor
REM   tools\upload_matrixlamp.bat --monitor
REM --monitor opens the serial monitor at 115200 after a successful upload.
REM There is no OTA upload. The window stays open at the end.

cd /d "%~dp0.."
if errorlevel 1 goto fail

echo Matrix Lamp: repo is %CD%
echo Updating from git...
git pull
if errorlevel 1 (
  echo WARNING: git pull failed ^(offline, or this checkout has local changes^).
  echo Continuing with the copy that is already here.
)

where arduino-cli >nul 2>&1
if errorlevel 1 (
  echo arduino-cli is not installed, or it is not on PATH.
  echo Install it, then open a new command prompt:
  echo   winget install ArduinoSA.CLI
  echo   Docs: https://arduino.github.io/arduino-cli/latest/installation/
  goto fail
)

echo Checking the Uno R4 WiFi core and Adafruit NeoPixel...
arduino-cli core update-index
if errorlevel 1 echo WARNING: could not refresh the board index. Continuing if the core is already installed.
arduino-cli core install arduino:renesas_uno
if errorlevel 1 (
  echo Could not install the arduino:renesas_uno core.
  echo In the Arduino IDE this is Boards Manager -^> Arduino UNO R4 Boards.
  goto fail
)
arduino-cli lib install "Adafruit NeoPixel"
if errorlevel 1 (
  echo Could not install the Adafruit NeoPixel library ^(1.12 or newer^).
  goto fail
)

if not exist "sketches\MatrixLamp_R4_Wifi\secrets.h" (
  echo WARNING: sketches\MatrixLamp_R4_Wifi\secrets.h does not exist. WiFi will stay off until you create it.
  echo   copy sketches\MatrixLamp_R4_Wifi\secrets.example.h sketches\MatrixLamp_R4_Wifi\secrets.h
  echo Then put the home network in WIFI_SSID and WIFI_PASS.
  echo secrets.h is gitignored. Do not commit it.
)

set "PORT="
set "MONITOR=0"
:parse
if "%~1"=="" goto parsed
if /I "%~1"=="--monitor" (
  set "MONITOR=1"
  shift
  goto parse
)
if /I "%~1"=="-m" (
  set "MONITOR=1"
  shift
  goto parse
)
if /I "%~1"=="--help" goto usage
if /I "%~1"=="-h" goto usage
if not defined PORT (
  set "PORT=%~1"
  shift
  goto parse
)
echo Unexpected extra argument: %~1
goto usage

:parsed
if defined PORT goto upload

echo Looking for an Arduino Uno R4 WiFi...
set "LIST=%TEMP%\matrixlamp-ports.txt"
powershell -NoProfile -Command "$ErrorActionPreference='Continue'; $raw = arduino-cli board list --json --discovery-timeout 5s | Out-String; $j = $raw | ConvertFrom-Json; $hits = @(); $shown = @(); $items = @(); if ($j.detected_ports) { $items = @($j.detected_ports) }; foreach ($item in $items) { $addr = [string]$item.port.address; if (-not $addr) { continue }; $names = @(); $match = $false; $boards = @(); if ($item.matching_boards) { $boards = @($item.matching_boards) }; foreach ($b in $boards) { if ($b.name) { $names += [string]$b.name }; if ($b.fqbn -eq 'arduino:renesas_uno:unor4wifi') { $match = $true } }; if ($names.Count -gt 0) { $shown += ($addr + '  (' + ($names -join ', ') + ')') } else { $shown += $addr }; if ($match) { $hits += $addr } }; Write-Output ('HITS ' + $hits.Count); foreach ($h in $hits) { Write-Output ('HIT ' + $h) }; Write-Output ('ALL ' + $shown.Count); foreach ($s in $shown) { Write-Output ('PORT ' + $s) }" > "%LIST%"
if errorlevel 1 (
  echo Could not list boards. Pass the port, for example: tools\upload_matrixlamp.bat COM5
  goto fail
)

set "HITS=0"
set "ONEPORT="
for /f "usebackq tokens=1* delims= " %%A in ("%LIST%") do (
  if "%%A"=="HITS" set "HITS=%%B"
  if "%%A"=="HIT" set "ONEPORT=%%B"
)
if "%HITS%"=="1" (
  set "PORT=%ONEPORT%"
  echo Using %ONEPORT%
  goto upload
)

if "%HITS%"=="0" (
  echo No Arduino Uno R4 WiFi was found.
) else (
  echo More than one Arduino Uno R4 WiFi was found:
  for /f "usebackq tokens=1* delims= " %%A in ("%LIST%") do (
    if "%%A"=="HIT" echo   %%B
  )
)
echo Ports arduino-cli can see:
set "SAWPORT=0"
for /f "usebackq tokens=1* delims= " %%A in ("%LIST%") do (
  if "%%A"=="PORT" (
    echo   %%B
    set "SAWPORT=1"
  )
)
if "%SAWPORT%"=="0" echo   ^(none^)
echo Pass the port, for example: tools\upload_matrixlamp.bat COM5
set /p PORT=Port ^(or press Enter to cancel^): 
if not defined PORT goto fail
if "%PORT%"=="" goto fail

:upload
echo Compiling and uploading to %PORT%
arduino-cli compile --upload -b arduino:renesas_uno:unor4wifi -p "%PORT%" sketches\MatrixLamp_R4_Wifi
if errorlevel 1 (
  echo Compile or upload failed.
  goto fail
)
echo Uploaded MatrixLamp_R4_Wifi to %PORT%
if "%MONITOR%"=="1" (
  echo Serial monitor at 115200 baud. Close the monitor window, or press Ctrl-C, to finish.
  arduino-cli monitor -p "%PORT%" -b arduino:renesas_uno:unor4wifi -c baudrate=115200
)
echo.
echo Done.
pause
exit /b 0

:usage
echo Usage: tools\upload_matrixlamp.bat [PORT] [--monitor]
echo   PORT      serial port, for example COM5. If omitted, the script looks for one Uno R4 WiFi.
echo   --monitor open the serial monitor at 115200 baud after upload.
goto fail

:fail
echo.
echo Stopped.
pause
exit /b 1
