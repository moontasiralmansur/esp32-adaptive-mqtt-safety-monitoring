# System Architecture

Adaptive MQTT Safety Monitoring System

CSE4418 - Internet of Things Lab
University of Liberal Arts Bangladesh (ULAB)

## 1. System Overview

This project is a distributed Internet of Things safety and environment
monitoring system. It consists of two ESP32-based nodes that communicate
through a local MQTT broker running on a Windows laptop. Each node reads its
sensors, derives a safety state, and publishes JSON telemetry over MQTT. The
MQTT delivery quality (QoS) is chosen adaptively based on the importance of
each message and the current wireless signal strength.

The two nodes are:

- `env-node`: environmental monitoring (temperature, humidity, light, OLED
  display)
- `sec-node`: security monitoring (motion detection and relay actuation)

Both nodes are publish-only MQTT clients. No node subscribes to any topic.

## 2. System Components

### env-node

| Item        | Detail                                        |
| ----------- | --------------------------------------------- |
| MCU         | ESP32-S3 development board                    |
| Sensors     | DHT11 temperature/humidity, analog LDR       |
| Display     | 0.96-inch SSD1306 OLED (I2C, address 0x3C)   |
| Firmware    | `firmware/env-node/env-node.ino`             |

env-node reads temperature, humidity, and light level, classifies the
environment as SAFE, WARNING, or DANGER, selects an adaptive MQTT QoS, and
publishes the readings as JSON telemetry on `safety/env/data`. The OLED shows
the current values, status, and MQTT connection state. Hardware details are
documented in `docs/hardware.md`.

### sec-node

| Component  | Description                                |
| ---------- | ------------------------------------------ |
| MCU        | ESP32 DevKit |
| Sensor     | PIR motion sensor (GPIO 27) |
| Actuator   | 4-channel relay module, one channel used (GPIO 33), active-LOW |
| Control    | `firmware/sec-node/sec-node.ino`           |

sec-node reads the PIR motion sensor. When motion is detected it energizes
the active-LOW relay (relay ON, alarm active) and publishes JSON on
`safety/security/data`. Hardware details are documented in
`docs/hardware.md`.

### Laptop

A Windows laptop performs three roles in this system:

1. **Local Wi-Fi hotspot** - creates the only access point the nodes connect
   to.
2. **Mosquitto MQTT broker** - runs the broker the nodes publish telemetry
   on.
3. **MQTT monitoring/subscriber** - subscribes to the project topics to
   observe the published data.

An optional Streamlit dashboard (`dashboard/dashboard.py`) provides a
browser-based alternative to the command-line subscriber. It is a read-only
MQTT observer and does not control the nodes.

The system is designed to operate entirely within that local network. It does
not depend on Internet access.

## 3. Network Architecture

The system uses a single local network in which the laptop is both the network
gateway/hotspot host and the MQTT broker.

| Role                    | Address                       |
| ----------------------- | ----------------------------- |
| Local hotspot host      | 192.168.137.1                 |
| MQTT broker             | mqtt://192.168.137.1:1883     |
| Wi-Fi network          | local hotspot provided by the laptop (SSID and password are not part of this repository) |

The ESP32 nodes connect to the laptop hotspot and publish to the broker at
`192.168.137.1:1883` (native ESP-IDF `mqtt_client`).

No external server or Internet connectivity is required. No TLS, credentials,
or device authentication is used.

## 4. High-Level Architecture Diagram

```
        Laptop
        Windows (Wi-Fi hotspot)
                 |
                 v
          Mosquitto MQTT broker
          192.168.137.1:1883
                 |
          +------+---------+
          |                |
          v                v
     env-node          sec-node
     ESP32-S3           ESP32
          |                |
    DHT11 + LDR       PIR + Relay (active-LOW)
          |
        OLED (SSD1306)
```

Data flow over the broker:

```
     env-node  ---------------------------->  Laptop / MQTT subscriber
       |
       v                                        ^
  Mosquitto broker ---------------------------+        
       ^                                        |
       |                                        |
     sec-node  ---------------------------+
        publish                                observer
```

Note that the two ESP32 nodes do not communicate with each other directly. All
communication is publish-to-broker, and the laptop MQTT subscriber reads the
telemetry published by both nodes.

## 5. Environment Node Data Flow

The env-node sensor loop is:

- DHT11 + LDR (temperature, humidity, light)
- ESP32-S3 reads the sensor values
- determine the environmental status (SAFE / WARNING / DANGER)
- select the adaptive MQTT QoS
- build the JSON payload
- publish to Mosquitto on `safety/env/data`
- the laptop MQTT subscriber receives the message

The environmental status is derived from temperature and humidity thresholds.
The three possible states are:

| Status  | Condition                |
| ------- | ------------------------ |
| SAFE    | normal conditions        |
| WARNING | rising temperature/humidity indicating caution |
| DANGER  | critical temperature     |

Live temperature, humidity, light, environmental status, selected QoS, and
MQTT connection state are also shown on the node OLED.

## 6. Security Node Data Flow

```
PIR
  |
  v
ESP32
  |
  +--> motion detection
  |        |
  |        v
  |     relay control (active-LOW: LOW = ON)
  |
  +--> select adaptive MQTT QoS
  |        |
  |        v
  |     create JSON payload
  |        |
  |        v
  +--> publish to Mosquitto (safety/security/data)
            |
            v
       MQTT subscriber
```

Motion detection turns the active-LOW relay ON (LOW = energised). When motion
clears, the relay returns to OFF (HIGH). Relay, motion, and alarm state are
included in the JSON payload.

## 7. MQTT Communication

Both nodes publish JSON telemetry to six topics, three per node:
`safety/env/{data,status,heartbeat}` from env-node and
`safety/security/{data,status,heartbeat}` from sec-node. The nodes are
publish-only clients; they never subscribe. Status messages are published at
QoS 1 and heartbeats at QoS 0; data messages carry the adaptive QoS selected
by each node.

The complete topic and payload reference is in [MQTT](mqtt.md).

## 8. JSON Telemetry

The nodes publish hand-built JSON payloads with self-documented field names,
matching those used in the firmware. env-node data carries `device`,
`temperature`, `humidity`, `light`, `status`, `rssi`, and `qos`; sec-node
data carries `device`, `motion`, `alarm`, `relay`, `rssi`, and `qos`.

Canonical example payloads for every topic are in [MQTT](mqtt.md).

## 9. Adaptive QoS

Each node selects the MQTT QoS at publish time. The decision depends on the
importance of the message (environmental status or motion state) and, for
env-node, on the current Wi-Fi RSSI; a weaker signal delivers a higher QoS to
reduce the risk of message loss.

The complete decision rules are documented in [Adaptive QoS](adaptive-qos.md).

## 10. Local/Offline Operation

The laptop provides everything the system needs:

- a Wi-Fi hotspot the nodes connect to
- a local broker for the MQTT messaging
- a monitoring subscriber

Because the network and broker are both hosted on the laptop, the complete
system runs on a closed local network without any Internet access. The nodes
do not reach any external service. This design keeps the demonstration fully
local and offline-capable.

The ESP32 nodes do not talk to each other directly; all messages flow through
the Mosquitto broker on the laptop.

## 11. Current System Boundaries

Implemented functionality:

| Capability | Status |
| ------------- | ------------ |
| Two ESP32 nodes (env-node, sec-node) | Implemented |
| Sensor monitoring (temperature, humidity, light, motion) | Implemented |
| Relay actuation (active-LOW) | Implemented |
| MQTT publishing | Implemented |
| JSON telemetry | Implemented |
| Adaptive QoS | Implemented |
| Local MQTT broker (Mosquitto) | Implemented |
| Local MQTT monitoring/subscription | Implemented |

The current implementation does not include databases, cloud services,
Node-RED, mobile applications, TLS, authentication, or over-the-air updates.
These are not part of the implemented system and are potential future
extensions only.

## References

Within this repository:

- `firmware/env-node/env-node.ino`
- `firmware/sec-node/sec-node.ino`
- `dashboard/dashboard.py`
- `dashboard/requirements.txt`
- `docs/hardware.md`
- `docs/adaptive-qos.md`
- `docs/mqtt.md`
- `docs/setup-and-run.md`
- `mosquitto-config/mosquitto.conf.example`