@echo off
:: Переключаем консоль в кодировку UTF-8, чтобы русские буквы читались правильно
chcp 65001 > nul

set "PIO_PATH=C:\pio.exe"
cd /d "C:\STINOL-102RV2"

echo === STARTING PLATFORMIO UPLOAD ===
echo Project path: %CD%
echo Executable: %PIO_PATH%
echo ==================================

:: Запуск прошивки
"%PIO_PATH%" run --target upload

:: Проверяем, успешно ли прошла прошивка (errorlevel 0 означает успех)
if %errorlevel% equ 0 (
    echo ==================================
    echo Прошивка успешна! Запускаю Монитор Порта...
    echo Для выхода из монитора нажмите Ctrl+C
    echo ==================================
    timeout /t 3 > nul
    
    :: Запуск монитора порта
    "%PIO_PATH%" device monitor
) else (
    echo ==================================
    echo ОШИБКА: Прошивка завершилась неудачно.
    pause
)
