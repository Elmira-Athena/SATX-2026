@echo off
setlocal
cd /d "%~dp0"

set PORT=%1
if "%PORT%"=="" set PORT=8080
set URL=http://localhost:%PORT%

echo SATx - giao dien cuc bo
echo   %URL%
echo.
echo   Dung Chrome hoac Edge (Web Serial khong co tren Safari/Firefox).
echo   Chua co mach? Mo Cai dat -^> Nguon du lieu -^> Mo phong.
echo.
echo   Ctrl+C de dung.
echo.

:: Mo trinh duyet mac dinh
start %URL%

:: Kiem tra Python
where python >nul 2>nul
if %ERRORLEVEL% equ 0 (
    python -m http.server %PORT% --bind 127.0.0.1
    goto :eof
)

:: Kiem tra npx (Node.js)
where npx >nul 2>nul
if %ERRORLEVEL% equ 0 (
    npx --yes serve -l %PORT% .
    goto :eof
)

echo [LOI] Can cai dat Python hoac Node.js de chay server tinh.
pause