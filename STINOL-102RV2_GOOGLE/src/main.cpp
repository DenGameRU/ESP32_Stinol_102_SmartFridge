#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <time.h>

// ========== НАСТРОЙКИ WI-FI ==========
const char* ssid = "SSID";
const char* password = "PASSWORD";

// ========== НАСТРОЙКИ VK API ==========
const char* vkToken = "VK API"; 
const char* vkUserId = "VK USER ID";

// ========== НАСТРОЙКИ ВРЕМЕНИ ==========
const char* TZ_INFO = "MSK-3"; 
const char* ntpServer = "pool.ntp.org";

// ========== ПИНЫ ПОДКЛЮЧЕНИЯ ==========
#define ONE_WIRE_BUS 5  
#define RELAY_PIN 7     

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WebServer server(80);
Preferences preferences;

// ========== ПЕРЕМЕННЫЕ ХОЛОДИЛЬНИКА ==========
float currentTemp = 0.0;
float setTempHigh = 8.0;   
float setTempLow = -14.0;    
bool compressorState = false;

unsigned long lastTurnOffTime = 0;
const unsigned long ANTI_SHORT_CYCLE = 300000; // 5 минут защиты компрессора
String logText = "";

// ========== ПЕРЕМЕННЫЕ ДЛЯ СТАТИСТИКИ ==========
unsigned long compressorTotalRunTime = 0;  // Всего миллисекунд работы за сутки
unsigned long compressorDayStartTime = 0;  // Время начала текущих суток (millis)
unsigned long lastStateChangeTime = 0;     // Время последнего изменения состояния компрессора
bool msgSentToday = false;                 // Флаг отправки отчета в 00:00
const float COMPRESSOR_POWER_KW = 0.200;   // Мощность компрессора: 200 Вт = 0.2 кВт

// Функция получения текущего времени в виде строки [HH:MM:SS]
String getTimeString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "[--:--:--]";
    }
    char timeStringBuff[12];
    strftime(timeStringBuff, sizeof(timeStringBuff), "[%H:%M:%S]", &timeinfo);
    return String(timeStringBuff);
}

void addLog(String message) {
    logText = getTimeString() + " " + message + "<br>" + logText;
    if (logText.length() > 1800) logText = logText.substring(0, 1800);
}

String urlEncode(String str) {
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (isalnum(c)) {
            encodedString += c;
        } else if (c == ' ') {
            encodedString += "%20";
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) code0 = c - 10 + 'A';
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
    }
    return encodedString;
}

// Обновленная функция отправки сообщений в VK по новым правилам API
void sendVK(String message) {
    WiFiClientSecure client;
    client.setInsecure(); // Отключаем проверку SSL для экономии памяти ESP32-C3
    
    if (client.connect("api.vk.com", 443)) {
        long randomId = random(1, 2147483647);
        String fullMessage = getTimeString() + " " + message;
        
        // Формируем тело запроса (параметры метода)
        String body = "user_id=" + String(vkUserId) + 
                      "&message=" + urlEncode(fullMessage) + 
                      "&random_id=" + String(randomId) + 
                      "&v=5.131";
                      
        // Отправляем POST-запрос с токеном в заголовке Authorization (Bearer) по документации VK
        client.print("POST /method/messages.send HTTP/1.1\r\n");
        client.print("Host: api.vk.com\r\n");
        client.print("Authorization: Bearer " + String(vkToken) + "\r\n"); // Передача токена в заголовке
        client.print("Content-Type: application/x-www-form-urlencoded\r\n");
        client.print("Content-Length: " + String(body.length()) + "\r\n");
        client.print("Connection: close\r\n\r\n");
        client.print(body); // Отправляем параметры
                     
        // Кратковременное чтение ответа для корректного закрытия соединения
        uint32_t timeout = millis();
        while (client.available() == 0) {
            if (millis() - timeout > 3000) {
                client.stop();
                return;
            }
        }
        client.stop();
    }
}

// Функция сохранения статистики во флеш-память
void saveStatsToFlash() {
    preferences.begin("fridge-stats", false);
    preferences.putULong("runtime", compressorTotalRunTime);
    preferences.end();
}

// Главная веб-страница (HTML + CSS)
void handleRoot() {
    String html = R"rawliteral(
    <!DOCTYPE html><html><head><meta charset='utf-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>STINOL-102R Control</title>
    <style>
        body { font-family: Arial; text-align: center; background: #f4f4f4; color: #333; }
        .card { background: white; max-width: 400px; margin: 20px auto; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
        h1 { color: #0076a3; margin-bottom: 5px; }
        .time-box { font-size: 16px; color: #666; margin-bottom: 15px; font-weight: bold; }
        .temp { font-size: 48px; font-weight: bold; color: #2bc; margin: 10px 0; }
        .status { padding: 8px; border-radius: 5px; font-weight: bold; display: inline-block; }
        .on { background: #d4edda; color: #155724; }
        .off { background: #f8d7da; color: #721c24; }
        .btn { padding: 10px 20px; font-size: 18px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; background: #0076a3; color: white; }
        .log-box { text-align: left; background: #333; color: #fff; padding: 10px; height: 160px; overflow-y: auto; font-family: monospace; border-radius: 5px; font-size: 12px; }
    </style>
    <script>
        setInterval(function() {
            fetch('/data').then(response => response.json()).then(data => {
                document.getElementById('time').innerText = "Время на плате: " + data.time;
                document.getElementById('temp').innerText = data.temp + ' °C';
                document.getElementById('comp').innerText = data.comp ? 'РАБОТАЕТ' : 'ВЫКЛЮЧЕН';
                document.getElementById('comp').className = data.comp ? 'status on' : 'status off';
                document.getElementById('thigh').innerText = data.thigh;
                document.getElementById('tlow').innerText = data.tlow;
                document.getElementById('log').innerHTML = data.log;
            });
        }, 3000);
    </script>
    </head><body>
    <div class='card'>
        <h1>❄️ СТИНОЛ-102R ❄️</h1>
        <div class='time-box' id='time'>Время на плате: ...</div>
        <div class='temp' id='temp'>... °C</div>
        <p>Компрессор: <span id='comp' class='status off'>...</span></p>
        <hr>
        <h3>Уставки температуры</h3>
        <p>Включение (>): <span id='thigh'>...</span>°C 
            <button class='btn' onclick="location.href='/set?h=up'">+</button>
            <button class='btn' onclick="location.href='/set?h=down'">-</button>
        </p>
        <p>Выключение (<): <span id='tlow'>...</span>°C 
            <button class='btn' onclick="location.href='/set?l=up'">+</button>
            <button class='btn' onclick="location.href='/set?l=down'">-</button>
        </p>
        <hr>
        <h3>Логи системы</h3>
        <div class='log-box' id='log'>...</div>
    </div>
    </body></html>)rawliteral";
    server.send(200, "text/html", html);
}

void handleData() {
    String rawTime = getTimeString();
    rawTime.replace("[", "");
    rawTime.replace("]", "");

    String json = "{";
    json += "\"time\":\"" + rawTime + "\",";
    json += "\"temp\":" + String(currentTemp, 1) + ",";
    json += "\"comp\":" + String(compressorState ? "true" : "false") + ",";
    json += "\"thigh\":" + String(setTempHigh, 1) + ",";
    json += "\"tlow\":" + String(setTempLow, 1) + ",";
    json += "\"log\":\"" + logText + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

void handleSet() {
    preferences.begin("fridge-settings", false);
    if (server.hasArg("h")) {
        if (server.arg("h") == "up") setTempHigh += 0.5;
        if (server.arg("h") == "down") setTempHigh -= 0.5;
        preferences.putFloat("high", setTempHigh);
        addLog("Изменен верхний порог: " + String(setTempHigh, 1) + "C");
    }
    if (server.hasArg("l")) {
        if (server.arg("l") == "up") setTempLow += 0.5;
        if (server.arg("l") == "down") setTempLow -= 0.5;
        preferences.putFloat("low", setTempLow);
        addLog("Изменен нижний порог: " + String(setTempLow, 1) + "C");
    }
    preferences.end();
    server.sendHeader("Location", "/");
    server.send(303);
}

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // При старте реле гарантированно выключено

    sensors.begin();
    randomSeed(analogRead(0));

    // Загрузка настроек термостата
    preferences.begin("fridge-settings", true);
    setTempHigh = preferences.getFloat("high", 6.0);
    setTempLow = preferences.getFloat("low", 3.0);
    preferences.end();

    // Загрузка накопленной статистики
    preferences.begin("fridge-stats", true);
    compressorTotalRunTime = preferences.getULong("runtime", 0);
    preferences.end();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Подключение к Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi подключен!");

    configTzTime(TZ_INFO, ntpServer);
    Serial.println("Синхронизация времени запущена...");

    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.on("/set", handleSet);
    server.begin();

    // ЗАЩИТА ОТ КРАТКОВРЕМЕННОГО ОТКЛЮЧЕНИЯ СВЕТА:
    // Инициализируем lastTurnOffTime текущим временем millis() (равным 0 при старте).
    // Это заставит систему выждать ровно ANTI_SHORT_CYCLE (5 минут) перед первым возможным включением реле.
    lastTurnOffTime = millis(); 

    lastStateChangeTime = millis();
    compressorDayStartTime = millis();

    addLog("Система Стинол-102R успешно запущена. Защита компрессора активна.");
    sendVK("🔔 Стинол-102R запущен после сброса питания!\nВключена стартовая задержка компрессора (5 мин).\nIP: " + WiFi.localIP().toString() + "\nПороги: " + String(setTempLow,1) + "C - " + String(setTempHigh,1) + "C" + "\nТемпература сейчас: "String(currentTemp, 1) + "C");
}

void loop() {
server.handleClient();
unsigned long currentTime = millis();

// ========== БЛОК ПРОВЕРКИ И ОТПРАВКИ СТАТИСТИКИ В 00:00 ==========
struct tm timeinfo;
if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
        if (!msgSentToday) {
            unsigned long dayActiveTime = compressorTotalRunTime;
            
            if (compressorState) {
                dayActiveTime += (currentTime - lastStateChangeTime);
            }
            
            unsigned long totalDayTime = currentTime - compressorDayStartTime;
            if (totalDayTime == 0) totalDayTime = 1;
            
            unsigned long dayIdleTime = (totalDayTime > dayActiveTime) ? (totalDayTime - dayActiveTime) : 0;
            
            float activeHours = (float)dayActiveTime / 3600000.0;
            float idleHours = (float)dayIdleTime / 3600000.0;
            float energyConsumed = activeHours * COMPRESSOR_POWER_KW;
            
            String report = "📊 СУТОЧНАЯ СТАТИСТИКА:\n";
            report += "🔹 Работа компрессора: " + String(activeHours, 2) + " ч.\n";
            report += "🔸 Время отдыха: " + String(idleHours, 2) + " ч.\n";
            report += "⚡ Потребление за сутки: " + String(energyConsumed, 3) + " кВт⋅ч";
            
            addLog("Отправлена суточная статистика.");
            sendVK(report);
            
            compressorTotalRunTime = 0;
            saveStatsToFlash();
            compressorDayStartTime = currentTime;
            lastStateChangeTime = currentTime;
            msgSentToday = true;
        }
    } else {
        msgSentToday = false;
    }
}

// ========== ОСНОВНОЙ ТЕРМОСТАТ (КАЖДЫЕ 5 СЕКУНД) ==========
static uint32_t tmr = 0;
if (currentTime - tmr >= 5000) {
    tmr = currentTime;
    sensors.requestTemperatures();
    currentTemp = sensors.getTempCByIndex(0);
    
    // Проверка на отключение датчика
    if (currentTemp == DEVICE_DISCONNECTED_C) {
        if (compressorState) {
            compressorTotalRunTime += (currentTime - lastStateChangeTime);
            saveStatsToFlash();
            digitalWrite(RELAY_PIN, LOW);
            compressorState = false;
            lastStateChangeTime = currentTime;
            lastTurnOffTime = currentTime;
            addLog("АВАРИЯ: Датчик температуры отключен!");
            sendVK("⚠ АВАРИЯ: Датчик температуры отключен! Компрессор аварийно выключен.");
        }
        return;
    }
    
    // Логика включения компрессора
    if (currentTemp >= setTempHigh && !compressorState) {
        // Проверка анти-дребезга (защита от короткого цикла)
        if (currentTime - lastTurnOffTime >= ANTI_SHORT_CYCLE) {
            digitalWrite(RELAY_PIN, HIGH);
            compressorState = true;
            lastStateChangeTime = currentTime;
            String msg = "Включение компрессора при " + String(currentTemp, 1) + "C";
            addLog(msg);
            sendVK(msg);
        } else {
            unsigned long left = (ANTI_SHORT_CYCLE - (currentTime - lastTurnOffTime)) / 1000;
            Serial.printf("Защита компрессора после включения света: осталось ждать %d сек\n", left);
        }
    }
    // Логика выключения компрессора
    else if (currentTemp <= setTempLow && compressorState) {
        compressorTotalRunTime += (currentTime - lastStateChangeTime);
        saveStatsToFlash();
        digitalWrite(RELAY_PIN, LOW);
        compressorState = false;
        lastStateChangeTime = currentTime;
        lastTurnOffTime = currentTime;
        String msg = "Выключение компрессора при " + String(currentTemp, 1) + "C";
        addLog(msg);
        sendVK(msg);
    }
}
}