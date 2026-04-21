#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <SPI.h>
#include <SD.h>
#include "driver/i2s.h"

#define SD_CS 13

#define I2S_WS   25
#define I2S_SD   33
#define I2S_SCK  32

WebServer server(80);

String sensorName = "INMP441";
String liveValue = "--";
String liveUnit  = "lvl";

String peakValue = "--";
String rmsValue  = "--";

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
  <div class="sub" id="coords">Peak: -- | RMS: --</div>

  <script>
    async function updateData() {
      const r = await fetch('/data');
      const d = await r.json();
      document.getElementById('title').innerText = d.sensor;
      document.getElementById('val').innerText = d.value;
      document.getElementById('unit').innerText = d.unit;
      document.getElementById('coords').innerText = "Peak: " + d.peak + " | RMS: " + d.rms;
    }
    setInterval(updateData, 300);
    updateData();
  </script>
</body>
</html>
)rawliteral";

void initI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void readSensor() {
  const int samples = 128;
  int32_t buffer[samples];
  size_t bytesRead = 0;

  esp_err_t err = i2s_read(I2S_NUM_0, (void*)buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
  if (err != ESP_OK || bytesRead == 0) {
    liveValue = "READ ERR";
    liveUnit = "";
    return;
  }

  int count = bytesRead / sizeof(int32_t);
  long long sumSq = 0;
  int32_t peak = 0;

  for (int i = 0; i < count; i++) {
    int32_t s = buffer[i] >> 8;
    if (s < 0) s = -s;
    if (s > peak) peak = s;
    sumSq += (long long)s * (long long)s;
  }

  float rms = sqrt((float)sumSq / count);

  peakValue = String(peak);
  rmsValue = String(rms, 1);
  liveValue = rmsValue;
  liveUnit = "lvl";

  Serial.print("Peak: ");
  Serial.print(peakValue);
  Serial.print(" RMS: ");
  Serial.println(rmsValue);
}

void logToSD(const String& sensor, const String& value, const String& unit,
             const String& peak, const String& rms) {
  File f = SD.open("/log.txt", FILE_APPEND);
  if (f) {
    f.println("{\"sensor\":\"" + sensor + "\",\"value\":\"" + value + "\",\"unit\":\"" + unit +
              "\",\"peak\":\"" + peak + "\",\"rms\":\"" + rms + "\"}");
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
  json += "\"peak\":\"" + peakValue + "\",";
  json += "\"rms\":\"" + rmsValue + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

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

  initI2S();
  Serial.println("INMP441 OK");

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
    logToSD(sensorName, liveValue, liveUnit, peakValue, rmsValue);
  }

  delay(50);
}