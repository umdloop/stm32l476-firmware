@echo off
setlocal

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..

where py >nul 2>nul
if %errorlevel%==0 (
    py -3 "%PROJECT_DIR%\tools\dbc_to_c.py" "%PROJECT_DIR%\App\dbc\5.12.2026.dbc" "%PROJECT_DIR%\App\dbc\can_dbc_text.c" --no-install-dbc
    exit /b %errorlevel%
)

where python >nul 2>nul
if %errorlevel%==0 (
    python "%PROJECT_DIR%\tools\dbc_to_c.py" "%PROJECT_DIR%\App\dbc\5.12.2026.dbc" "%PROJECT_DIR%\App\dbc\can_dbc_text.c" --no-install-dbc
    exit /b %errorlevel%
)

echo Python 3 was not found.
echo Install Python 3 from python.org and make sure py or python is available in PATH.
exit /b 1