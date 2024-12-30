#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// -------------------- DEFINES & CONSTANTS --------------------
#define LED_BUILTIN         2
#define SERVO_PIN           21
#define DEEP_SLEEP_MINUTE   60000000
#define DEEP_SLEEP_TIME     (DEEP_SLEEP_MINUTE * 20)
#define DEEP_SLEEP_TIME_TEST 4000000

// Wi-Fi credentials
const char* ssid        = "[enter your SSID here]";
const char* password    = "[enter your password here]";

// API endpoint and authentication
const char* apiEndpoint = "[your API endpoint here]";
const char* authorizationHeader =
  "Bearer [your authorization token here]";

// Test mode flag
const bool testmode = false;

// -------------------- GLOBAL OBJECTS --------------------
WiFiClient   wifiClient;
HTTPClient   http;
Servo        myservo;

// --------------------------------------------------------
//                      SETUP
// --------------------------------------------------------
void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  connect_to_wifi();
}

// --------------------------------------------------------
//              CONNECT TO WIFI FUNCTION
// --------------------------------------------------------
void connect_to_wifi() {
  int attempts = 0;
  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  // Limit the connection attempts to 10 with exponential backoff
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500 * (1 << attempts));  // 500ms, 1s, 2s, 4s...
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));  // Toggle LED
    attempts++;
  }

  digitalWrite(LED_BUILTIN, LOW);  // Turn off LED after connection attempt

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
  } else {
    Serial.println("\nFailed to connect to WiFi");
    go_to_sleep();
  }
}

// --------------------------------------------------------
//                INIT TIME VIA NTP
// --------------------------------------------------------
void init_time() {
  // Central European Time (Berlin): 3600 offset + 3600 DST
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for NTP time sync...");

  time_t nowSecs = time(nullptr);
  while (nowSecs < 24 * 3600) {
    delay(500);
    Serial.print(".");
    nowSecs = time(nullptr);
  }
  Serial.println("\nTime synchronized");
}

// --------------------------------------------------------
//              GET CURRENT LOCAL TIME
// --------------------------------------------------------
tm get_current_time() {
  time_t now;
  tm localTime;
  time(&now);
  localtime_r(&now, &localTime);
  return localTime;
}

// --------------------------------------------------------
//              GET SENSOR DATA (HTTP GET)
// --------------------------------------------------------
int get_sensor_data() {
  float state = -1;

  if (WiFi.status() == WL_CONNECTED) {
    http.begin(wifiClient, apiEndpoint);
    http.addHeader("Authorization", authorizationHeader);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        state = doc["state"].as<float>();
      } else {
        Serial.println("Failed to parse JSON");
      }
    } else {
      Serial.println("Error in HTTP request");
    }
    http.end();
  }

  return state;
}

// --------------------------------------------------------
//              GO TO DEEP SLEEP
// --------------------------------------------------------
void go_to_sleep() {
  myservo.detach();        // Detach servo to save power
  WiFi.disconnect(true);   // Disconnect WiFi to save power
  WiFi.mode(WIFI_OFF);     // Turn off WiFi module

  Serial.println("Servo off, WiFi off, going to sleep.");
  delay(500);
  Serial.end();
  delay(2000);

  if (testmode) {
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME_TEST);
  } else {
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME);
  }

  esp_deep_sleep_start();
}

// --------------------------------------------------------
//                       LOOP
// --------------------------------------------------------
void loop() {
  // Initialize time via NTP
  init_time();

  // Get current local time
  tm currentTime = get_current_time();
  Serial.print("Hour: ");
  Serial.println(currentTime.tm_hour);

  // If it's night time (0:00 - 6:00), go to sleep
  if (currentTime.tm_hour < 6) {
    Serial.println("It's night time, going to sleep...");
    go_to_sleep();
  }

  // Get battery state from API or use random for testing
  int battery_state = (testmode) ? random(0, 100) : get_sensor_data();
  if (battery_state == -1) {
    Serial.println("API request failed, going to sleep...");
    go_to_sleep();
  }

  int servo_rotation = map(battery_state, 0, 100, 0, 180);

  // Print debug info
  Serial.print("Battery: ");
  Serial.println(battery_state);
  Serial.print("Servo Rotation: ");
  Serial.println(servo_rotation);

  // Move the servo
  myservo.attach(SERVO_PIN);
  myservo.write(servo_rotation);
  delay(1000);

  // Sleep
  go_to_sleep();
}

/*
Example curl request for testing:

curl \
  -H "Authorization: Bearer <YOUR_LONG_TOKEN>" \
  -H "Content-Type: application/json" \
  http://192.168.178.69:8123/api/states/sensor.s10x_state_of_charge
*/
