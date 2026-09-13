#include <Preferences.h>

Preferences prefs;

void setup() {
  Serial.begin(115200);
  prefs.begin("config", false);
  prefs.putString("WIFI_SSID", "MyWiFi");
  prefs.putString("WIFI_PASSWORD ", "MySecretPassword");
  prefs.putString("API_KEY", "MySecretApiKey");
  prefs.end();
}

void loop() {}
