@echo off
SET QT_DIR=C:\Qt\6.9.0\mingw_64
SET TARGET_DIR=C:\Users\JCox\Projects\GOJI\build\Desktop_Qt_6_9_0_MinGW_64_bit-Debug\debug

REM Create necessary directories
mkdir "%TARGET_DIR%\platforms"
mkdir "%TARGET_DIR%\styles"
mkdir "%TARGET_DIR%\imageformats"
mkdir "%TARGET_DIR%\sqldrivers"
mkdir "%TARGET_DIR%\tls"
mkdir "%TARGET_DIR%\iconengines"

REM Copy essential Qt DLLs
copy "%QT_DIR%\bin\Qt6Core.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\bin\Qt6Gui.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\bin\Qt6Widgets.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\bin\Qt6Sql.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\bin\Qt6Network.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\bin\Qt6Concurrent.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\bin\Qt6Core5Compat.dll" "%TARGET_DIR%\"

REM Copy platform plugins
copy "%QT_DIR%\plugins\platforms\qwindows.dll" "%TARGET_DIR%\platforms\"

REM Copy style plugins
copy "%QT_DIR%\plugins\styles\qwindowsvistastyle.dll" "%TARGET_DIR%\styles\"

REM Copy image format plugins
copy "%QT_DIR%\plugins\imageformats\qgif.dll" "%TARGET_DIR%\imageformats\"
copy "%QT_DIR%\plugins\imageformats\qico.dll" "%TARGET_DIR%\imageformats\"
copy "%QT_DIR%\plugins\imageformats\qjpeg.dll" "%TARGET_DIR%\imageformats\"
copy "%QT_DIR%\plugins\imageformats\qsvg.dll" "%TARGET_DIR%\imageformats\"

REM Copy SQL drivers
copy "%QT_DIR%\plugins\sqldrivers\qsqlite.dll" "%TARGET_DIR%\sqldrivers\"
copy "%QT_DIR%\plugins\sqldrivers\qsqlodbc.dll" "%TARGET_DIR%\sqldrivers\"

REM Copy TLS plugins
copy "%QT_DIR%\plugins\tls\qcertonlybackend.dll" "%TARGET_DIR%\tls\"
copy "%QT_DIR%\plugins\tls\qschannelbackend.dll" "%TARGET_DIR%\tls\"

REM Copy icon engines
copy "%QT_DIR%\plugins\iconengines\qsvgicon.dll" "%TARGET_DIR%\iconengines\"

REM Copy MinGW DLLs
copy "%QT_DIR%\bin\libgcc_s_seh-1.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\bin\libstdc++-6.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\bin\libwinpthread-1.dll" "%TARGET_DIR%\"

echo Deployment Complete!