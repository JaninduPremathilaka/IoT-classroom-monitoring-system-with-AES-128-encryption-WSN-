#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AESLib.h>
#include <Base64.h>  // Add this library for Base64 encoding
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ================== WiFi Credentials ==================
#define WIFI_SSID "project"
#define WIFI_PASSWORD "198506601642vV"

// ================== Firebase Configuration ==================
#define API_KEY "AIzaSyCw3E1iA9GN0jX358HUgCf45QnMZoPOonk"
#define DATABASE_URL "https://smart-classroom-monitori-54e79-default-rtdb.asia-southeast1.firebasedatabase.app"

// ================== Sensor Pins ==================
#define DHTPIN 18
#define DHTTYPE DHT22
#define MQ135_PIN 34

// ================== OLED Configuration ==================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================== Firebase Objects ==================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ================== Global Variables ==================
DHT dht(DHTPIN, DHTTYPE);
bool signupOK = false;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000; // 10 seconds

// ================== AES Encryption Setup ==================
AESLib aesLib;
byte aes_key[16] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
                     0x39, 0x30, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66 }; // "1234567890abcdef"
byte iv[16] = {0};

char clearText[128];
char encryptedText[128];

String encrypt(String msg) {
  msg.toCharArray(clearText, sizeof(clearText));
  int len = aesLib.encrypt64((const byte*)clearText, strlen(clearText), encryptedText, aes_key, 128, iv);
  
  // Encode encrypted text into Base64
  String encoded = base64::encode((const uint8_t*)encryptedText, len);
  return encoded;
}

void setup() {
  Serial.begin(115200);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed!");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("System Starting...");
  display.display();

  // Start DHT and MQ135
  dht.begin();
  pinMode(MQ135_PIN, INPUT);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  display.setCursor(0, 10);
  display.println("Connecting WiFi...");
  display.display();
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  display.setCursor(0, 20);
  display.println("WiFi Connected!");
  display.display();

  // Firebase Config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  // Anonymous Sign-Up
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase signup successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase signup failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  delay(2000);
  display.clearDisplay();
}

void loop() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int air = analogRead(MQ135_PIN);

  // Handle failed readings
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Failed to read from DHT22!");
    return;
  }

  // Print to Serial
  Serial.println("------ Sensor Data ------");
  Serial.print("Temperature: "); Serial.print(temp); Serial.println(" °C");
  Serial.print("Humidity: "); Serial.print(hum); Serial.println(" %");
  Serial.print("Air Quality: "); Serial.println(air);

  // Display on OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("SCRMS");
  display.println("----------------");
  display.print("Temp: "); display.print(temp); display.println(" C");
  display.print("Hum:  "); display.print(hum); display.println(" %");
  display.print("Air:  "); display.println(air);
  display.display();

  // Encrypt and Send to Firebase
  if (Firebase.ready() && signupOK && millis() - lastSendTime > sendInterval) {
    lastSendTime = millis();
    FirebaseJson json;
    json.add("temperature", encrypt(String(temp)));
    json.add("humidity", encrypt(String(hum)));
    json.add("air_quality", encrypt(String(air)));

    if (Firebase.RTDB.setJSON(&fbdo, "/sensorData", &json)) {
      Serial.println("Encrypted data sent to Firebase successfully");
    } else {
      Serial.print("Firebase Error: ");
      Serial.println(fbdo.errorReason());
    }
  }

  delay(2000); // Read every 2 sec (sending every 10 sec)
}
