# Hardware

## 1. Overview

The project consists of two independent hardware nodes, each built around an
ESP32-family board and connected to the same local Wi-Fi hotspot:

- **ENV-NODE** (ESP32-S3): environmental monitoring. Reads temperature,
  humidity, and light, and displays live values on an OLED.
- **SEC-NODE** (ESP32 DevKit): security monitoring. Detects motion with a PIR
  sensor and drives a relay module as an alarm output.

Both nodes are publish-only MQTT clients. All pin assignments below are taken
directly from the firmware (`firmware/env-node/env-node.ino` and
`firmware/sec-node/sec-node.ino`).

## 2. ENV-NODE

### Components

| Component | Role |
| --------- | ---- |
| ESP32-S3 development board | Main microcontroller |
| DHT11 | Temperature and humidity |
| LDR + resistor divider | Light intensity sensor |
| SSD1306 0.96-inch OLED | Local display |

### Pin Layout

| Component | Signal | GPIO | Notes |
| --------- | ------ | ---- | ----- |
| DHT11 | DATA | 4 | Digital data, 1-wire |
| LDR | ANALOG OUT | 1 | Read with `analogRead` |
| OLED | SDA | 8 | I2C data |
| OLED | SCL | 9 | I2C clock |
| OLED | ADDRESS | 0x3C | I2C device address |

### OLED Configuration

- Controller: SSD1306
- Size: 128 x 64 pixels
- Interface: I2C
- I2C address: `0x3C`
- I2C pins: SDA = GPIO 8, SCL = GPIO 9

### Wiring

```
              ESP32-S3
              --------
   DHT11 ---- GPIO 4   -----> Temperature/Humidity

   LDR   ---- GPIO 1   -----> Light intensity (analog)

   OLED      SDA = GPIO 8
             SCL = GPIO 9   -----> I2C display, addr 0x3C

   Power: module VCC -> 3.3 V, GND -> GND (board level)
```

### Operating Notes

- The DHT11 is configured with a 1000 ms read delay (`dht11.setDelay(1000)`).
- The LDR is read once per sensor cycle with `analogRead(LDR_PIN)`.
- The OLED is initialised before Wi-Fi and MQTT are started; a failed OLED
  initialisation is reported over the serial monitor but does not stop the
  node.
- Displayed values are refreshed every sensor interval together with the
  current status and MQTT state.
- The node publishes the derived status (SAFE / WARNING / DANGER), RSSI, and
  selected MQTT QoS to `safety/env/data`.

### Power and Ground

The firmware does not configure any power or ground pins, so the board-level
power connections are not defined by the project code.

- The DHT11, LDR, and OLED must be powered from the 3.3 V rail of the
  development board.
- All GND pins of the modules should be connected to the common GND of the
  development board.

## 3. SEC-NODE

### Components

| Component | Role |
| --------- | ---- |
| ESP32 DevKit | Main microcontroller |
| PIR motion sensor | Motion detection |
| 4-channel relay module | Alarm actuator, active-LOW |

### Pin Layout

| Component | Signal | GPIO | Notes |
| --------- | ------ | ---- | ----- |
| PIR | OUT | 27 | Digital input, HIGH = motion |
| Relay module | IN (channel 1) | 33 | Digital output, active-LOW |

### Relay Behavior

The relay module is active-LOW: the input is inverted, as configured in the
firmware:

- `LOW` = relay ON (energised)
- `HIGH` = relay OFF (de-energised)

```cpp
// firmware/sec-node/sec-node.ino
if (motionDetected) {
  digitalWrite(RELAY_PIN, LOW);   // ON
} else {
  digitalWrite(RELAY_PIN, HIGH);  // OFF
}
```

At boot the relay is set to `HIGH` (OFF) before the Wi-Fi connection is
attempted.

### Wiring

```
              ESP32 DevKit
              ------------
   PIR   ---- GPIO 27   -----> Motion input

   RELAY ---- GPIO 33   -----> Relay module channel 1 input (active-LOW)

   Power: PIR VCC -> 3.3 V,  Relay VCC/coil -> module supply rail,
          all GND -> common ground
```

### Operating Notes

- The PIR is allowed to stabilise for 30 seconds during setup, before the
  Wi-Fi connection is started (`delay(30000)`).
- Motion is read every loop iteration and the relay directly follows the
  motion state.
- The node publishes motion, alarm, relay state, RSSI, and the selected MQTT
  QoS to `safety/security/data`.

### Power and Ground

The firmware does not configure any power or ground pins. The board-level
power wiring is therefore not defined by the project code and must follow the
requirements of the selected relay module:

- A 5 V DC supply is commonly required for a relay module; check the module
  documentation for the coil and drive voltage.
- The PIR can usually be powered from the 3.3 V rail.
- All modules must share a common GND with the development board.

## 4. Hardware Notes

- The relay module has four channels; this project uses one channel, wired to
  GPIO 33.
- The relay is active-LOW, so `LOW` energises the relay and `HIGH` releases
  it. If the relay is ON when you expect OFF, check the wiring uses the
  active-LOW input and that the relay is powered per its documentation.
- The 30-second PIR stabilisation delay at startup reduces false triggers
  from the sensor powering up.
- If the OLED fails to initialise, the ENV-NODE still runs; the failure is
  reported on the serial monitor.
- Both nodes require a common GND between all connected modules and the
  development board.