@echo off
echo Starting data processing sequence...

echo Step 1: Running input file processing...
python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\TRACHMAR\DATA PROCESSING\01INPUTFILEPROCESSING.py"
if %ERRORLEVEL% NEQ 0 (
    echo Error in Step 1. Processing halted.
    pause
    exit /b %ERRORLEVEL%
)

echo Step 2: Running phone number processing...
python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\TRACHMAR\DATA PROCESSING\02PHONENUMBERS (AUTO).py"
if %ERRORLEVEL% NEQ 0 (
    echo Error in Step 2. Processing halted.
    pause
    exit /b %ERRORLEVEL%
)

echo Step 3: Running rename operation...
python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\TRACHMAR\DATA PROCESSING\03DPRENAME.py"
if %ERRORLEVEL% NEQ 0 (
    echo Error in Step 3. Processing halted.
    pause
    exit /b %ERRORLEVEL%
)

echo All processing steps completed successfully!
pause