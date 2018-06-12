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
const char* ssid = "Chaos-Platz";
const char* password = "FalafelPower69";
const char* mqtt_server = "192.168.0.73";
//const char* ssid = "UPC1374847";
//const char* password = "HHYPUMFP";
//const char* mqtt_server = "192.168.0.27";
char macAddress[20];
long lastReconnectAttempt = 0;

/********** MQTT *************/
const char* MQTT_TOPIC = "sensor";
const char* MQTT_TOPIC_STATUS = "status";

/********** OTA Update *************/
bool otaUpdate = false;

/********* JSON CONFIG *************/
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
/********** LED CONFIG *************/
#define DATA_PIN    4 // working on nodeMcu V3: D2=GPIO4, GPIO4 // not working on nodeMcu V3: D0=GPIO16, D1=GPIO5, D3=GPIO0
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS    60
CRGB leds[NUM_LEDS];
#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  120

// fire effect
bool gReverseDirection = false;

// rainbow
uint8_t gHue = 0;

// on - off
bool stateOn = false;
int brightness = 0;
/********** LED CONFIG END *************/

/********** SENSOR *************/
int SENSOR_MIN = 250;
int SENSOR_MAX = 900;
int inputVal = 0;
int mappedSensorVal = 0;
float distance = 0;
int lastInputVal = 0;
long lastSensorUpdate = 0;
FilterOnePole lowpassFilter( LOWPASS, 100.0 );  
/********** SENSOR *************/


WiFiClient espClient;
PubSubClient client(espClient);
Eprom eprom;

long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  eprom.begin();
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(ssid, password);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
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
  for (int i = 0; i < length; i++) {
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
    }
  }

  return true;
}

boolean reconnect() {
  if (client.connect("arduinoClient")) {
    client.subscribe("inTopic");
  }
  return client.connected();
}
long loopsPerSek = 0;
void loop() {

  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5 * 1000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
        Serial.println("Connected!");
      } else {
        Serial.println("Connection attempt failed. next try in 5 seconds...");
      }
    }
  } else {
    // Client connected   
    if(otaUpdate) {
      setColor(100, 0, 0);
      FastLED.show(); 
      patchFirmware();
      otaUpdate = false;
    }
  }
  loopsPerSek++;
  EVERY_N_MILLISECONDS( 5000 ) { // measure performance: currently we have 37000 loops per sek
    Serial.print("loops per sek: ");Serial.println(loopsPerSek / 5);
    loopsPerSek = 0;
  }
 
  EVERY_N_MILLISECONDS( 10 ) { // 100Hz sampling rate
    inputVal = lowpassFilter.input(analogRead(A0)); 
    
    mappedSensorVal = map(inputVal, SENSOR_MIN, SENSOR_MAX, 0, 100);
    if(inputVal - lastInputVal > 200) { // instant ON signal, is sent imediatelly
      publishSensorVal(mappedSensorVal);
      stateOn = true;
    } else if(inputVal - lastInputVal < -150) { // instant OFF signal
      publishSensorVal(mappedSensorVal);
      stateOn = false;
    } else if(inputVal > SENSOR_MIN){ // continuous values as long as input is above a certain treshold
      stateOn = true;
      //brightness = map(inputVal, SENSOR_MIN, SENSOR_MAX, 0, 100);
      //FastLED.setBrightness(brightness);
      EVERY_N_MILLISECONDS( 500 ) {
        publishSensorVal(mappedSensorVal);
      }
    }  else { // hearbeat -> just send 0 values to the server
      EVERY_N_MILLISECONDS( 3000 ) {
        publishSensorVal(mappedSensorVal);
      }
      stateOn = false;
    }
    lastInputVal = inputVal;
  }
  
  EVERY_N_MILLISECONDS( 20 ) { 
  
    if(stateOn){
      int range = constrain(map(inputVal, SENSOR_MIN, SENSOR_MAX, 1, NUM_LEDS), 1, NUM_LEDS);
      rainbow(range);
      gHue++;
    } else {
      setColor(0, 0, 30);
    }
    FastLED.show(); // display this frame
  }

  client.loop();
}

void publishSensorVal(int sensorVal) {
  snprintf (msg, 75, "{\"k\":\"%s\",\"v\":\"%d\"}", macAddress, mappedSensorVal);
  Serial.println(msg);
  client.publish(MQTT_TOPIC, msg);
}
