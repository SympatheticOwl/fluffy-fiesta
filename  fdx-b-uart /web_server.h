#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>  // For persistent storage
#include "stepper_control.h"  // For the scheduledTasks array

class TaskSchedulerWebServer {
private:
    WebServer server;
    const char* ssid;
    const char* password;
    IPAddress localIP;
    bool serverStarted;
    Preferences preferences;  // For persistent storage

    // Methods to handle the various API endpoints
    void handleRoot();
    void handleGetTasks();
    void handleSaveTasks();
    void handleNotFound();

    // Method to apply task updates to the scheduledTasks array
    bool updateScheduledTasks(const JsonArray& tasksArray);

    // Method to convert scheduledTasks to JSON
    String tasksToJson();

    // Methods for persistent storage
    bool saveTasks();
    bool loadTasks();

public:
    TaskSchedulerWebServer(const char* wifi_ssid, const char* wifi_password, int port = 80);
    ~TaskSchedulerWebServer();

    bool begin();
    void handleClient();
    IPAddress getIP() const;
    bool isRunning() const;
};

#endif // WEB_SERVER_H