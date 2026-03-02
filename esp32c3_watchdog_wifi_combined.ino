#include <WiFi.h>
#include <FreeRTOS.h>

// WiFi Credentials
const char* ssid = "OPPO A5";
const char* password = "12345678";

// FreeRTOS Task Handles
TaskHandle_t scanTaskHandle = NULL;
TaskHandle_t watchdogTaskHandle = NULL;

// Watchdog Timer
hw_timer_t *watchdogTimer = NULL;
volatile bool watchdogTriggered = false;

void IRAM_ATTR onWatchdogTimer() {
    watchdogTriggered = true;
}

void wifiScanTask(void *parameter) {
    while (true) {
        Serial.println("Scanning for WiFi...");
        int n = WiFi.scanNetworks();
        Serial.printf("Found %d networks:
", n);
        for (int i = 0; i < n; ++i) {
            Serial.printf("%d: %s (%d)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        }
        vTaskDelay(pdMS_TO_TICKS(30000)); // Scan every 30 seconds
    }
}

void watchdogTask(void *parameter) {
    while (true) {
        if (watchdogTriggered) {
            Serial.println("Watchdog triggered! System will reset.");
            esp_restart(); // Restart the ESP32
        }
        // Reset watchdog
        watchdogTriggered = false;
        vTaskDelay(pdMS_TO_TICKS(5000)); // Check every 5 seconds
    }
}

void setup() {
    Serial.begin(115200);
    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");
    
    // Start FreeRTOS tasks
    xTaskCreate(wifiScanTask, "Wifi Scan Task", 2048, NULL, 1, &scanTaskHandle);
    xTaskCreate(watchdogTask, "Watchdog Task", 2048, NULL, 1, &watchdogTaskHandle);

    // Configure Watchdog Timer
    watchdogTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(watchdogTimer, &onWatchdogTimer, true);
    timerAlarmWrite(watchdogTimer, 5000 * 1000, false); // 5 seconds
    timerAlarmEnable(watchdogTimer);
}

void loop() {
    // Main loop does nothing, everything is handled in tasks
}