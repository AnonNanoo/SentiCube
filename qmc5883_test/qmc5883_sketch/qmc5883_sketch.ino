#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define SD_CS   13
#define QMC5883P_ADDR 0x2C

WebServer server(80);

String sensorName = "QMC5883P";
String liveValue = "--";
String liveUnit  = "deg";

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

bool qmcOK = false;

void readSensor() {
  if (!qmcOK) {
    liveValue = "ERR";
    liveUnit = "";
    return;
  }

  Wire.beginTransmission(QMC5883P_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) {
    liveValue = "READ ERR";
    liveUnit = "";
    return;
  }

  if (Wire.requestFrom(QMC5883P_ADDR, (uint8_t)6) < 6) {
    liveValue = "READ ERR";
    liveUnit = "";
    return;
  }

  int16_t x = (int16_t)(Wire.read() | (Wire.read() << 8));
  int16_t y = (int16_t)(Wire.read() | (Wire.read() << 8));
  int16_t z = (int16_t)(Wire.read() | (Wire.read() << 8));

  float heading = atan2((float)y, (float)x) * 180.0 / PI;
  if (heading < 0) heading += 360.0;

  liveValue = String(heading, 1);
  liveUnit = "deg";

  Serial.print("X: ");
  Serial.print(x);
  Serial.print(" Y: ");
  Serial.print(y);
  Serial.print(" Z: ");
  Serial.print(z);
  Serial.print(" Azimuth: ");
  Serial.println(heading, 1);
}

void logToSD(const String& sensor, const String& value, const String& unit) {
  File f = SD.open("/log.txt", FILE_APPEND);
  if (f) {
    f.println("{\"sensor\":\"" + sensor + "\",\"value\":\"" + value + "\",\"unit\":\"" + unit + "\"}");
    f.close();
  }
}

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

  Wire.beginTransmission(QMC5883P_ADDR);
  qmcOK = (Wire.endTransmission() == 0);

  if (qmcOK) {
    Wire.beginTransmission(QMC5883P_ADDR);
    Wire.write(0x0A);
    Wire.write(0xCF);
    Wire.endTransmission();

    Wire.beginTransmission(QMC5883P_ADDR);
    Wire.write(0x0B);
    Wire.write(0x08);
    Wire.endTransmission();

    Serial.println("QMC5883P OK");
  } else {
    Serial.println("QMC5883P fail");
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

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