

/*
  * Generic ESP8266 Module
  Versione 1.00
    Unificazione messaggi Mqtt

 */
#include <Arduino.h>
#include <DHTesp.h>
#include <ArduinoJson.h>
#include <EspMQTTClient.h>
#include "../lib/secrets.h"

//*******************************
// Dati inizializzazione MQTT
//*******************************
EspMQTTClient client(
  secrets::WIFI_SSID,
  secrets::WIFI_PASSWORD,
  secrets::MQTT_BROKER,
  secrets::MQTT_USERNAME,
  secrets::MQTT_PASSWORD,
  secrets::MQTT_CLIENT_NAME,
  secrets::MQTT_PORT
);
static_assert(sizeof(secrets::MQTT_CLIENT_NAME) > 1, "Web username must not be empty");
static_assert(sizeof(secrets::OTA_PASSWORD) > 1, "OTA/web password must not be empty");
//#define LED_BUILTIN 2
#define DHTPIN 0
#define DHTPWR 2
#define ERRORE_TEMPERATURA -0.7
#define TIMEOUTINVIO 60000
// NETWORK: Static IP details...// Set your Static IP address
IPAddress local_IP(192, 168, 1, 36);
// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);   //optional

DHTesp dht;
TempAndHumidity newValues;

float Temperature[3], Humidity[3], Dew[3];
ComfortState cf;
String comfortStatus;
byte Perception;
String PerceptionStatus;

String chipID;
char charChipID[10];
int THDcount=0;
int sampleCount=0;
uint32_t MinDHTDelay;
const char* vers = "\"version\":\"1.00\",\"date\":\"2024.12.03\"";
char resp_vers[300];
const char* sensor_name = secrets::MQTT_CLIENT_NAME;
uint32_t runTime = 0;  // time for read operation
uint32_t runTime1 = 0;  // time for next send data

void ReadDHT(){
  newValues = dht.getTempAndHumidity();
  const float temperature = newValues.temperature + ERRORE_TEMPERATURA;
  const float humidity = newValues.humidity;
  const float dew = dht.computeDewPoint(temperature, humidity, false);
  if (!isfinite(temperature) || !isfinite(humidity) || humidity <= 0 ||
      humidity > 100 || !isfinite(dew)) {
    Serial.println(F("Failed to read valid DHT data!"));
    sampleCount = 0;
    THDcount = 0;
    return;
  }
  Temperature[THDcount] = temperature;
  Humidity[THDcount] = humidity;
  Dew[THDcount] = dew;
  THDcount = (THDcount + 1) % 3;
  if (sampleCount < 3) ++sampleCount;
}

void SendData(){
  if (sampleCount == 0 || !client.isMqttConnected()) {
    return;
  }
  float TempT = 0, HumiT = 0, DewT = 0;
  for (int i = 0; i < sampleCount; ++i) {
    TempT += Temperature[i];
    HumiT += Humidity[i];
    DewT += Dew[i];
  }
  TempT /= sampleCount;
  HumiT /= sampleCount;
  DewT /= sampleCount;
  JsonDocument message;
  dht.getComfortRatio(cf, TempT, HumiT, false);
  switch(cf) {
    case Comfort_OK:
      comfortStatus = "Confortevole";
      break;
    case Comfort_TooHot:
      comfortStatus = "Caldo";
      break;
    case Comfort_TooCold:
      comfortStatus = "Freddo";
      break;
    case Comfort_TooDry:
      comfortStatus = "Secco";
      break;
    case Comfort_TooHumid:
      comfortStatus = "Umido";
      break;
    case Comfort_HotAndHumid:
      comfortStatus = "Caldo Umido";
      break;
    case Comfort_HotAndDry:
      comfortStatus = "Caldo Secco";
      break;
    case Comfort_ColdAndHumid:
      comfortStatus = "Freddo Umido";
      break;
    case Comfort_ColdAndDry:
      comfortStatus = "Freddo Secco";
      break;
    default:
      comfortStatus = "Unknown";
      break;
  };
  Perception = dht.computePerception(TempT, HumiT, false);
  switch(Perception) {
    case 0:
      PerceptionStatus = "Secco";
      break;
    case 1:
      PerceptionStatus = "Molto Confortevole";
      break;
    case 2:
      PerceptionStatus = "Confortevole";
      break;
    case 3:
      PerceptionStatus = "OK";
      break;
    case 4:
      PerceptionStatus = "Sconfortevole";
      break;
    case 5:
      PerceptionStatus = "Abbastanza Sconfortevole";
      break;
    case 6:
      PerceptionStatus = "Molto Sconfortevole";
      break;
    case 7:
      PerceptionStatus = "Estremamente Sconfortevole";
      break;
    default:
      PerceptionStatus = "Unknown";
      break;
  };
  message["sensor_name"]= sensor_name;
  message["chip_id"]= chipID;
  message["telemetry"]["temperature"]= roundf(TempT * 10) / 10.0f;
  message["telemetry"]["humidity"]= roundf(HumiT * 10) / 10.0f;
  message["telemetry"]["dew_point"]= roundf(DewT * 10) / 10.0f;
  message["telemetry"]["confort"]= comfortStatus;
  message["telemetry"]["percezione"]= PerceptionStatus;
  message["telemetry"]["time"]= "";

  String output="";
  serializeJson(message, output);
  if(! client.publish( "home/garage/sensor/availability", "online" )){
    Serial.println(F("Failed To Send Availability to HA"));
  }
  else {
    Serial.println(F("Availability HA Sent OK!"));
  };
  if(! client.publish( "home/garage/sensor", output )){
    Serial.println(F("Failed To Send Data"));
  }
  else {
    Serial.println(F("Data Sent OK!"));
    Serial.println(output);
  };
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  };
  Serial.println();
  delay(500);
  // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("STA Failed to configure");
  };
  WiFi.setHostname(sensor_name); //define hostname
  // Optional functionalities of EspMQTTClient
  //Change the keep alive interval (15 seconds by default)
  client.setKeepAlive(90);
  //Tell the broker to establish a persistent connection. Disabled by default. Must be called before the first loop() execution
  client.enableMQTTPersistence();
  //Change the delay between each MQTT reconnection attempt. Default is 15 seconds.
  //client.setMqttReconnectionAttemptDelay(const unsigned int milliseconds);
  //Change the delay between each Wifi reconnection attempt. Default is 60 seconds.
  //client.setWifiReconnectionAttemptDelay(const unsigned int milliseconds);
  /*
  bool isConnected(); // Return true if everything is connected.
  bool isWifiConnected(); // Return true if WiFi is connected.
  bool isMqttConnected(); // Return true if MQTT is connected.
  bool getConnectionEstablishedCount() // Return the number of time onConnectionEstablished has been called since the beginning
  */
  // EspMQTTClient debug output includes the web password: keep it disabled.
  client.enableDebuggingMessages(false);
  if (!client.setMaxPacketSize(512)) {
    Serial.println(F("Failed to allocate MQTT packet buffer"));
  }
  client.enableHTTPWebUpdater(secrets::MQTT_CLIENT_NAME, secrets::OTA_PASSWORD);
  client.enableOTA(secrets::OTA_PASSWORD);
  client.enableLastWillMessage("home/garage/sensor/availability", "offline");  // You can activate the retain flag by setting the third parameter to true

  Serial.println(F("GiFe MQTT Read Sensors Internal Version"));
  Serial.println(vers);
  dht.setup(DHTPIN,DHTesp::DHT22); // data pin 4=D2 2=D4
  // initialize digital pin LED_BUILTIN as an output.
  //pinMode(LED_BUILTIN, OUTPUT);
  //pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DHTPWR, OUTPUT);
  digitalWrite(DHTPWR, LOW);
  MinDHTDelay=dht.getMinimumSamplingPeriod();
  if(MinDHTDelay<2000){
    MinDHTDelay=2000;
  }
  MinDHTDelay=MinDHTDelay*5; // Read every 10 seconds for a DHT22.
  Serial.print("Min DHt Delay: ");
  Serial.println(MinDHTDelay);
  int i;
  for (i=0;i<3;i++){
    Temperature[i]=0;
    Humidity[i]=0;
    Dew[i]=0;
  };
  chipID = String(ESP.getChipId(), HEX);
  ReadDHT();
  runTime = millis();
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished(){
  // Subscribe to "mytopic/test" and display received message to Serial
  client.subscribe("forceread", [](const String & payload) {
    if (payload == sensor_name) {
      Serial.println(payload);
      SendData();
    };
  });
  client.subscribe("fw_version", [](const String & payload) {
    Serial.println(payload);
    if (payload == sensor_name) {
      Serial.println("Got: Version Request");
      if (! client.publish("fw_version_resp",resp_vers)) {
        Serial.println("Failed To Send Version");
      }
      else {
        Serial.println(F("Version Sent OK!"));
        Serial.println(vers);
      };
    };
  });
  if(! client.publish( "home/garage/sensor/availability", "online" )){
    Serial.println(F("Failed To Send Availability to HA"));
  }
  else {
    Serial.println(F("Availability HA Sent OK!"));
  };
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  chipID = String(ESP.getChipId(), HEX);
  chipID.toCharArray(charChipID, sizeof(charChipID));
  String ipaddress = WiFi.localIP().toString();
  char charipaddress[16];
  ipaddress.toCharArray(charipaddress, sizeof(charipaddress));
  snprintf(resp_vers, sizeof(resp_vers),
    "{\"Sensor Name\":\"%s\",%s,\"IP\":\"%s\",\"Chip ID\":\"%s\"}",
    sensor_name, vers, charipaddress, charChipID);
  Serial.println(resp_vers);
  SendData();
}

void loop() {
  if (millis() - runTime >= MinDHTDelay) {
    Serial.println("Leggo Sensore");
    ReadDHT();
	  runTime = millis();
  };
  if (millis() - runTime1 >= TIMEOUTINVIO) {
    SendData();
	  runTime1 = millis();
  };
  client.loop();
  delay(200);
}
