@echo off
REM Combined Qt Project Clean Build and UI Regeneration Script
REM This script first cleans Qt build artifacts and then regenerates ui_GOJI.h from GOJI.ui
REM Author: Auto-generated script

echo ====================================
echo Qt Project Clean Build and UI Regeneration Script
echo ====================================
echo.

REM Store the current directory
set "PROJECT_DIR=%~dp0"
set "PROJECT_NAME=GOJI"

echo Current directory: %PROJECT_DIR%
echo Project name: %PROJECT_NAME%
echo.

REM Check if Qt Creator is running and warn user
tasklist /FI "IMAGENAME eq qtcreator.exe" 2>NUL | find /I /N "qtcreator.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo WARNING: Qt Creator appears to be running!
    echo Please close Qt Creator before running this script.
    echo.
    echo Press C to continue or Q to quit...
    choice /c CQ /n /m "Your choice: "
    if errorlevel 2 exit /b 0
)

echo Step 1: Cleaning with qmake and make...
REM Run qmake clean (adjust Qt path as needed)
C:\Qt\6.10.0\mingw_64\bin\qmake.exe %PROJECT_NAME%.pro
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: qmake failed
    echo Press Q to quit...
    choice /c Q /n /m ""
    exit /b 1
)

C:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe clean
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: make clean failed, continuing anyway...
)

echo.
echo Step 2: Removing generated directories...

REM Remove generated directories
for %%d in (.ui .moc .obj .rcc release debug) do (
    if exist "%%d" (
        echo Removing %%d
        rmdir /s /q "%%d" 2>nul
    ) else (
        echo %%d does not exist, skipping
    )
)

echo.
echo Step 3: Removing Qt Creator files and generated UI headers...

REM Remove Qt Creator specific files
for %%f in (%PROJECT_NAME%.pro.user .qmake.stash) do (
    if exist "%%f" (
        echo Removing %%f
        del /q "%%f"
    ) else (
        echo %%f does not exist, skipping
    )
)

REM Remove the specific ui_GOJI.h file that's causing the build issue
if exist "ui_GOJI.h" (
    echo Removing ui_GOJI.h
    del /q "ui_GOJI.h"
) else (
    echo ui_GOJI.h does not exist, skipping
)

REM Remove build directory if it exists
if exist "build" (
    echo Removing build directory
    rmdir /s /q "build" 2>nul
) else (
    echo build directory does not exist, skipping
)

echo.
echo Step 4: Removing specific build directories...

REM Remove the specific build-Goji directory with forced deletion
if exist "C:\Users\JCox\Projects\GOJI\build-Goji" (
    echo Removing C:\Users\JCox\Projects\GOJI\build-Goji
    rmdir /s /q "C:\Users\JCox\Projects\GOJI\build-Goji" 2>nul
) else (
    echo C:\Users\JCox\Projects\GOJI\build-Goji does not exist, skipping
)

REM Remove build-GOJI directory (case variation)
if exist "C:\Users\JCox\Projects\GOJI\build-GOJI" (
    echo Removing C:\Users\JCox\Projects\GOJI\build-GOJI
    rmdir /s /q "C:\Users\JCox\Projects\GOJI\build-GOJI" 2>nul
) else (
    echo C:\Users\JCox\Projects\GOJI\build-GOJI does not exist, skipping
)

REM Remove relative build directories
if exist "build-Goji" (
    echo Removing build-Goji from current directory
    rmdir /s /q "build-Goji" 2>nul
)

if exist "build-GOJI" (
    echo Removing build-GOJI from current directory
    rmdir /s /q "build-GOJI" 2>nul
)

echo.
echo Step 5: Additional cleanup...

REM Remove any Makefile* files
for %%f in (Makefile*) do (
    if exist "%%f" (
        echo Removing %%f
        del /q "%%f"
    )
)

REM Remove any .tmp files
if exist "*.tmp" (
    echo Removing temporary files
    del /q "*.tmp"
)

REM Remove any .o object files
if exist "*.o" (
    echo Removing object files
    del /q "*.o"
)

echo.
echo ====================================
echo Cleanup completed successfully!
echo ====================================
echo.

echo Changing to GOJI project directory...
cd /d "C:\Users\JCox\Projects\GOJI"

echo.
echo Checking if GOJI.ui exists...
if not exist "GOJI.ui" (
    echo ERROR: GOJI.ui file not found in current directory!
    echo Current directory: %CD%
    echo Press Q to quit...
    choice /c Q /n /m ""
    exit /b 1
)

echo.
echo Regenerating ui_GOJI.h from GOJI.ui...
REM Use full path to uic since it's not in PATH
C:\Qt\6.10.0\mingw_64\bin\uic.exe GOJI.ui -o ui_GOJI.h

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS: ui_GOJI.h has been regenerated successfully!
    echo File location: %CD%\ui_GOJI.h
) else (
    echo.
    echo ERROR: Failed to regenerate ui_GOJI.h
    echo Error code: %ERRORLEVEL%
)

echo.
echo ====================================
echo Clean build and UI regeneration completed!
echo ====================================
echo.
echo Next steps:
echo 1. Open Qt Creator
echo 2. Open your project file (%PROJECT_NAME%.pro)
echo 3. Reconfigure the project if prompted
echo 4. Run qmake (Build menu ^> Run qmake)
echo 5. Build your project
echo.
echo Press Q to quit...
choice /c Q /n /m ""
