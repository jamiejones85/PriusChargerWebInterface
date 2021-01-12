#ifndef WEB_SERVER_H
#define WEB_SERVER_H
#include <Arduino.h>
#include <ESP8266WebServer.h>

class WebServer {
    private:
        File fsUploadFile;
        ESP8266WebServer server;
        int resetPin;

        void handleFileList();
        void handleWifi();
        void handleFileUpload();
        void handleUpdate();
        bool handleFileRead(String path);
        String getContentType(String filename);

    public:
        void setup(int resetPin);
        void loop();

};

#endif