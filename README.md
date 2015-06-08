# ESP8266Device
Extension for Arduino IDE and ESP8266 for setup device Wi-Fi, register device owner and connect to MTQQ server.

# Dependency
* PubSubClient https://github.com/Imroy/pubsubclient

# Installation
Just download ZIP and unpack to `Documents/Arduino/libraries` directory.

# How to
You create new Arduino project and copy following code to sketch.
```cpp
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiServer.h>
#include <ESP8266WebServer.h>
#include <MQTT.h>
#include <PubSubClient.h>
#include <ESP8266Device.h>

// Here we configure device
// "waterer" - is device name
// IPAddress(192, 168, 0, 101) - is address to your favorite MQTT brocker
// 1883 - is port of MQTT brocker
ESP8266Device dev("waterer", IPAddress(192, 168, 0, 101), 1883);

// Variable for log time (not important)
long int time = 0;

void setup() {
  Serial.begin(115200);
  
  // Disable WATCH DOG
  ESP.wdtDisable();
  
  Serial.println("Hello.");
  
  // Don't forget about next line
  dev.begin();
  
  // Here you can add your own handlers for data from MQTT brocker
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
    Serial.println("Light on");
    Serial.println(payload);
  });
  
  // Here is two examples how you can send data to MQTT brocker
  // dev.send("light/value", "1234");
  // dev.send("ground/value", "5678");
}

void loop() {
  // Next line is important
  dev.loop();
  
  // Send data to MQTT brocker every 10 sec
  if (millis() - 10000 > time) {
    dev.send("water/value", "Hello, world!");
    Serial.println(ESP.getFreeHeap());
    time = millis();
  }
}
```
