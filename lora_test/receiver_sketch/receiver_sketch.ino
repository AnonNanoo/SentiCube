#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <SD.h>

#define LORA_SS   4
#define LORA_RST  16
#define LORA_DIO0 17
#define LORA_SCK  5
#define LORA_MISO 26
#define LORA_MOSI 27

#define SD_CS 13
const char* LOG_FILE = "/lora_log.json";

void ensureLogFile() {
  if (!SD.exists(LOG_FILE)) {
    File f = SD.open(LOG_FILE, FILE_WRITE);
    if (f) {
      f.print("{\"data\":[]}");
      f.close();
    }
  }
}

void appendJsonToLog(const String& rawJson) {
  DynamicJsonDocument root(2048);

  File f = SD.open(LOG_FILE, FILE_READ);
  if (f) {
    DeserializationError err = deserializeJson(root, f);
    f.close();
    if (err) {
      root.clear();
      root["data"] = JsonArray();
    }
  } else {
    root["data"] = JsonArray();
  }

  JsonArray arr = root["data"].to<JsonArray>();
  JsonObject entry = arr.createNestedObject();
  entry["raw"] = rawJson;
  entry["rssi"] = LoRa.packetRssi();
  entry["millis"] = millis();

  DynamicJsonDocument msgDoc(256);
  if (deserializeJson(msgDoc, rawJson) == DeserializationError::Ok) {
    JsonObject msg = msgDoc.as<JsonObject>();
    for (JsonPair kv : msg) {
      entry[kv.key().c_str()] = kv.value();
    }
  }

  SD.remove(LOG_FILE);
  File out = SD.open(LOG_FILE, FILE_WRITE);
  if (out) {
    serializeJsonPretty(root, out);
    out.close();
    Serial.println("Saved to SD");
  } else {
    Serial.println("Failed to write log file");
  }
}

void setup() {
  Serial.begin(115200);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  while (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed");
    delay(500);
  }
  LoRa.setSyncWord(0xF3);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed");
    while (true) delay(1000);
  }

  ensureLogFile();
  Serial.println("LoRa RX + SD ready");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  Serial.println("LoRa packet received");

  String payload;
  while (LoRa.available()) {
    payload += (char)LoRa.read();
  }

  Serial.print("Payload: ");
  Serial.println(payload);
  Serial.print("RSSI: ");
  Serial.println(LoRa.packetRssi());

  appendJsonToLog(payload);
}