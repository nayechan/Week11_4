@echo off
REM Git Hooks 설치 스크립트 (Windows)
REM 이 스크립트는 .githooks 디렉토리의 hook들을 .git/hooks로 복사합니다.

setlocal enabledelayedexpansion

echo Installing Git hooks...
echo.

REM Git hooks 디렉토리 찾기
for /f "delims=" %%i in ('git rev-parse --git-dir') do set GIT_DIR=%%i
set HOOKS_DIR=%GIT_DIR%\hooks

echo Source: %~dp0
echo Target: %HOOKS_DIR%
echo.

REM Hook 파일 복사
copy /Y "%~dp0post-checkout" "%HOOKS_DIR%\post-checkout" >nul 2>&1
if !errorlevel! equ 0 echo [OK] Installed: post-checkout

copy /Y "%~dp0post-merge" "%HOOKS_DIR%\post-merge" >nul 2>&1
if !errorlevel! equ 0 echo [OK] Installed: post-merge

copy /Y "%~dp0pre-push" "%HOOKS_DIR%\pre-push" >nul 2>&1
if !errorlevel! equ 0 echo [OK] Installed: pre-push

echo.
echo Git hooks installed successfully!
echo.
echo Installed hooks:
echo   - post-checkout: Auto-update submodules after branch checkout
echo   - post-merge: Auto-update submodules after merge
echo   - pre-push: Warn if submodule commits are not pushed
echo.
pause
