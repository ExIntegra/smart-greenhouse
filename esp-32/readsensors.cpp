// readsensors.cpp - файл для считывания датчиков и обработки данных, полученных
// из этих датчиков. применяется esp32 - микроконтроллер с WiFi модулем.
// перед подключением к сети убедитесь, что контакты подключены правильно и нет риска
// возникновения короткого замыкания.

#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"
#include <ArduinoJson.h>
#include <Preferences.h>

String WIFI_SSID;
String WIFI_PASSWORD;
String API_KEY;


const char* SERVER_URL ="http://192.168.0.133:8000/api/v1/telemetry";

// Метка для метрик, чтобы понимать от какого устройства приходят данные
const char* DEVICE_ID = "greenhouse-esp32-01";

// Имена датчиков для меток в метриках
const char* DS18B20_SENSOR_ID       = "ds18b20-01";
const char* DHT11_SENSOR_ID         = "dht11-01";
const char* SOIL_MOISTURE_SENSOR_ID = "soil-moisture-01";
const char* LIGHT_SENSOR_ID         = "light=sensor-01";

// Калибровочные значения датчика влажности почвы
const int SOIL_DRY = 3400;
const int SOIL_WET = 1140;

// Калибровочные значения датчика светат
const int LIGHT_DARK = 4095;
const int LIGHT_BRIGHT = 0;

// Подключенные пины датчиков к GPIO ESP32
const int ONE_WIRE_PIN      = 4;
const int DHT11_PIN         = 5;
const int SOIL_MOISTURE_PIN = 34;
const int LIGHT_SENSOR_PIN  = 35;

// Таймер. 1000 - 1 сек.
const unsigned long SEND_INTERVAL = 10000;
unsigned long lastSend = 0;

// Инициализируем объект preferences, считывая чувствительные даные (Wi-Fi, API-KEY)
// вшитые в микроконтроллер ESP32
Preferences prefs;

// Инициализация датчиков DHT11, DS18B20
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

DHT dht(DHT11_PIN, DHT11);

struct DhtData {
  float temperature;
  float humidity;
};


DhtData readDht() {
  DhtData data;

  data.temperature = dht.readTemperature();
  data.humidity = dht.readHumidity();

  return data;
}

float readSoilMoisture() {
  int raw = analogRead(SOIL_MOISTURE_PIN);

  float percent =
    (SOIL_DRY - raw) * 100.0 /
    (SOIL_DRY - SOIL_WET);

  percent = constrain(percent, 0.0, 100.0);

  Serial.print("Soil raw: ");
  Serial.println(raw);

  Serial.print("Soil moisture: ");
  Serial.print(percent);
  Serial.println("%");

  return percent;
}

float readLightSensor() {
  int raw = analogRead(LIGHT_SENSOR_PIN);

  float percent =
    (LIGHT_DARK - raw) * 100.0 /
    (LIGHT_DARK - LIGHT_BRIGHT);

  percent = constrain(percent, 0.0, 100.0);

  Serial.print("Light raw: ");
  Serial.println(raw);

  Serial.print("Light: ");
  Serial.print(percent);
  Serial.println("%");

  return percent;
}

bool dataIsValid(const DhtData& data) {
  if (isnan(data.temperature) || isnan(data.humidity)) {
    return false;
  }

  if (data.humidity < 0 || data.humidity > 100) {
    return false;
  }

  return true;
}

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

void sendTelemetry(const JsonDocument& doc) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection error");
    return;
  }

  HTTPClient http;

  http.begin(SERVER_URL);
  http.setTimeout(5000);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", API_KEY);

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

  return (
    !WIFI_SSID.isEmpty() &&
    !WIFI_PASSWORD.isEmpty() &&
    !API_KEY.isEmpty()
  );
}

void setup() {
  Serial.begin(115200);

  dht.begin();
  sensors.begin();

  if (!loadConfig()) {
    Serial.println("NVS config error");

    while (true) {
      delay(1000);
    }
  }

  WiFi.mode(WIFI_STA);
  connectWiFi();
}

void loop() {

  // ============ TIMER ==============
  unsigned long now = millis();

  if (now - lastSend < SEND_INTERVAL) {
    return;
  }

  lastSend = now;

  //============ DS18B20 ============

  sensors.requestTemperatures();

  float dsTemperature = sensors.getTempCByIndex(0);

  if (dsTemperature == DEVICE_DISCONNECTED_C) {
    Serial.println("DS18B20 ERROR");
  } else {
    Serial.print("DS18B20 temperature: ");
    Serial.println(dsTemperature);

    JsonDocument dsJson;

    dsJson["device_id"] = DEVICE_ID;
    dsJson["sensor_id"] = DS18B20_SENSOR_ID;
    dsJson["temperature_c"] = dsTemperature;

    sendTelemetry(dsJson);
  }

  //============= DHT11 ==============

  DhtData dhtData = readDht();

  if (!dataIsValid(dhtData)) {
    Serial.println("DHT11 ERROR");
  } else {
    Serial.print("DHT11 temperature: ");
    Serial.println(dhtData.temperature);

    Serial.print("DHT11 humidity: ");
    Serial.println(dhtData.humidity);

    JsonDocument dhtJson;

    dhtJson["device_id"] = DEVICE_ID;
    dhtJson["sensor_id"] = DHT11_SENSOR_ID;
    dhtJson["temperature_c"] = dhtData.temperature;
    dhtJson["humidity_percent"] = dhtData.humidity;

    sendTelemetry(dhtJson);
  }
  
  //============ SOIL_MOISTURE ===========

  float soilMoisture = readSoilMoisture();

  JsonDocument soilMoistureJson;

  soilMoistureJson["device_id"] = DEVICE_ID;
  soilMoistureJson["sensor_id"] = SOIL_MOISTURE_SENSOR_ID;
  soilMoistureJson["humidity_percent"] = soilMoisture;
  
  sendTelemetry(soilMoistureJson);

  //============= LIGHT_SENSOR ============

  float lightSensor = readLightSensor();

  JsonDocument lightSensorJson;

  lightSensorJson["device_id"] = DEVICE_ID;
  lightSensorJson["sensor_id"] = LIGHT_SENSOR_ID;
  lightSensorJson["light_percent"] = lightSensor;

  sendTelemetry(lightSensorJson);
}
