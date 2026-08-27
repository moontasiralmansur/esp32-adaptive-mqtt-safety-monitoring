# MQTT Communication

## Overview

MQTT is the messaging protocol used by both nodes to report telemetry. Each
node publishes to the local Mosquitto broker; the traffic can be observed
either through the `mosquitto_sub` command-line subscriber or the optional
Streamlit dashboard (see [Setup and Run](setup-and-run.md)).

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
| `safety/env/data` | env-node | Temperature, humidity, light, status, RSSI, QoS | adaptive |
| `safety/env/status` | env-node | Online notification on connect | 1 |
| `safety/env/heartbeat` | env-node | Periodic keep-alive with RSSI | 0 |
| `safety/security/data` | sec-node | Motion, alarm, relay state, RSSI, QoS | adaptive |
| `safety/security/status` | sec-node | Online notification on connect | 1 |
| `safety/security/heartbeat` | sec-node | Periodic keep-alive with RSSI | 0 |

## JSON Payloads

The payloads are hand-built by the firmware; field names below are
byte-for-byte as published.

env-node, `safety/env/data`:

```json
{"device":"env-node","temperature":28,"humidity":55,"light":512,"status":"SAFE","rssi":-58,"qos":0}
```

env-node, `safety/env/status`:

```json
{"device":"env-node","status":"ONLINE"}
```

env-node, `safety/env/heartbeat`:

```json
{"device":"env-node","status":"ONLINE","rssi":-58}
```

sec-node, `safety/security/data`, no motion:

```json
{"device":"sec-node","motion":false,"alarm":false,"relay":"OFF","rssi":-61,"qos":0}
```

sec-node, `safety/security/data`, motion detected:

```json
{"device":"sec-node","motion":true,"alarm":true,"relay":"ON","rssi":-61,"qos":2}
```

sec-node, `safety/security/status`:

```json
{"device":"sec-node","status":"ONLINE"}
```

sec-node, `safety/security/heartbeat`:

```json
{"device":"sec-node","status":"ONLINE","rssi":-61}
```

Placeholder values above are examples; actual values depend on the
environment and network conditions.

## QoS Levels Used

- env-node data: adaptive (0, 1, or 2, based on status and RSSI).
- sec-node data: 0 (no motion) or 2 (motion).
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