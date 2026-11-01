@echo off
setlocal EnableExtensions
REM Compile and upload sketches\MatrixLamp_R4_Wifi to an Arduino Uno R4 WiFi.
REM Double-click this file, or from a command prompt:
REM   tools\upload_matrixlamp.bat
REM   tools\upload_matrixlamp.bat COM5
REM   tools\upload_matrixlamp.bat COM5 --monitor
REM   tools\upload_matrixlamp.bat --monitor
REM   tools\upload_matrixlamp.bat ota
REM   tools\upload_matrixlamp.bat --ota 192.168.1.20
REM --monitor opens the serial monitor at 115200 after a successful USB upload.
REM ota / --ota [host] compiles and sends the firmware over WiFi.
REM The host defaults to matrixlamp.local. USB stays the default.
REM The window stays open at the end.

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

echo Checking the Uno R4 WiFi core, Adafruit NeoPixel, and ArduinoOTA...
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
arduino-cli lib install "ArduinoOTA"
if errorlevel 1 (
  echo Could not install the ArduinoOTA library ^(JAndrassy, 1.1.0 or newer^).
  goto fail
)

if not exist "sketches\MatrixLamp_R4_Wifi\secrets.h" (
  echo WARNING: sketches\MatrixLamp_R4_Wifi\secrets.h does not exist.
  echo A USB build will not contain the WiFi name. A blank board opens MatrixLamp-Setup.
  echo A board that already stored a network keeps using it.
  echo   copy sketches\MatrixLamp_R4_Wifi\secrets.example.h sketches\MatrixLamp_R4_Wifi\secrets.h
  echo Then put the home network in WIFI_SSID and WIFI_PASS, and set OTA_PASSWORD.
  echo secrets.h is gitignored. Do not commit it.
)

set "PORT="
set "MONITOR=0"
set "OTA=0"
set "HOST="
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
if /I "%~1"=="--ota" (
  set "OTA=1"
  shift
  if "%~1"=="" goto parsed
  echo %~1 | findstr /b "-" >nul
  if errorlevel 1 (
    set "HOST=%~1"
    shift
  )
  goto parse
)
if /I "%~1"=="ota" (
  set "OTA=1"
  shift
  goto parse
)
if not defined PORT (
  if "%OTA%"=="1" if not defined HOST (
    set "HOST=%~1"
    shift
    goto parse
  )
  set "PORT=%~1"
  shift
  goto parse
)
echo Unexpected extra argument: %~1
goto usage

:parsed
if "%OTA%"=="1" (
  if "%MONITOR%"=="1" (
    echo --monitor is only for a USB upload.
    goto fail
  )
  if defined PORT (
    echo Pass either a serial port or ota, not both.
    goto fail
  )
  if not defined HOST set "HOST=matrixlamp.local"
  goto ota
)
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

:ota
echo OTA target http://%HOST%/
if exist "build\matrixlamp" rmdir /s /q "build\matrixlamp"
mkdir "build\matrixlamp"
echo Compiling
arduino-cli compile -b arduino:renesas_uno:unor4wifi --output-dir build\matrixlamp sketches\MatrixLamp_R4_Wifi
if errorlevel 1 (
  echo FAILED: compile failed.
  goto fail
)
if not exist "build\matrixlamp\MatrixLamp_R4_Wifi.ino.bin" (
  echo FAILED: no .bin was produced.
  goto fail
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0matrixlamp_ota.ps1" -Bin "%CD%\build\matrixlamp\MatrixLamp_R4_Wifi.ino.bin" -HostName "%HOST%"
if errorlevel 1 (
  echo FAILED: the lamp did not take the update.
  echo If matrixlamp.local does not resolve, rerun with the IP from the web page or the serial monitor.
  goto fail
)
echo.
echo Done.
pause
exit /b 0

:usage
echo Usage: tools\upload_matrixlamp.bat [PORT] [--monitor]
echo        tools\upload_matrixlamp.bat ota [HOST]
echo        tools\upload_matrixlamp.bat --ota [HOST]
echo   PORT       serial port, for example COM5. If omitted, the script looks for one Uno R4 WiFi.
echo   --monitor  open the serial monitor at 115200 baud after a USB upload.
echo   ota        compile and send the firmware over WiFi. HOST defaults to matrixlamp.local.
goto fail

:fail
echo.
echo Stopped.
pause
exit /b 1
