# SentiCube - Modular IoT Monitoring Cube

SentiCube is a **modular, compact IoT device** designed to monitor motion, orientation, distance, environmental conditions and sound. It works out-of-the-box in **local mode** and can optionally connect to a cloud dashboard (ThingsBoard) for **remote monitoring and logging**.

---

## Features

* **ESP32-U with external antenna** for reliable Wi-Fi connectivity
* **Distance measurement:** VL53L0X / VL53L1X Time-of-Flight (ToF) sensor
* **Orientation / motion:** MPU-6050 (Gyroscope + Accelerometer)
* **Magnetic heading:** QMC5883P Magnetometer (Compass)
* **Environmental sensing:** AHT10 Temperature & Humidity sensor
* **Sound detection:** INMP441 I2S microphone
* **Local web interface** served by ESP32 for setup & monitoring
* **Wi-Fi provisioning** using **Wi-FiManager** (ESP32 hotspot)
* **Optional cloud integration** with ThingsBoard
* **Hybrid wake logic:**

  * Wakes on motion, sound, or distance events
  * Periodic heartbeat every **10 to 15 minutes** to update readings
  * **Offline logging** to SD card if cloud or Wi-Fi is unavailable

---

## Secondary Cube (LoRa Backup)

SentiCube supports an optional **secondary cube** that communicates with the main unit using **LoRa**. This backup cube operates within the same room (or nearby) and sends sensor data to the **primary cube**, which then handles Wi-Fi/cloud communication.

* **Primary cube:** Wi-Fi + ThingsBoard + LoRa receiver
* **Secondary cube:** LoRa transmitter (can run without Wi-Fi)
* Enables **redundancy, extended coverage, and offline resilience**

> If the primary cube loses Wi-Fi, data from both cubes is stored locally and uploaded once connectivity is restored.
> Even if the primary cube is destroyed, the secondary cube will persist the data.

---

## How It Works

### 1. Local Mode (Plug-and-Play)

1. Power SentiCube → ESP32 creates a **Wi-Fi hotspot** (`SentiCube-XXXX`).
2. Connect to the hotspot using a phone or laptop.
3. A **local web page (captive portal)** opens automatically.
4. Monitor **local sensor readings** immediately, no internet required.

---

### 2. Cloud Mode (Remote Monitoring)

SentiCube can optionally stream data to **ThingsBoard**:

1. **Cloud token setup:**

   * After purchasing cloud access, the user receives a **unique token**.
   * This token is **entered in the local web page** served by the ESP32.
   * The cube is **manually added to the ThingsBoard instance** (admin side) using the same token.
2. Cube connects to Wi-Fi using credentials provided in the local portal.
3. Cube publishes telemetry to ThingsBoard (MQTT/HTTP), allowing **real-time monitoring from any device**.

> **Note:** Each cube stores the token and Wi-Fi credentials persistently, allowing automatic reconnection.

---

### 3. LoRa Mirror Mode (Optional Backup Unit)

SentiCube supports an optional **secondary “Mirror Cube”** connected via LoRa for redundancy and local visibility:

1. The **primary cube continues normal operation** (local + cloud mode).
2. In parallel, the primary cube **sends a copy of all telemetry data via LoRa**.
3. The **mirror cube only receives this data** (it does not generate or transmit its own data).
4. The mirror cube can:

   * Display received sensor values locally
   * Store data as a backup log (optional SD card)
   * Act as a **fallback monitoring point** if cloud or Wi-Fi is unavailable

> **Note:** The mirror cube is fully optional and not required for system operation. It functions purely as a **redundant receiver for extended reliability and local backup visibility**.

---

## Sensors & Connections

| Sensor / Component       | Function                  | Connection Notes                      |
| ------------------------ | ------------------------- | ------------------------------------- |
| **ESP32-U**              | Microcontroller & Wi-Fi   | External antenna for better reception |
| **VL53L0X / VL53L1X**    | Time-of-Flight distance   | I2C, recommend 3.3v                   |
| **INMP441**              | I2S microphone            | I2S interface                         |
| **MPU-6050 (GY-521)**    | Gyroscope + Accelerometer | I2C, uses INT for motion interrupts   |
| **QMC5883P**             | Magnetometer / Compass    | I2C                                   |
| **AHT10**                | Temperature & Humidity    | I2C                                   |
| **18650 Battery Shield** | Rechargeable Battery Pack | VIN and GND                           |
| **MicroSD Module**       | Offline data logging      | SPI interface, stores queued events   |
| **SX1278 RA-02 (LoRa Module)** | Wireless communication (Primary ↔ Mirror Cube) | SPI interface, 3.3V only, used for LoRa mirror/backup link |
| **10 µF capacitor**      | Stabilize Voltage         | VCC and GND before the LoRa Module        |
| **500 µF capacitor**     | Stabilize Voltage         | VCC and GND after the battery pack


---

## Dependencies / Libraries

* **ESP32 Board definitions** for Arduino IDE
* **Adafruit VL53L0X / VL53L1X Library**
* **Adafruit MPU6050 Library**
* **QMC5883P Magnetometer Library**
* **AHT10 Library**
* **Wi-FiManager** - for hotspot-based Wi-Fi provisioning
* **PubSubClient / HTTPClient** - for ThingsBoard MQTT/HTTP publishing
* **I2S Library** - for INMP441 microphone
* **SD / SPI Library** - for offline logging

---

## Setup Instructions

### 1. Local Mode

1. Flash the ESP32 with the SentiCube firmware.
2. Power the cube → ESP32 hotspot appears (`SentiCube-XXXX`).
3. Connect to the hotspot → local web page opens.
4. Optional: monitor local sensor readings immediately.

### 2. Cloud Mode

1. **Obtain a ThingsBoard token** from the admin (project owner).
2. Connect to SentiCube hotspot → open local web page.
3. Enter:
   * Wi-Fi SSID & password
   * Cloud token
4. Cube stores credentials and token → reboots
5. Cube automatically connects to home Wi-Fi and **starts streaming telemetry** to ThingsBoard.
6. Log in to ThingsBoard dashboard → monitor your cube remotely.

> Optional: cube supports multiple sensors streaming simultaneously with thresholds, events and logging. Offline events are stored on the SD card and uploaded when connectivity is restored.

---

## Example JSON Telemetry Format

```json
{
  "temperature": 24.7,
  "humidity": 50.2,
  "distance_mm": 120,
  "pitch": 1.2,
  "roll": -0.3,
  "yaw": 45.0,
  "mag_x": 123,
  "mag_y": -45,
  "mag_z": 67,
  "sound_level": 56,
  "event_logged": true,
  "last_upload_status": "pending"
}
```

---

## Notes / Tips

* Cube can operate **fully offline** using local web interface.
* Cloud mode requires Wi-Fi and valid token.
* Ensure **I2C addresses don't conflict** if using multiple modules on same bus.
* Use **INT pins** from MPU6050 or ToF sensor for motion-triggered events.
* Use **EEPROM / SPIFFS** or SD card to persist Wi-Fi credentials, cloud token and **queued events for delayed upload**.

---

## License

This project is for **educational and prototyping purposes**. You may freely modify the code for learning and experimentation.

---
