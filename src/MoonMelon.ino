/*
  Start Mosquitto MQTT server with
  $ /usr/local/Cellar/mosquitto/1.4.14_2/sbin/mosquitto
  -------------------------------- 
  NodeMcu IO Index          = Esp8266 GPIO
  static const uint8_t D0   = 16;
  static const uint8_t D1   = 5;
  static const uint8_t D2   = 4;
  static const uint8_t D3   = 0;
  static const uint8_t D4   = 2;
  static const uint8_t D5   = 14;
  static const uint8_t D6   = 12;
  static const uint8_t D7   = 13;
  static const uint8_t D8   = 15;
  static const uint8_t D9   = 3;
  static const uint8_t D10  = 1;

*/

#define FASTLED_INTERRUPT_RETRY_COUNT 0
#include <ESP8266WiFi.h>
#include "FastLED.h"
#include <Filters.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// OTA Updates
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "Eprom.h"

// sensor signal filtering
#include <Filters.h>

#include "Utils.h"

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

/********** WIFI *************/
const char* SSID1 = "MoonMelonField";
const char* WIFI_PASSWORD1 = "moonsalon";
const char* MQTT_SERVER_IP1 = "192.168.1.100";
const char* SSID2 = "Chaos-Platz";
const char* WIFI_PASSWORD2 = "FalafelPower69";
const char* MQTT_SERVER_IP2 = "192.168.0.73";

//const char* SSID2 = "UPC1374847";
//const char* WIFI_PASSWORD2 = "HHYPUMFP";
//const char* MQTT_SERVER_IP2 = "192.168.0.27";
char macAddress[20];
long lastReconnectAttempt = 0;
bool offlineMode = false;
// fallback solution: if wifi with SSID1 is not reachable, connect to wifi with SSID2
int currentNetworkIndex = 0;

/********** MQTT *************/
const char* MQTT_TOPIC = "sensor";
const char* MQTT_TOPIC_STATUS = "status";

/********** OTA Update *************/
bool otaUpdate = false;
String otaUpdateUrl = "";

/********* JSON CONFIG *************/
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
/********** LED CONFIG *************/
#define DATA_PIN    4 // working on nodeMcu V3: D2=GPIO4, GPIO4 // not working on nodeMcu V3: D0=GPIO16, D1=GPIO5, D3=GPIO0
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS    60
#define LEDS_PER_RING 12
CRGB leds[NUM_LEDS];
#define FRAMES_PER_SECOND  120

CRGBPalette16 currentPalette = OceanColors_p;
int startIndex = 0;

// fire effect
bool gReverseDirection = false;


// on - off
bool stateOn = false;
uint8_t brightness = 96;
/********** LED CONFIG END *************/

/********** SENSOR *************/
int SENSOR_MIN = 100;
int SENSOR_MAX = 900;
int inputVal = 0;
int sensorTiggerLevel = 180;
int mappedSensorVal = 0;
float distance = 0;
int lastInputVal = 0;
long lastSensorUpdate = 0;
FilterOnePole lowpassFilter( LOWPASS, 100.0 );  
/********** SENSOR *************/


WiFiClient wifiClient;
PubSubClient client(wifiClient);
Eprom eprom;

long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  eprom.begin();
  Serial.begin(9600);
  setupWifi();
  if(currentNetworkIndex == 0){
    client.setServer(MQTT_SERVER_IP1, 1883);
  } else {
    client.setServer(MQTT_SERVER_IP2, 1883);
  }
  client.setCallback(callback);

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness);
}

void setupWifi() {
  delay(10);
  Serial.println(); 
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  if(currentNetworkIndex == 0){
    Serial.print("Connecting to "); Serial.println(SSID1);
    WiFi.begin(SSID1, WIFI_PASSWORD1);
  } else {
    Serial.print("Connecting to "); Serial.println(SSID2);
    WiFi.begin(SSID2, WIFI_PASSWORD2);
  }
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  int i= 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    i++;
    if(i > 20) {
      if(currentNetworkIndex == 0){ // try the other network
        Serial.println("Connection could not be established, trying fallback network...");
        currentNetworkIndex++;
        setupWifi();
      } else {
        Serial.println("Connection could not be established, continue with no-internet mode...");
        offlineMode = true;  
      }
      return;
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.macAddress().toCharArray(macAddress, 20);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char message[length + 1];
  for (uint16_t i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  if (!processJson(message)) {
    return;
  }
}

/********************************** START PROCESS JSON*****************************************/
bool processJson(char* message) {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }

  if (root.containsKey("state")) {
    if (strcmp(root["state"], "on") == 0) {
      stateOn = true;
    }
    else if (strcmp(root["state"], "off") == 0) {
      stateOn = false;
    }
  }

  if (root.containsKey("command")){
    const char* command = root["command"];
    if (strcmp(command, "patch") == 0) {
      otaUpdate = true;
      otaUpdateUrl = String(root["url"].as<char*>()); // http://192.168.0.164:8000/Firmware/moon_melon.bin
    } else if (strcmp(command, "setTriggerLevel") == 0) {
      sensorTiggerLevel = root["val"];
      Serial.print("set new trigger level"); Serial.println(sensorTiggerLevel);
    } else if (strcmp(command, "setBrightness") == 0) {
      brightness = root["val"];
    }
  }

  return true;
}

boolean reconnect() {
  char clientId[30];
  sprintf(clientId, "moonMelon%s", macAddress);
  char myTopic[50];
  sprintf(myTopic, "sensorControl%s", macAddress);
  if (client.connect(clientId)) {
    client.subscribe("sensorControl");
    Serial.println("Subscribing to topic: sensorControl");
    client.subscribe(myTopic);
    Serial.print("Subscribing to topic:"); Serial.println(myTopic);
  }
  return client.connected();
}
long loopsPerSek = 0;
void loop() {

  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5 * 1000 && !offlineMode) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
        publishConnected();
      } else {
        Serial.println("Connection attempt failed. next try in 5 seconds...");
      }
    }
  } else {
    // Client connected   
    if(otaUpdate) {
      setColor(100, 0, 0);
      FastLED.show(); 
      patchFirmware(otaUpdateUrl);
      otaUpdate = false;
    }
  }
  client.loop();
  loopsPerSek++;
  EVERY_N_MILLISECONDS( 5000 ) { // measure performance: currently we have 37000 loops per sek
    Serial.print("loops per sek: ");Serial.println(loopsPerSek / 5);
    loopsPerSek = 0;
  }
 
  EVERY_N_MILLISECONDS( 10 ) { // 100Hz sampling rate
    int rawInput = analogRead(A0);
    inputVal = lowpassFilter.input(rawInput); 
    mappedSensorVal = map(inputVal, SENSOR_MIN, SENSOR_MAX, 0, 100);
    //Serial.println(rawInput);
    
    if(inputVal > sensorTiggerLevel) { // instant ON signal, is sent imediatelly
      if(!stateOn){
        Serial.print("stateOn. SensorTriggerLevel: "); Serial.println(sensorTiggerLevel);
        publishSensorVal(inputVal, 1);
        stateOn = true;
      }
    } else { 
      if(stateOn){
        Serial.println("stateOff");
        publishSensorVal(inputVal, 0);
        stateOn = false;
      }
    }
    lastInputVal = inputVal;
  }
  
  
  EVERY_N_MILLISECONDS( 20 ) { 
  
    if(stateOn){
      int range = constrain(map(inputVal, SENSOR_MIN, SENSOR_MAX, 1, NUM_LEDS), 1, NUM_LEDS);
      rainbow(range);
    } else {
      
      if(random(500) == 1){
        randomRing();
      }
      darken();
    }
    
    //setColor(0, 0, 30);
    //static uint8_t startIndex = 0;
    //startIndex = startIndex + 1;
    //fillLEDsFromPaletteColors( startIndex);
    //FastLED.show(); // display this frame
    if(brightness != FastLED.getBrightness()){
      Serial.print("set brightness to "); Serial.println(brightness);
      FastLED.setBrightness(brightness);
    }
  
    FastLED.show(); // display this frame
  }
  

}


void publishSensorVal(int sensorVal, int trigger) {
  // k: mac address of sensor, v: sensor value, s: above (1) or below trigger level (0)
  snprintf (msg, 75, "{\"k\":\"%s\",\"v\":\"%d\",\"s\":%d}", macAddress, sensorVal, trigger);
  Serial.println(msg);
  if(!offlineMode){
    client.publish(MQTT_TOPIC, msg);
    client.loop();
  }
}

void publishConnected() {
  // k: mac address of sensor, v: sensor value, s: above (1) or below trigger level (0)
  snprintf (msg, 75, "{\"k\":\"%s\",\"status\":\"connected\"}", macAddress);
  Serial.println(msg);
  if(!offlineMode){
    client.publish(MQTT_TOPIC_STATUS, msg);
    client.loop();
  }
}