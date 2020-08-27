/*
Code for Wifi Single triac 2 amps mini board
This code is for ESP8266
Firmware Version: 0.2
Hardware Version: 0.2
USE Board as Node mcu 1.0 while compiling

Code Edited By :Naren N Nayak
Date: 09/06/2018
Last Edited By:Naren N Nayak
Date: 01/03/2018

 
 *  This sketch is running a web server for configuring WiFI if can't connect or for controlling of one GPIO to switch a light/LED
 *  Also it supports to change the state of the light via MQTT message and gives back the state after change.
 *  The push button has to switch to ground. It has following functions: Normal press less than 1 sec but more than 50ms-> Switch light. Restart press: 3 sec -> Restart the module. Reset press: 20 sec -> Clear the settings in EEPROM
 *  While a WiFi config is not set or can't connect:
 *    http://server_ip will give a config page with 
 *  While a WiFi config is set:
 *    http://server_ip/gpio -> Will display the GIPIO state and a switch form for it
 *    http://server_ip/gpio?state=0 -> Will change the GPIO directly and display the above aswell
 *    http://server_ip/cleareeprom -> Will reset the WiFi setting and rest to configure mode as AP
 *  server_ip is the IP address of the ESP8266 module, will be 
 *  printed to Serial when the module is connected. (most likly it will be 192.168.4.1)
 * To force AP config mode, press button 20 Secs!
 *  For several snippets used, the credit goes to:
 *  - https://github.com/esp8266
 *  - https://github.com/chriscook8/esp-arduino-apboot
 *  - https://github.com/knolleary/pubsubclient
 *  - https://github.com/vicatcu/pubsubclient <- Currently this needs to be used instead of the origin
 *  - https://gist.github.com/igrr/7f7e7973366fc01d6393
 *  - http://www.esp8266.com/viewforum.php?f=25
 *  - http://www.esp8266.com/viewtopic.php?f=29&t=2745
 *  - And the whole Arduino and ESP8266 comunity
 */

#define DEBUG
//#define WEBOTA
//debug added for information, change this according your needs

#ifdef DEBUG
  #define Debug(x)    Serial.print(x)
  #define Debugln(x)  Serial.println(x)
  #define Debugf(...) Serial.printf(__VA_ARGS__)
  #define Debugflush  Serial.flush
#else
  #define Debug(x)    {}
  #define Debugln(x)  {}
  #define Debugf(...) {}
  #define Debugflush  {}
#endif

#include <Espalexa.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
//#include <EEPROM.h>
#include <Ticker.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "FS.h"

extern "C" {
  #include "user_interface.h" //Needed for the reset command
}

//callback functions
void firstLightChanged(uint8_t brightness);

//***** Settings declare ********************************************************************************************************* 
String hostName ="Armtronix"; //The MQTT ID -> MAC adress will be added to make it kind of unique
int iotMode=0; //IOT mode: 0 = Web control, 1 = MQTT (No const since it can change during runtime)
//select GPIO's
#define INPIN 0  // input pin (push button)
#define RESTARTDELAY 3 //minimal time in sec for button press to reset
#define HUMANPRESSDELAY 50 // the delay in ms untill the press should be handled as a normal push by human. Button debounce. !!! Needs to be less than RESTARTDELAY & RESETDELAY!!!
#define RESETDELAY 20 //Minimal time in sec for button press to reset all settings and boot to config mode
#define RESET_PIN 16   

//##### Object instances ##### 
MDNSResponder mdns;
ESP8266WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqttClient;
Ticker btn_timer;
Ticker otaTickLoop;
Espalexa espalexa; 

//##### Flags ##### They are needed because the loop needs to continue and cant wait for long tasks!
int rstNeed=0;   // Restart needed to apply new settings
int toPub=0; // determine if state should be published.
int configToClear=0; // determine if config should be cleared.
int otaFlag=0;
boolean inApMode=0;
//##### Global vars ##### 
int webtypeGlob;
int otaCount=300; //imeout in sec for OTA mode
int current; //Current state of the button

unsigned long count = 0; //Button press time counter
String st; //WiFi Stations HTML list
String state; //State of light
char buf[40]; //For MQTT data recieve
char* host; //The DNS hostname
//To be read from Config file
String esid="";
String epass = "";
String pubTopic;
String subTopic;
String mqttServer = "";
const char* otaServerIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
boolean wifiConnected = false;

/*Alexa event names */
String firstName;
char string[32];
char byteRead;
String serialReceived="";
String serialReceived_buf="";

int dimmer_state;
int new_dimmer_state;
int mqtt_dimmer_state;
volatile boolean mqtt_dimpub =false;

//-------------- void's -------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(10);
  // prepare GPIOS
  pinMode(RESET_PIN, OUTPUT); 
  digitalWrite(RESET_PIN, HIGH); 
  pinMode(INPIN, INPUT_PULLUP);
  //digitalWrite(OUTLED, HIGH);
  btn_timer.attach(0.05, btn_handle);
  Debugln("DEBUG: Entering loadConfig()");
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
  }
  
  uint8_t mac[6];
  WiFi.macAddress(mac);
  hostName += "-";
  hostName += macToStr(mac);
  String hostTemp=hostName;
  hostTemp.replace(":","-");
  host = (char*) hostTemp.c_str();
  loadConfig();
  //loadConfigOld();
  Debugln("DEBUG: loadConfig() passed");
  
  // Connect to WiFi network
  Debugln("DEBUG: Entering initWiFi()");
  initWiFi();
  Debugln("DEBUG: initWiFi() passed");
  Debug("iotMode:");
  Debugln(iotMode);
  Debug("webtypeGlob:");
  Debugln(webtypeGlob);
  Debug("otaFlag:");
  Debugln(otaFlag);
  Debugln("CONNECTED");
  //wifiConnected = 1;
  if(wifiConnected){
    server.on("/", HTTP_GET, [](){
    server.send(200, "text/plain", "This is an example index page your server may send.");
    });
    server.on("/test", HTTP_GET, [](){
    server.send(200, "text/plain", "This is a second subpage you may have.");
    });
    server.onNotFound([](){
      if (!espalexa.handleAlexaApiCall(server.uri(),server.arg(0))) //if you don't know the URI, ask espalexa whether it is an Alexa control request
      {
        //whatever you want to do with 404s
        server.send(404, "text/plain", "Not found");
      }
    });
  }
  // Define your devices here.
  espalexa.addDevice((char*)firstName.c_str(), firstLightChanged,0); //simplest definition, default state off

  espalexa.begin(&server); //give espalexa a pointer to your server object so it can use your server instead of creating its own
}

void btn_handle()
{
  if(!digitalRead(INPIN)){
    ++count; // one count is 50ms
  } else {
    if (count > 1 && count < HUMANPRESSDELAY/5) { //push between 50 ms and 1 sec      
      Serial.print("button pressed "); 
      Serial.print(count*0.05); 
      Serial.println(" Sec."); 
      if(iotMode==1 && mqttClient.connected()){
        toPub=1;        
        Debugln("DEBUG: toPub set to 1");
      }
    } else if (count > (RESTARTDELAY/0.05) && count <= (RESETDELAY/0.05)){ //pressed 3 secs (60*0.05s)
      Serial.print("button pressed "); 
      Serial.print(count*0.05); 
      Serial.println(" Sec. Restarting!"); 
      setOtaFlag(!otaFlag);      
      ESP.reset();
    } else if (count > (RESETDELAY/0.05)){ //pressed 20 secs
      Serial.print("button pressed "); 
      Serial.print(count*0.05); 
      Serial.println(" Sec."); 
      Serial.println(" Clear settings and resetting!");       
      configToClear=1;
      }
    count=0; //reset since we are at high
  }
}



//-------------------------------- Main loop ---------------------------
void loop() {

     Serial.println("status:"); 
          if(Serial.available())
             {
               size_t len = Serial.available();
               uint8_t sbuf[len];
               Serial.readBytes(sbuf, len); 
               serialReceived = (char*)sbuf;
               if(serialReceived.substring(0,2) == "D:")
               {
//               mqttClient.publish((char*)pubTopic.c_str(),serialReceived.substring(2,4).c_str());
               serialReceived_buf = serialReceived;
               serialReceived="";
               dimmer_state = serialReceived_buf.substring(2,4).toInt();
                   if(dimmer_state != new_dimmer_state)
                   {
                    new_dimmer_state = dimmer_state;
                    mqtt_dimmer_state = dimmer_state;
                    mqtt_dimpub = true;
                    Serial.println(dimmer_state);
                   }
               serialReceived_buf="";
               }  
             }

              if(mqtt_dimmer_state == 0 && mqtt_dimpub == true)
                       {
                        Serial.println("dimmer_state == 0");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS0");
                        mqtt_dimpub = false;
                       }
                     else if((mqtt_dimmer_state >0 && mqtt_dimmer_state <= 5 )&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 5");
                       mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS5");
                       mqtt_dimpub = false; 
                      }
                      else if((mqtt_dimmer_state >5 && mqtt_dimmer_state <= 10) && mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 10");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS10");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >10 && mqtt_dimmer_state <= 15) && mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 15");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS15");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >15 && mqtt_dimmer_state <= 20) && mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 20");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS20");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >20 && mqtt_dimmer_state <= 25)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 25");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS25");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >25 && mqtt_dimmer_state <= 30)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 30");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS30");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >30 && mqtt_dimmer_state <= 35)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 35");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS35");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >35 && mqtt_dimmer_state <= 40)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 40");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS40");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >40 && mqtt_dimmer_state <= 45)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 45");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS45");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >45 && mqtt_dimmer_state <= 50)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 50");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS50");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >50 && mqtt_dimmer_state <= 55)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 55");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS55");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >55 && mqtt_dimmer_state <= 60)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 60");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS60");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >60 && mqtt_dimmer_state <= 65)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 65");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS65");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >65 && mqtt_dimmer_state <= 70)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 70");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS70");
                        mqtt_dimpub = false;
                      }
                       else if((mqtt_dimmer_state >70 && mqtt_dimmer_state <= 75)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 75");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS75");
                        mqtt_dimpub = false;
                      }
                        else if((mqtt_dimmer_state >75 && mqtt_dimmer_state <= 80)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 80");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS80");
                        mqtt_dimpub = false;
                      }
                        else if((mqtt_dimmer_state >80 && mqtt_dimmer_state <= 85)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 85");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS85");
                        mqtt_dimpub = false;
                      }
                        else if((mqtt_dimmer_state >85 && mqtt_dimmer_state <= 90)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 90");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS90");
                        mqtt_dimpub = false;
                      }
                        else if((mqtt_dimmer_state >90 && mqtt_dimmer_state <= 95)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 95");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS95"); 
                        mqtt_dimpub = false;
                      }
                        else if((mqtt_dimmer_state >95 && mqtt_dimmer_state <= 100)&& mqtt_dimpub == true)
                      {
                        Serial.println("dimmer_state == 100");
                        mqttClient.publish((char*)pubTopic.c_str(),"DimmerIS99");
                        mqtt_dimpub = false;
                        
                      } 
  
  //Debugln("DEBUG: loop() begin");
  if(configToClear==1){
    //Debugln("DEBUG: loop() clear config flag set!");
    clearConfig()? Serial.println("Config cleared!") : Serial.println("Config could not be cleared");
    delay(1000);
    ESP.reset();
  }
  //Debugln("DEBUG: config reset check passed");  
  if (WiFi.status() == WL_CONNECTED && otaFlag){
    if(otaCount<=1) {
      Serial.println("OTA mode time out. Reset!"); 
      setOtaFlag(0);
      ESP.reset();
      delay(100);
    }
    server.handleClient();
    delay(1);
  } else if (WiFi.status() == WL_CONNECTED || webtypeGlob == 1){
    //Debugln("DEBUG: loop() wifi connected & webServer ");
    if (iotMode==0 || webtypeGlob == 1){
      //Debugln("DEBUG: loop() Web mode requesthandling ");
      server.handleClient();
      delay(1);
      if(esid != "" && WiFi.status() != WL_CONNECTED) //wifi reconnect part
      {
        Scan_Wifi_Networks();
      }
    } else if (iotMode==1 && webtypeGlob != 1 && otaFlag !=1){
          //Debugln("DEBUG: loop() MQTT mode requesthandling ");
          if (!connectMQTT()){
              delay(200);          
          }                    
          if (mqttClient.connected()){            
              //Debugln("mqtt handler");
              mqtt_handler();
          } else {
              Debugln("mqtt Not connected!");
          }
    }
  } else{
    Debugln("DEBUG: loop - WiFi not connected");  
    delay(1000);
    initWiFi(); //Try to connect again
  }
  espalexa.loop();
   delay(1);
    //Debugln("DEBUG: loop() end");
}


//our callback functions
void firstLightChanged(uint8_t brightness) {
    Serial.println("Device 1 changed to ");
    
    //do what you need to do here

    //EXAMPLE
    if (brightness == 255) {
      Serial.println("Dimmer:99");
    }
    else if (brightness == 0) {
      Serial.println("Dimmer:0");
    }
    else if (brightness == 1) {
      Serial.println("Dimmer:0");
    }
    else {
      float mul=0.388;//  99/255 for values between 0-99
      float bness=(brightness*mul);
      int ss=bness;
      Serial.print("Dimmer:"); 
      Serial.println(ss);
    }
}

