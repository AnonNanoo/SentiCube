#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

#define LORA_SS   4
#define LORA_RST  16
#define LORA_DIO0 17
#define LORA_SCK  5
#define LORA_MISO 26
#define LORA_MOSI 27

void setup() {
  Serial.begin(115200);
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  while (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed");
    delay(500);
  }

  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa TX ready");
}

void loop() {
  StaticJsonDocument<128> doc;
  doc["device"] = "esp32_tx";
  doc["counter"] = millis() / 1000;
  doc["message"] = "hello";

  String payload;
  serializeJson(doc, payload);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  Serial.println("Sent:");
  Serial.println(payload);

  delay(1000);
}