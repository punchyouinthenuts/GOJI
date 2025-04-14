@echo off
echo Starting Post-Proof Scripts Sequence...

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\CBC\04POSTPROOFCBC.py"
timeout /t 2 /nobreak

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\EXC\03POSTPROOFEXC.py"
timeout /t 2 /nobreak

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\INACTIVE\03POSTPROOFINACTIVE.py"
timeout /t 2 /nobreak

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\NCWO\05POSTPROOFNCWO.py"
timeout /t 2 /nobreak

python "C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS\RAC\WEEKLIES\PREPIF\03POSTPROOFPREPIF.py"

echo All scripts completed successfully!
pause
