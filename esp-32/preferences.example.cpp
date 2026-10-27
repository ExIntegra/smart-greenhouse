// этот файл служит примеров как можно вшивать
// чувствительные данные в долгосрочную память микрокотнроллера.
// для работы необходима библиотека <Preferences.h>. скачать ее
// можно через библиотеку arduino или github.
// также можно загрузить в память SERVER_URL.

#include <Preferences.h>

Preferences prefs;

void setup() {
  Serial.begin(115200);
  prefs.begin("config", false);
  prefs.putString("WIFI_SSID", "MyWiFi");
  prefs.putString("WIFI_PASSWORD", "MySecretPassword");
  prefs.putString("API_KEY", "MySecretApiKey");
  prefs.end();
}

void loop() {}
