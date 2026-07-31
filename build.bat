
@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "OPENCV_ROOT=%OPENCV_DIR%"

if not defined OPENCV_ROOT set "OPENCV_ROOT=C:\Tools\opencv_4_8\build"
if not exist "%OPENCV_ROOT%\include\opencv2\opencv.hpp" (
  for %%D in ("C:\Tools\opencv_4_8\build" "C:\opencv\build" "C:\Program Files\OpenCV\build" "C:\Program Files\OpenCV") do (
    if exist "%%~fD\include\opencv2\opencv.hpp" (
      set "OPENCV_ROOT=%%~fD"
      goto :found_opencv
    )
  )
  echo OpenCV headers were not found. Install OpenCV 4.8.0 or set OPENCV_DIR to the build root.
  exit /b 1
)

:found_opencv
set "OPENCV_LIB=%OPENCV_ROOT%\x64\vc16\lib"
if not exist "%OPENCV_LIB%\opencv_world480.lib" (
  if exist "%OPENCV_ROOT%\x64\vc16\lib\opencv_world480.lib" set "OPENCV_LIB=%OPENCV_ROOT%\x64\vc16\lib"
  if exist "%OPENCV_ROOT%\x64\vc17\lib\opencv_world480.lib" set "OPENCV_LIB=%OPENCV_ROOT%\x64\vc17\lib"
)

if not exist "%OPENCV_LIB%\opencv_world480.lib" (
  echo OpenCV 4.8.0 libraries were not found under "%OPENCV_ROOT%".
  exit /b 1
)

set "OUTPUT_EXE=%SCRIPT_DIR%ocr.exe"

cl /std:c++20 /EHsc /I"%OPENCV_ROOT%\include" "%SCRIPT_DIR%ocr.cpp" /Fe"%OUTPUT_EXE%" /link /LIBPATH:"%OPENCV_LIB%" opencv_world480.lib /machine:x64
if errorlevel 1 exit /b %errorlevel%

call :copy_runtime_dlls "%OUTPUT_EXE%"
exit /b %errorlevel%

:copy_runtime_dlls
set "OUTPUT_EXE=%~1"
set "OUTPUT_DIR=%~dp1"
if not defined OUTPUT_DIR set "OUTPUT_DIR=%SCRIPT_DIR%"

if not exist "%OUTPUT_EXE%" exit /b 0

set "OPENCV_BIN="
if exist "%OPENCV_ROOT%\x64\vc16\bin\opencv_world480.dll" set "OPENCV_BIN=%OPENCV_ROOT%\x64\vc16\bin"
if not defined OPENCV_BIN if exist "%OPENCV_ROOT%\x64\vc17\bin\opencv_world480.dll" set "OPENCV_BIN=%OPENCV_ROOT%\x64\vc17\bin"
if not defined OPENCV_BIN if exist "%OPENCV_ROOT%\bin\opencv_world480.dll" set "OPENCV_BIN=%OPENCV_ROOT%\bin"

if defined OPENCV_BIN (
  for %%F in ("%OPENCV_BIN%\*.dll") do copy /Y "%%~fF" "%OUTPUT_DIR%" >nul
  echo Copied OpenCV runtime DLLs to "%OUTPUT_DIR%".
) else (
  echo OpenCV runtime DLLs were not found under "%OPENCV_ROOT%".
)

set "VCLIBS_ROOT="
for /d %%D in ("C:\Program Files\WindowsApps\Microsoft.VCLibs.140.00_*") do (
  if exist "%%~fD\msvcp140_app.dll" (
    set "VCLIBS_ROOT=%%~fD"
    goto :found_vclibs
  )
)

if not defined VCLIBS_ROOT (
  echo Microsoft Visual C++ runtime DLLs were not found. Install the Microsoft VCLibs package or copy the required DLLs manually.
  exit /b 0
)

:found_vclibs
for %%F in ("%VCLIBS_ROOT%\*.dll") do copy /Y "%%~fF" "%OUTPUT_DIR%" >nul
echo Copied Microsoft Visual C++ runtime DLLs to "%OUTPUT_DIR%".
exit /b 0

