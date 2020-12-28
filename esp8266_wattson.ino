
/*
 * OH2MP ESP8266 Wattson
 *
 * See https://github.com/oh2mp/esp8266_wattson
 *
 */

#include <SoftwareSerial.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

/* ------------------------------------------------------------------------------- */
// Pin vs GPIO mappings
#define PIN_D5  14 
#define PIN_D6  12
#define PIN_D7  13
#define PIN_D8  15

#define RXP PIN_D7   // in case you use NodeMCU. 
#define TXP PIN_D6

#define LED 16
#define LED_ON HIGH  // Reverse these if you use in NodeMCU dev module. 
#define LED_OFF LOW

#define BUTTON PIN_D5     // Push button for starting portal mode.
#define APTIMEOUT 120000  // Portal timeout 2 minutes
char serial[10];          // Wattson unit serial number
char dbuf[256];           // Data buffer for data coming from Wattson.

// placeholder values
char myhostname[24] = "esp8266-wattson";   // Maxlen in ESP8266WiFi class is 24
char ntp_server[64] = "pool.ntp.org";
char topicbase[256] = "wattson";
char mqtt_user[64]  = "foo";
char mqtt_pass[64]  = "bar";
char mqtt_host[64]  = "192.168.36.99";
int  mqtt_port      = 1883;

SoftwareSerial wattsonSerial(RXP, TXP);
ESP8266WiFiMulti WiFiMulti;
WiFiClient wificlient;
PubSubClient mqttclient(wificlient);
File file;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntp_server, 0); // we use UTC, so offset seconds = 0
ESP8266WebServer server(80);
IPAddress apIP(192,168,4,1);   // portal ip address

uint32_t last_ntp = 0;
uint32_t last_pwr = -3000; // funny to set negative to unsigned int32 :)
uint32_t portal_timer = 0;

/* ------------------------------------------------------------------------------- */
void loadWifis() {
    if (SPIFFS.exists("/known_wifis.txt")) {
        char ssid[33];
        char pass[65];
        
        file = SPIFFS.open("/known_wifis.txt", "r");
        while (file.available()) {
            memset(ssid,'\0',sizeof(ssid));
            memset(pass,'\0',sizeof(pass));
            file.readBytesUntil('\t', ssid, 32);
            file.readBytesUntil('\n', pass, 64);
            WiFiMulti.addAP(ssid, pass);
            Serial.printf("wifi loaded: %s / %s\n",ssid,pass);
        }
        file.close();
    } else {
        Serial.println("no wifis configured");
    }
    if (SPIFFS.exists("/myhostname.txt")) {
        file = SPIFFS.open("/myhostname.txt", "r");
        memset(myhostname, 0, sizeof(myhostname));
        file.readBytesUntil('\n', myhostname, sizeof(myhostname));
        file.close();
    }
    Serial.printf("My hostname: %s\n",myhostname);
}
/* ------------------------------------------------------------------------------- */
void loadOthers() {
    if (SPIFFS.exists("/other.txt")) {
        char tmpstr[8];
        memset(tmpstr, 0, sizeof(tmpstr));
        memset(mqtt_host, 0, sizeof(mqtt_host));
        memset(mqtt_user, 0, sizeof(mqtt_user));
        memset(mqtt_pass, 0, sizeof(mqtt_pass));
        memset(topicbase, 0, sizeof(topicbase));
        
        file = SPIFFS.open("/other.txt", "r");
        while (file.available()) {
            file.readBytesUntil(':', mqtt_host, sizeof(mqtt_host));
            file.readBytesUntil('\n', tmpstr, sizeof(tmpstr));
            mqtt_port = atoi(tmpstr);
            if (mqtt_port < 1 || mqtt_port > 65535) mqtt_port = 1883; // default
            file.readBytesUntil(':', mqtt_user, sizeof(mqtt_user));
            file.readBytesUntil('\n', mqtt_pass, sizeof(mqtt_pass));
            file.readBytesUntil('\n', topicbase, sizeof(topicbase));
            file.readBytesUntil('\n', myhostname, sizeof(myhostname));
            file.readBytesUntil('\n', ntp_server, sizeof(ntp_server));
        }
        file.close();
        Serial.printf("MQTT broker: %s:%d\nTopic prefix: %s\n",mqtt_host,mqtt_port,topicbase);
        Serial.printf("My hostname: %s\nNTP-server: %s\n",myhostname,ntp_server);
    }
}
/* ------------------------------------------------------------------------------- */
void setup() {
    memset(serial,0,sizeof(serial));
    memset(dbuf,0,sizeof(dbuf));
  
    pinMode(RXP, INPUT);
    pinMode(TXP, OUTPUT);
    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(LED, OUTPUT);
    digitalWrite(LED,LED_OFF);
    
    Serial.begin(115200);
    Serial.print("\n\nESP8266 Wattson - OH2MP 2020\n\n");

    SPIFFS.begin();
    loadWifis();
    loadOthers();
    
    if (strlen(myhostname) > 0) WiFi.hostname(myhostname);

    mqttclient.setServer(mqtt_host, mqtt_port);
    if (strlen(ntp_server) > 0) timeClient.begin();
    
    wattsonSerial.begin(19200);

    // Get serial number of our Wattson
    wattsonSerial.printf("nows\r");
}
/* ------------------------------------------------------------------------------- */

void loop() {
    char c = 0;

    if (portal_timer > 0) {
        // Blink LED when portal is on
        if (millis() % 1000 < 500) {
            digitalWrite(LED,LED_ON);
        } else {
            digitalWrite(LED,LED_OFF);
        }
        server.handleClient();
        if (millis() - portal_timer > APTIMEOUT) {
            Serial.println("Portal timeout. Booting.");
            delay(1000);
            ESP.restart();
        }
    }
    if (digitalRead(BUTTON) == LOW && portal_timer == 0) {
        startPortal();
    }

    if (portal_timer == 0) {
    // Ask power consumption every 3 seconds
        if (millis() - last_pwr >= 3000) {
            wattsonSerial.printf("nowp\r");
            last_pwr = millis();
        }

        // Read data from Wattson
        while (wattsonSerial.available()) {
            c = wattsonSerial.read();
            if (c > 0x1F) {
                dbuf[strlen(dbuf)] = c;
            } else if (strlen(dbuf) > 0) {
                Serial.printf("raw data: %s = ",dbuf);
                handle_data();
                memset(dbuf,0,sizeof(dbuf));
            }
        }
    
        if (WiFiMulti.run() == WL_CONNECTED) {
            // If NTP is enabled, try to synchronize clock at boot and then every 64 seconds
            if (strlen(ntp_server) > 0) {
                if ((millis() - last_ntp) > 64000 || last_ntp == 0) {
                    /* If time synchronization from NTP is successful, then synchronize it to Wattson too */
                    if (timeClient.update()) {
                        char timecmd[32];
                        time_t justnow = timeClient.getEpochTime();
                        strftime(timecmd, 32, "nowT%y/%m/%d %T\r", gmtime(&justnow)); // I ♥ POSIX
                        wattsonSerial.print(timecmd);
                        while (wattsonSerial.available()) c = wattsonSerial.read();   // empty serial buffer
            
                        Serial.printf("Sending time sync command: %s\n",timecmd);
                        last_ntp = millis();
                    }
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
// Here we handle the data coming from Wattson.

void handle_data() {
    // Power
    if (dbuf[0] == 'p') {
        int pwr = nibble(dbuf[1])*4096 + nibble(dbuf[2])*256 + nibble(dbuf[3])*16 + nibble(dbuf[4]);
        Serial.printf("Power: %d W\n",pwr);
        if (strlen(serial) > 0) {
            if (WiFiMulti.run() == WL_CONNECTED) {
                char json[32];  // This is enough space here
                char topic[128];
                digitalWrite(LED,LED_ON);
                // We send in milliwatts.
                // See: https://github.com/oh2mp/esp32_ble2mqtt/blob/main/DATAFORMATS.md
                //      TAG_WATTSON = 8
                sprintf(json,"{\"type\":8,\"p\":%d}",pwr*1000);
                sprintf(topic,"%s/%s",topicbase,serial);
                if (mqttclient.connect(myhostname,mqtt_user,mqtt_pass)) {
                    mqttclient.publish(topic, json);
                } else {
                    Serial.printf("Failed to connect MQTT broker, state=%d\n",mqttclient.state());
                }
                digitalWrite(LED,LED_OFF);
            } else {
                Serial.printf("Failed to connect WiFi, status=%d\n", WiFi.status());
            }
        } else {
            wattsonSerial.printf("nows\r"); // Ask for serial
        }
        return;
    }
    // Serial number
    if (dbuf[0] == 's') {
        sprintf(serial,"%s",dbuf);
        Serial.printf("Serial: %s\n",serial);
        return;
    }
    if (dbuf[0] == 'T') {
        Serial.printf("time set.\n");
        return;
    }

}

/* ------------------------------------------------------------------------------- */
/*  Convert hex char to value 
 */
 
char nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;  // Not a valid hexadecimal character
}

/* ------------------------------------------------------------------------------- */
/* Portal code begins here
 *  
 *   This portal code is mainly the same as in some of my other projects.
 *   Functions are so self explanatory that they have almost no comment lines.
 *  
 *   Yeah, I know that String objects are pure evil 😈, but this is meant to be
 *   rebooted immediately after saving all parameters, so it is quite likely that 
 *   the heap will not fragmentate yet. 
 */
/* ------------------------------------------------------------------------------- */
void startPortal() {
   
    Serial.print("Starting portal...");
    portal_timer = millis();

    timeClient.end();
    ntpUDP.stop();
    
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("ESP8266 Wattson");
    
    server.on("/", httpRoot);
    server.on("/style.css", httpStyle);   
    server.on("/wifis.html", httpWifi);
    server.on("/savewifi", httpSaveWifi);
    server.on("/other.html", httpOther);
    server.on("/saveother", httpSaveOther);
    server.on("/boot", httpBoot);
    
    server.onNotFound([]() {
        server.sendHeader("Refresh", "1;url=/"); 
        server.send(404, "text/plain", "QSD QSY");
    });
    server.begin();
    Serial.println("Portal running.");
}
/* ------------------------------------------------------------------------------- */

void httpRoot() {
    portal_timer = millis();
    String html;

    file = SPIFFS.open("/index.html", "r");
    html = file.readString();
    file.close();

    server.send(200, "text/html; charset=UTF-8", html);
}

/* ------------------------------------------------------------------------------- */
void httpStyle() {
    portal_timer = millis();
    String css;

    file = SPIFFS.open("/style.css", "r");
    css = file.readString();
    file.close();       
    server.send(200, "text/css", css);
}

/* ------------------------------------------------------------------------------- */
void httpBoot() {
    portal_timer = millis();
    String html;
    
    file = SPIFFS.open("/ok.html", "r");
    html = file.readString();
    file.close();
    
    server.sendHeader("Refresh", "2;url=about:blank");
    server.send(200, "text/html; charset=UTF-8", html);
    delay(1000);
    
    ESP.restart();
}
/* ------------------------------------------------------------------------------- */
void httpWifi() {
    String html;
    char tablerows[1024];
    char rowbuf[256];
    char ssid[33];
    char pass[33];
    int counter = 0;
    
    portal_timer = millis();
    memset(tablerows, '\0', sizeof(tablerows));
    
    file = SPIFFS.open("/wifis.html", "r");
    html = file.readString();
    file.close();
    
    if (SPIFFS.exists("/known_wifis.txt")) {
        file = SPIFFS.open("/known_wifis.txt", "r");
        while (file.available()) {
            memset(rowbuf, '\0', sizeof(rowbuf)); 
            memset(ssid, '\0', sizeof(ssid));
            memset(pass, '\0', sizeof(pass));
            file.readBytesUntil('\t', ssid, 33);
            file.readBytesUntil('\n', pass, 33);
            sprintf(rowbuf,"<tr><td>SSID</td><td><input type=\"text\" name=\"ssid%d\" maxlength=\"32\" value=\"%s\"></td></tr>",counter,ssid);
            strcat(tablerows,rowbuf);
            sprintf(rowbuf,"<tr><td>PASS</td><td><input type=\"text\" name=\"pass%d\" maxlength=\"32\" value=\"%s\"></td></tr>",counter,pass);
            strcat(tablerows,rowbuf);
            counter++;
        }
        file.close();
    }
    html.replace("###TABLEROWS###", tablerows);
    html.replace("###COUNTER###", String(counter));
    
    if (counter > 3) {
        html.replace("table-row", "none");
    }
    
    server.send(200, "text/html; charset=UTF-8", html);
}
/* ------------------------------------------------------------------------------- */
void httpSaveWifi() {
    portal_timer = millis();
    String html;
        
    file = SPIFFS.open("/known_wifis.txt", "w");
    
    for (int i = 0; i < server.arg("counter").toInt(); i++) {
         if (server.arg("ssid"+String(i)).length() > 0) {
             file.print(server.arg("ssid"+String(i)));
             file.print("\t");
             file.print(server.arg("pass"+String(i)));
             file.print("\n");
         }
    }
    // Add new
    if (server.arg("ssid").length() > 0) {
        file.print(server.arg("ssid"));
        file.print("\t");
        file.print(server.arg("pass"));
        file.print("\n");
    }    
    file.close();
  
    file = SPIFFS.open("/ok.html", "r");
    html = file.readString();
    file.close();

    server.sendHeader("Refresh", "2;url=/");
    server.send(200, "text/html; charset=UTF-8", html);
}
/* ------------------------------------------------------------------------------- */
void httpOther() {
    portal_timer = millis();
    String html;
    
    file = SPIFFS.open("/other.html", "r");
    html = file.readString();
    file.close();
    
    html.replace("###HOSTPORT###", String(mqtt_host)+":"+String(mqtt_port));
    html.replace("###USERPASS###", String(mqtt_user)+":"+String(mqtt_pass));
    html.replace("###TOPICBASE###", String(topicbase));
    html.replace("###MYHOSTNAME###", String(myhostname));
    html.replace("###NTPSERVER###", String(ntp_server));
       
    server.send(200, "text/html; charset=UTF-8", html);
}
/* ------------------------------------------------------------------------------- */
void httpSaveOther() {
    portal_timer = millis();
    String html;

    file = SPIFFS.open("/other.txt", "w");
    file.printf("%s\n",server.arg("hostport").c_str());
    file.printf("%s\n",server.arg("userpass").c_str());
    file.printf("%s\n",server.arg("topicbase").c_str());
    file.printf("%s\n",server.arg("myhostname").c_str());
    file.printf("%s\n",server.arg("ntpserver").c_str());
    file.close();
    loadOthers(); // reread

    file = SPIFFS.open("/ok.html", "r");
    html = file.readString();
    file.close();

    server.sendHeader("Refresh", "2;url=/");
    server.send(200, "text/html; charset=UTF-8", html);    
}
/* ------------------------------------------------------------------------------- */
