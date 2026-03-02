#include <WiFi.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ============================================================================
// CONFIGURATION - CHANGE THESE FOR YOUR MOBILE HOTSPOT
// ============================================================================
#define TARGET_SSID "OPPO A5"         // Your mobile hotspot SSID
#define TARGET_PASSWORD "12345678"    // Your mobile hotspot password
#define WDT_TIMEOUT_MS 10000          // Watchdog timeout: 10 seconds
#define WIFI_CONNECT_TIMEOUT 20000    // WiFi must connect within 20 seconds
#define LED_PIN 8                      // Built-in LED on ESP32-C3 (GPIO 8)

// Task handles
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t task2Handle = NULL;
TaskHandle_t task3Handle = NULL;

// Global variables
volatile bool wifiConnected = false;
volatile unsigned long wifiStartTime = 0;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════════════════════╗");
  Serial.println("║  ESP32-C3: WiFi Scan + 3 Tasks + Watchdog (Combined)  ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  
  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Step 1: Scan and verify target hotspot exists
  Serial.println("\n[SETUP] Scanning for your mobile hotspot...");
  delay(500);
  scanForTargetNetwork();
  
  // Step 2: Initialize watchdog
  Serial.println("\n[SETUP] Initializing Watchdog Timer...");
  delay(500);
  initializeWatchdog();
  
  // Step 3: Create FreeRTOS tasks
  Serial.println("\n[SETUP] Creating FreeRTOS tasks (3 total)...");
  
  // Task 1: WiFi Connection (MANDATORY - must connect within 20 seconds)
  xTaskCreate(
    wifiTask,
    "WiFi_Task",
    4096,
    NULL,
    2,  // Higher priority
    &wifiTaskHandle
  );
  
  // Task 2: Optional background task (e.g., sensor reading)
  xTaskCreate(
    task2Function,
    "Optional_Task2",
    2048,
    NULL,
    1,  // Normal priority
    &task2Handle
  );
  
  // Task 3: Optional background task (e.g., data logging)
  xTaskCreate(
    task3Function,
    "Optional_Task3",
    2048,
    NULL,
    1,  // Normal priority
    &task3Handle
  );
  
  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║          SETUP COMPLETE - SYSTEM READY                ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  Serial.println("\n✓ WiFi scanning enabled");
  Serial.println("✓ Watchdog monitoring active (10 seconds timeout)");
  Serial.println("✓ 3 FreeRTOS tasks running");
  Serial.println("✓ All tasks feeding watchdog every 2-5 seconds");
  Serial.println("\nIf WiFi doesn't connect in 20 seconds → Watchdog triggers reset!\n");
}

// ============================================================================
// MAIN LOOP (Runs on core 1 - Main task)
// ============================================================================
void loop() {
  // Feed the watchdog from main task
  esp_task_wdt_reset();
  
  // Blink LED to show main task is alive
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  
  // Display status
  Serial.printf("[MAIN] Watchdog fed | WiFi: %s | Uptime: %lu seconds\n", 
                wifiConnected ? "CONNECTED ✓" : "DISCONNECTED ✗",
                millis() / 1000);
  
  delay(2000); // Feed watchdog every 2 seconds
}

// ============================================================================
// SCAN FOR TARGET NETWORK
// ============================================================================
void scanForTargetNetwork() {
  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║           Scanning for Target Hotspot                  ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  
  Serial.printf("\n[SCAN] Looking for: %s\n", TARGET_SSID);
  
  // Turn on WiFi for scanning
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // Perform scan
  int numNetworks = WiFi.scanNetworks();
  
  if (numNetworks == 0) {
    Serial.println("[SCAN] ✗ No WiFi networks found!");
    Serial.println("[SCAN] Please check your mobile hotspot is ON");
    while (1) {
      Serial.println("[SCAN] Retrying in 5 seconds...");
      delay(5000);
      numNetworks = WiFi.scanNetworks();
      if (numNetworks > 0) break;
    }
  }
  
  Serial.printf("\n[SCAN] Found %d networks:\n\n", numNetworks);
  
  // Display all networks
  bool targetFound = false;
  for (int i = 0; i < numNetworks; i++) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    String security = getSecurityType(WiFi.encryptionType(i));
    
    Serial.printf("%d. SSID: %-30s | Signal: %d dBm | Security: %s\n",
                  i + 1, ssid.c_str(), rssi, security.c_str());
    
    // Check if target network found
    if (ssid == TARGET_SSID) {
      targetFound = true;
      Serial.printf("   ↓ TARGET FOUND! ✓\n");
    }
  }
  
  if (!targetFound) {
    Serial.println("\n[SCAN] ✗ Target hotspot NOT found!");
    Serial.printf("[SCAN] Searched for: %s\n", TARGET_SSID);
    Serial.println("[SCAN] Please turn ON your mobile hotspot and restart device");
    while (1) {
      Serial.println("[SCAN] Waiting for target network...");
      delay(3000);
    }
  }
  
  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║         Target Hotspot Found! Ready to Connect        ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  Serial.printf("\nTarget SSID:   %s\n", TARGET_SSID);
  Serial.printf("Target Password: ••••••••\n\n");
}

// ============================================================================
// GET SECURITY TYPE NAME
// ============================================================================
String getSecurityType(int encryptionType) {
  switch (encryptionType) {
    case WIFI_AUTH_OPEN:
      return "Open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    default:
      return "Unknown";
  }
}

// ============================================================================
// TASK 1: WiFi Connection Task (MANDATORY)
// ============================================================================
void wifiTask(void *parameter) {
  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║        WiFi Task Started (MANDATORY TASK)              ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  
  Serial.printf("\n[WiFi Task] Target: %s\n", TARGET_SSID);
  Serial.println("[WiFi Task] Connecting to mobile hotspot...");
  Serial.println("[WiFi Task] Timeout: 20 seconds\n");
  
  // Register this task to watchdog
  esp_task_wdt_add(NULL);
  
  // Start WiFi connection
  wifiStartTime = millis();
  WiFi.mode(WIFI_STA);
  WiFi.begin(TARGET_SSID, TARGET_PASSWORD);
  
  int attempts = 0;
  const int MAX_ATTEMPTS = 40; // 40 * 500ms = 20 seconds
  
  // Try to connect for 20 seconds
  while (WiFi.status() != WL_CONNECTED && attempts < MAX_ATTEMPTS) {
    delay(500);
    Serial.print(".");
    
    // Feed watchdog to keep this task alive while connecting
    esp_task_wdt_reset();
    attempts++;
    
    // Check if 20 seconds exceeded
    unsigned long elapsedTime = millis() - wifiStartTime;
    if (elapsedTime > WIFI_CONNECT_TIMEOUT) {
      Serial.println("\n\n");
      Serial.println("╔════════════════════════════════════════════════════════╗");
      Serial.println("║           WiFi Connection FAILED - TIMEOUT             ║");
      Serial.println("╚════════════════════════════════════════════════════════╝");
      
      Serial.printf("\n[WiFi Task] ✗ Failed to connect to: %s\n", TARGET_SSID);
      Serial.printf("[WiFi Task] Elapsed time: %lu seconds\n", elapsedTime / 1000);
      Serial.println("[WiFi Task] Possible reasons:");
      Serial.println("    • Hotspot SSID incorrect");
      Serial.println("    • Password incorrect");
      Serial.println("    • Mobile hotspot is OFF");
      Serial.println("    • Device is out of range");
      Serial.println("\n[WiFi Task] ⚠️  TRIGGERING WATCHDOG RESET...\n");
      Serial.println("[WiFi Task] Stopping watchdog feed to trigger reset...");
      
      // Stop feeding watchdog - let it timeout and trigger reset
      while (1) {
        delay(1000);
        Serial.println("[WiFi Task] Waiting for watchdog timeout (10 seconds)...");
      }
      // Code never reaches here - watchdog will force reset
    }
  }
  
  // Check if successfully connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════════════════════╗");
    Serial.println("║         WiFi Connection SUCCESSFUL ✓                  ║");
    Serial.println("╚════════════════════════════════════════════════════════╝");
    
    Serial.printf("\n[WiFi Task] ✓ Connected to: %s\n", WiFi.SSID().c_str());
    Serial.printf("[WiFi Task] IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi Task] Signal Strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("[WiFi Task] Connection time: %lu seconds\n\n", 
                  (millis() - wifiStartTime) / 1000);
    
    wifiConnected = true;
  }
  
  // Keep feeding watchdog while WiFi is healthy
  Serial.println("[WiFi Task] Entering monitoring loop...");
  Serial.println("[WiFi Task] Feeding watchdog every 3 seconds\n");
  
  int watchdogFeedCount = 0;
  
  while (1) {
    if (wifiConnected) {
      // Feed watchdog every 3 seconds while WiFi is healthy
      esp_task_wdt_reset();
      watchdogFeedCount++;
      
      Serial.printf("[WiFi Task] Fed watchdog #%d | RSSI: %d dBm | Status: GOOD ✓\n", 
                    watchdogFeedCount, WiFi.RSSI());
      delay(3000);
    } else {
      // If WiFi disconnected, try to reconnect
      Serial.println("[WiFi Task] WiFi disconnected! Attempting to reconnect...");
      WiFi.reconnect();
      delay(1000);
    }
  }
}

// ============================================================================
// TASK 2: Optional Background Task (Sensor Reading Example)
// ============================================================================
void task2Function(void *parameter) {
  Serial.println("\n[Task 2] Optional background task started");
  Serial.println("[Task 2] Example: Sensor reading task");
  Serial.println("[Task 2] Feeding watchdog every 5 seconds\n");
  
  // Register this task to watchdog
  esp_task_wdt_add(NULL);
  
  int sensorCounter = 0;
  
  while (1) {
    sensorCounter++;
    
    // Feed watchdog to keep this task alive
    esp_task_wdt_reset();
    
    Serial.printf("[Task 2] Reading sensor #%d | Value: %d\n", 
                  sensorCounter, (sensorCounter * 42) % 1000);
    
    delay(5000); // Do work every 5 seconds
    
    // Optional: To test watchdog trigger with Task 2, uncomment below:
    // if (sensorCounter > 10) {
    //   Serial.println("[Task 2] ✗ SENSOR FAILURE - Stopping watchdog feed!");
    //   while(1); // Infinite loop - stops feeding watchdog
    // }
  }
}

// ============================================================================
// TASK 3: Optional Background Task (Data Logging Example)
// ============================================================================
void task3Function(void *parameter) {
  Serial.println("\n[Task 3] Optional background task started");
  Serial.println("[Task 3] Example: Data logging task");
  Serial.println("[Task 3] Feeding watchdog every 4 seconds\n");
  
  // Register this task to watchdog
  esp_task_wdt_add(NULL);
  
  int dataPoints = 0;
  
  while (1) {
    dataPoints++;
    
    // Feed watchdog to keep this task alive
    esp_task_wdt_reset();
    
    Serial.printf("[Task 3] Logging data point #%d | Timestamp: %lu ms\n", 
                  dataPoints, millis());
    
    delay(4000); // Do work every 4 seconds
    
    // Optional: To test watchdog trigger with Task 3, uncomment below:
    // if (dataPoints > 15) {
    //   Serial.println("[Task 3] ✗ LOGGING FAILURE - Stopping watchdog feed!");
    //   while(1); // Infinite loop - stops feeding watchdog
    // }
  }
}

// ============================================================================
// WATCHDOG INITIALIZATION
// ============================================================================
void initializeWatchdog() {
  // Configure watchdog timer
  esp_task_wdt_config_t twdt_config = {
    .timeout_ms = WDT_TIMEOUT_MS,      // 10 second timeout
    .idle_core_mask = 0,               // Monitor core 0 (ESP32-C3 is single-core)
    .trigger_panic = true              // Trigger panic (automatic reset) on timeout
  };
  
  // Deinitialize if already initialized
  esp_task_wdt_deinit();
  
  // Initialize watchdog with config
  ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
  
  // Add main task to watchdog (loop function runs on core 1)
  ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
  
  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║           Watchdog Timer Configured                   ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  Serial.printf("\n[WDT] Timeout: %u milliseconds (10 seconds)\n", WDT_TIMEOUT_MS);
  Serial.println("[WDT] Monitoring: Main task + 3 FreeRTOS tasks");
  Serial.println("[WDT] Trigger action: Automatic device reset");
  Serial.println("[WDT] Status: ACTIVE\n");
}