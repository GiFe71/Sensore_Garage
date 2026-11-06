#pragma once

// Copy to secrets.h and fill in the local values. Never commit secrets.h.
namespace secrets
{
  constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
  constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
  constexpr char MQTT_BROKER[] = "192.168.1.12";
  constexpr char MQTT_USERNAME[] = "";
  constexpr char MQTT_PASSWORD[] = "";
  constexpr char MQTT_CLIENT_NAME[] = "Garage_1";
  constexpr short MQTT_PORT = 1883;
  constexpr char OTA_PASSWORD[] = "CHANGE_ME";
}
