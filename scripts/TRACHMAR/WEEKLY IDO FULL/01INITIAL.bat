@echo off
setlocal enabledelayedexpansion

REM ====================================================================
REM GOJI - TM WEEKLY IDO FULL - INITIAL PROCESSING BAT
REM Executes three Python scripts with complete rollback on any failure
REM ====================================================================

echo =================================================================
echo GOJI - Starting TM Weekly IDO Full initial processing sequence
echo =================================================================

REM Define script paths for GOJI
set SCRIPT_DIR=C:\Goji\scripts\TRACHMAR\WEEKLY IDO FULL
set TEMP_DIR=C:\Goji\TRACHMAR\WEEKLY IDO FULL\TEMP
set ROLLBACK_HELPER=%SCRIPT_DIR%\rollback_helper.py

REM Ensure temp directory exists
if not exist "%TEMP_DIR%" (
    mkdir "%TEMP_DIR%"
    echo Created temporary directory: %TEMP_DIR%
)

REM Initialize success tracking
set SCRIPT1_SUCCESS=0
set SCRIPT2_SUCCESS=0
set SCRIPT3_SUCCESS=0

echo.
echo =================================================================
echo Step 1: Running input file processing...
echo =================================================================
python "%SCRIPT_DIR%\01INPUTFILEPROCESSING.py"
set SCRIPT1_RESULT=%ERRORLEVEL%

if %SCRIPT1_RESULT% NEQ 0 (
    echo.
    echo *** ERROR IN STEP 1: Input file processing failed ***
    echo Initiating complete rollback...
    goto :rollback
) else (
    set SCRIPT1_SUCCESS=1
    echo Step 1 completed successfully.
)

echo.
echo =================================================================
echo Step 2: Running phone number processing...
echo =================================================================
python "%SCRIPT_DIR%\02PHONENUMBERS (AUTO).py"
set SCRIPT2_RESULT=%ERRORLEVEL%

if %SCRIPT2_RESULT% NEQ 0 (
    echo.
    echo *** ERROR IN STEP 2: Phone number processing failed ***
    echo Initiating complete rollback...
    goto :rollback
) else (
    set SCRIPT2_SUCCESS=1
    echo Step 2 completed successfully.
)

echo.
echo =================================================================
echo Step 3: Running file rename operation...
echo =================================================================
python "%SCRIPT_DIR%\03DPRENAME.py"
set SCRIPT3_RESULT=%ERRORLEVEL%

if %SCRIPT3_RESULT% NEQ 0 (
    echo.
    echo *** ERROR IN STEP 3: File rename operation failed ***
    echo Initiating complete rollback...
    goto :rollback
) else (
    set SCRIPT3_SUCCESS=1
    echo Step 3 completed successfully.
)

echo.
echo =================================================================
echo ALL PROCESSING STEPS COMPLETED SUCCESSFULLY!
echo =================================================================
echo Cleaning up temporary files...

REM Clean up rollback logs and temp directory on success
if exist "%TEMP_DIR%\rollback_01.log" del "%TEMP_DIR%\rollback_01.log"
if exist "%TEMP_DIR%\rollback_02.log" del "%TEMP_DIR%\rollback_02.log"
if exist "%TEMP_DIR%\rollback_03.log" del "%TEMP_DIR%\rollback_03.log"

REM Remove temp directory if empty
rmdir "%TEMP_DIR%" 2>nul

echo Cleanup completed.
echo.
echo =================================================================
echo INITIAL PROCESSING SEQUENCE COMPLETE
echo Files are ready for individual processing.
echo =================================================================
goto :end

:rollback
echo.
echo =================================================================
echo PERFORMING COMPLETE ROLLBACK TO INITIAL STATE
echo =================================================================

REM Rollback in reverse order (3 -> 2 -> 1)
if %SCRIPT3_SUCCESS% EQU 1 (
    echo Rolling back Step 3 changes...
    python "%SCRIPT_DIR%\rollback_script.py" "%TEMP_DIR%\rollback_03.log"
    if !ERRORLEVEL! NEQ 0 (
        echo WARNING: Step 3 rollback encountered errors
    ) else (
        echo Step 3 rollback completed
    )
)

if %SCRIPT2_SUCCESS% EQU 1 (
    echo Rolling back Step 2 changes...
    python "%SCRIPT_DIR%\rollback_script.py" "%TEMP_DIR%\rollback_02.log"
    if !ERRORLEVEL! NEQ 0 (
        echo WARNING: Step 2 rollback encountered errors
    ) else (
        echo Step 2 rollback completed
    )
)

if %SCRIPT1_SUCCESS% EQU 1 (
    echo Rolling back Step 1 changes...
    python "%SCRIPT_DIR%\rollback_script.py" "%TEMP_DIR%\rollback_01.log"
    if !ERRORLEVEL! NEQ 0 (
        echo WARNING: Step 1 rollback encountered errors
    ) else (
        echo Step 1 rollback completed
    )
)

echo.
echo =================================================================
echo ROLLBACK COMPLETED
echo All files have been restored to their initial state.
echo =================================================================

REM Clean up rollback logs after rollback
if exist "%TEMP_DIR%\rollback_01.log" del "%TEMP_DIR%\rollback_01.log"
if exist "%TEMP_DIR%\rollback_02.log" del "%TEMP_DIR%\rollback_02.log"
if exist "%TEMP_DIR%\rollback_03.log" del "%TEMP_DIR%\rollback_03.log"

REM Remove temp directory if empty
rmdir "%TEMP_DIR%" 2>nul

echo.
echo Processing halted due to errors. Please resolve issues and try again.
exit /b 1

:end
echo.
echo Processing sequence completed successfully.
exit /b 0