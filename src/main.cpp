#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <SimpleTimer.h>
#include <Wire.h>
#include <Adafruit_Si7021.h>
#include <Adafruit_BMP085.h>
#include <OLED.h>
#include <RestClient.h>

void setup(void);
void loop(void);
void callback(char* p_topic, byte* p_payload, unsigned int p_length);
void reconnect(void);
void UpdateDisplay(void);
void UpdateConsole(void);
void UpdatePWS(void);

#define MQTT_VERSION MQTT_VERSION_3_1_1
#define SWITCH_DURATION 2000
#define MAX_SRV_CLIENTS 1

const char* ssid = _WIFI_SSID_;
const char* password = _WIFI_PASS_;

// MQTT: ID, server IP, port, username and password
const PROGMEM char* MQTT_CLIENT_ID = _MQTT_CLIENT_ID_;
const PROGMEM char* MQTT_SERVER_IP = _MQTT_SERVER_IP_;
const PROGMEM uint16_t MQTT_SERVER_PORT = _MQTT_SERVER_PORT_;
const PROGMEM char* MQTT_USER = _MQTT_USER_;
const PROGMEM char* MQTT_PASSWORD = _MQTT_PASSWORD_;
String PWS_ID = _PWS_ID_;
String PWS_PASSWORD = _PWS_PASSWORD_;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

WiFiServer server(23);
WiFiClient serverClients[MAX_SRV_CLIENTS];

Adafruit_Si7021 sensor = Adafruit_Si7021();
Adafruit_BMP085 bmp;

SimpleTimer appTimer;

// Declare OLED display
OLED display(D4, D5);

RestClient pws = RestClient("weatherstation.wunderground.com");

String gTemperature, gPressure, gHumidity;
String gUploadStatus = "N/U";

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }

  Serial.println(p_topic);
  Serial.print("Processing payload: ");
  Serial.println(payload);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("INFO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(_MQTT_CLIENT_ID_, _MQTT_USER_, _MQTT_PASSWORD_)) {
      Serial.println("INFO: connected");
    } else {
      Serial.print("ERROR: failed, rc=");
      Serial.print(client.state());
      Serial.println("DEBUG: try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void UpdateDisplay() {
  display.print("Weather Station");

  display.print("WiFi sig: ", 1, 0);
  String s = String(WiFi.RSSI());
  display.print((char *)s.c_str(), 1, 9);
  display.print("dBm", 1, 13);

  display.print("Humid: ", 2, 0);
  s = String(sensor.readHumidity(), 1);
  gHumidity = s;
  display.print((char *)s.c_str(), 2, 10);
  display.print("%", 2, 15);

  display.print("Temp:  ", 3, 0);
  s = String((sensor.readTemperature() * 9 / 5) + 32, 1);
  gTemperature = s;
  display.print((char *)s.c_str(), 3, 10);
  display.print("F", 3, 15);

  display.print("Press: ", 4, 0);
  s = String(bmp.readSealevelPressure(241.20) * 0.0002953);
  gPressure = s;
  display.print((char *)s.c_str(), 4, 10);
  display.print("\"", 4, 15);

  display.print("PWS: ", 6, 0);
  display.print((char *)gUploadStatus.c_str(), 6, 5);
}

void UpdateConsole() {
    Serial.print("BMP180 Temp: ");
    Serial.print((bmp.readTemperature() * 9 / 5) + 32.0);
    Serial.println(" F");

    Serial.print("BMP180 Pressure: ");
    Serial.print(bmp.readPressure() * 0.0002953);
    Serial.println(" in");

    Serial.print("Si7021 Humidity: ");
    Serial.print(sensor.readHumidity());
    Serial.println(" %");

    Serial.print("Si7021 Temp: ");
    Serial.print((sensor.readTemperature() * 9 / 5) + 32.0);
    Serial.println(" F");

    Serial.print("BMP180 Sealevel pressure: ");
    Serial.print(bmp.readSealevelPressure(241.20) * 0.0002953);
    Serial.println(" in");

    Serial.print("WiFi signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}

String response;
void UpdatePWS() {

  // https://weatherstation.wunderground.com/weatherstation/updateweatherstation.php?ID=KCASANFR5&PASSWORD=XXXXXX&dateutc=2000-01-01+10%3A32%3A35&winddir=230&windspeedmph=12&windgustmph=12&tempf=70&rainin=0&baromin=29.1&dewptf=68.2&humidity=90&weather=&clouds=&softwaretype=vws%20versionxx&action=updateraw

  String request;

  request = "/weatherstation/updateweatherstation.php?ID=";
  request += _PWS_ID_;
  request += "&PASSWORD=";
  request += _PWS_PASSWORD_;
  request += "&dateutc=now";
  request += "&tempf=";
  request += gTemperature;
  request += "&baromin=";
  request += gPressure;
  request += "&humidity=";
  request += gHumidity;
  request += "&action=updateraw";

  Serial.println(request);
  response = "";
  int status = pws.get(request.c_str(), &response);

  Serial.print("Request status: ");
  Serial.println(status);

  Serial.print("Server response: ");
  Serial.println(response);

  gUploadStatus = response;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  server.begin();
  server.setNoDelay(true);

  // initialize the temp/humidity sensor
  sensor.begin();

  // initialize the pressure sensor
  if (!bmp.begin(1)) {
    Serial.println("Could not find a valid BMP085 sensor, check wiring!");
  //while (1) {}
  }

  // Initialize display
  display.begin();

  // Update the user interface and telnet client
  appTimer.setInterval(2000, UpdateConsole);
  appTimer.setInterval(2000, UpdateDisplay);
  appTimer.setInterval(30000, UpdatePWS);

  Serial.print("INFO: Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Connected.");

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(_MQTT_CLIENT_ID_);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
    //String type;
    // if (ArduinoOTA.getCommand() == U_FLASH)
      // type = "sketch";
    // else // U_SPIFFS
      // type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    // Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // init the MQTT connection
  client.setServer(_MQTT_SERVER_IP_, _MQTT_SERVER_PORT_);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  uint8_t i;
  //check if there are any new clients
  if (server.hasClient()){
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      //find free/disconnected spot
      if (!serverClients[i] || !serverClients[i].connected()){
        if(serverClients[i]) serverClients[i].stop();
        serverClients[i] = server.available();
        Serial1.print("New client: "); Serial1.print(i);
        continue;
      }
    }
    //no free/disconnected spot so reject
    WiFiClient serverClient = server.available();
    serverClient.stop();
  }

  //check clients for data
  for(i = 0; i < MAX_SRV_CLIENTS; i++){
    if (serverClients[i] && serverClients[i].connected()){
      if(serverClients[i].available()){
        //get data from the telnet client and push it to the UART
        while(serverClients[i].available()) Serial.write(serverClients[i].read());
      }
    }
  }

  //check UART for data
  // To echo local debug information (from Serial.println()) place a
  // jumper wire between TX & RX on the ESP8266.
  if(Serial.available()){
    size_t len = Serial.available();
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);
    //push UART data to all connected telnet clients
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      if (serverClients[i] && serverClients[i].connected()){
        serverClients[i].write(sbuf, len);
        delay(1);
      }
    }
  }

  //UpdateDisplay();
  //UpdateConsole();

  appTimer.run();
  ArduinoOTA.handle();
  client.loop();
}
