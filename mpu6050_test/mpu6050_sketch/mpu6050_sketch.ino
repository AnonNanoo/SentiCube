#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define SD_CS   13
#define MPU_ADDR 0x68

WebServer server(80);

String sensorName = "MPU6050";
String liveValue = "--";
String liveUnit  = "";

String accelX = "--";
String accelY = "--";
String accelZ = "--";
String gyroX = "--";
String gyroY = "--";
String gyroZ = "--";
String tempC = "--";

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
  <div class="sub" id="coords">A: -- -- -- | G: -- -- -- | T: --</div>

  <script>
    async function updateData() {
      const r = await fetch('/data');
      const d = await r.json();
      document.getElementById('title').innerText = d.sensor;
      document.getElementById('val').innerText = d.value;
      document.getElementById('unit').innerText = d.unit;
      document.getElementById('coords').innerText =
        "A: " + d.ax + " " + d.ay + " " + d.az +
        " | G: " + d.gx + " " + d.gy + " " + d.gz +
        " | T: " + d.temp;
    }
    setInterval(updateData, 300);
    updateData();
  </script>
</body>
</html>
)rawliteral";

bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool readByte(uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)1) != 1) return false;
  val = Wire.read();
  return true;
}

bool readBytes(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool mpuOK = false;

void initMPURaw() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission();
}

void readSensor() {
  if (!mpuOK) {
    liveValue = "ERR";
    liveUnit = "";
    return;
  }

  uint8_t data[14];
  if (!readBytes(0x3B, data, 14)) {
    liveValue = "READ ERR";
    liveUnit = "";
    return;
  }

  int16_t ax = (int16_t)((data[0] << 8) | data[1]);
  int16_t ay = (int16_t)((data[2] << 8) | data[3]);
  int16_t az = (int16_t)((data[4] << 8) | data[5]);
  int16_t rawTemp = (int16_t)((data[6] << 8) | data[7]);
  int16_t gx = (int16_t)((data[8] << 8) | data[9]);
  int16_t gy = (int16_t)((data[10] << 8) | data[11]);
  int16_t gz = (int16_t)((data[12] << 8) | data[13]);

  float aX = ax / 16384.0;
  float aY = ay / 16384.0;
  float aZ = az / 16384.0;

  float gX = gx / 131.0;
  float gY = gy / 131.0;
  float gZ = gz / 131.0;

  float t = (rawTemp / 340.0) + 36.53;

  accelX = String(aX, 2);
  accelY = String(aY, 2);
  accelZ = String(aZ, 2);
  gyroX = String(gX, 2);
  gyroY = String(gY, 2);
  gyroZ = String(gZ, 2);
  tempC = String(t, 2);

  liveValue = tempC;
  liveUnit = "C";

  Serial.print("AX: "); Serial.print(accelX);
  Serial.print(" AY: "); Serial.print(accelY);
  Serial.print(" AZ: "); Serial.print(accelZ);
  Serial.print(" | GX: "); Serial.print(gyroX);
  Serial.print(" GY: "); Serial.print(gyroY);
  Serial.print(" GZ: "); Serial.print(gyroZ);
  Serial.print(" | T: "); Serial.println(tempC);
}

void logToSD(const String& sensor, const String& value, const String& unit,
             const String& ax, const String& ay, const String& az,
             const String& gx, const String& gy, const String& gz,
             const String& temp) {
  File f = SD.open("/log.txt", FILE_APPEND);
  if (f) {
    f.println("{\"sensor\":\"" + sensor + "\",\"value\":\"" + value + "\",\"unit\":\"" + unit +
              "\",\"ax\":\"" + ax + "\",\"ay\":\"" + ay + "\",\"az\":\"" + az +
              "\",\"gx\":\"" + gx + "\",\"gy\":\"" + gy + "\",\"gz\":\"" + gz +
              "\",\"temp\":\"" + temp + "\"}");
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
  json += "\"ax\":\"" + accelX + "\",";
  json += "\"ay\":\"" + accelY + "\",";
  json += "\"az\":\"" + accelZ + "\",";
  json += "\"gx\":\"" + gyroX + "\",";
  json += "\"gy\":\"" + gyroY + "\",";
  json += "\"gz\":\"" + gyroZ + "\",";
  json += "\"temp\":\"" + tempC + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("BOOT");

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!SD.begin(SD_CS)) Serial.println("SD fail (continuing anyway)");
  else Serial.println("SD OK");

  WiFiManager wm;
  bool ok = wm.autoConnect("ESP32-Setup");
  if (!ok) {
    Serial.println("WiFiManager failed, rebooting...");
    ESP.restart();
  }

  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  if (i2cPresent(MPU_ADDR)) {
    Serial.println("MPU6050 detected at 0x68");
    uint8_t who = 0;
    if (readByte(0x75, who)) {
      Serial.print("WHO_AM_I: 0x");
      Serial.println(who, HEX);
    }
    initMPURaw();
    mpuOK = true;
    Serial.println("MPU6050 OK");
  } else {
    Serial.println("MPU6050 fail");
    mpuOK = false;
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
    logToSD(sensorName, liveValue, liveUnit, accelX, accelY, accelZ, gyroX, gyroY, gyroZ, tempC);
  }

  delay(50);
}