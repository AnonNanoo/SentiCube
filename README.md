# Sensor Testing (SentiCube)

This branch is used to test each SentiCube sensor individually before integration.

---

## Goal

Verify that every sensor works correctly on its own:

* stable readings
* correct wiring (I2C / SPI)
* no crashes
* SD JSON logging

---

## Sensors tested here (tested sensors / modules will be marked with an "X")

* VL53L0X / VL53L1X (distance) 
* AHT10 (temperature & humidity)
* MPU-6050 (motion)
* QMC5883P (compass)
* INMP441 (microphone)
* SX1278 LoRa module
* MicroSD module
* WiFiManager setup

---

## Structure

Each sensor has its own sketch:

```text id="s1"
vl53_test/
aht10_test/
mpu6050_test/
qmc5883_test/
inmp441_test/
lora_test/
```

---

## SD Logging

All tests save JSON data to SD (structure not final):

```json id="s2"
{ "sensor": "VL53", "value": 1200 }
```

---

## Workflow

1. Test sensor in isolation
2. Check Serial output
3. check SD logs
4. Fix issues if needed
5. Move to dev branch eventually

---

## ⚠️ Note

This branch is only for testing, not final system code.
