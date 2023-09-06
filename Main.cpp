#define PIN_LED1 12
#define PIN_LED2 13
#define PIN_LED3 15

#define PIN_LIGHT A0

#define PIN_BUTT1 14
#define PIN_BUTT2 0
#define PIN_BUTT3 2

#define DATA_FASTLED_PIN 16

#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>
#define NUM_LEDS_IN_STRIPLINE 8
CRGB ledsLine[NUM_LEDS_IN_STRIPLINE];

//include wire - Spi bus
#include <Wire.h>
#include <SFE_BMP180.h>
SFE_BMP180 pressureSens;
struct BMP180val{
  double  temperature,
          pressure;
};

//init axelerometer sensor
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
/* Assign a unique ID to this sensor at the same time */
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12346);

//init guest sensor
#include <SparkFun_APDS9960.h>
// Global Variables
SparkFun_APDS9960 apds = SparkFun_APDS9960();

#include "StandData.h"
StandData standData;

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
ESP8266WebServer server(80);

const char* ssid = "TP-LINK_6EF532";
const char* password = "49344059";
const char* mdnsName = "esp8266";

//Index HTML page
const String IndexPage = "<html>\
  <head>\
    <title>ESP8266 Web Server POST handling</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>POST plain text to /postplain/ route</h1>\
    <form method=\"post\" enctype=\"text/plain\" action=\"/postvalue\">\
      <input type=\"text\" name=\"json\" value=\'{JSON string}\'>\
      <br><br>\
      <input type=\"text\" name=\"text\" value=\'\'>\
      <input type=\"submit\" name=\"button\" value=\"Send string\" >\
    </form>\
  </body>\
</html>";

void getMain ()
{
  server.send(200, F("text/html"), IndexPage);
  BlinkPWRled(4, 40);
}

void getSensVal ()
{
  standData.LED1 = digitalRead(PIN_LED1);
  standData.LED2 = digitalRead(PIN_LED2);
  standData.LED3 = digitalRead(PIN_LED3);
  
  standData.button1State = digitalRead(PIN_BUTT1);
  standData.button2State = digitalRead(PIN_BUTT2);
  standData.button3State = digitalRead(PIN_BUTT3);
  
  standData.lightness = analogRead(PIN_LIGHT);

  BMP180val tmp = GetBMP180Val();
  standData.temperature = tmp.temperature;
  Serial.println(standData.temperature);
  standData.pressure = tmp.pressure;
  Serial.println(standData.pressure);

  if(!apds.readAmbientLight(standData.ambient_light) ||
     !apds.readRedLight(standData.red_light) ||
     !apds.readGreenLight(standData.green_light) ||
     !apds.readBlueLight(standData.blue_light) )
  Serial.println("Error reading light values");

  //for axeleration
  sensors_event_t event; 
  accel.getEvent(&event);
  standData.acceleration_x = event.acceleration.x;
  standData.acceleration_y = event.acceleration.y;
  standData.acceleration_z = event.acceleration.z;
  
  for(int i = 0; i < NUM_LEDS_IN_STRIPLINE; i++)
    standData.stripLedsColors[i] = ledsLine[i];
  
  String jsonMassage = standData.GetJSONString();
  server.send(200, F("text/plain"), jsonMassage);
  BlinkPWRled(4, 40);
}

void postSensValue() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
  } else {
    server.send(200, "text/plain", "POST body was:\n" + server.arg("plain"));
    String json = server.arg("plain");
    Serial.print("args : "); Serial.println(json);
    
    StaticJsonDocument<700> jsonDocument;
    deserializeJson(jsonDocument, json);

    standData.LED1 = jsonDocument["LED1"];
    digitalWrite(PIN_LED1, standData.LED1);
    standData.LED2 = jsonDocument["LED2"];
    digitalWrite(PIN_LED2, standData.LED2);
    standData.LED3 = jsonDocument["LED3"];
    digitalWrite(PIN_LED3, standData.LED3);

    for(int i = 0 ; i < NUM_LEDS_IN_STRIPLINE; i++)
    {
      String nameLeds = "leds" + String(i+1);
        JsonObject obj_led = jsonDocument[nameLeds];
        Serial.println("Json object is; ");
        Serial.println(nameLeds);
        standData.stripLedsColors[i].r = obj_led["red"];
        standData.stripLedsColors[i].b = obj_led["blue"];
        standData.stripLedsColors[i].g = obj_led["green"];
        Serial.println(standData.stripLedsColors[i].b);
        Serial.println(standData.stripLedsColors[i].r);
        Serial.println(standData.stripLedsColors[i].g);
    }
    //show a new line colors
    for(int i = 0; i < NUM_LEDS_IN_STRIPLINE; i++)
      ledsLine[i] = standData.stripLedsColors[i];
    FastLED.show();
  }
  BlinkPWRled(4, 40);
}

// Define routing
void restServerRouting() {
    //return HTML page
    server.on("/", HTTP_GET, getMain);
    //return JSON data value representation
    server.on(F("/sensval"), HTTP_GET, getSensVal);
    server.on(F("/hello"), HTTP_GET, hallo);

    //get JSON value for parsing
    //return actual data value (sens and LEDs)
    server.on("/postvalue", HTTP_POST, postSensValue);
}

void hallo()
{
  server.send(200, "text/plain", "Hallo");
  BlinkPWRled(4, 40);
}

// Manage not found URL
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  
  Serial.println("error query from client");
  Serial.println(millis());
  BlinkPWRled(4, 40);
}

void writeLog(){
  static unsigned long t_in = 0;
  static const int decay_sec = 2; //write log each
  
  if(t_in + decay_sec * 1000 < millis())
  { 
    t_in = millis();
    Serial.print("Log time: "); Serial.print(t_in / 1000 /60); Serial.print(":"); Serial.println(t_in / 1000 % 60);
  }
}

void LedAlarmBlink()
{
  while(1)
  {
    digitalWrite(PIN_LED3, !digitalRead(PIN_LED1));
    delay(400);
  }
}

BMP180val GetBMP180Val(){
  BMP180val tmp;
    char status;
    status = pressureSens.startTemperature();
    if (status != 0)
    {
      delay(status);
      status = pressureSens.getTemperature(tmp.temperature);
      if (status != 0)
      {
        status = pressureSens.startPressure(3);
        if (status != 0)
        {
          delay(status);
          status = pressureSens.getPressure(tmp.pressure, tmp.temperature);
          if (status == 0)
            Serial.println("error retrieving pressure measurement\n");
        }
        else Serial.println("error starting pressure measurement\n");
      }
      else Serial.println("error retrieving temperature measurement\n");
    }
    else Serial.println("error starting temperature measurement\n");

  return tmp;
}

void BlinkPWRled(int blinkNum, int decay)
{
  if(blinkNum < 2) blinkNum = 2;
  for(int i = 0; i < blinkNum; i++)
  {
    digitalWrite(PIN_LED3, 1);
    delay(decay);
    digitalWrite(PIN_LED3, 0);
    delay(decay);
  }
}
void setup(void) {
  pinMode(PIN_LED3, OUTPUT);
  BlinkPWRled(5, 50);
  
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Serial Start");
  BlinkPWRled(2, 40);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Start connnect");
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.print(".");
  }
  BlinkPWRled(4, 100);
  Serial.print("Connected to ");  Serial.println(ssid);
  Serial.print("IP address: ");  Serial.println(WiFi.localIP());
 
  // Activate mDNS this is used to be able to connect to the server
  // with local DNS hostmane esp8266.local
  if (MDNS.begin("esp8266")) Serial.println("MDNS responder started");
  MDNS.addService("http", "tcp", 80);
  
  restServerRouting();
  // Set not found response
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  BlinkPWRled(2, 300);
  //////SENS INIT/////
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_LED3, OUTPUT);
  
  pinMode(PIN_BUTT1, INPUT_PULLUP);
  pinMode(PIN_BUTT2, INPUT_PULLUP);
  pinMode(PIN_BUTT3, INPUT_PULLUP);


  //Led strip init
  FastLED.addLeds<WS2812B, DATA_FASTLED_PIN, RGB>(ledsLine, NUM_LEDS_IN_STRIPLINE);
  //global lightness
  FastLED.setBrightness(64);
  CRGB color = CRGB(r, g, b);
  CHSV color = CHSV(h, s, v);
  ledsLine[i] = CRGB::Red;
  
  //pressure init
  if (!pressureSens.begin())
  {
    // Oops, something went wrong, this is usually a connection problem,
    // see the comments at the top of this sketch for the proper connections.
    Serial.println("BMP180 init fail (disconnected?)\n\n");
    LedAlarmBlink();
  }

  //axeleration Sens init
  if(!accel.begin())
  {
    Serial.println("Ooops, no ADXL345 detected ... Check your wiring!");
    LedAlarmBlink();
  }
  accel.setRange(ADXL345_RANGE_16_G);

  //Guest sensor init
  if ( !apds.init() ) 
  {
    Serial.println(F("Something went wrong during APDS-9960 init!"));
    LedAlarmBlink();
  }
  // Start running the APDS-9960 light sensor (no interrupts)
  if ( !apds.enableLightSensor(false) )
  {
    Serial.println(F("Something went wrong during light sensor init!"));
    LedAlarmBlink();
  }
  // Wait for initialization and calibration to finish
  digitalWrite(PIN_LED3, 1);
  Serial.println("Setup continue");
}
 
void loop(void) {
  server.handleClient();
  MDNS.update();
  writeLog();
}
