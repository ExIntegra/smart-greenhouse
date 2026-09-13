#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <Preferences.h>

String WIFI_SSID;
String WIFI_PASSWORD;
String API_KEY;

const char* SERVER_URL = "http://192.168.1.100:8000/api/v1/telemetry";

const char* DEVICE_ID = "greenhouse-esp32-01";
const char* SENSOR_ID = "ds18b20-01";

// Подключение к цифровому пину 4.
const int ONE_WIRE_PIN = 4;
// Интервал перед отправкой по http 10 сек.
const unsigned long SEND_INTERVAL = 10000;
// Время последней отправки в миллисекундах с момента запуска ESP32.
unsigned long lastSend = 0;

// Для сохранения секретов в энергонезависимую память
// микроконтроллера и дальшнейшее чтение этих данных
Preferences prefs;

// Создаем объект и передаем к какому выводу подключен датчик и передаем ссылку на объект oneWire в sensors.
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  WiFi.disconnect();
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());

  unsigned long start = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 10000
  ) {
    delay(250);
  }
}

void sendTemperature(float temperature) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;

  http.begin(SERVER_URL);
  http.setTimeout(5000);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", API_KEY);

  // Инициализируем объект для отпрвки JSON серверу.
  JsonDocument doc;

  doc["device_id"] = DEVICE_ID;
  doc["sensor_id"] = SENSOR_ID;
  doc["temperature_c"] = temperature;

  String json;
  serializeJson(doc, json);

  int code = http.POST(json);

  Serial.print("POST ");
  Serial.print(code);
  Serial.print(" ");
  Serial.println(json);

  if (code > 0) {
    Serial.println(http.getString());
  }

  http.end();
}

bool loadConfig() {
  prefs.begin("config", true);

  WIFI_SSID = prefs.getString("WIFI_SSID", "");
  WIFI_PASSWORD = prefs.getString("WIFI_PASSWORD", "");
  API_KEY = prefs.getString("API_KEY", "");

  prefs.end();

  return !WIFI_SSID.isEmpty() && !WIFI_PASSWORD.isEmpty() && !API_KEY.isEmpty();
}

void setup() {
  Serial.begin(115200);

  if (!loadConfig()) {
    Serial.println("NVS config error");

    // При неисправной конфигурации останавливаем работу esp32.
    while (true) {
      delay(1000);
    }
  }

  sensors.begin();

  WiFi.mode(WIFI_STA);
  connectWiFi();
}

void loop() {
  unsigned long now = millis();

  if (now - lastSend < SEND_INTERVAL) {
    return;
  }

  lastSend = now;

  // Выполняем преобразование температуры.
  sensors.requestTemperatures();

  // Считываем температуру с датчиков.
  float temperature = sensors.getTempCByIndex(0);

  if (temperature == DEVICE_DISCONNECTED_C) {
    Serial.println("DS18B20 ERROR");
    return;
  }

  Serial.print("Temperature: ");
  Serial.println(temperature);

  sendTemperature(temperature);
}
