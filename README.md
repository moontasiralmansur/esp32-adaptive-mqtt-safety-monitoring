# ESP32 Adaptive MQTT Safety Monitoring

A distributed IoT safety and environment monitoring system built for an academic laboratory. Two ESP32 nodes sense their environment, derive a safety state, and publish JSON telemetry to a local Mosquitto MQTT broker over Wi-Fi. The MQTT QoS of every telemetry message is selected adaptively based on message importance and the current wireless signal strength.

## Academic Context

This is an academic project developed as part of my coursework at the University of Liberal Arts Bangladesh (ULAB). I am preserving my academic projects on GitHub as a record of my learning journey, coursework, and progression throughout my studies.

This repository is intended for educational and archival purposes.

## Academic Information

- **Author:** Moontasir Al Mansur
- **Institution:** University of Liberal Arts Bangladesh (ULAB)
- **Course:** CSE4418 (Internet of Things Lab)

## Configuration Placeholders

This repository uses placeholder values for private configuration:

- **Wi-Fi credentials**: `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` in the firmware files must be replaced with your actual Wi-Fi network credentials before uploading.
- **MQTT broker address**: `0.0.0.0` in the firmware and dashboard files is a placeholder that must be replaced with your actual broker IP address. The Mosquitto configuration uses `0.0.0.0` as a server bind address (meaning it listens on all interfaces), which is different from the client destination address used in the firmware and dashboard.

Real Wi-Fi credentials and machine-specific broker IP addresses have been intentionally removed from this public repository for security and privacy reasons.

## Project Overview

The system monitors environmental conditions and physical security in a room or laboratory using two ESP32 nodes working alongside a Windows laptop as host:

- **env-node** (ESP32-S3): measures temperature, humidity, and light with a DHT11 and an analog LDR, and shows live values on an SSD1306 OLED.
- **sec-node** (ESP32 DevKit): detects motion with a PIR sensor and drives a relay module as an alarm output.

Both nodes are publish-only MQTT clients. They publish telemetry to a local Mosquitto broker. The broker address in the firmware (`0.0.0.0`) is a placeholder that must be replaced with the actual broker IP before uploading. Because the laptop provides the Wi-Fi hotspot and hosts the broker, the complete system runs locally and offline without Internet access. An optional Streamlit dashboard is included for observing live MQTT telemetry in a browser.

## System Architecture

The system is a client–broker topology: the two nodes communicate only through the broker and never talk to each other directly.

```
Laptop
├── Wi-Fi hotspot
├── Mosquitto MQTT broker
└── MQTT subscriber (mosquitto_sub)

env-node  -----------> Mosquitto -----------> MQTT subscriber
sec-node  -----------> Mosquitto -----------> MQTT subscriber
```

The full design and data flow are described in [System Architecture](docs/architecture.md).

## Features

- DHT11 temperature/humidity monitoring.
- Analog LDR light monitoring.
- SSD1306 OLED local display.
- PIR motion detection.
- Relay actuation (active-LOW: LOW = ON).
- MQTT telemetry publishing with JSON payloads.
- Adaptive MQTT QoS (RSSI-aware for env-node, event-based for sec-node).
- Status and heartbeat messages.
- Local Mosquitto broker; fully offline operation.

## Hardware / Components Used

| Node | Components |
| ---- | ---------- |
| env-node | ESP32-S3, DHT11, LDR + resistor divider, SSD1306 0.96-inch OLED (I2C) |
| sec-node | ESP32 DevKit, PIR motion sensor, 4-channel relay module (one channel) |

## Pin Configuration

All GPIO assignments, wiring, and hardware notes are documented in [Hardware](docs/hardware.md).

## Libraries

- **env-node**: `DHT11` by Dhruba Saha, `Adafruit GFX`, `Adafruit SSD1306`, installed via the Arduino Library Manager.
- **sec-node**: no third-party library; uses the native ESP32 MQTT client and Wi-Fi libraries built into the ESP32 Arduino core.

Both sketches require the ESP32 board support (Arduino core).

## Environmental Monitoring

env-node reads the DHT11 (temperature, humidity) and the LDR (light) every two seconds, classifies the environment, and publishes every five seconds. The status classification:

- `temperature >= 38°C` → DANGER
- `temperature >= 30°C` OR `humidity >= 80%` → WARNING
- otherwise → SAFE

The status and the selected QoS are shown on the OLED and included in the MQTT payload.

## Security Monitoring

sec-node polls the PIR every loop iteration. When motion is detected, the active-LOW relay turns ON (GPIO `LOW`, alarm active); when motion clears, the relay returns to OFF (`HIGH`). Data is published every three seconds. At startup the firmware waits 30 seconds for the PIR to stabilize before connecting to Wi-Fi.

## MQTT Communication

Both nodes publish to the local Mosquitto broker:

| Topic | Publisher | Purpose | QoS |
| ----- | --------- | ------- | --- |
| `safety/env/data` | env-node | Sensor telemetry | adaptive |
| `safety/env/status` | env-node | Online notification | 1 |
| `safety/env/heartbeat` | env-node | Keep-alive | 0 |
| `safety/security/data` | sec-node | Motion/alarm/relay telemetry | adaptive |
| `safety/security/status` | sec-node | Online notification | 1 |
| `safety/security/heartbeat` | sec-node | Keep-alive | 0 |

The complete topic and payload reference is in [MQTT](docs/mqtt.md).

## Adaptive QoS

QoS is chosen at publish time by deterministic rules:

- env-node: DANGER → QoS 2; WARNING → QoS 2 with RSSI < -75 dBm, otherwise QoS 1; SAFE → QoS 1 with RSSI < -75 dBm, otherwise QoS 0.
- sec-node: motion detected → QoS 2; no motion → QoS 0.
- Status messages → QoS 1; heartbeat messages → QoS 0.

The complete rules are documented in [Adaptive QoS](docs/adaptive-qos.md).

## Network Architecture

The laptop hosts everything: the Wi-Fi hotspot, the Mosquitto broker (configured to listen on all interfaces), and the MQTT subscriber. The system therefore runs entirely on a closed local network without Internet access; no cloud service is involved.

## Telemetry / JSON Data

env-node data (`safety/env/data`):

```json
{"device":"env-node","temperature":27,"humidity":51,"light":1024,"status":"SAFE","rssi":-58,"qos":0}
```

sec-node data (`safety/security/data`), no motion and motion detected:

```json
{"device":"sec-node","motion":false,"alarm":false,"relay":"OFF","rssi":-61,"qos":0}
{"device":"sec-node","motion":true,"alarm":true,"relay":"ON","rssi":-61,"qos":2}
```

Status and heartbeat examples:

```json
{"device":"env-node","status":"ONLINE"}
{"device":"env-node","status":"ONLINE","rssi":-58}
```

## Requirements

- **Hardware**: ESP32-S3 development board, ESP32 DevKit, DHT11, LDR, SSD1306 OLED, PIR motion sensor, 4-channel relay module, USB cables, Windows laptop.
- **Software**: Arduino IDE with the ESP32 board support, the Arduino libraries listed above (env-node only), Mosquitto MQTT broker, PowerShell.

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
3. Start the subscriber: `mosquitto_sub -h <broker-ip> -p 1883 -t "safety/#" -v` (replace `<broker-ip>` with your Mosquitto broker address). Alternatively, run the optional Streamlit dashboard for a browser-based view (see [Setup and Run](docs/setup-and-run.md)).
4. Observe the telemetry; trigger environmental/security events and watch the `qos` field change.

## Project Structure

```
adaptive-mqtt-safety-monitoring/
├── README.md
├── .gitignore
├── firmware/
│   ├── env-node/
│   │   └── env-node.ino
│   └── sec-node/
│       └── sec-node.ino
├── mosquitto-config/
│   └── mosquitto.conf.example
├── dashboard/
│   ├── dashboard.py
│   └── requirements.txt
└── docs/
    ├── architecture.md
    ├── hardware.md
    ├── adaptive-qos.md
    ├── mqtt.md
    └── setup-and-run.md
```

## Limitations

These are documented facts of the current implementation, not features:

- No cloud backend or database.
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
- Historical data analysis.