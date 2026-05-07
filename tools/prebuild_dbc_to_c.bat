@echo off
setlocal

set SCRIPT_DIR=%~dp0

py -3 "%SCRIPT_DIR%prebuild_dbc_to_c.py"
if %ERRORLEVEL% EQU 0 exit /b 0

python "%SCRIPT_DIR%prebuild_dbc_to_c.py"
if %ERRORLEVEL% EQU 0 exit /b 0

echo Failed to regenerate App\dbc\can_dbc_text.c from App\dbc\4.13.2026.dbc
echo Install Python 3 or regenerate App\dbc\can_dbc_text.c manually with tools\dbc_to_c.py.
exit /b 1
