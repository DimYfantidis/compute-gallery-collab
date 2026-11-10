@echo off
rem Windows entry point for PowerShell / cmd: runs run.sh with Git for Windows' bash.
rem (Not C:\Windows\System32\bash.exe, which would start WSL instead.)
setlocal

set "CG_BASH="
for /f "delims=" %%G in ('where git 2^>nul') do if not defined CG_BASH (
    if exist "%%~dpG..\bin\bash.exe" set "CG_BASH=%%~dpG..\bin\bash.exe"
    if exist "%%~dpG..\..\bin\bash.exe" set "CG_BASH=%%~dpG..\..\bin\bash.exe"
)
if not defined CG_BASH if exist "%ProgramFiles%\Git\bin\bash.exe" set "CG_BASH=%ProgramFiles%\Git\bin\bash.exe"

if not defined CG_BASH (
    echo Git for Windows was not found. Install it from https://git-scm.com/download/win and retry.
    exit /b 1
)

"%CG_BASH%" "%~dp0run.sh" %*
exit /b %ERRORLEVEL%
