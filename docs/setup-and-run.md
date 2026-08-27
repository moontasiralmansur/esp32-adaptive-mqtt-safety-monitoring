# Setup and Run

Step-by-step guide to set up and run the Adaptive MQTT Safety Monitoring
project in a university laboratory. It assumes you have cloned the GitHub
repository and start from an empty local copy.

The project is a local IoT safety and environment monitoring system with two
ESP32 nodes (`env-node`, `sec-node`) publishing JSON telemetry to a Mosquitto
MQTT broker running on a Windows laptop. It runs inside a single local
network (laptop hotspot) and does not require Internet access.

## 1. Requirements

### Hardware

| Item | Used by |
| ---- | ------- |
| ESP32-S3 development board | env-node |
| ESP32 DevKit | sec-node |
| DHT11 temperature/humidity sensor | env-node |
| LDR / light sensor | env-node |
| 0.96-inch SSD1306 OLED | env-node |
| PIR motion sensor | sec-node |
| 4-channel relay module | sec-node (one channel) |
| USB cables (data) | both nodes |
| Windows laptop | hotspot + broker + monitor |

### Software

| Software | Purpose |
| -------- | ------- |
| Arduino IDE | Compile and upload the firmware |
| ESP32 board support (Arduino core) | Build targets for the ESP32 boards |
| Arduino libraries listed in section 4 | Required third-party libraries |
| Mosquitto MQTT broker | The local MQTT broker |
| PowerShell | Run Mosquitto and the MQTT monitor |

No other software is required for the firmware setup. The optional
Streamlit dashboard requires Python and is described in section 18.

## 2. Project Preparation

Clone or download the repository and open it in a PowerShell window at the
project root. The repository contains:

- `firmware/` - the two Arduino sketches, one per node.
- `mosquitto-config/` - the Mosquitto configuration template.
- `dashboard/` - optional Streamlit dashboard for live MQTT observation.
- `docs/` - architecture, hardware, QoS, MQTT, and this guide.

## 3. Hardware Preparation

Wire each node exactly according to the pinout guide
[Hardware](hardware.md) before uploading any firmware; the sketches read and
drive those specific pins.

Key points:

- The relay channel is active-LOW (`LOW` = ON, `HIGH` = OFF).
- The PIR needs its 30-second stabilization time at startup.

## 4. Arduino IDE Setup

1. Install the Arduino IDE for Windows from the official site.
2. Add the ESP32 board package URL under File > Preferences > Additional
   Boards Manager URLs, then install the ESP32 core from the Boards Manager.
3. Choose the board per node: env-node -> the ESP32-S3 board matching your
   development board; sec-node -> ESP32 Dev Module.
4. Connect the board over USB and select the matching COM port in Tools >
   Port. Install the USB driver if the port does not appear; if both boards
   are connected, verify the correct port (see section 16,
   Troubleshooting).
5. Open the sketch: `firmware/env-node/env-node.ino` or
   `firmware/sec-node/sec-node.ino`. In the Arduino IDE an `.ino` file must
   sit in a folder named like the sketch; the repository already uses that
   layout under `firmware/`.
6. For **env-node**, install these libraries from the Arduino Library Manager
   (no version numbers are pinned by this project):
   - **DHT11** (author Dhruba Saha) - used by `#include <DHT11.h>`
   - **Adafruit GFX Library** - used by `#include <Adafruit_GFX.h>`
   - **Adafruit SSD1306** - used by `#include <Adafruit_SSD1306.h>`
   For **sec-node**, no third-party library is required; the sketch uses the
   native ESP32 MQTT client built into the ESP32 core (`mqtt_client.h`)
   together with the built-in Wi-Fi library.

## 5. Wi-Fi Credential Configuration

The firmware is designed for a local network and does not need Internet
access. The laptop provides the Wi-Fi hotspot that both nodes join.

Both firmware files contain placeholder constants:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

Before uploading, edit the local working copy of each `.ino` file and replace
these placeholders with your own hotspot credentials. The hotspot must be
created on the laptop (Windows Mobile Hotspot mode); the same values can be
used for both nodes.

`YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` are placeholders. Real Wi-Fi
credentials are not part of this repository and must never be committed.

## 6. Mosquitto Setup

Mosquitto must be installed on the laptop. Three different configuration
files exist; do not confuse them:

| File | Role |
| ---- | ---- |
| `C:\Program Files\Mosquitto\mosquitto.conf` | Installed Mosquitto default configuration. **Do not modify it**; it belongs to the installed Mosquitto and is separate from this project. |
| `mosquitto-config/mosquitto.conf.example` | Project template. Read-only reference; never edit it directly. |
| `mosquitto-config/adaptive-mqtt-safety-monitoring.conf` | Local project configuration. You create this file from the template and run the broker with it. |

The local project configuration is ignored by Git (see `.gitignore`).

Mosquitto may already be installed and running as a Windows service. If port
1883 is occupied, the project broker cannot start on the same port. Note
that the service does not necessarily use the project configuration; it runs
with Mosquitto's own installed configuration.

## 7. Create the Project Mosquitto Configuration

In PowerShell, from the project root:

```powershell
cd mosquitto-config
Copy-Item mosquitto.conf.example adaptive-mqtt-safety-monitoring.conf
```

The example template must not be modified directly; edit only the local copy
if needed.

## 8. Start the Broker

```powershell
mosquitto -v -c adaptive-mqtt-safety-monitoring.conf
```

This starts Mosquitto using the **project configuration**, not the default
configuration installed under Program Files. `-v` enables verbose logging;
`-c` points to the project's local configuration file. Keep this terminal
window open while testing the ESP32 nodes. With verbose logging you should
see the broker bind to `192.168.137.1:1883`.

## 9. Verify Port 1883

```powershell
Get-NetTCPConnection -LocalPort 1883 -State Listen
```

Or, if the cmdlet is unavailable:

```powershell
netstat -ano | findstr :1883
```

If port 1883 is already occupied, likely by the Mosquitto Windows service:

1. Find the listening process (the last column is the PID):
   `netstat -ano | findstr :1883`
2. Check what that PID is: `tasklist | findstr <PID>`
3. If it is the Mosquitto service, stop it (do not uninstall Mosquitto and
   do not delete the service):
   `sc.exe stop mosquitto`
4. Start the project broker manually:
   `mosquitto -v -c adaptive-mqtt-safety-monitoring.conf`
5. After the demonstration, the service can be started again:
   `sc.exe start mosquitto`

## 10. Start the MQTT Subscriber

Open a new PowerShell window and subscribe to all project topics. The broker
must already be running (section 8):

```powershell
mosquitto_sub -h 192.168.137.1 -p 1883 -t "safety/#" -v
```

This window displays every message published by the two nodes
(`safety/env/*` and `safety/security/*`). You can also use `safety/env/#` or
`safety/security/#` to subscribe to only one node's topics.

An alternative to `mosquitto_sub` is the optional Streamlit dashboard, which
provides a browser-based live view of the same telemetry (see section 18).

## 11. Upload env-node

1. Connect the ESP32-S3 via USB.
2. Open `firmware/env-node/env-node.ino`.
3. Set Tools > Board to the ESP32-S3 board type.
4. Select the COM port of the ESP32-S3.
5. Click Upload.
6. Open the Serial Monitor and set the baud rate to the one used by the
   firmware: **115200**.
7. Wait for Wi-Fi and MQTT connection (see section 13 for the expected
   startup messages).

Once running:

- DHT11 temperature/humidity and LDR light readings every two seconds.
- OLED displays temperature, humidity, light, status, QoS, and MQTT state.
- MQTT data on `safety/env/data` every five seconds.
- A status message on `safety/env/status` after connecting.
- A heartbeat on `safety/env/heartbeat` every thirty seconds.

## 12. Upload sec-node

1. Connect the ESP32 DevKit via USB.
2. Open `firmware/sec-node/sec-node.ino`.
3. Set Tools > Board to the ESP32 (ESP32 Dev Module).
4. Select the COM port of the DevKit.
5. Click Upload.
6. Open the Serial Monitor at baud rate **115200**.

Allow the PIR to stabilize: the firmware waits 30 seconds during startup
before starting Wi-Fi. Wait for the "PIR ready." message, then for Wi-Fi and
MQTT connection.

Once running:

- PIR motion is read continuously.
- When motion is detected, the active-LOW relay turns ON (`LOW`); when motion
  clears, the relay returns to OFF (`HIGH`).
- MQTT data on `safety/security/data` every three seconds.
- A status message on `safety/security/status` after connecting.
- A heartbeat on `safety/security/heartbeat` every thirty seconds.

## 13. Expected Output

env-node serial monitor:

- `env-node starting...`, `Wi-Fi connected`, `MQTT connected`,
  `Status published.`
- DHT11 errors (if any) report the error code; the node keeps the last good
  value.

sec-node serial monitor:

- `sec-node starting...`, `PIR configured.`, `Relay configured (active
  low).`, `Waiting for the PIR to stabilize...`, `PIR ready.`, then the
  Wi-Fi and MQTT messages.
- `Motion detected.` / `Motion cleared.` on PIR transitions.

## 14. MQTT Verification

With the subscriber running (section 10), `safety/env/data` messages look
like:

```json
{"device":"env-node","temperature":27,"humidity":51,"light":1024,"status":"SAFE","rssi":-58,"qos":0}
```

`safety/security/data` with no motion:

```json
{"device":"sec-node","motion":false,"alarm":false,"relay":"OFF","rssi":-61,"qos":0}
```

When motion is detected:

```json
{"device":"sec-node","motion":true,"alarm":true,"relay":"ON","rssi":-61,"qos":2}
```

Field meanings:

| Field | Meaning |
| ----- | ------- |
| `device` | Node ID (`env-node` / `sec-node`) |
| `temperature` | DHT11 temperature in degrees Celsius |
| `humidity` | DHT11 relative humidity in percent |
| `light` | Raw analog reading of the LDR |
| `status` | Derived environmental state SAFE / WARNING / DANGER |
| `motion` / `alarm` | PIR state; alarm mirrors motion |
| `relay` | Relay state ON / OFF |
| `rssi` | Wi-Fi received signal strength in dBm |
| `qos` | The QoS the firmware selected for this data message |

The complete topic and payload reference is in [MQTT](mqtt.md).

## 15. Demonstrate Adaptive QoS

The project selects the MQTT QoS dynamically at publish time based on the
environmental status, the Wi-Fi RSSI, and the motion state. Simple
observations:

- env-node: SAFE with a good RSSI publishes at QoS 0; raising the
  temperature into the WARNING/DANGER range raises the QoS up to 2.
- sec-node: no motion publishes at QoS 0; motion detected publishes at QoS 2.

The selected QoS is visible in the `qos` field of each data message. The
complete decision rules are in [Adaptive QoS](adaptive-qos.md).

## 16. Troubleshooting

- **ESP32 cannot connect to Wi-Fi**: check that the credential placeholders
  in the `.ino` file were replaced with the correct SSID/password and that
  the laptop hotspot is active before upload. Confirm the hotspot SSID and
  the current band are supported.
- **MQTT connection refused**: verify Mosquitto is running and listening on
  `192.168.137.1:1883` (sections 8 and 9). Verify the ESP32 IP can reach the
  broker on that address.
- **Mosquitto only listening on 127.0.0.1**: the local project configuration
  binds to `192.168.137.1`. If you started Mosquitto without
  `-c adaptive-mqtt-safety-monitoring.conf` it falls back to the default
  localhost-only listener. Check the config path and the bind line.
- **Port 1883 already in use**: another process (often the Mosquitto Windows
  service) owns the port. Diagnose with `netstat -ano | findstr :1883` and
  `tasklist | findstr <PID>`, then stop the service with
  `sc.exe stop mosquitto` (section 9).
- **Wrong COM port**: if the board does not connect or upload fails, try
  each COM port in turn; more than one USB serial device may be present.
  Unplug other boards or select the correct one.
- **Missing Arduino library**: the IDE logs a not-found reference
  (`fatal error: DHT11.h: No such file`). Install the libraries listed in
  section 4 for the env-node. sec-node has no third-party library.
- **PIR false triggering**: room motion, heat sources, or an unstable PIR
  cause false triggers. Keep the PIR pointed away from heat sources and give
  it its 30 s stabilization time.
- **Relay active-LOW behavior**: the relay is active LOW, so the relay is ON
  when its input is set LOW (GPIO LOW). If the relay is ON when you expect
  OFF, check the wiring uses the active-LOW input and that the relay is
  powered per its documentation.
- **DHT11 read errors**: DHT11 reads occasionally report an error code on
  the serial monitor; the node keeps the last good value. An unstable 3.3 V
  supply or loose wiring can cause repeated errors. Verify supply and wiring.
- **MQTT messages not appearing**: confirm the subscriber was started and
  the nodes are connected (check the serial), and that the topics match
  `safety/env/*` or `safety/security/*`. The nodes do not subscribe to
  anything; if nothing is published there is nothing to receive.

## 17. Shutdown

To shut the lab down cleanly:

- Stop the MQTT subscriber: press `Ctrl+C` in the PowerShell window running
  `mosquitto_sub`.
- Stop the Mosquitto broker: press `Ctrl+C` in the window running
  `mosquitto` (with `-v`).
- Stop the ESP32 nodes: disconnect the USB cable, or press the reset/enable
  button; each node stops reading and publishing.

Once all three are stopped the system is off. There is nothing else to clean
up.

## 18. Optional: Streamlit Dashboard

The project includes an optional Streamlit dashboard that displays live MQTT
telemetry in a browser. It is an alternative to the `mosquitto_sub`
command-line subscriber.

### Requirements

- Python 3
- Dependencies listed in `dashboard/requirements.txt` (Streamlit, paho-mqtt)

### Setup

1. Navigate to the `dashboard/` directory.
2. Create and activate a Python virtual environment if desired:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
```

3. Install the dependencies:

```powershell
pip install -r requirements.txt
```

4. Open `dashboard/dashboard.py` and replace the broker address placeholder
   with the actual IP address of your Mosquitto broker. For the standard
   laptop-hosted setup, use `127.0.0.1` if the dashboard runs on the same
   machine as the broker:

```python
MQTT_BROKER_HOST = "127.0.0.1"
```

5. Run the dashboard:

```powershell
streamlit run dashboard.py
```

The dashboard opens in a browser and shows:

- MQTT connection status
- Environmental Node: temperature, humidity, light, status, RSSI, QoS
- Security Node: motion, relay state, RSSI, QoS

The dashboard subscribes to `safety/#` and refreshes automatically. It is
read-only and does not control the ESP32 nodes.

## Documentation links

- [System Architecture](architecture.md)
- [Hardware](hardware.md)
- [Adaptive QoS](adaptive-qos.md)
- [MQTT](mqtt.md)
- [mosquitto.conf.example](../mosquitto-config/mosquitto.conf.example)