# Adaptive MQTT QoS

CSE4418 - Internet of Things Lab
University of Liberal Arts Bangladesh (ULAB)

## 1. Introduction

The two ESP32 nodes in this project do not use one fixed QoS level for every
telemetry message. Instead, each node selects the MQTT QoS at publish time
based on the current situation. The selection is a rule-based mechanism that
combines:

- the importance of the message (environmental status or security event),
- the environmental condition (SAFE / WARNING / DANGER),
- the security event (motion detected or not),
- the measured Wi-Fi signal strength (RSSI), and
- the MQTT QoS level used for that publication.

The goal is to spend a higher delivery guarantee only on messages that matter
more. The system does not measure packet loss or latency, and it does not base
its decisions on any network statistics beyond the RSSI threshold.

## 2. MQTT QoS Levels Used

The project uses all three standard MQTT quality-of-service levels:

| Level | Name | Meaning used in this project |
| ----- | ---- | ---------------------------- |
| QoS 0 | At most once | The broker attempts one delivery, no confirmation. Sufficient for routine, non-critical telemetry. |
| QoS 1 | At least once | The broker retries until it receives an acknowledgement. The message is guaranteed to arrive at least once; used for warning and medium-importance messages. |
| QoS 2 | Exactly once | A four-handshake confirms delivery exactly once. Used for the most critical environmental and security events. |

This project does not rely on quality guarantees to control behavior: the
subscriber only observes the data. QoS selection is about protecting the
delivery of important messages inside a single-hop local network, not about a
full MQTT textbook trade-off analysis.

## 3. env-node QoS Decision

The environment node selects the QoS using this rule in firmware
(`determineQoS()` in `firmware/env-node/env-node.ino`):

| Environment status | Condition | QoS |
| ------------------ | --------- | --- |
| DANGER | (any RSSI) | 2 |
| WARNING | RSSI < -75 dBm | 2 |
| WARNING | RSSI >= -75 dBm | 1 |
| SAFE | RSSI >= -75 dBm | 0 |
| SAFE | RSSI < -75 dBm | 1 |

Note the threshold is strictly `rssi < -75`. Equal to or stronger than
`-75` dBm falls on the "strong" side.

## 4. Environmental Status Classification

The status classification in the firmware (`determineStatus()`) runs before
the QoS decision:

```
Classify (from temperature and humidity):
    temperature >= 38°C        -> DANGER
    temperature >= 30°C  OR  humidity >= 80%   -> WARNING
    otherwise                  -> SAFE
```

Specifically for env-node the DANGER condition is the strongest:
`temperature >= 38`, else `temperature >= 30 || humidity >= 80`, else SAFE.

Status classification is always performed first; the derived status is then
used alone or together with RSSI to choose the QoS.

## 5. env-node Decision Flow

```
Sensor readings (DHT11 + LDR)
        |
        v
Determine environmental status
        |
        +---- DANGER ------> QoS 2
        |
        +---- WARNING -----> Check WiFi RSSI
        |                       |
        |                       +-- RSSI < -75 dBm --> QoS 2
        |                       |
        |                       +-- RSSI >= -75 dBm -> QoS 1
        |
        +---- SAFE ---------> Check WiFi RSSI
                                |
                                +-- RSSI >= -75 dBm -> QoS 0
                                |
                                +-- RSSI <  -75 dBm -> QoS 1
```

## 6. sec-node QoS Decision

The security node uses a simpler decision based purely on motion state
(`determineQoS()` in `firmware/sec-node/sec-node.ino`):

| Condition | QoS |
| --------- | --- |
| Motion detected | 2 |
| No motion | 0 |

In the project's rule-based policy, motion is treated as a critical security
event: an intruder detection must reach the observation point with the
strongest delivery guarantee, so motion publishes at QoS 2. Routine
no-motion reports are ordinary telemetry and use QoS 0.

This is the project's chosen policy. It is not claimed to be a universally
optimal QoS strategy.

## 7. Security Event Flow

Motion detected:

```
PIR
 |
 v
Motion detected
 |
 v
Alarm = true
 |
 v
Relay = ON
 |
 v
JSON payload qos = 2
 |
 v
MQTT publish (safety/security/data)
```

No-motion state:

```
PIR
 |
 v
No motion
 |
 v
Alarm = false
 |
 v
Relay = OFF
 |
 v
JSON payload qos = 0
 |
 v
MQTT publish (safety/security/data)
```

## 8. Fixed QoS Messages

Not every message is adaptive. Two message types always use a fixed QoS:

| Message    | Topic                      | QoS |
| ---------- | -------------------------- | --- |
| Status     | `safety/env/status`, `safety/security/status` | 1 |
| Heartbeat  | `safety/env/heartbeat`, `safety/security/heartbeat` | 0 |

These two are separated from the adaptive data messages because:

- A status message confirms the node is online after an MQTT connect. It
  should not be silently lost, so it is sent at QoS 1 (at least once).
- A heartbeat is constant, repeated every time, and will be replaced by the
  next heartbeat a short time later. It carries no urgent information and is
  sent at QoS 0 (at most once).

Both are therefore determined by message type, not by the QoS policy applied
to the sensor data messages.

## 9. JSON Examples

The data payload published by each node includes a field `"qos"` that records
the QoS level the firmware selected for that exact publication. QoS is applied
at the MQTT layer and also reported in the JSON for monitoring purposes.

env-node example (`safety/env/data`, WARNING + strong RSSI):

```json
{
  "device": "env-node",
  "temperature": 31,
  "humidity": 66,
  "light": 4095,
  "status": "WARNING",
  "rssi": -45,
  "qos": 1
}
```

sec-node example (`safety/security/data`, motion detected):

```json
{
  "device": "sec-node",
  "motion": true,
  "alarm": true,
  "relay": "ON",
  "rssi": -24,
  "qos": 2
}
```

## 10. Example State Transitions

Using the exact -75 dBm boundary from the firmware:

| # | Condition | QoS |
| --| --------------------------- | --- |
| 1 | SAFE + RSSI >= -75 dBm | 0 |
| 2 | SAFE + RSSI < -75 dBm | 1 |
| 3 | WARNING + RSSI >= -75 dBm | 1 |
| 4 | WARNING + RSSI < -75 dBm | 2 |
| 5 | DANGER (any RSSI) | 2 |
| 6 | No motion | 0 |
| 7 | Motion detected | 2 |

## 11. Design Rationale

The project's reasoning for adaptive QoS:

- Ordinary telemetry (routine sensor readings) can be delivered at a lower
  QoS: if one normal reading is lost, the next one is available.
- Warning information is a step above normal and uses a higher delivery
  guarantee than normal telemetry.
- Critical environmental conditions (DANGER) get QoS 2 so the important
  message is not lost.
- Critical security events (motion) get QoS 2 for the same reason.
- Weak signal strength (RSSI) can raise the QoS for environmental telemetry
  so an important message is not lost when the link is poor.

This is a rule-based adaptive QoS mechanism implemented specifically for this
laboratory project. It is deterministic: the same conditions always map to
the same QoS.

## 12. Current Limitations

The current implementation does NOT perform:

- packet-loss measurement,
- latency measurement,
- throughput measurement,
- congestion detection,
- dynamic broker-side QoS optimization,
- machine-learning-based QoS prediction, or
- any automatic network-quality scoring beyond the single RSSI threshold.

The only network interface QoS input is the Wi-Fi RSSI at the time of the
publish. The mechanism is a decision table; none of the above features exist
in the firmware.

## 13. Possible Future Improvements

For reference and not implemented:

- packet-loss-aware QoS selection,
- latency-aware QoS selection,
- a more detailed network-quality scoring,
- historical data analysis,
- additional sensor nodes,
- authenticated MQTT,
- TLS,
- a visualization dashboard.

These are listed as future directions only; none are implemented in the
current project.

## Source of Truth

- `firmware/env-node/env-node.ino` - `determineStatus()`, `determineQoS()`
- `firmware/sec-node/sec-node.ino` - `determineQoS()`
- `docs/mqtt.md` - topics and JSON payloads