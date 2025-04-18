#include "web_server.h"
#include "html_content.h" // Include the HTML content header file

TaskSchedulerWebServer::TaskSchedulerWebServer(const char* wifi_ssid, const char* wifi_password, int port) 
    : server(port), ssid(wifi_ssid), password(wifi_password), serverStarted(false) {
}

TaskSchedulerWebServer::~TaskSchedulerWebServer() {
    if (serverStarted) {
        server.stop();
    }
}

bool TaskSchedulerWebServer::begin() {
    // Connect to WiFi
    WiFi.begin(ssid, password);
    
    // Wait for connection, with timeout
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nFailed to connect to WiFi");
        return false;
    }
    
    localIP = WiFi.localIP();
    Serial.println("");
    Serial.print("Connected to WiFi with IP address: ");
    Serial.println(localIP);
    
    // Set up the server routes
    server.on("/", HTTP_GET, [this](){ this->handleRoot(); });
    server.on("/get-tasks", HTTP_GET, [this](){ this->handleGetTasks(); });
    server.on("/save-tasks", HTTP_POST, [this](){ this->handleSaveTasks(); });
    server.onNotFound([this](){ this->handleNotFound(); });
    
    // Start server
    server.begin();
    serverStarted = true;
    Serial.println("HTTP server started");
    
    return true;
}

void TaskSchedulerWebServer::handleClient() {
    if (serverStarted) {
        server.handleClient();
    }
}

IPAddress TaskSchedulerWebServer::getIP() const {
    return localIP;
}

bool TaskSchedulerWebServer::isRunning() const {
    return serverStarted;
}

void TaskSchedulerWebServer::handleRoot() {
    server.send(200, "text/html", INDEX_HTML); // INDEX_HTML is now from html_content.h
}

void TaskSchedulerWebServer::handleGetTasks() {
    String tasksJson = tasksToJson();
    server.send(200, "application/json", tasksJson);
}

void TaskSchedulerWebServer::handleSaveTasks() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "No data received");
        return;
    }
    
    String jsonString = server.arg("plain");
    
    // Parse JSON
    DynamicJsonDocument doc(2048); // Adjust size based on expected payload
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        String errorMsg = "Failed to parse JSON: ";
        errorMsg += error.c_str();
        server.send(400, "text/plain", errorMsg);
        return;
    }
    
    // Process the JSON data
    JsonArray tasksArray = doc.as<JsonArray>();
    
    if (updateScheduledTasks(tasksArray)) {
        server.send(200, "text/plain", "Tasks updated successfully");
    } else {
        server.send(500, "text/plain", "Failed to update tasks");
    }
}

void TaskSchedulerWebServer::handleNotFound() {
    server.send(404, "text/plain", "Not found");
}

bool TaskSchedulerWebServer::updateScheduledTasks(const JsonArray& tasksArray) {
    // Check if we have too many tasks
    if (tasksArray.size() > MAX_SCHEDULED_TASKS - 1) { // Leave room for NULL terminator
        Serial.println("Too many tasks!");
        return false;
    }
    
    // Reset all tasks
    for (int i = 0; i < MAX_SCHEDULED_TASKS; i++) {
        scheduledTasks[i].minute = -1;
        scheduledTasks[i].hour = -1;
        scheduledTasks[i].dayOfMonth = -1;
        scheduledTasks[i].month = -1;
        scheduledTasks[i].dayOfWeek = -1;
        scheduledTasks[i].lastRun = false;
        scheduledTasks[i].name = NULL;
    }
    
    // Update tasks from the received JSON
    int i = 0;
    for (JsonVariant taskVar : tasksArray) {
        JsonObject task = taskVar.as<JsonObject>();
        
        // Create a new permanent storage for the name string
        // We need to do this because the task.name in your structure is a pointer that needs
        // to point to permanent storage, not temporary variables
        char* taskName = (char*)malloc(50); // Allocate memory for task name
        if (taskName == NULL) {
            Serial.println("Memory allocation failed!");
            return false;
        }
        
        // Copy the name from the JSON into the allocated memory
        strlcpy(taskName, task["name"].as<const char*>(), 50);
        
        // Update the scheduledTasks array
        scheduledTasks[i].minute = task["minute"].as<int>();
        scheduledTasks[i].hour = task["hour"].as<int>();
        scheduledTasks[i].dayOfMonth = task["dayOfMonth"].as<int>();
        scheduledTasks[i].month = task["month"].as<int>();
        scheduledTasks[i].dayOfWeek = task["dayOfWeek"].as<int>();
        scheduledTasks[i].lastRun = false;
        scheduledTasks[i].name = taskName;
        
        i++;
    }
    
    // Set the terminator
    scheduledTasks[i].name = NULL;
    
    Serial.print("Updated ");
    Serial.print(i);
    Serial.println(" tasks");
    
    return true;
}

String TaskSchedulerWebServer::tasksToJson() {
    DynamicJsonDocument doc(2048); // Adjust size based on expected payload
    JsonArray tasksArray = doc.to<JsonArray>();
    
    // Convert scheduledTasks to JSON
    for (int i = 0; i < MAX_SCHEDULED_TASKS; i++) {
        if (scheduledTasks[i].name == NULL) break;
        
        JsonObject task = tasksArray.createNestedObject();
        task["name"] = scheduledTasks[i].name;
        task["minute"] = scheduledTasks[i].minute;
        task["hour"] = scheduledTasks[i].hour;
        task["dayOfMonth"] = scheduledTasks[i].dayOfMonth;
        task["month"] = scheduledTasks[i].month;
        task["dayOfWeek"] = scheduledTasks[i].dayOfWeek;
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}