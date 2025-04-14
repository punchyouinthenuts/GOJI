@echo off
setlocal enabledelayedexpansion

REM Define constants
set "SCRIPT_PATH=C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\SWEEPS\Subscripts"
set "PYTHON_EXE=python"
set "LOG_FILE=%SCRIPT_PATH%\sweeps_log.txt"

REM Create timestamp
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
set "timestamp=%datetime:~0,4%-%datetime:~4,2%-%datetime:~6,2%_%datetime:~8,2%-%datetime:~10,2%-%datetime:~12,2%"

echo [%timestamp%] Starting SWEEPS processing... >> "%LOG_FILE%"

REM Execute scripts with direct paths
for %%s in (FORMATCBC2.py EXTRACTPROOFDATA.py) do (
    echo [%time%] Running %%s... >> "%LOG_FILE%"
    "%PYTHON_EXE%" "%SCRIPT_PATH%\%%s"
    if !errorlevel! equ 0 (
        echo [%time%] %%s completed successfully >> "%LOG_FILE%"
    ) else (
        echo [%time%] %%s failed with error code !errorlevel! >> "%LOG_FILE%"
        goto :error
    )
)

:success
echo [%timestamp%] All processing completed successfully! >> "%LOG_FILE%"
goto :end

:error
echo [%timestamp%] Processing terminated with errors >> "%LOG_FILE%"
exit /b 1

:end
echo Press any key to exit...
pause >nul
endlocal
exit /b 0
