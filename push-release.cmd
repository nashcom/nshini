@echo off
setlocal

set /p VERSION=<version.txt
set "RELEASE=v%VERSION%"

call :header Pushing release %RELEASE%

git tag -d "%RELEASE%"
if errorlevel 1 exit /b %errorlevel%

git tag "%RELEASE%"
if errorlevel 1 exit /b %errorlevel%

git push --force origin "%RELEASE%"
if errorlevel 1 exit /b %errorlevel%

goto :eof


:print_delim
echo --------------------------------------------------------------------------------
exit /b 0


:header
echo.
call :print_delim
echo %*
call :print_delim
echo.
exit /b 0
