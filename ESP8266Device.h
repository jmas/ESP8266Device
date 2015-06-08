//
//  ESP8266Device.h
//  
//
//  Created by Alexander Maslakov on 07.06.15.
//  License: MIT
//
//

#ifndef _ESP8266Device_h
#define _ESP8266Device_h

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiServer.h>
#include <ESP8266WebServer.h>
#include <MQTT.h>
#include <PubSubClient.h>

#define EVENTS_LEN 10
#define NETWORKS_LEN 50
#define RETRIES_LEN 10

class ESP8266Device {
public:
    static const int STATE_NONE = -1;
    static const int STATE_UNCONFIGURED = 0;
    static const int STATE_CONFIGURED = 1;
    static const int STATE_DISCONNECTED = 2;
    String name;
    int state;
    
    // constructor
    ESP8266Device(String n, IPAddress mqttServerIp, int mqttPort): webServer(80), mqttClient(mqttServerIp, mqttPort) {
        name = n;
        eventsSize = 0;
    }
    
    // begin
    void begin(void) {
        state = STATE_NONE;
        EEPROM.begin(4096);
        eepromRead(0, config);
#ifdef DEBUG
        Serial.println("config.ssid: " + String(config.ssid));
        Serial.println("config.pwd: " + String(config.pwd));
        Serial.println("config.email: " + String(config.email));
#endif
        if (config.configured == true) {
            beginConfigured();
            state = STATE_CONFIGURED;
        } else {
            beginUnconfigured();
            state = STATE_UNCONFIGURED;
        }
#ifdef DEBUG
        Serial.println("Device name: " + name);
#endif
    }
    
    // loop
    void loop(void) {
        switch (state) {
            case STATE_UNCONFIGURED:
                webServer.handleClient();
                break;
            case STATE_CONFIGURED:
                // if not connected to Wi-Fi or lost connection
                if (WiFi.status() != WL_CONNECTED) {
                    connectWiFi();
                    break;
                }
                // connect to mqtt
                if (! mqttClient.connected()) {
                    if (mqttClient.connect(mqttId)) {
                        // register events
                        for (int i=0; i<eventsSize; i++) {
                            mqttClient.subscribe(events[i].name);
                        }
#ifdef DEBUG
                        Serial.println("MQTT connected.");
#endif
                    } else {
#ifdef DEBUG
                        Serial.println("MQTT connect fail.");
                        delay(500);
#endif
                    }
                } else {
                    mqttClient.loop();
                }
                break;
        }
    }
    
    // send
    void send(String topic, String payload) {
        if (! mqttClient.connected()) {
            return;
        }
        mqttClient.publish(topic, payload);
    }
    
    // on
    void on(String eventName, std::function<void(String)> callback) {
        if (eventsSize < EVENTS_LEN) {
            events[eventsSize].name = eventName;
            events[eventsSize].callback = callback;
            eventsSize++;
        }
    }
    
private:
    struct Event {
        String name;
        std::function<void(String)> callback;
    };
    struct Config {
        char ssid[50];
        char pwd[50];
        char email[50];
        boolean configured;
    };
    Config config;
    const String PAGE_TPL = "<!doctype html><html><head><meta charset=\"UTF-8\"/><meta name=\"viewport\" content=\"initial-scale=1,maximum-scale=1\"/><title>{TITLE}</title><style>html,body{margin:0;padding:0;}h1{margin:0 0 1em 0;}input{font-family:sans-serif;font-size:1em;padding:.5em;background-color:#fff;border-width:1px;border-style:solid;border-radius:.2em;}body{background-color:#fafafa;padding:1em;font-family:sans-serif;}form label{display:block;margin:1.5em 0;}form label strong{display:block;margin-bottom:.25em;}form input[type=\"text\"],form input[type=\"email\"]{width:96%;}input[type=\"text\"],input[type=\"email\"]{border-color:#ddd;}input[type=\"submit\"]{border-color:blue;color:blue;}section{margin:1.5em auto;background-color:#fff;border:1px solid #ddd;border-radius:.25em;padding:1.5em;}.error{color:red;}</style></head><body>{CONTENT}</body></html>";
    Event events[EVENTS_LEN];
    int eventsSize;
    ESP8266WebServer webServer;
    PubSubClient mqttClient;
    String mqttId;
    
    // begin unconfigured
    void beginUnconfigured(void) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(name.c_str(), name.c_str());
#ifdef DEBUG
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
#endif
        webServer.on("/", std::bind(&ESP8266Device::handleWebRoot, this));
        webServer.begin();
    }
    
    // begin configured
    void beginConfigured(void) {
        // mqtt setup
        mqttClient.set_callback([this](const MQTT::Publish& pub){
            for (int i=0; i<eventsSize; ++i) {
                if (pub.topic() == events[i].name) {
                    events[i].callback(pub.payload_string());
                }
            }
#ifdef DEBUG
            Serial.println("Data received.");
            Serial.println("topic:");
            Serial.println(pub.topic());
            Serial.println("payload:");
            Serial.println(pub.payload_string());
#endif
        });
        // get mac address
        uint8_t mac[6];
        WiFi.macAddress(mac);
        String macString = macToStr(mac);
        // prepare mqtt id
        mqttId = "";
        mqttId.concat(config.email);
        mqttId.concat("--");
        mqttId.concat(macString);
        mqttId.concat("--");
        mqttId.concat(name);
#ifdef DEBUG
        Serial.print("mqttId: ");
        Serial.println(mqttId);
#endif
    }
    
    void connectWiFi(void) {
#ifdef DEBUG
        Serial.println("Connect to Wi-Fi.");
#endif
        int retries = 0;
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.ssid, config.pwd);
        while (WiFi.status() != WL_CONNECTED) {
            retries++;
            delay(500);
#ifdef DEBUG
            Serial.print(".");
#endif
            if (retries > RETRIES_LEN) {
#ifdef DEBUG
                Serial.print("Can\'t connect to W-Fi. Restart.");
#endif
//                config.configured = false;
//                eepromWrite(0, config);
                delay(500);
                ESP.reset();
                return;
            }
        }
#ifdef DEBUG
        Serial.println("");
        Serial.println("Wi-Fi connected.");
        Serial.println("");
        Serial.print("STA IP: ");
        Serial.println(WiFi.localIP());
#endif
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
            ssid = urldecode(webServer.arg("ssid"));
            pwd = urldecode(webServer.arg("pwd"));
            email = urldecode(webServer.arg("email"));
            if (ssid.length() == 0 || email.length() == 0) {
                error = "Wi-Fi Network and E-mail address are required.";
            } else if (! validateEmail(email)) {
                error = "E-mail address is not valid.";
            } else {
                ssid.toCharArray(config.ssid, 50);
                pwd.toCharArray(config.pwd, 50);
                email.toCharArray(config.email, 50);
                config.configured = true;
                eepromWrite(0, config);
#ifdef DEBUG
                Serial.println("config.ssid: " + String(config.ssid));
                Serial.println("config.pwd: " + String(config.pwd));
                Serial.println("config.email: " + String(config.email));
                Serial.println("Config saved.");
#endif
                success = true;
            }
        }
        if (! success) {
            scanNetworks(networks);
            String networksOptionsHtml;
            String selected;
            for (int i=0; i<NETWORKS_LEN; ++i) {
                if (networks[i] == "") {
                    continue;
                }
                selected = (networks[i] == ssid ? "selected": "");
                networksOptionsHtml.concat("<option " + selected + ">" + networks[i] + "</option>");
            }
            html.replace("{CONTENT}", "<section style=\"max-width:30em;\"><form method=\"post\"><h1>Initial Settings</h1>{ERROR}<label><strong>Wi-Fi Network</strong><select name=\"ssid\">{SSID_OPTIONS}</select></label><label><strong>Wi-Fi Password</strong><input type=\"text\" name=\"pwd\" value=\"{PWD}\"/></label><label><strong>E-mail address</strong><input type=\"email\" name=\"email\" value=\"{EMAIL}\"/></label><label><input type=\"submit\" value=\"Continue\"/></label></form></section>");
            html.replace("{ERROR}", error != "" ? "<p class=\"error\">" + error + "</p>": "");
            html.replace("{SSID_OPTIONS}", networksOptionsHtml);
            html.replace("{PWD}", pwd);
            html.replace("{EMAIL}", email);
        } else {
            html.replace("{CONTENT}", "<section style=\"max-width:30em;\"><h1>Congrats!</h1><p>Device is configured. Now it reboot and try to connect selected Wi-Fi Network.</p><p>If device is connected right - you will get e-mail to <strong>" + email + "</strong>.</p><p>Good luck!</p></section>");
        }
        html.replace("{TITLE}", name);
        webServer.send(200, "text/html", html);
        if (success) {
            delay(500);
            ESP.reset();
        }
    }
    
    // scan networks
    void scanNetworks(String *networks) {
#ifdef DEBUG
        Serial.println("Scan networks...");
#endif
        int n = WiFi.scanNetworks();
#ifdef DEBUG
        Serial.println("Scan finished.");
#endif
        if (n == 0) {
#ifdef DEBUG
            Serial.println("No networks found.");
#endif
        } else {
#ifdef DEBUG
            Serial.print("Found ");
            Serial.print(n);
            Serial.println(" networks.");
#endif
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
    
    // @from: http://m.oschina.net/blog/322576
    inline String urldecode(const String &sIn) {
        String sOut;
        for (size_t ix = 0; ix < sIn.length(); ix++) {
            BYTE ch = 0;
            if (sIn[ix]=='%') {
                ch = (fromHex(sIn[ix+1])<<4);
                ch |= fromHex(sIn[ix+2]);
                ix += 2;
            } else if(sIn[ix] == '+') {
                ch = ' ';
            } else {
                ch = sIn[ix];
            }
            sOut += (char)ch;
        }
        return sOut;
    }
    
    // @related: urlencode
    typedef unsigned char BYTE;
    
    // @related: urlencode
    inline BYTE fromHex(const BYTE &x) {
        return isdigit(x) ? x-'0' : x-'A'+10;
    }
    
    // validate email address
    boolean validateEmail(String str) {
        int index = str.indexOf("@");
        if (index == -1) {
            return false;
        }
        return (str.indexOf(".", index) == -1 ? false: true);
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
    
    // mac to string
    String macToStr(const uint8_t* mac) {
        String result;
        for (int i = 0; i < 6; ++i) {
            result += String(mac[i], 16);
        }
        return result;
    }
};

#endif
