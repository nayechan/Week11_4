@echo off
REM Visual Studio Pre-Build Event용 코드 생성 스크립트

REM 임베디드 Python 체크
if exist "%~dp0..\Python\python.exe" (
    "%~dp0..\Python\python.exe" "%~dp0generate.py" %*
    exit /b %ERRORLEVEL%
)

REM 시스템 Python 체크
py --version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    py "%~dp0generate.py" %*
    exit /b %ERRORLEVEL%
)

REM Python 없음
echo [ERROR] Python not found!
echo Please install Python to Tools\Python\ or system PATH
echo See Tools\PYTHON_SETUP.md for instructions
exit /b 1
