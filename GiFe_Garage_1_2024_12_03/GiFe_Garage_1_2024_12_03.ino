

/*
  * Generic ESP8266 Module
  Versione 1.00
    Unificazione messaggi Mqtt

 */
#include <Arduino.h>
#include <DHTesp.h>
#include <ArduinoJson.h>
#include <EspMQTTClient.h>

//*******************************
// Dati inizializzazione MQTT
//*******************************
EspMQTTClient client(
  "MyWiFiLow",
  "lorenzo091006",
  "192.168.1.12",  // MQTT Broker server ip
  "",   // Can be omitted if not needed
  "",   // Can be omitted if not needed
  "Garage_1",     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);
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
int MinDHTDelay;
const char* vers = "\"version\":\"1.00\",\"date\":\"2024.12.03\"";
char resp_vers[300];
const char* sensor_name = "Garage_1";
uint32_t runTime = 0;  // time for read operation
uint32_t runTime1 = 0;  // time for next send data
uint32_t runTime2 = 0;  // time for next blink

void ReadDHT(){
  newValues = dht.getTempAndHumidity();
  int i;
  Serial.print(".");
  Serial.println(newValues.humidity);
  Serial.println(newValues.temperature);
  if (isnan(newValues.humidity) || isnan(newValues.temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    int i=0;
    while(i<3){
      Temperature[i]=0;
      Humidity[i]=0;
      Dew[i] = 0;
      i++;
    }
    return;
  }
  else{
    Temperature[THDcount]=newValues.temperature+ERRORE_TEMPERATURA;
    Humidity[THDcount]=newValues.humidity;
    //double gamma = log(newValues.humidity/100) + ((17.62*(newValues.temperature+ERRORE_TEMPERATURA)) / (243.5+newValues.temperature+ERRORE_TEMPERATURA));
    //Dew[THDcount] = 243.5*gamma / (17.62-gamma);
    Dew[THDcount] = dht.computeDewPoint(Temperature[THDcount], Humidity[THDcount], false);
    for (i=0;i<3;i++){
      if(Temperature[i]==0){Temperature[i]=newValues.temperature+ERRORE_TEMPERATURA;};
      if(Humidity[i]==0){Humidity[i]=newValues.humidity;};
      if(Dew[i]==0){Dew[i]=Dew[THDcount];};
    }
    if (THDcount==2){
      THDcount=0;
    }
    else{
      THDcount++;
    }
  }
}

void SendData(){
  int TempT, HumiT, DewT;
  JsonDocument message;
  String Sens_name=(String)sensor_name;
  String json = "{\"sensor_name\":\"" + Sens_name + "\",\"chip_id\":\"1351824120\",\"telemetry\":{\"temperature\":24,\"humidity\":50,\"dew_point\":24,\"confort\":\"\",\"percezione\":\"\"}}";
  DeserializationError error = deserializeJson(message,json);
  if (error)
    return;
  TempT=(int)((Temperature[0]+Temperature[1]+Temperature[2])/3*10);
  HumiT=(int)((Humidity[0]+Humidity[1]+Humidity[2])/3*10);
  DewT=(int)((Dew[0]+Dew[1]+Dew[2])/3*10);
  if(TempT!=0 && HumiT!=0 && DewT!=0){
    float crn = dht.getComfortRatio(cf, TempT, HumiT, false);
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
    Perception = dht.computePerception(Temperature[THDcount], Humidity[THDcount], false);
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
    message["telemetry"]["temperature"]= TempT/10.0;
    message["telemetry"]["humidity"]= HumiT/10.0;
    message["telemetry"]["dew_point"]= DewT/10.0;
    message["telemetry"]["confort"]= comfortStatus;
    message["telemetry"]["percezione"]= PerceptionStatus;
    message["telemetry"]["time"]= "";
    
    String output="";
    serializeJson(message, output);
    //message.printTo(output);
    int len=output.length()+5;
    char attributes[len];
    output.toCharArray( attributes, len );
    if(! client.publish( "home/garage/sensor/availability", "online" )){
      Serial.println(F("Failed To Send Availability to HA"));
    } 
    else {
      Serial.println(F("Availability HA Sent OK!"));
    };
    if(! client.publish( "home/garage/sensor", attributes )){
      Serial.println(F("Failed To Send Data"));
    } 
    else {
      Serial.println(F("Data Sent OK!"));
      Serial.println(attributes);
    };
  }
  else{
    Serial.println(F("Invalid Data To Send"));
    /*
    message["sensor_name"]= sensor_name;
    message["chip_id"]= chipID;
    message["telemetry"]["temperature"]= "";
    message["telemetry"]["humidity"]= "";
    message["telemetry"]["dew_point"]= "";
    message["telemetry"]["confort"]= "";
    message["telemetry"]["percezione"]= "";
    String output="";
    serializeJson(message, output);
    //message.printTo(output);
    int len=output.length()+5;
    char attributes[len];
    output.toCharArray( attributes, len );
    if(! client.publish( "sensor", attributes )){
      Serial.println(F("Failed To Send Data"));
    } 
    else {
      Serial.println(F("Data Sent OK!"));
      Serial.println(attributes);
    };
    */
  }
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
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword.
  // These can be overridded with enableHTTPWebUpdater("user", "password").
  client.enableOTA("GiFe"); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. 
  //Can be overridden with enableOTA("password", port).
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
  MinDHTDelay=MinDHTDelay*5;
  Serial.print("Min DHt Delay: ");
  Serial.println(MinDHTDelay);
  int i;
  for (i=0;i<3;i++){
    Temperature[i]=0;
    Humidity[i]=0;
    Dew[i]=0;
  };
  ReadDHT();
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
  resp_vers[0]=0;
  strcat(resp_vers,"{\"Sensor Name\":\"");
  strcat(resp_vers,sensor_name);
  strcat(resp_vers,"\",");
  strcat(resp_vers,vers);
  strcat(resp_vers,",\"IP\":\"");
  strcat(resp_vers,charipaddress);
  strcat(resp_vers,"\",\"Chip ID\":\"");
  strcat(resp_vers,charChipID);
  strcat(resp_vers,"\"}");
  Serial.println(resp_vers);
  SendData();
}

void loop() {
  if (millis() - runTime >= MinDHTDelay*5) {
    Serial.println("Leggo Sensore");
    ReadDHT();
	  runTime = millis();
  };
  if (millis() - runTime1 >= TIMEOUTINVIO) {
    SendData();
	  runTime1 = millis();
  };
  //if (millis() - runTime2 >= 800) {
  //  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));  // toggle the LED
	//  runTime2 = millis();
  //};
  client.loop();
  delay(200);
}