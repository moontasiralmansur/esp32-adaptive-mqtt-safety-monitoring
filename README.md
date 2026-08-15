# ESP32 Adaptive MQTT Safety Monitoring

A distributed IoT safety and environment monitoring system built for an academic laboratory. Two ESP32 nodes sense their environment, derive a safety state, and publish JSON telemetry to a local Mosquitto MQTT broker over Wi-Fi. The MQTT QoS of every telemetry message is selected adaptively based on message importance and the current wireless signal strength.

## Academic Context

This is an academic project developed as part of my coursework at the University of Liberal Arts Bangladesh (ULAB). I am preserving my academic projects on GitHub as a record of my learning journey, coursework, and progression throughout my studies.

This repository is intended for educational and archival purposes.

## Academic Information

- **Author:** Moontasir Al Mansur
- **Institution:** University of Liberal Arts Bangladesh (ULAB)
- **Course:** CSE4418 (Internet of Things Lab)

## Project Overview

The system monitors environmental conditions and physical security in a room or laboratory using two ESP32 nodes working alongside a Windows laptop as host:

- **ENV-NODE** (ESP32-S3): measures temperature, humidity, and light with a DHT11 and an analog LDR, and shows live values on an SSD1306 OLED.
- **SEC-NODE** (ESP32 DevKit): detects motion with a PIR sensor and drives a relay module as an alarm output.

Both nodes are publish-only MQTT clients. They publish telemetry to a Mosquitto broker at `192.168.137.1:1883`. Because the laptop provides the Wi-Fi hotspot and hosts the broker, the complete system runs locally and offline without Internet access.

## System Architecture

The system is a client–broker topology: the two nodes communicate only through the broker and never talk to each other directly.

```
Laptop
├── Wi-Fi hotspot
├── Mosquitto MQTT broker (192.168.137.1:1883)
└── MQTT subscriber (mosquitto_sub)

ENV-NODE  -----------> Mosquitto -----------> MQTT subscriber
SEC-NODE  -----------> Mosquitto -----------> MQTT subscriber
```

The full design and data flow are described in [System Architecture](docs/architecture.md).

## Features

- DHT11 temperature/humidity monitoring.
- Analog LDR light monitoring.
- SSD1306 OLED local display.
- PIR motion detection.
- Relay actuation (active-LOW: LOW = ON).
- MQTT telemetry publishing with JSON payloads.
- Adaptive MQTT QoS (RSSI-aware for ENV-NODE, event-based for SEC-NODE).
- Status and heartbeat messages.
- Local Mosquitto broker; fully offline operation.

## Hardware / Components Used

| Node | Components |
| ---- | ---------- |
| ENV-NODE | ESP32-S3, DHT11, LDR + resistor divider, SSD1306 0.96-inch OLED (I2C) |
| SEC-NODE | ESP32 DevKit, PIR motion sensor, 4-channel relay module (one channel) |

## Pin Configuration

All GPIO assignments, wiring, and hardware notes are documented in [Hardware](docs/hardware.md).

## Libraries

- **ENV-NODE**: `DHT11` by Dhruba Saha, `Adafruit GFX`, `Adafruit SSD1306`, installed via the Arduino Library Manager.
- **SEC-NODE**: no third-party library; uses the native ESP32 MQTT client and Wi-Fi libraries built into the ESP32 Arduino core.

Both sketches require the ESP32 board support (Arduino core).

## Environmental Monitoring

ENV-NODE reads the DHT11 (temperature, humidity) and the LDR (light) every two seconds, classifies the environment, and publishes every five seconds. The status classification:

- `temperature >= 38` C → DANGER
- `temperature >= 30` C OR `humidity >= 80` % → WARNING
- otherwise → SAFE

The status and the selected QoS are shown on the OLED and included in the MQTT payload.

## Security Monitoring

SEC-NODE polls the PIR every loop iteration. When motion is detected, the active-LOW relay turns ON (GPIO `LOW`, alarm active); when motion clears, the relay returns to OFF (`HIGH`). Data is published every three seconds. At startup the firmware waits 30 seconds for the PIR to stabilize before connecting to Wi-Fi.

## MQTT Communication

Both nodes publish to the local Mosquitto broker at `192.168.137.1:1883`:

| Topic | Publisher | Purpose | QoS |
| ----- | --------- | ------- | --- |
| `safety/env/data` | ENV-NODE | Sensor telemetry | adaptive |
| `safety/env/status` | ENV-NODE | Online notification | 1 |
| `safety/env/heartbeat` | ENV-NODE | Keep-alive | 0 |
| `safety/security/data` | SEC-NODE | Motion/alarm/relay telemetry | adaptive |
| `safety/security/status` | SEC-NODE | Online notification | 1 |
| `safety/security/heartbeat` | SEC-NODE | Keep-alive | 0 |

The complete topic and payload reference is in [MQTT](docs/mqtt.md).

## Adaptive QoS

QoS is chosen at publish time by deterministic rules:

- ENV-NODE: DANGER → QoS 2; WARNING → QoS 2 with RSSI < -75 dBm, otherwise QoS 1; SAFE → QoS 1 with RSSI < -75 dBm, otherwise QoS 0.
- SEC-NODE: motion detected → QoS 2; no motion → QoS 0.
- Status messages → QoS 1; heartbeat messages → QoS 0.

The complete rules are documented in [Adaptive QoS](docs/adaptive-qos.md).

## Network Architecture

The laptop hosts everything: the Wi-Fi hotspot, the Mosquitto broker (bound to `192.168.137.1:1883`), and the MQTT subscriber. The system therefore runs entirely on a closed local network without Internet access; no cloud service is involved.

## Telemetry / JSON Data

ENV-NODE data (`safety/env/data`):

```json
{"device":"ENV-NODE","temperature":27,"humidity":51,"light":1024,"status":"SAFE","rssi":-58,"qos":0}
```

SEC-NODE data (`safety/security/data`), no motion and motion detected:

```json
{"device":"SEC-NODE","motion":false,"alarm":false,"relay":"OFF","rssi":-61,"qos":0}
{"device":"SEC-NODE","motion":true,"alarm":true,"relay":"ON","rssi":-61,"qos":2}
```

Status and heartbeat examples:

```json
{"device":"ENV-NODE","status":"ONLINE"}
{"device":"ENV-NODE","status":"ONLINE","rssi":-58}
```

## Requirements

- **Hardware**: ESP32-S3 development board, ESP32 DevKit, DHT11, LDR, SSD1306 OLED, PIR motion sensor, 4-channel relay module, USB cables, Windows laptop.
- **Software**: Arduino IDE with the ESP32 board support, the Arduino libraries listed above (ENV-NODE only), Mosquitto MQTT broker, PowerShell.

## Setup / Wiring

- Wiring and pinouts: [Hardware](docs/hardware.md)
- Step-by-step setup and run guide: [Setup and Run](docs/setup-and-run.md)

## Mosquitto Configuration

- Broker setup guide: [Setup and Run](docs/setup-and-run.md)
- Configuration template: [mosquitto.conf.example](mosquitto-config/mosquitto.conf.example)

Copy the template to a local `mosquitto-config/adaptive-mqtt-safety-monitoring.conf` (ignored by Git) and start the broker with `mosquitto -v -c adaptive-mqtt-safety-monitoring.conf`. The installed Mosquitto configuration under Program Files must not be modified.

## Usage

1. Create the laptop Wi-Fi hotspot and configure the broker (see [Setup and Run](docs/setup-and-run.md)).
2. Replace the `YOUR_WIFI_SSID` / `YOUR_WIFI_PASSWORD` placeholders in both sketches, then upload `firmware/env-node/env-node.ino` to the ESP32-S3 and `firmware/sec-node/sec-node.ino` to the ESP32 DevKit.
3. Start the subscriber: `mosquitto_sub -h 192.168.137.1 -p 1883 -t "safety/#" -v`.
4. Observe the telemetry; trigger environmental/security events and watch the `qos` field change.

## Project Structure

```
adaptive-mqtt-safety-monitoring/
├── README.md
├── LICENSE
├── .gitignore
├── firmware/
│   ├── env-node/
│   │   └── env-node.ino
│   └── sec-node/
│       └── sec-node.ino
├── mosquitto-config/
│   └── mosquitto.conf.example
└── docs/
    ├── architecture.md
    ├── hardware.md
    ├── adaptive-qos.md
    ├── mqtt.md
    └── setup-and-run.md
```

## Limitations

These are documented facts of the current implementation, not features:

- No cloud backend, database, or dashboard.
- No MQTT authentication or TLS.
- No over-the-air (OTA) updates.
- No packet-loss or latency measurement.
- No machine-learning-based QoS prediction.
- Depends on the local broker and local hotspot; no Internet fallback.

## Future Improvement Possibilities

Future work only; none of these are implemented:

- Packet-loss-aware and latency-aware QoS selection.
- More detailed network-quality scoring beyond the RSSI threshold.
- Additional sensor nodes.
- MQTT authentication and TLS.
- Dashboard / visualization.
- Historical data analysis.

## License

This project is licensed under the [MIT License](LICENSE).