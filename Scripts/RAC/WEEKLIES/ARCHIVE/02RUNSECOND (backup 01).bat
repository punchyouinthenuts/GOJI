@echo off
setlocal enabledelayedexpansion

:: Define the script directory
set SCRIPT_DIR=%~dp0Subscripts

:: Parse command-line arguments
set BASE_PATH=%~1
set JOB_NUM=%~2
set WEEK=%~3

:: Validate arguments
if "%BASE_PATH%"=="" (
    echo ERROR: --base_path is required.
    exit /b 1
)
if "%JOB_NUM%"=="" (
    echo ERROR: --job_num is required.
    exit /b 1
)
if "%WEEK%"=="" (
    echo ERROR: --week is required.
    exit /b 1
)

:: Define job types for backup and rollback
set JOB_TYPES=CBC EXC INACTIVE NCWO PREPIF

:: Create a timestamped backup directory
set TIMESTAMP=%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set BACKUP_DIR=%BASE_PATH%\Backup\%TIMESTAMP%

:: Backup directories for each job type
echo Starting backup of OUTPUT and PROOF directories...
for %%j in (%JOB_TYPES%) do (
    set JOB_DIR=%BASE_PATH%\%%j\JOB
    for %%d in (OUTPUT PROOF) do (
        set DIR_TO_BACKUP=!JOB_DIR!\%%d
        set BACKUP_TARGET=%BACKUP_DIR%\%%j\JOB\%%d
        if exist "!DIR_TO_BACKUP!" (
            echo Backing up !DIR_TO_BACKUP!...
            xcopy "!DIR_TO_BACKUP!" "!BACKUP_TARGET!\" /E /I /H /K /Y >nul
        )
    )
)
echo Backup completed.

:: List of scripts to run
set SCRIPTS=01CBCa.py 02EXCa.py 03INACTIVEa.py 04NCWOa.py 05PREPIFa.py 06QUOTECHECK.py

:: Run the scripts
echo Starting RAC Weekly Processing...
for %%s in (%SCRIPTS%) do (
    echo Running %%s...
    python "%SCRIPT_DIR%\%%s" --base_path "%BASE_PATH%" --job_num "%JOB_NUM%" --week "%WEEK%"
    if !errorlevel! neq 0 (
        echo.
        echo ERROR: %%s failed.
        echo Performing rollback...
        for %%j in (%JOB_TYPES%) do (
            set JOB_DIR=%BASE_PATH%\%%j\JOB
            for %%d in (OUTPUT PROOF) do (
                set DIR_TO_RESTORE=!JOB_DIR!\%%d
                set BACKUP_SOURCE=%BACKUP_DIR%\%%j\JOB\%%d
                if exist "!BACKUP_SOURCE!" (
                    echo Restoring !DIR_TO_RESTORE!...
                    rmdir /S /Q "!DIR_TO_RESTORE!" 2>nul
                    xcopy "!BACKUP_SOURCE!" "!DIR_TO_RESTORE!\" /E /I /H /K /Y >nul
                )
            )
        )
        rmdir /S /Q "%BACKUP_DIR%"
        echo Rollback completed.
        exit /b 1
    )
)

:: If all scripts succeed, delete the backup
rmdir /S /Q "%BACKUP_DIR%"
echo.
echo All RAC Weekly Processing completed successfully!
exit /b 0