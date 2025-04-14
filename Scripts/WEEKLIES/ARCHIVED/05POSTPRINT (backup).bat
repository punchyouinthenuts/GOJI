@echo off

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\SUB\02MOVEPROOFZIPS.py"
if errorlevel 1 (
    echo Terminating process as requested
    pause
    exit /b 0
)
timeout /t 2 /nobreak >nul

for %%f in (
    "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\CBC\05POSTPRINTCBC.py"
    "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\EXC\04POSTPRINTEXC.py"
    "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\INACTIVE\04MOVEDATATOBUSKRO.py"
    "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\NCWO\06POSTPRINTNCWO.py"
    "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\PREPIF\04POSTPRINTPREPIF.py"
) do (
    python "%%f"
    if errorlevel 1 (
        echo ERROR WITH SCRIPT %%f
        pause
        exit /b 1
    )
    timeout /t 2 /nobreak >nul
)

echo All scripts completed successfully
pause
