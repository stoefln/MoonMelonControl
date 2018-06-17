# 1 "/var/folders/5h/gsjh01jn2079222pq9p4yhsr0000gn/T/tmpxzXhM5"
#include <Arduino.h>
# 1 "/Users/steph/Documents/PlatformIO/Projects/Moon Melon/src/MoonMelon.ino"
# 20 "/Users/steph/Documents/PlatformIO/Projects/Moon Melon/src/MoonMelon.ino"
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#include <ESP8266WiFi.h>
#include "FastLED.h"
#include <Filters.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "Eprom.h"


#include <Filters.h>

#include "Utils.h"

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif


const char* ssid = "Chaos-Platz";
const char* password = "FalafelPower69";
const char* MQTT_SERVER_IP = "192.168.0.73";



char macAddress[20];
long lastReconnectAttempt = 0;
bool offlineMode = false;


const char* MQTT_TOPIC = "sensor";
const char* MQTT_TOPIC_STATUS = "status";


bool otaUpdate = false;


const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);

#define DATA_PIN 4
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];
#define BRIGHTNESS 96
#define FRAMES_PER_SECOND 120


bool gReverseDirection = false;


uint8_t gHue = 0;


bool stateOn = false;
int brightness = 0;



int SENSOR_MIN = 250;
int SENSOR_MAX = 900;
int inputVal = 0;
int mappedSensorVal = 0;
float distance = 0;
int lastInputVal = 0;
long lastSensorUpdate = 0;
FilterOnePole lowpassFilter( LOWPASS, 100.0 );



WiFiClient wifiClient;
PubSubClient client(wifiClient);
Eprom eprom;

long lastMsg = 0;
char msg[50];
int value = 0;
void setup();
void setup_wifi();
void callback(char* topic, byte* payload, unsigned int length);
bool processJson(char* message);
boolean reconnect();
void loop();
void publishSensorVal(int sensorVal);
void Fire2012();
void rainbow(int range);
int getIndex(int i);
void setColor(int inR, int inG, int inB);
#line 103 "/Users/steph/Documents/PlatformIO/Projects/Moon Melon/src/MoonMelon.ino"
void setup() {
  eprom.begin();
  Serial.begin(9600);
  setup_wifi();
  client.setServer(MQTT_SERVER_IP, 1883);
  client.setCallback(callback);

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}

void setup_wifi() {

  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(ssid, password);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  int i= 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    i++;
    if(i > 20) {
      Serial.println("Connection could not be established, continue with no-internet mode");
      offlineMode = true;
      break;
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
    if (now - lastReconnectAttempt > 5 * 1000 && !offlineMode) {
      lastReconnectAttempt = now;

      if (reconnect()) {
        lastReconnectAttempt = 0;
        Serial.println("Connected!");
      } else {
        Serial.println("Connection attempt failed. next try in 5 seconds...");
      }
    }
  } else {

    if(otaUpdate) {
      setColor(100, 0, 0);
      FastLED.show();
      patchFirmware();
      otaUpdate = false;
    }
  }
  client.loop();
  loopsPerSek++;
  EVERY_N_MILLISECONDS( 5000 ) {
    Serial.print("loops per sek: ");Serial.println(loopsPerSek / 5);
    loopsPerSek = 0;
  }

  EVERY_N_MILLISECONDS( 10 ) {
    inputVal = lowpassFilter.input(analogRead(A0));

    mappedSensorVal = map(inputVal, SENSOR_MIN, SENSOR_MAX, 0, 100);
    if(inputVal - lastInputVal > 200) {
      publishSensorVal(mappedSensorVal);
      stateOn = true;
    } else if(inputVal - lastInputVal < -150) {
      publishSensorVal(mappedSensorVal);
      stateOn = false;
    } else if(inputVal > SENSOR_MIN){
      stateOn = true;


      EVERY_N_MILLISECONDS( 500 ) {
        publishSensorVal(mappedSensorVal);
      }
    } else {
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
    FastLED.show();
  }


}

void publishSensorVal(int sensorVal) {
  snprintf (msg, 75, "{\"k\":\"%s\",\"v\":\"%d\"}", macAddress, mappedSensorVal);
  Serial.println(msg);
  if(!offlineMode){
    client.publish(MQTT_TOPIC, msg);
    client.loop();
  }
}
# 1 "/Users/steph/Documents/PlatformIO/Projects/Moon Melon/src/LedAnimations.ino"

#define COOLING 55




#define SPARKING 200

void Fire2012()
{
  int sparking = (int) ((float) SPARKING * (float) brightness / 100.0);

  static byte heat[NUM_LEDS];


    for( int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8( heat[i], random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
    }


    for( int k= NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }


    if( random8() < sparking ) {
      int y = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }


    for( int j = 0; j < NUM_LEDS; j++) {
      CRGB color = HeatColor( heat[j]);
      int pixelnumber;
      if( gReverseDirection ) {
        pixelnumber = (NUM_LEDS-1) - j;
      } else {
        pixelnumber = j;
      }
      leds[pixelnumber] = color;
    }
}



void rainbow(int range)
{


  CHSV hsv;
  hsv.hue = range * 3;
  hsv.val = 255;
  hsv.sat = 240;
  for( int i = 0; i < NUM_LEDS; i++) {
      if(i < range){
        leds[getIndex(i)] = hsv;
        hsv.hue += 1;
      } else {
        leds[getIndex(i)] = CRGB::Black;
      }
  }
}

int getIndex(int i)
{

  return i;
}
# 1 "/Users/steph/Documents/PlatformIO/Projects/Moon Melon/src/LedControl.ino"
void setColor(int inR, int inG, int inB) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].red = inR;
    leds[i].green = inG;
    leds[i].blue = inB;
  }
}