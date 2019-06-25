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

// OTA Updates
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "Eprom.h"

// sensor signal filtering
#include <Filters.h>

#include "Utils.h"
#include <QueueArray.h>

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

/********** WIFI *************/
const char* SSID1 = "MoonMelonField";
const char* WIFI_PASSWORD1 = "moonsalon";
const char* MQTT_SERVER_IP1 = "192.168.0.10";

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

bool stateOn = false; // on - off
bool justSwitchedOff = false; //gets set to true when stateOn just went to false
uint8_t brightness = 255;
uint8_t fadeOutAmount = 2; // 0-255 (0-100%)
uint8_t sleepAnimationType = 0;
uint16_t animationProgress = 0;
uint8_t glitter = 0;
uint8_t sleepModePulsatingSpeed = 5;
uint8_t sleepModeBrightness = 80;

// indicates when the sensor has been triggered the last time
long lastActiveTimestamp = 0;
const CRGBPalette16 WaterColors1 = CRGBPalette16(0x000000, 0x000000, 0x000000, 0x000000, 
                                                    0x210048, 0x6700ff, 0x9000ff, 0x9000ff, 
                                                    0x6200e5, 0x0409bf, 0x0409bf, 0x009be9,
                                                    0x009be9, 0x00b6c1, 0x00b6c1, 0x00b6c1);

const CRGBPalette16 FireColors1 = CRGBPalette16(0x000000, 0x000000, 0x000000, 0x000000, 
                                                    0xff6900, 0xff6900, 0xff3500, 0xff3500, 
                                                    0xff0008, 0xff0008, 0xff0008, 0xff0008,
                                                    0xf600cc, 0xf600cc, 0xf600ff, 0xf600ff);

bool blink = false;
/********** LED CONFIG END *************/

/********** SENSOR *************/
int SENSOR_MIN = 100;
int SENSOR_MAX = 700;
int inputVal = 0;
int sensorTiggerLevel = 560;
int mappedSensorVal = 0;
float distance = 0;
int lastInputVal = 0;
long lastSensorUpdate = 0;
FilterOnePole lowpassFilter( LOWPASS, 100.0 );  
QueueArray <int> sensorQ;
/********** SENSOR *************/


WiFiClient wifiClient;
PubSubClient client(wifiClient);
Eprom eprom;

long lastMsg = 0;
char msg[50];
char topic[50];
int value = 0;

void setup() {
  eprom.begin();
  Serial.begin(9600);
  setupWifi();
  client.setServer(MQTT_SERVER_IP1, 1883);
  client.setCallback(callback);

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness);
}
long blinkStart = 0;

void setupWifi() {
  delay(random(1,20));
  Serial.println(); 
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  Serial.print("Connecting to "); Serial.println(SSID1);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID1, WIFI_PASSWORD1);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  int i= 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    i++;
    if(i > 5) {
      
      setColor(255, 100, 100);
      Serial.println("Connection could not be established, continue with no-internet mode...");
      delay(1000);
      FastLED.show(); 
      offlineMode = true;  
      return;
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.macAddress().toCharArray(macAddress, 20);
}

void callback(char* mqttTopic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");Serial.print(mqttTopic); Serial.print("] ");
  char message[length + 1];
  for (uint16_t i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);
  String topic = String(mqttTopic);
  if(topic.indexOf("set/brightness") >= 0){
    brightness = atoi(message);
  } else if(topic.indexOf("set/sleepModeBrightness") >= 0){
    sleepModeBrightness = atoi(message);
    Serial.print("set sleepModeBrightness "); Serial.println(sleepModeBrightness);
  } else if(topic.indexOf("set/triggerLevel") >= 0){
    sensorTiggerLevel = atoi(message);
    Serial.print("set new trigger level "); Serial.println(sensorTiggerLevel);
  } else if(topic.indexOf("set/sleepModePulsatingSpeed") >= 0){
    sleepModePulsatingSpeed = atoi(message);
    Serial.print("set sleepModePulsatingSpeed "); Serial.println(sleepModePulsatingSpeed);
  } else if(topic.indexOf("set/fadeOutAmount") >= 0){
    fadeOutAmount = atoi(message);
    Serial.print("set fadeOutAmount "); Serial.println(fadeOutAmount);
  } else if(topic.indexOf("set/sleepAnimationType") >= 0){
    sleepAnimationType = atoi(message);
    Serial.print("set sleepAnimationType "); Serial.println(sleepAnimationType);
  } else if(topic.indexOf("set/glitter") >= 0){
    glitter = atoi(message);
    Serial.print("set glitter "); Serial.println(glitter);
  } else if(topic.indexOf("blink") >= 0){
    blinkStart = millis();
    Serial.println("blink!");
  } else if(topic.indexOf("patch") >= 0){
    otaUpdate = true;
    otaUpdateUrl = String(message); // http://192.168.0.164:8000/Firmware/moon_melon.bin
  } 
}

boolean reconnect() {
  char clientId[30];
  sprintf(clientId, "moonMelon%s", macAddress);
  char myTopic[50];
  sprintf(myTopic, "sensorControl%s", macAddress);
  if (client.connect(clientId)) {
    client.subscribe("sensorControl/#");
    Serial.println("Subscribing to topic: sensorControl");
    
    char myTopic[60];
    sprintf(myTopic, "sensorControl%s/#", macAddress);
    client.subscribe(myTopic);
    Serial.print("Subscribing to topic:"); Serial.println(myTopic);
  }
  return client.connected();
}

void subscribeToOwnTopic(const char * topic){
  char myTopic[60];
  sprintf(myTopic, "sensorControl/%s/%s", macAddress, topic);
  client.subscribe(myTopic);
  Serial.print("Subscribing to topic:"); Serial.println(myTopic);
}
int out = 0;
int lastOut = 0;
long rampStart = 0;
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
    //Serial.print("loops per sek: ");Serial.print(loopsPerSek / 5); Serial.print("  input val: "); Serial.println(inputVal);
    loopsPerSek = 0;
  }
 
  EVERY_N_MILLISECONDS( 10 ) { // 100Hz sampling rate
    int rawInput = analogRead(A0);
    inputVal = lowpassFilter.input(rawInput); 
    //Serial.print(inputVal);Serial.print(" ");Serial.print(lowpassFilter2.input(rawInput));Serial.print(" ");Serial.println(out);
    mappedSensorVal = map(inputVal, SENSOR_MIN, SENSOR_MAX, 0, 100);
    //Serial.println(rawInput);
    
    if(inputVal > sensorTiggerLevel) { // instant ON signal, is sent imediatelly
      if(rampStart != 0 && millis() - rampStart > 100) {
        lastOut = out;
        out = 1;
      }
      if(!stateOn){
        rampStart = millis();
        //Serial.print("stateOn. SensorTriggerLevel: "); Serial.println(sensorTiggerLevel);
        //publishSensorVal(inputVal, 1);
        stateOn = true;
      }
      if(lastOut == 0 && out == 1){
        Serial.print("stateOn. SensorTriggerLevel: "); Serial.println(sensorTiggerLevel);
        publishSensorVal(inputVal, 1);
      }
      sensorQ.push(inputVal);
      if(sensorQ.count() > 20){
        sensorQ.pop();
      }
    } else { 
      if(stateOn){
        out = 0;
        rampStart = 0;
        Serial.println("stateOff. ");
        publishSensorVal(inputVal, 0);
        justSwitchedOff = true;
        stateOn = false;
      }
    }
  }
  
  EVERY_N_MILLISECONDS( 20 ) { 
  
    if(stateOn){
      rainbow(mapVal(inputVal));
      lastActiveTimestamp = millis();
    } else if(justSwitchedOff){ 
      int max = getMaxQVal(sensorQ);
      //Serial.print("stateOff maxVal: ");Serial.print(max); Serial.print(" currentVal:"); Serial.println(inputVal);
      rainbow(mapVal(max));
    } else if(millis() - lastActiveTimestamp > 5000) { // wait for a while till starting the sleep mode animation again
      pulsate();
    }
    darken(fadeOutAmount);

    if(glitter > 0) {
      addGlitter(glitter);
    }

    if(brightness != FastLED.getBrightness()){
      Serial.print("set brightness to "); Serial.println(brightness);
      FastLED.setBrightness(brightness);
    }
    if(blinkStart != 0){
      if(millis() - blinkStart < 1000) {
        setColor(100, 255, 0);
      } else {
        blinkStart = 0;
      }
      publishSensorVal(10000, 1);
    }
    FastLED.show(); // display this frame
    justSwitchedOff = false;
  }
  
  EVERY_N_MILLISECONDS( 250 ) { 
    publishDebug(inputVal);
  }
}

int mapVal(int inputVal){
  return constrain(map(inputVal, sensorTiggerLevel, SENSOR_MAX, 1, NUM_LEDS), 1, NUM_LEDS);
}

void publishSensorVal(int sensorVal, int trigger) {
  // k: mac address of sensor, v: sensor value, s: above (1) or below trigger level (0)
  snprintf (msg, 75, "{\"k\":\"%s\",\"v\":\"%d\",\"s\":%d}", macAddress, sensorVal, trigger);
  //Serial.println(msg);
  if(!offlineMode){
    client.publish(MQTT_TOPIC, msg);
    client.loop();
  }
}

void publishDebug(int sensorVal) {
  snprintf (topic, 75, "sensorInfo/%s", macAddress);
  snprintf (msg, 75, "%d", sensorVal);
  if(!offlineMode){
    client.publish(topic, msg);
    client.loop();
  }
}

void publishEcho() {
  snprintf (msg, 75, "{\"k\":\"%s\",\"v\":\"echo\"}", macAddress);
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