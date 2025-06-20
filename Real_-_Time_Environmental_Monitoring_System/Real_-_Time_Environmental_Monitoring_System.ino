#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "ThingSpeak.h"

// WiFi credentials
const char* ssid = "POCO M4";
const char* password = "Roberto1234";

// ThingSpeak credentials
WiFiClient client;
unsigned long myChannelNumber = 2994284;
const char *myWriteAPIKey = "SUWI2ZM12G9JSBVU";

// OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT Sensor
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Web server
WebServer server(80);

// Timing
unsigned long lastTime = 0;
const unsigned long timerDelay = 30000;

// LEDs
#define LED_RED 2
#define LED_GREEN 5

// LED blink
bool ledState = false;
unsigned long lastBlink = 0;
const unsigned long blinkInterval = 300;

// HTML Handler with browser notification
void handleRoot() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  char msg[2000];
  snprintf(msg, 2000,
    "<html>\
    <head>\
      <meta http-equiv='refresh' content='4'/>\
      <meta name='viewport' content='width=device-width, initial-scale=1'>\
      <link rel='stylesheet' href='https://use.fontawesome.com/releases/v5.7.2/css/all.css'>\
      <title>ESP32 DHT Server</title>\
      <style>\
        html { font-family: Arial; text-align: center; }\
        h2 { font-size: 2.5rem; }\
        p { font-size: 2.5rem; }\
        .units { font-size: 1.2rem; }\
        .dht-labels { font-size: 1.5rem; vertical-align:middle; padding-bottom: 15px; }\
      </style>\
    </head>\
    <body>\
      <h2>ESP32 DHT Server</h2>\
      <p><i class='fas fa-thermometer-half' style='color:#ca3517;'></i>\
      <span class='dht-labels'>Temperature</span>\
      <span id='temp'>%.2f</span><sup class='units'>&deg;C</sup></p>\
      <p><i class='fas fa-tint' style='color:#00add6;'></i>\
      <span class='dht-labels'>Humidity</span>\
      <span>%.2f</span><sup class='units'>&percnt;</sup></p>\
      <script>\
        const temp = %.2f;\
        document.addEventListener('DOMContentLoaded', () => {\
          if ('Notification' in window && Notification.permission !== 'granted') {\
            Notification.requestPermission();\
          }\
          if (temp > 35 && Notification.permission === 'granted') {\
            new Notification('🔥 Overheat Alert!', {\
              body: 'Temperature is ' + temp.toFixed(2) + '°C',\
              icon: 'https://cdn-icons-png.flaticon.com/512/4814/4814481.png'\
            });\
          }\
        });\
      </script>\
    </body>\
    </html>", t, h, t);

  server.send(200, "text/html", msg);
}

// Connect to WiFi using DHCP
void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.println(WiFi.localIP());
    display.display();

    // Pause for 30 seconds to allow user to note the IP
    Serial.println("Waiting 30 seconds for user to access IP...");
    delay(30000);  // 30 seconds pause
  } else {
    Serial.println("\nFailed to connect to WiFi.");
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED initialization failed"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();
  delay(2000);

  connectToWiFi();
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Sensor Error!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sensor Error!");
    display.display();
    delay(2000);
    return;
  }

  Serial.printf("Temperature: %.2f °C | Humidity: %.2f %%\n", temperature, humidity);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Real-Time Sensor Data");
  display.println("---------------------");
  display.print("Temp: ");
  display.print(temperature);
  display.println(" *C");
  display.print("Humidity: ");
  display.print(humidity);
  display.println(" %");
  display.display();

  // Blinking red LED for overheat alert
  if (temperature > 35) {
    if (millis() - lastBlink >= blinkInterval) {
      lastBlink = millis();
      ledState = !ledState;
      digitalWrite(LED_RED, ledState);
    }
    digitalWrite(LED_GREEN, LOW);
  } else {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
  }

  // Update ThingSpeak every 30s
  if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() != WL_CONNECTED) {
      connectToWiFi();
      if (WiFi.status() != WL_CONNECTED) {
        lastTime = millis();
        return;
      }
    }

    ThingSpeak.setField(1, temperature);
    ThingSpeak.setField(2, humidity);
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (x == 200) {
      Serial.println("ThingSpeak update successful.");
    } else {
      Serial.println("ThingSpeak error code: " + String(x));
    }

    lastTime = millis();
  }

  server.handleClient();
}
