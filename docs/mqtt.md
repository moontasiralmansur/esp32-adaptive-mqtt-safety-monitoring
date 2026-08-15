# MQTT Communication

## Overview

MQTT is the messaging protocol used by both nodes to report telemetry. Each
node publishes to the local Mosquitto broker; a subscriber on the laptop
observes the traffic.

- Broker: Mosquitto running on the laptop at `192.168.137.1:1883` (see
  [Setup and Run](setup-and-run.md) for broker setup).
- Both ESP32 nodes are **publish-only** clients. They never subscribe to any
  topic; the subscriber only observes the telemetry.
- Broker configuration template: `mosquitto-config/mosquitto.conf.example`
  (laboratory configuration: anonymous, unencrypted, local network only).
- The adaptive QoS design is documented in
  [Adaptive QoS](adaptive-qos.md).

## Topics

| Topic | Publisher | Content | QoS |
| ---------------------- | --------- | ------------------------------------------- | -------- |
| `safety/env/data` | ENV-NODE | Temperature, humidity, light, status, RSSI, QoS | adaptive |
| `safety/env/status` | ENV-NODE | Online notification on connect | 1 |
| `safety/env/heartbeat` | ENV-NODE | Periodic keep-alive with RSSI | 0 |
| `safety/security/data` | SEC-NODE | Motion, alarm, relay state, RSSI, QoS | adaptive |
| `safety/security/status` | SEC-NODE | Online notification on connect | 1 |
| `safety/security/heartbeat` | SEC-NODE | Periodic keep-alive with RSSI | 0 |

## JSON Payloads

The payloads are hand-built by the firmware; field names below are
byte-for-byte as published.

ENV-NODE, `safety/env/data`:

```json
{"device":"ENV-NODE","temperature":28,"humidity":55,"light":512,"status":"SAFE","rssi":-58,"qos":0}
```

ENV-NODE, `safety/env/status`:

```json
{"device":"ENV-NODE","status":"ONLINE"}
```

ENV-NODE, `safety/env/heartbeat`:

```json
{"device":"ENV-NODE","status":"ONLINE","rssi":-58}
```

SEC-NODE, `safety/security/data`, no motion:

```json
{"device":"SEC-NODE","motion":false,"alarm":false,"relay":"OFF","rssi":-61,"qos":0}
```

SEC-NODE, `safety/security/data`, motion detected:

```json
{"device":"SEC-NODE","motion":true,"alarm":true,"relay":"ON","rssi":-61,"qos":2}
```

SEC-NODE, `safety/security/status`:

```json
{"device":"SEC-NODE","status":"ONLINE"}
```

SEC-NODE, `safety/security/heartbeat`:

```json
{"device":"SEC-NODE","status":"ONLINE","rssi":-61}
```

Placeholder values above are examples; actual values depend on the
environment and network conditions.

## QoS Levels Used

- ENV-NODE data: adaptive (0, 1, or 2, based on status and RSSI).
- SEC-NODE data: 0 (no motion) or 2 (motion).
- Status messages: QoS 1.
- Heartbeat messages: QoS 0.

The full rule tables are in [Adaptive QoS](adaptive-qos.md).
`mosquitto_sub` subscribes regardless of the publish QoS.

## Observing Broker Traffic

Check that Mosquitto is installed and on `PATH`:

```powershell
mosquitto -h
mosquitto_sub -h
mosquitto_pub -h
```

If a command is not found, call it by absolute path, for example:

```powershell
& "C:\Program Files\mosquitto\mosquitto_sub.exe" -h
```

Subscribe to all project topics (verbose flag prepends the topic name):

```powershell
mosquitto_sub -h 192.168.137.1 -p 1883 -t "safety/#" -v
```

Subscribe to one node's topics:

```powershell
mosquitto_sub -h 192.168.137.1 -p 1883 -t "safety/env/#" -v
mosquitto_sub -h 192.168.137.1 -p 1883 -t "safety/security/#" -v
```

Send a broker sanity-test message (no node subscribes to it; observe it with
a separate subscriber on `safety/#`):

```powershell
mosquitto_pub -h 192.168.137.1 -p 1883 -t "safety/env/test" -m "hello from laptop"
```