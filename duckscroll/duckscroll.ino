#include "GSMinit.h"
#include "MatrixFont.h"
#include <FastLED.h>
#include <QueueArray.h>

#define CHIPSET     WS2811
#define LED_PIN     11
#define COLOR_ORDER RGB
#define BRIGHTNESS  255
QueueArray <char> queue;

int textSpeed = 20;
boolean animating = false;
boolean waitingForAnimation = false;
boolean holding = true;
uint8_t holdingChoice;
const uint8_t kMatrixWidth = 50;
const uint8_t kMatrixHeight = 10;

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
CRGB leds_plus_safety_pixel[ NUM_LEDS + 1];
CRGB* const leds( leds_plus_safety_pixel + 1);
boolean bLeds [NUM_LEDS];

char character = alphaNums[0];
int characterPos = 0;
const bool    kMatrixSerpentineLayout = true;
const char* broker = "m10.cloudmqtt.com";
String message = "DUCKDUCKGOOSESTORE.COM            ";
String temp = "";
int tred, tblue, tgreen = 255; 
int bred, bblue, bgreen = 0;
int stringLen = message.length();
long lastReconnectAttempt = 0;
bool isText = true;

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial2.begin(115200);
  delay(3000);
  Serial1.begin(115200);
  delay(10);
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  bootGSM();
  mqttClient.setServer(broker, 11725);
  mqttClient.connect("duckduckgoose", "enhwevjl", "bE4i40iYYuJg", "duck", 0, true, "");
  mqttClient.subscribe("duck");
  mqttClient.setCallback(retrieveData);
}

void loop() {
  if (mqttClient.connected()) {
    mqttClient.loop();
    scrollText();
  } else {
    unsigned long t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }
}

boolean mqttConnect() {
  Serial.print("Connecting to ");
  Serial.print(broker);
  if (!mqttClient.connect("duckduckgoose", "enhwevjl", "bE4i40iYYuJg", "duck", 0, true, "")) {
    Serial.println("Failed to connect to MQTT. Rebooting GSM...");
    bootGSM();
    return false;
  }
  Serial.println(" OK");
  mqttClient.subscribe("duck");
  return mqttClient.connected();
}

void retrieveData(char* topic, byte* payload, unsigned int len){
  
  message = "";
  temp = "";
  Serial.println("Topic: " + String(topic));
  for(int i = 0; i < len; i++){
      temp += (char(payload[i])); 
  }
  do {
    scrollText();
  } 
  
  while (!matrixCleared());
    if (bred > 0){
      for( int k = bred; k >= 0; k--){
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i].r = k;        
        }
        FastLED.show();
        delay(2);
      }
    }
    
    if (bgreen > 0){
      for( int k = bgreen; k >= 0; k--){
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i].g = k;        
        }
        FastLED.show();
        delay(2);
      }
    }
    
    if (bblue > 0){
      for( int k = bblue; k >= 0; k--){
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i].b = k;        
        } 
        FastLED.show();
        delay(2);
      }
    }
  message = temp.substring(0, len-20) + "               ";
  tred = temp.substring(len-20, len-17).toInt();
  tgreen = temp.substring(len-17, len-14).toInt();
  tblue = temp.substring(len-14, len-11).toInt();
  bred = temp.substring(len-11, len-8).toInt();
  bgreen = temp.substring(len-8, len-5).toInt();
  bblue = temp.substring(len-5, len-2).toInt();
  textSpeed = temp.substring(len-2, len).toInt();
  
  Serial.println("Message: " + message);
  if (bred > 0){
    for( int k = 0; k <= bred; k++){
      for (int i = 0; i < NUM_LEDS; i++) {
          leds[i].r = k;        
      }
      FastLED.show();
      delay(2);
    }
  }
  if (bgreen > 0){
    for( int k = 0; k <= bgreen; k++){
      for (int i = 0; i < NUM_LEDS; i++) {
          leds[i].g = k;        
      }
      FastLED.show();
      delay(2);
    }
  }
  if (bblue > 0){
    for( int k = 0; k <= bblue; k++){
      for (int i = 0; i < NUM_LEDS; i++) {
          leds[i].b = k;        
      }
      FastLED.show();
      delay(2);
    }
  }
}

uint16_t XY( uint8_t x, uint8_t y)
{
  uint16_t i;
  if ( y & 0x01) {
    // Odd rows run backwards
    uint8_t reverseX = (kMatrixWidth - 1) - x;
    i = (y * kMatrixWidth) + reverseX;
  } else {
    // Even rows run forwards
    i = (y * kMatrixWidth) + x;
  }
  return i;
}

uint16_t XYsafe( uint8_t x, uint8_t y)
{
  if ( x >= kMatrixWidth) return -1;
  if ( y >= kMatrixHeight) return -1;
  return XY(x, y);
}


void populateMessage(uint8_t choice) {
  stringLen = message.length();
  for (int i = 0; i < stringLen; i++) {
    queue.push(message[i]);
  }
}

boolean matrixCleared() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (!(leds[i].r == bred && leds[i].g == bgreen && leds[i].b == bblue)) {
      return false;
    }
  }
  return true;
}

void clearMatrix() {
  for (int i = 0; i < NUM_LEDS; i++) {
    bLeds[i] = 0;
  }
}

void scrollText() {
    long timer = millis();
    
    for (int  x = 0; x < kMatrixWidth - 1 ; x++) {
      for (int y = 0; y < kMatrixHeight; y++) {
        bLeds[ XY( x, y) ] = bLeds[XY(x + 1, y)];
      }
    }
    
    if (waitingForAnimation) {
      if (!matrixCleared()) {
        Serial.println("boooo");
        goto end_of_loop;
      } else {
        waitingForAnimation = false;
        character = ' ';
      }
    }
    
    for (int i = 0; i < 10; i++) {
      int posTemp  = 0;
      for (int q = 0; q < FONTSIZE; q++) {
        if (character == alphaNums[q] ) {
            isText = true;
            posTemp = q;
            if (alphaNumsFont[posTemp][i][characterPos] == 1) {
                bLeds[XY(kMatrixWidth - 1, i)] = 1;
            }  
      
            else {
                bLeds[XY(kMatrixWidth - 1, i)] = 0;
            }
        }

        else if(character == imageNums[q] ) {
            isText = false;
            posTemp = q;
            if (imageNumsFont[posTemp][i][characterPos] == 1) {
                bLeds[XY(kMatrixWidth - 1, i)] = 1;
            }  
      
            else {
                bLeds[XY(kMatrixWidth - 1, i)] = 0;
            }
        }
      }
    }
    
    characterPos++;
    Serial.println(character);
    if(isText){
      if (characterPos == 9) {
      characterPos = 0;
      if (queue.isEmpty()) {
        mqttClient.publish("done", "done");
        holding = true;
        populateMessage(holdingChoice);
      }
      else {
        character = queue.pop();
        if (character == '\1') {
          character = queue.pop();
          Serial.println("animation");
          waitingForAnimation = true;
          Serial.println("hello");
        }
      }
      }
    }else{
    

    if (characterPos == 49) {
      characterPos = 0;
      if (queue.isEmpty()) {
        mqttClient.publish("done", "done");
        holding = true;
        populateMessage(holdingChoice);
      }
      else {
        character = queue.pop();
        if (character == '\1') {
          character = queue.pop();
          Serial.println("animation");
          waitingForAnimation = true;
          Serial.println("hello");
        }
      }
     }
    }
    
    end_of_loop:
    
    for (int i = 0; i < NUM_LEDS; i++) {
      if (bLeds[i]) {
          leds[i].g = tgreen;
          leds[i].b = tblue;
          leds[i].r = tred;
      } else {
          leds[i].g = bgreen;
          leds[i].b = bblue;
          leds[i].r = bred;
      }
    }
    
    while (millis() - timer < textSpeed);
    FastLED.show();
}
