/*
 * Framework code for
 *  - an ESP8266
 *  - to establish a STA wifi connection
 *  - to create secure client with root certificates
 *  - to request and retrieve response
 *
 *  - Additional: add basic authentification to request
 * 
 *  - time is based on NL location (but uses UTC)
 *  - use automatic versioning (for platformio)
 */ 
#if !defined(ESP8266)
#error This file is for ESP8266 only
#endif

// **************************************** compiler ****************************************
#define APPNAME "WiFi and secure client example for ESP8266"
#include <version.h>          // include BUILD_NUMBER, VERSION, VERSION_SHORT
#define DEBUG_WIFI 1

// **************************************** include ****************************************
#include <Arduino.h>          // Arduino library - https://github.com/esp8266/Arduino/blob/master/cores/esp8266/Arduino.h
#include <time.h>             // Arduino library - https://github.com/esp8266/Arduino/blob/master/tools/sdk/libc/xtensa-lx106-elf/include/time.h
#include <ESP8266WiFi.h>      // Arduino library - https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFi.h
#include <WiFiClientSecure.h> // Arduino library - https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiClientSecure.h

// **************************************** constants ****************************************
// WIFI
#include "secrets.h"          // WiFi Access
#ifndef SECRETS_H
#define SECRETS_H
const char WIFI_SSID[] = "ssid";
const char WIFI_PASSWORD[] = "password";
#endif

// TIME
#define TIME_NTPSERVER_1       "nl.pool.ntp.org"
#define TIME_NTPSERVER_2       "pool.ntp.org"
#define TIME_ENV_TZ            "CET-1CEST,M3.5.0,M10.5.0/3"

// URL request
#ifndef URL_HOST
#define URL_HOST               "www.info.com"
#define URL_PORT               443
#define URL_PATH               "/api/status"
#define URL_BASICAUTH          "userid:password"
#endif
// **************************************** globals ****************************************

// wifi related
enum ConnectionState
{
  WIFI_Disconnected,
  WIFI_Connected,
  WIFI_GotIP,
  CLIENT_Ready
};
ConnectionState _wifiState = WIFI_Disconnected;

BearSSL::WiFiClientSecure client;
BearSSL::CertStore certStore;
BearSSL::Session session; // session cache used to remember secret keys established with clients, to support session resumption.

// time related
time_t now;
struct tm timeinfo;

// **************************************** functions ****************************************

// ********************  SECURE CLIENT  ********************
void fetchURL(BearSSL::WiFiClientSecure *client, const char *host, const uint16_t port, const char *path)
{
  // Connect to host
  Serial.printf("URL: %s:%d%s ... ", host, port, path);
  client->connect(host, port);

  // Check connection
  if (!client->connected())
  {
    Serial.printf("*** Can't connect. ***\n-------\n");
    return;
  }
  else
  {
    Serial.printf("connected!\n-------\n");
  }

  // write request
  client->write("GET ");
  client->write(path);
  client->write(" HTTP/1.0\r\n");

  client->write("Host: ");
  client->write(host);
  client->write("\r\n");

  client->write("User-Agent: ESP8266\r\n");
  client->write("Authorization: Basic ");
  client->write(URL_BASICAUTH);
  client->write("\r\n");
  client->write("Connection: close\r\n");
  client->write("\r\n");

  // retrieve response
  uint32_t to = millis() + 5000;
  if (client->connected())
  {
    // header
    do
    {
      String line = client->readStringUntil('\n');
      Serial.println(line); // print header
      if (line == "\r")
        break;
    } while (millis() < to);
    // body
    do
    {
      String line = client->readStringUntil('\n');
      Serial.println(line);
    } while (millis() < to);
  }
  client->stop();

  Serial.printf("\n-------\n\n");
}

// ********************  WIFI  ********************
// More events: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.h
void onSTAConnected(WiFiEventStationModeConnected e /*String ssid, uint8 bssid[6], uint8 channel*/) {
  Serial.printf("Connected to SSID %s @ bssid %.2X:%.2X:%.2X:%.2X:%.2X:%.2X channel %.2d\n",
    e.ssid.c_str(), e.bssid[0], e.bssid[1], e.bssid[2], e.bssid[3], e.bssid[4], e.bssid[5], e.channel);
  _wifiState = WIFI_Connected;
 }

void onSTADisconnected(WiFiEventStationModeDisconnected e /*String ssid, uint8 bssid[6], WiFiDisconnectReason reason*/) {
  Serial.printf("Disconnected from SSID %s @ bssid %.2X:%.2X:%.2X:%.2X:%.2X:%.2X reason %d\n",
    e.ssid.c_str(), e.bssid[0], e.bssid[1], e.bssid[2], e.bssid[3], e.bssid[4], e.bssid[5], e.reason);
  // Reason: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiType.h
  _wifiState = WIFI_Disconnected;

  // respond to disconnection 
  ESP.restart();
}

void onSTAGotIP(WiFiEventStationModeGotIP e /*IPAddress ip, IPAddress mask, IPAddress gw*/) {
  Serial.printf("Got IP: %s mask %s gateway %s\n",
    e.ip.toString().c_str(), e.mask.toString().c_str(), e.gw.toString().c_str());
  _wifiState = WIFI_GotIP;
}

void WiFibegin() {
  // WiFi start
  WiFi.disconnect(/* wifioff */ true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ********************  TIME  ********************

void InitialiseTime()
{
  Serial.print("Initialise Time: ");
  // void configTime(int timezone, int daylightOffset_sec, const char* server1, const char* server2, const char* server3)
  configTime(0, 0, TIME_NTPSERVER_1, TIME_NTPSERVER_2);
  setenv("TZ", TIME_ENV_TZ, /*overwrite*/ 1);
  tzset();
  
  // wait for NTP returns value and it is not the first day op epoc.
  now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print("."); 
    delay(100);
    now = time(nullptr);
  }
  // show datetime in Serial
  localtime_r(&now, &timeinfo);
  Serial.printf(" localtime: %s", asctime(&timeinfo));
}

void LoadCertificates()
{
  Serial.print(F("Load certificates: "));

  // Start SPIFFS and retrieve certificates.
  SPIFFS.begin();
  int numCerts = certStore.initCertStore(SPIFFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.print(F("Number of CA certs read: "));
  Serial.print(numCerts);
  if (numCerts == 0) {
    Serial.println(F("\nNo certs found. Did you run certs-from-mozill.py and upload the SPIFFS directory before running?"));
    // return; // Can't connect to anything w/o certs!
  }

  client.setSession(&session); // certificate session to have more performance with subsequent calls
  client.setCertStore(&certStore);
  Serial.println(F("... done"));
}


// ******************** setup ********************
void setup() {
  // Serial
  Serial.begin(115200);
  Serial.println();
  Serial.println(APPNAME);
  Serial.println(VERSION);
  Serial.println();
  
  // WiFi
  static WiFiEventHandler e1, e2, e4;
  e1 = WiFi.onStationModeConnected(onSTAConnected);
  e2 = WiFi.onStationModeDisconnected(onSTADisconnected);
  e4 = WiFi.onStationModeGotIP(onSTAGotIP);

  // MORE SETUP

  // WiFi start
  WiFibegin();
}

// ******************** loop ********************
void loop() {
  static int lastloopminute = 99;

  // Once if got IP
  if (_wifiState == WIFI_GotIP)
  {
    InitialiseTime();
    LoadCertificates();
    _wifiState = CLIENT_Ready;
  }

  // put your main code here, to run repeatedly:
  now = time(nullptr);
  localtime_r(&now, &timeinfo);
  // each minute
  if (lastloopminute != timeinfo.tm_min)
  {
    lastloopminute = timeinfo.tm_min;
    if (_wifiState == CLIENT_Ready)
    {
      Serial.printf("%d:%d Retrieving ", timeinfo.tm_hour, timeinfo.tm_min);
      fetchURL(&client, URL_HOST, URL_PORT, URL_PATH);
    }
  }

}