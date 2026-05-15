#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

// Pins
#define SDA_PIN 21
#define SCL_PIN 22
#define SD_CS   13

// Server
WebServer server(80);

// Data
String sensorName = "GENERIC";
String liveValue = "--";
String liveUnit  = "";

// html
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Sensor Test</title>
  <style>
    body { font-family: Arial; text-align:center; margin-top:50px; }
    .box { font-size:50px; font-weight:bold; }
    .sub { font-size:18px; color:#666; margin-top:10px; }
  </style>
</head>
<body>
  <h2 id="title">Sensor Test</h2>
  <div class="box" id="val">--</div>
  <div class="sub" id="unit"></div>

  <script>
    async function updateData() {
      const r = await fetch('/data');
      const d = await r.json();
      document.getElementById('title').innerText = d.sensor;
      document.getElementById('val').innerText = d.value;
      document.getElementById('unit').innerText = d.unit;
    }
    setInterval(updateData, 300);
    updateData();
  </script>
</body>
</html>
)rawliteral";

// sensor logic
void readSensor() {
  // Example:
  // liveValue = String(distance);
  // liveUnit = "mm";
}

// sd logging
void logToSD(const String& sensor, const String& value, const String& unit) {
  File f = SD.open("/log.txt", FILE_APPEND);
  if (f) {
    f.println("{\"sensor\":\"" + sensor + "\",\"value\":\"" + value + "\",\"unit\":\"" + unit + "\"}");
    f.close();
  }
}

// routes (for now)
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleData() {
  String json = "{";
  json += "\"sensor\":\"" + sensorName + "\",";
  json += "\"value\":\"" + liveValue + "\",";
  json += "\"unit\":\"" + liveUnit + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// setup
void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD fail (continuing anyway)");
  } else {
    Serial.println("SD OK");
  }

  WiFiManager wm;
  bool ok = wm.autoConnect("ESP32-Setup");
  if (!ok) {
    Serial.println("WiFiManager failed, rebooting...");
    ESP.restart();
  }

  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

// loop, why do I even have to specify this
void loop() {
  server.handleClient();

  readSensor();

  static unsigned long lastLog = 0;
  if (millis() - lastLog > 1000) {
    lastLog = millis();
    logToSD(sensorName, liveValue, liveUnit);
  }

  delay(50);
}
