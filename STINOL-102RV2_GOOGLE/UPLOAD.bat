@echo off
:: Переключаем консоль в кодировку UTF-8, чтобы русские буквы читались правильно
chcp 65001 > nul

set "PIO_PATH=C:\pio.exe"
cd /d "C:\STINOL-102RV2"

echo === STARTING PLATFORMIO UPLOAD ===
echo Project path: %CD%
echo Executable: %PIO_PATH%
echo ==================================

"%PIO_PATH%" run --target upload

echo ==================================
echo Compilation finished.
pause
