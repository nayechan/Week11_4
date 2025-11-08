@echo off
REM ============================================================
REM Mundi Engine - Lua Binding Code Generator
REM ============================================================

echo.
echo ============================================================
echo    Mundi Engine - Lua Binding Code Generator
echo ============================================================
echo.

REM Change to script directory
cd /d "%~dp0"

REM Try to find Python in the following order:
REM 1. Embedded Python in Tools/Python/
REM 2. System Python in PATH

set PYTHON_CMD=

REM Check for embedded Python first
if exist "Tools\Python\python.exe" (
    set PYTHON_CMD=Tools\Python\python.exe
    echo [INFO] Using embedded Python: Tools\Python\python.exe
    echo.
) else (
    REM Check if system Python is available
    python --version >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Python not found!
        echo.
        echo Please either:
        echo   1. Install embedded Python to Tools\Python\ (see Tools\PYTHON_SETUP.md)
        echo   2. Install Python 3.7+ and add it to PATH
        echo.
        pause
        exit /b 1
    )
    set PYTHON_CMD=python
    echo [INFO] Using system Python
    echo.
)

REM Verify Python version
%PYTHON_CMD% --version
echo.

REM Check if jinja2 is installed
%PYTHON_CMD% -c "import jinja2" >nul 2>&1
if errorlevel 1 (
    echo [WARNING] jinja2 is not installed!
    echo Installing jinja2...
    %PYTHON_CMD% -m pip install jinja2
    if errorlevel 1 (
        echo [ERROR] Failed to install jinja2!
        echo If using embedded Python, see Tools\PYTHON_SETUP.md for pip setup
        pause
        exit /b 1
    )
    echo.
)

echo [1/3] Running code generator...
%PYTHON_CMD% Tools\CodeGenerator\generate.py --source-dir Source\Runtime --output-dir Generated

if errorlevel 1 (
    echo.
    echo [ERROR] Code generation failed!
    pause
    exit /b 1
)

echo.
echo [2/3] Checking generated files...
if not exist "Generated\*.generated.cpp" (
    echo [WARNING] No .generated.cpp files found!
) else (
    echo [OK] Generated files found in Generated\ folder
)

echo.
echo [3/3] Done!
echo.
echo ============================================================
echo You can now build the project in Visual Studio.
echo Generated files are in: Generated\
echo ============================================================
echo.

pause
