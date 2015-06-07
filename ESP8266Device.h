//
//  ESP8266Device.h
//  
//
//  Created by Alexander Maslakov on 07.06.15.
//
//

#ifndef _ESP8266Device_h
#define _ESP8266Device_h

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <MQTT.h>
#include <PubSubClient.h>

#define EVENTS_LEN 10
#define NETWORKS_LEN 50

class ESP8266Device {
public:
    static const int MODE_NONE = -1;
    static const int MODE_UNCONFIGURED = 0;
    static const int MODE_CONFIGURED = 1;
    String name;
    
    // constructor
    ESP8266Device(String n, IPAddress mqttServerIp): webServer(80), mqttClient(mqttServerIp) {
        name = n;
        mode = -1;
        eventsSize = 0;
    }
    
    // begin
    void begin(void) {
        EEPROM.begin(4096);
        eepromRead(0, config);
        Serial.println("ssid: " + String(config.ssid));
        Serial.println("pwd: " + String(config.pwd));
        Serial.println("email: " + String(config.email));
        if (config.configured == true) {
            beginConfigured();
        } else {
            beginUnconfigured();
        }
        Serial.println("Device name: " + name);
    }
    
    // loop
    void loop(void) {
        switch (mode) {
            case MODE_UNCONFIGURED:
                webServer.handleClient();
                break;
            case MODE_CONFIGURED:
                mqttClient.loop();
                break;
        }
    }
    
    // send
    void send(String topic, String payload) {
        //@TODO
    }
    
    // on
    void on(String eventName, std::function<void()> callback) {
        if (eventsSize < EVENTS_LEN) {
            events[eventsSize].name = eventName;
            events[eventsSize].callback = callback;
            eventsSize++;
        }
    }
    
    // get mode
    int getMode(void) {
        return mode;
    }
    
private:
    struct Event {
        String name;
        std::function<void()> callback;
    };
    struct Config {
        char ssid[50];
        char pwd[50];
        char email[50];
        char token[50];
        int syncInterval;
        boolean configured;
    };
    Config config;
    const String PAGE_TPL = "<!doctype html><html><head><meta charset=\"UTF-8\"/><meta name=\"viewport\" content=\"initial-scale=1,maximum-scale=1\"/><title>{TITLE}</title><style>html,body{margin:0;padding:0;}h1{margin:0 0 1em 0;}input{font-family:sans-serif;font-size:1em;padding:.5em;background-color:#fff;border-width:1px;border-style:solid;border-radius:.2em;}body{background-color:#fafafa;padding:1em;font-family:sans-serif;}form label{display:block;margin:1.5em 0;}form label strong{display:block;margin-bottom:.25em;}form input[type=\"text\"],form input[type=\"email\"]{width:96%;}input[type=\"text\"],input[type=\"email\"]{border-color:#ddd;}input[type=\"submit\"]{border-color:blue;color:blue;}section{margin:1.5em auto;background-color:#fff;border:1px solid #ddd;border-radius:.25em;padding:1.5em;}.error{color:red;}</style></head><body>{CONTENT}</body></html>";
    Event events[EVENTS_LEN];
    int eventsSize;
    int mode;
    ESP8266WebServer webServer;
    PubSubClient mqttClient;
    
    // begin unconfigured
    void beginUnconfigured(void) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(name.c_str(), name.c_str());
        Serial.print("IP: ");
        Serial.println(WiFi.softAPIP());
        webServer.on("/", std::bind(&ESP8266Device::handleWebRoot, this));
        webServer.begin();
        mode = MODE_UNCONFIGURED;
    }
    
    // begin configured
    void beginConfigured(void) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.ssid, config.pwd);
        int waitCounter = 0;
        while (WiFi.status() != WL_CONNECTED) {
            waitCounter++;
            delay(500);
            Serial.print(".");
            if (waitCounter > 10) {
                Serial.print("Have wrong configuration.");
                config.configured = false;
                eepromWrite(0, config);
                delay(1000);
                ESP.reset();
                return;
            }
        }
        Serial.println("");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        mqttClient.set_callback([](const MQTT::Publish& pub){
            Serial.println("Data received.");
            //@TODO
        });
        
        if (mqttClient.connect("arduinoClient")) {
            mqttClient.publish("test/mqttclient/publish1", "hello world");
            mqttClient.subscribe("test/mqttclient/topic1");
            //@TODO
        }
    }
    
    // handle web root
    void handleWebRoot(void) {
        String networks[NETWORKS_LEN];
        boolean success = false;
        String error = "";
        String html = PAGE_TPL;
        String ssid = "";
        String pwd = "";
        String email = "";
        if (webServer.method() == HTTP_POST) {
            ssid = webServer.arg("ap_ssid");
            pwd = webServer.arg("ap_pwd");
            email = webServer.arg("email");
            if (ssid.length() == 0 || email.length() == 0) {
                error = "Wi-Fi Network and E-mail are required.";
            } else {
                Serial.println("SSID: " + ssid);
                Serial.println("PWD: " + pwd);
                Serial.println("E-mail: " + email);
                ssid.toCharArray(config.ssid, 50);
                pwd.toCharArray(config.pwd, 50);
                email.toCharArray(config.email, 50);
                config.configured = true;
                eepromWrite(0, config);
                success = true;
            }
        }
        if (! success) {
            scanNetworks(networks);
            String networksOptionsHtml;
            for (int i=0; i<NETWORKS_LEN; ++i) {
                if (networks[i] == "") {
                    continue;
                }
                networksOptionsHtml.concat("<option>" + networks[i] + "</option>");
            }
            html.replace("{CONTENT}", "<section style=\"max-width:30em;\"><form method=\"post\"><h1>Initial Settings</h1>{ERROR}<label><strong>Wi-Fi Network</strong><select name=\"ap_ssid\">{SSID_OPTIONS}</select></label><label><strong>Wi-Fi Password</strong><input type=\"text\" name=\"ap_pwd\" value=\"{PWD}\"/></label><label><strong>E-mail address</strong><input type=\"email\" name=\"email\" value=\"{EMAIL}\"/></label><label><input type=\"submit\" value=\"Continue\"/></label></form></section>");
            html.replace("{ERROR}", error != "" ? "<p class=\"error\">" + error + "</p>": "");
            html.replace("{SSID_OPTIONS}", networksOptionsHtml);
            html.replace("{PWD}", pwd);
            html.replace("{EMAIL}", email);
        } else {
            html.replace("{CONTENT}", "<section style=\"max-width:30em;\"><h1>Congrats!</h1><p>System is configured. Now it reboot and check connection.</p><p>If system is connected right - you will get e-mail to <strong>" + email + "</strong>.</p><p>Good luck!</p></section>");
        }
        webServer.send(200, "text/html", html);
        if (success) {
            delay(500);
            ESP.reset();
        }
    }
    
    // scan networks
    void scanNetworks(String *networks) {
        Serial.println("Scan networks...");
        int n = WiFi.scanNetworks();
        Serial.println("Scan: finished.");
        if (n == 0) {
            Serial.println("No networks found");
        } else {
            Serial.print(n);
            Serial.println(" networks found");
            for (int i = 0; i < NETWORKS_LEN; ++i) {
                if (i+1 > n) {
                    networks[i] = "";
                    continue;
                }
                networks[i] = WiFi.SSID(i);
            }
        }
        WiFi.disconnect();
        delay(100);
    }
    
    // eeprom write
    template <class T> int eepromWrite(int ee, const T& value) {
        const byte* p = (const byte*)(const void*)&value;
        unsigned int i;
        for (i = 0; i < sizeof(value); i++) {
            EEPROM.write(ee++, *p++);
        }
        EEPROM.commit();
        return i;
    }
    
    // eeprom read
    template <class T> int eepromRead(int ee, T& value) {
        byte* p = (byte*)(void*)&value;
        unsigned int i;
        for (i = 0; i < sizeof(value); i++) {
            *p++ = EEPROM.read(ee++);
        }
        return i;
    }
};

#endif
