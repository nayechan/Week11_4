@echo off
REM Visual Studio Pre-Build Event용 코드 생성 스크립트

REM 시스템 Python 체크
py --version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    py "%~dp0generate.py" %*
    exit /b %ERRORLEVEL%
)

REM Python 없음
echo [ERROR] Python not found!
echo Please install Python and add it to system PATH
exit /b 1
