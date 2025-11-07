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

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python is not installed or not in PATH!
    echo Please install Python 3.7+ and add it to PATH.
    pause
    exit /b 1
)

REM Check if jinja2 is installed
python -c "import jinja2" >nul 2>&1
if errorlevel 1 (
    echo [WARNING] jinja2 is not installed!
    echo Installing jinja2...
    pip install jinja2
    if errorlevel 1 (
        echo [ERROR] Failed to install jinja2!
        pause
        exit /b 1
    )
)

echo [1/3] Running code generator...
python Tools\CodeGenerator\generate.py --source-dir Source\Runtime --output-dir Generated

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
