@echo off
setlocal

where py.exe >nul 2>nul
if not errorlevel 1 goto use_py

where python.exe >nul 2>nul
if errorlevel 1 (
    echo Error: Python 3.10 or newer was not found. 1>&2
    exit /b 1
)
python.exe "%~dp0bml.py" %*
exit /b %errorlevel%

:use_py
py.exe -3 "%~dp0bml.py" %*
exit /b %errorlevel%
