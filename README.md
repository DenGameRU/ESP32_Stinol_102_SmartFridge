# ESP32-C3 Smart Fridge Controller (STINOL-102RV2 Mod)

An advanced, IoT-enabled smart thermostat firmware written in C++ for the **ESP32-C3 SuperMini** development board to replace the unreliable stock electromechanical thermostats in the classic dual-compressor **STINOL-102** refrigerator.

## Features
- **Precise Digital Thermostat Engine:** Replaces inaccurate capillary tubes with a digital Dallas/Maxim DS18B20 temperature probe (`OneWire`), adjusting relay behaviors inside tight 0.5°C threshold steps.
- **Hardware Compressor Anti-Short Cycle Protection:** Implements a strict 5-minute initial and post-operation countdown buffer (`ANTI_SHORT_CYCLE`) to prevent motor windings from burning out due to immediate high-pressure restarts during sudden mains power flickering.
- **Automated VK API Telegram Alerts:** Streams real-time operational markers, power-on diagnostics, sensor disconnections, and runtime stats directly to your personal VK social media inbox utilizing HTTP POST Bearer Token Authorization requests.
- **Smart Power Consumption Tracker:** Measures exact cumulative active compressor cycles against a 200W load factor (`COMPRESSOR_POWER_KW`) to output comprehensive daily energy utilization metrics (kWh) at midnight.
- **Asynchronous AJAX Control Web Server:** Hosts a local lightweight web interface featuring custom CSS injection, real-time JSON data streaming updates every 3 seconds, and interactive controls to adjust temperature trigger configurations.
- **Non-Volatile Preferences Buffering:** Uses the ESP32's native NVS architecture via `Preferences.h` to sustain runtime configurations across hard reboots without fatiguing specific flash memory addresses.

## Hardware Pinout Configuration (ESP32-C3 SuperMini)
- `GPIO 5` -> OneWire Bus (Data pin with a 4.7kΩ pull-up resistor to 3.3V) for DS18B20 sensor.
- `GPIO 7` -> Power Relay Gate Control Output (Compressor relay module driver).

## Custom Deployment & Configuration Setup
1. Open the project cluster environment inside **PlatformIO** (or use the standalone `.bat` compilation upload pipelines).
2. Set your custom router credentials inside the `ssid` and `password` parameters.
3. Inject your private application parameters into the `vkToken` and `vkUserId` variables.
4. Flash the layout directly over the native USB CDC interface supported by the `esp32-c3-supermini` configuration flags.

## Original Credits
Developed by **DenGame** (2026). Part of a home automation initiative to modernize vintage home appliances with smart micro-controllers.
