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
String liveX = "--";
String liveY = "--";
String liveZ = "--";

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
  <div class="sub" id="coords">X: -- Y: -- Z: --</div>

  <script>
    async function updateData() {
      const r = await fetch('/data');
      const d = await r.json();
      document.getElementById('title').innerText = d.sensor;
      document.getElementById('val').innerText = d.value;
      document.getElementById('unit').innerText = d.unit;
      document.getElementById('coords').innerText = "X: " + d.x + "  Y: " + d.y + "  Z: " + d.z;
    }
    setInterval(updateData, 300);
    updateData();
  </script>
</body>
</html>
)rawliteral";

bool qmcOK = false;

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(QMC5883P_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool readReg(uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(QMC5883P_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(QMC5883P_ADDR, (uint8_t)1) != 1) return false;
  val = Wire.read();
  return true;
}

bool initQMC() {
  uint8_t id = 0;
  if (!readReg(0x00, id)) return false;

  writeReg(0x0A, 0xCF);
  writeReg(0x0B, 0x08);

  return true;
}

bool readQMC(int16_t &x, int16_t &y, int16_t &z) {
  uint8_t status = 0;
  if (!readReg(0x09, status)) return false;
  if ((status & 0x01) == 0) return false;

  Wire.beginTransmission(QMC5883P_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom(QMC5883P_ADDR, (uint8_t)6) != 6) return false;

  int16_t rx = (int16_t)(Wire.read() | (Wire.read() << 8));
  int16_t ry = (int16_t)(Wire.read() | (Wire.read() << 8));
  int16_t rz = (int16_t)(Wire.read() | (Wire.read() << 8));

  x = rx;
  y = ry;
  z = rz;
  return true;
}

void readSensor() {
  if (!qmcOK) {
    liveValue = "ERR";
    liveUnit = "";
    liveX = "--";
    liveY = "--";
    liveZ = "--";
    return;
  }

  int16_t x, y, z;
  if (readQMC(x, y, z)) {
    float heading = atan2((float)y, (float)x) * 180.0 / PI;
    if (heading < 0) heading += 360.0;

    liveX = String(x);
    liveY = String(y);
    liveZ = String(z);
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
  } else {
    liveValue = "READ ERR";
    liveUnit = "";
  }
}

void logToSD(const String& sensor, const String& value, const String& unit, const String& x, const String& y, const String& z) {
  File f = SD.open("/log.txt", FILE_APPEND);
  if (f) {
    f.println("{\"sensor\":\"" + sensor + "\",\"value\":\"" + value + "\",\"unit\":\"" + unit + "\",\"x\":\"" + x + "\",\"y\":\"" + y + "\",\"z\":\"" + z + "\"}");
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
  json += "\"unit\":\"" + liveUnit + "\",";
  json += "\"x\":\"" + liveX + "\",";
  json += "\"y\":\"" + liveY + "\",";
  json += "\"z\":\"" + liveZ + "\"";
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

  qmcOK = initQMC();
  Serial.println(qmcOK ? "QMC5883P OK" : "QMC5883P fail");

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
    logToSD(sensorName, liveValue, liveUnit, liveX, liveY, liveZ);
  }

  delay(50);
}