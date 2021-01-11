/* 
  FSWebServer - Example WebServer with SPIFFS backend for esp8266
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the ESP8266WebServer library for Arduino environment.
 
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)
  or you can upload the contents of a folder if you CD in that folder and run the following command:
  for file in `ls -A1`; do curl -F "file=@$PWD/$file" esp8266fs.local/edit; done
  
  access the sample web page at http://esp8266fs.local
  edit the page by going to http://esp8266fs.local/edit
*/
/*
 * This file is part of the esp8266 web interface
 *
 * Copyright (C) 2018 Johannes Huebner <dev@johanneshuebner.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <Ticker.h>
#include "Stk500.h"
#include "IntelHexParse.h"

#define DBG_OUTPUT_PORT Serial
#define RESET_PIN D2

const char* host = "charger";
StaticJsonDocument<200> root;
bool sendSerial = false;

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void staCheck();
void broadcastMessage(String message);
Ticker sta_tick(staCheck, 1000, 0, MICROS);
int8_t connectionNumber = 0;
File fsUploadFile;

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  //DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    //DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    //DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

static void handleUpdate()
{
  if(!server.hasArg("file")) {server.send(500, "text/plain", "BAD ARGS"); return;}
    String filename = server.arg("file");
    File file = SPIFFS.open(filename, "r");

    if(file) {
      broadcastMessage("Resetting Atmega");
      Stk500 stk500(RESET_PIN);
      stk500.setupDevice();
      IntelHexParse hexParse = IntelHexParse();
      broadcastMessage("Sending File");

      while(file.available()) {

        byte buff[50];
        String data = file.readStringUntil('\n');
        data.getBytes(buff, data.length());
        hexParse.ParseLine(buff);
        
        if(hexParse.IsPageReady()){
          byte* page = hexParse.GetMemoryPage();
          byte* address = hexParse.GetLoadAddress();
          stk500.flashPage(address, page);
        }
      }
      stk500.exitProgMode();
      file.close();
    }
  Serial.begin(57600);


  
  server.send(200, "text/json", "{}");
}

void handleFileList() {
  String path = "/";
  if(server.hasArg("dir")) 
    String path = server.arg("dir");
  //DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  
  output += "]";
  server.send(200, "text/json", output);
}

static void handleWifi()
{
  bool updated = true;
  if(server.hasArg("apSSID") && server.hasArg("apPW")) 
  {
    WiFi.softAP(server.arg("apSSID").c_str(), server.arg("apPW").c_str());
  }
  else if(server.hasArg("staSSID") && server.hasArg("staPW")) 
  {
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(server.arg("staSSID").c_str(), server.arg("staPW").c_str());
  }
  else
  {
    File file = SPIFFS.open("/wifi.html", "r");
    String html = file.readString();
    file.close();
    html.replace("%staSSID%", WiFi.SSID());
    html.replace("%apSSID%", WiFi.softAPSSID());
    html.replace("%staIP%", WiFi.localIP().toString());
    server.send(200, "text/html", html);
    updated = false;
  }

  if (updated)
  {
    File file = SPIFFS.open("/wifi-updated.html", "r");
    size_t sent = server.streamFile(file, getContentType("wifi-updated.html"));
    file.close();    
  }
}

void staCheck(){
  sta_tick.stop();
  if(!(uint32_t)WiFi.localIP()){
    WiFi.mode(WIFI_AP); //disable station mode
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
    switch(type) {
        case WStype_ERROR:      
        case WStype_DISCONNECTED:      
            connectionNumber = webSocket.connectedClients();
            break;
        case WStype_CONNECTED: 
            connectionNumber = webSocket.connectedClients();
            break;
        
        case WStype_TEXT: {
            String _payload = String((char *) &payload[0]);
            DeserializationError error = deserializeJson(root, _payload);
            sendSerial = root["showLog"];
            root["showLog"] = sendSerial;
            
            String response;
            serializeJson(root, response);

            webSocket.broadcastTXT(response);

          }
          break;  

    
             
        case WStype_BIN:
            break;
  
    }
}

void setup(void){
  Serial.begin(57600);
  Serial.setTimeout(100);
  SPIFFS.begin();

  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, HIGH);

  //WIFI INIT
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin();
  sta_tick.start();
  
  MDNS.begin(host);
  
  //SERVER INIT
  ArduinoOTA.begin();
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/wifi", handleWifi);
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);
  server.on("/fwupdate", handleUpdate);


  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  server.begin();
  server.client().setNoDelay(1);

  Serial.println("Web Socket Begin");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
}

void broadcastMessage(String message) {
  String response;
  root["message"] = message;
  root["timestamp"] = millis();
  serializeJson(root, response);
  webSocket.broadcastTXT(response);
}
 
void loop(void){
  server.handleClient();
  ArduinoOTA.handle();
  webSocket.loop();

  if(connectionNumber > 0 && sendSerial && Serial.available()) {
      String message = Serial.readString();
      if (message.length() > 0) {
        broadcastMessage(message);
      }
   }
}
