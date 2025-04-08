#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "stepper_control.h"  // For the scheduledTasks array

class TaskSchedulerWebServer {
private:
    WebServer server;
    const char* ssid;
    const char* password;
    IPAddress localIP;
    bool serverStarted;
    
    // Methods to handle the various API endpoints
    void handleRoot();
    void handleGetTasks();
    void handleSaveTasks();
    void handleNotFound();
    
    // Method to apply task updates to the scheduledTasks array
    bool updateScheduledTasks(const JsonArray& tasksArray);
    
    // Method to convert scheduledTasks to JSON
    String tasksToJson();
    
public:
    TaskSchedulerWebServer(const char* wifi_ssid, const char* wifi_password, int port = 80);
    ~TaskSchedulerWebServer();
    
    bool begin();
    void handleClient();
    IPAddress getIP() const;
    bool isRunning() const;
};

#endif // WEB_SERVER_H