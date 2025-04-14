@echo off

echo Starting RAC Weekly Processing...

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\Subscripts\01CBCa.py"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: CBC Processing failed
    echo BAT will terminate - subsequent scripts will not run
    echo.
    pause
    exit /b 1
)

timeout /t 1 /nobreak >nul

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\Subscripts\02EXCa.py"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: EXC Processing failed
    echo BAT will terminate - subsequent scripts will not run
    echo.
    pause
    exit /b 1
)

timeout /t 1 /nobreak >nul

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\Subscripts\03INACTIVEa.py"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: INACTIVE Processing failed
    echo BAT will terminate - subsequent scripts will not run
    echo.
    pause
    exit /b 1
)

timeout /t 1 /nobreak >nul

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\Subscripts\04NCWOa.py"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: NCWO Processing failed
    echo BAT will terminate - subsequent scripts will not run
    echo.
    pause
    exit /b 1
)

timeout /t 1 /nobreak >nul

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\Subscripts\05PREPIFa.py"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: PREPIF Processing failed
    echo.
    pause
    exit /b 1
)

echo.
echo SCRIPT WILL NOW CHECK FOR DATA FILES THAT HAVE QUOTATION MARKS THAT COULD CAUSE PROBLEMS DURING PROOF AND PRINT FILE GENERATION
echo.
echo PRESS ANY KEY TO CONTINUE
pause >nul

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\Subscripts\06QUOTECHECK.py"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Quote Check Processing failed
    echo.
    pause
    exit /b 1
)

echo.
echo All RAC Weekly Processing completed successfully!
echo.
pause
exit /b 0
