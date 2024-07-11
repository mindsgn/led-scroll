#include <FastLED.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <queue.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "MatrixFont.h"

#define CHIPSET     WS2811
#define LED_PIN     5
#define COLOR_ORDER GRB
#define BRIGHTNESS  255

QueueArray <char> queue;
QueueArray <char> queue1;
QueueArray <char> queue2;

const uint8_t kMatrixWidth = 50;
const uint8_t kMatrixHeight = 10;
uint8_t holdingChoice;

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
CRGB leds_plus_safety_pixel[ NUM_LEDS + 1];
CRGB* const leds( leds_plus_safety_pixel + 1);
boolean bLeds [NUM_LEDS];

const char* host = "banner";
const char* ssid = "ssid";
const char* password = "password";
const char* mqttServer = "mqtt_server";

char character = alphabetAndNumbers[0];

const bool kMatrixSerpentineLayout = true;

WiFiClient espClient;
PubSubClient client(espClient);

String message = "DUCKDUCKGOOSESTORE.COM                         ";
String temp = "";
int tred, tblue, tgreen = 255; 
int bred, bblue, bgreen = 0;

int stringLen = message.length();
int characterPos = 0;
int textSpeed = 20;
boolean animating = false;
boolean waitingForAnimation = false;
boolean holding = true;

boolean connectedToWIFI = false;
boolean connectedToMQTT = false;
bool isText = true;

//web server
WebServer server(80);

const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";

const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

void startServer(){
  if (!MDNS.begin(host)) { 
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("mDNS responder started");
  
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });

  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
}

// banner scroller
uint16_t XY( uint8_t x, uint8_t y) {
  uint16_t i;
  if ( y & 0x01) {
    uint8_t reverseX = (kMatrixWidth - 1) - x;
    i = (y * kMatrixWidth) + reverseX;
  } else {
    i = (y * kMatrixWidth) + x;
  }
  return i;
}

uint16_t XYsafe( uint8_t x, uint8_t y) {
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
    
    for (int i = 0; i < 10; i++) {
      int posTemp  = 0;
      for (int q = 0; q < FONTSIZE; q++) {
        if (character == alphabetAndNumbers[q] ) {
            isText = true;
            posTemp = q;
            if (alphabetAndNumbersMatrix[posTemp][i][characterPos] == 1) {
                bLeds[XY(kMatrixWidth - 1, i)] = 1;
            }  
      
            else {
                bLeds[XY(kMatrixWidth - 1, i)] = 0;
            }
        }

        else if(character == largeImages[q] ) {
            isText = false;
            posTemp = q;
            if (largeImagesMatrix[posTemp][i][characterPos] == 1) {
                bLeds[XY(kMatrixWidth - 1, i)] = 1;
            }  
      
            else {
                bLeds[XY(kMatrixWidth - 1, i)] = 0;
            }
        }
      }
    }
    
    characterPos++;
    // Serial.println(character);
    if(isText){
      if (characterPos == 9) {
        characterPos = 0;
        if (queue.isEmpty()) {
          holding = true;
          populateMessage(holdingChoice);
        } else {
          character = queue.pop();
          if (character == '\1') {
            character = queue.pop();
            // Serial.println("animation");
            waitingForAnimation = true;
            // Serial.println("hello");
          }
        }
      }
    }else{
      if (characterPos == 49) {
        characterPos = 0;
        if (queue.isEmpty()) {
          holding = true;
          populateMessage(holdingChoice);
        } else {
          character = queue.pop();
          if (character == '\1') {
            character = queue.pop();
            // Serial.println("animation");
            waitingForAnimation = true;
            // Serial.println("hello");
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

void connectedToAPHandler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info) {
  // Serial.println("Connected To The WiFi Network");
  connectedToWIFI = true;
}

void gotIPHandler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info) {
  // Serial.print("Local ESP32 IP: ");
  // Serial.println(WiFi.localIP());
}

void wifiDisconnectedHandler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info) {
  // Serial.println("Disconnected From WiFi Network");
  connectedToWIFI = false;
  WiFi.begin(ssid, password);  
}

void connectWIFI() {
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(connectedToAPHandler, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(gotIPHandler, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(wifiDisconnectedHandler, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("IP address: ");
  Serial.print(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  //Function used to print out data retrieved from the MQTT broker
  message = "";
  temp = "";

  // Serial.println("Topic: " + String(topic)); 
  for(int i = 0; i < length; i++){
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

  message = temp.substring(0, length-20) + "               ";
  tred = temp.substring(length-20, length-17).toInt();
  tgreen = temp.substring(length-17, length-14).toInt();
  tblue = temp.substring(length-14, length-11).toInt();
  bred = temp.substring(length-11, length-8).toInt();
  bgreen = temp.substring(length-8, length-5).toInt();
  bblue = temp.substring(length-5, length-2).toInt();
  textSpeed = temp.substring(length-2, length).toInt();
  
  // Serial.println("Message: " + message);

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

void connectMQTT(){
  client.setServer(mqttServer, 1883);
  client.connect("store-banner", "user", "password", "banner", 0, true, "");
  client.subscribe("banner");
  client.setCallback(callback);
}

void reconnectMQTT() {
  while (!client.connected()) {
    // Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")) {
      // Serial.println("connected");
      client.subscribe("esp32/output");
    } else {
      // Serial.print("failed, rc=");
      // Serial.print(client.state());
      // Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  Serial.begin(115200);
  connectWIFI();
  delay(1000);
  startServer();
  delay(1000);
  connectMQTT();
  delay(1000);
}

void loop() {
  server.handleClient();
  if (client.connected()) {
    client.loop();
  }else{
    connectWIFI();
    connectMQTT();
  }

  scrollText();
}
