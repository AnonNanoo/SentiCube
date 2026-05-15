#include <SPI.h>
#include <LoRa.h>
#include <SD.h>

#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

#define LORA_CS   5
#define LORA_RST  16
#define LORA_DIO0 17

#define SD_CS     13

#define LORA_FREQ 433E6
const char* LOG_FILE = "/lora_log.txt";

void deselectAll() {
  digitalWrite(LORA_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
}

void appendLog(const String& rawText, int rssi) {
  deselectAll();
  digitalWrite(LORA_CS, HIGH);

  File f = SD.open(LOG_FILE, FILE_APPEND);
  if (!f) {
    Serial.println("Failed to open log file");
    return;
  }

  f.print("millis=");
  f.print(millis());
  f.print(" | rssi=");
  f.print(rssi);
  f.print(" | ");
  f.println(rawText);
  f.close();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LORA_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  deselectAll();

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa init failed");
    while (true) delay(1000);
  }
  LoRa.setSyncWord(0xF3);

  digitalWrite(LORA_CS, HIGH);

  if (!SD.begin(SD_CS, SPI, 400000)) {
    Serial.println("SD init failed");
    while (true) delay(1000);
  }

  Serial.println("MirrorCube ready");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String payload;
  while (LoRa.available()) {
    payload += (char)LoRa.read();
  }

  int rssi = LoRa.packetRssi();
  Serial.println(payload);
  appendLog(payload, rssi);
}