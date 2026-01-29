@echo off
setlocal enabledelayedexpansion

REM This BAT lives in: <repo>\tools\
REM So repo root is one directory up from this script's folder:
set "TOOLS_DIR=%~dp0"
set "REPO_ROOT=%TOOLS_DIR%.."

REM Input / output paths relative to repo root:
set "IN_DBC=%REPO_ROOT%\app\dbc\file.dbc"
set "OUT_C=%REPO_ROOT%\app\dbc\can_dbc_text.c"

echo.
echo === CAN DBC -> C string generator ===
echo Repo root : %REPO_ROOT%
echo Input     : %IN_DBC%
echo Output    : %OUT_C%
echo.

REM Prefer the Python launcher if available; fall back to python.
where py >NUL 2>&1
if %errorlevel%==0 (
    py -3 "%REPO_ROOT%\tools\dbc_to_c.py" "%IN_DBC%" "%OUT_C%"
) else (
    python "%REPO_ROOT%\tools\dbc_to_c.py" "%IN_DBC%" "%OUT_C%"
)

echo.
if %errorlevel%==0 (
    echo Success.
) else (
    echo FAILED with errorlevel %errorlevel%.
)

echo.
pause
