#define DEBUG 1

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiServer.h>
#include <ESP8266WebServer.h>
#include <MQTT.h>
#include <PubSubClient.h>
#include <ESP8266Device.h>

ESP8266Device dev("waterer", IPAddress(192, 168, 0, 101), 1883);

long int time = 0;

void setup() {
  Serial.begin(115200);
  
  Serial.println("Hello.");
  
  dev.begin();
  
  dev.on("ground/water", [](String payload){
    // push water
    Serial.println("Ground water");
    Serial.println(payload);
  });
  
  dev.on("light/on", [](String payload){
    // light on
    Serial.println("Light on");
    Serial.println(payload);
  });
  
  dev.on("light/off", [](String payload){
    // light off
  });
  
  // dev.send("light/value", "1234");
  // dev.send("ground/value", "5678");
}

void loop() {
  dev.loop();
  
  if (millis() - 10000 > time) {
    dev.send("water/value", "Hello, world!");
    time = millis();
  }
}
