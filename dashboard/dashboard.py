import json
import threading
import time

import paho.mqtt.client as mqtt
import streamlit as st

# MQTT broker address - replace 0.0.0.0 with your broker's actual IP address
# Use 127.0.0.1 if the dashboard and Mosquitto broker run on the same machine
MQTT_BROKER_HOST = "0.0.0.0"
MQTT_BROKER_PORT = 1883

TOPIC_ENV_DATA = "safety/env/data"
TOPIC_ENV_STATUS = "safety/env/status"
TOPIC_ENV_HEARTBEAT = "safety/env/heartbeat"
TOPIC_SEC_DATA = "safety/security/data"
TOPIC_SEC_STATUS = "safety/security/status"
TOPIC_SEC_HEARTBEAT = "safety/security/heartbeat"

ENV_TOPICS = (TOPIC_ENV_DATA, TOPIC_ENV_STATUS, TOPIC_ENV_HEARTBEAT)
SEC_TOPICS = (TOPIC_SEC_DATA, TOPIC_SEC_STATUS, TOPIC_SEC_HEARTBEAT)

REFRESH_SECONDS = 1

@st.cache_resource
def _get_shared_state():
    return {
        "lock": threading.Lock(),
        "connected": False,
        "last_message_topic": None,
        "last_message_time": None,
        "latest": {},
    }


def _badge(text, color):
    return (
        f'<span style="background-color:{color};color:#ffffff;'
        f'padding:2px 12px;border-radius:4px;font-weight:600">{text}</span>'
    )


def _time_str(t):
    return time.strftime("%H:%M:%S", time.localtime(t)) if t else None


def _on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        client.subscribe("safety/#", qos=2)
        with userdata["lock"]:
            userdata["connected"] = True


def _on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
    with userdata["lock"]:
        userdata["connected"] = False


def _on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return
    if not isinstance(payload, dict):
        return
    now = time.time()
    with userdata["lock"]:
        userdata["latest"][msg.topic] = (payload, now)
        userdata["last_message_topic"] = msg.topic
        userdata["last_message_time"] = now


def _run_mqtt_loop(client):
    while True:
        client.connect_async(MQTT_BROKER_HOST, MQTT_BROKER_PORT, keepalive=60)
        client.loop_forever(retry_first_connection=True)
        time.sleep(2)


@st.cache_resource
def _get_mqtt_client():
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id="safety-dashboard",
        userdata=_get_shared_state(),
    )
    client.reconnect_delay_set(min_delay=1, max_delay=5)
    client.on_connect = _on_connect
    client.on_disconnect = _on_disconnect
    client.on_message = _on_message
    threading.Thread(target=_run_mqtt_loop, args=(client,), daemon=True).start()
    return client


def _shared_state():
    return _get_shared_state()


def _render_mqtt_status():
    st.markdown("## MQTT Status")
    shared = _shared_state()
    with shared["lock"]:
        connected = shared["connected"]
        last_topic = shared["last_message_topic"]
        last_time = shared["last_message_time"]

    if connected:
        status_html = _badge("CONNECTED", "#2e7d32")
    else:
        status_html = _badge("DISCONNECTED - RETRYING", "#c62828")

    c1, c2, c3 = st.columns(3)
    c1.markdown(f"**Connection**<br>{status_html}", unsafe_allow_html=True)
    c2.markdown(
        f"**Broker**<br>`mqtt://{MQTT_BROKER_HOST}:{MQTT_BROKER_PORT}`",
        unsafe_allow_html=True,
    )
    if last_time:
        c3.markdown(
            f"**Last message**<br>{last_topic} — {_time_str(last_time)}",
            unsafe_allow_html=True,
        )
    else:
        c3.markdown("**Last message**<br>Waiting for data...", unsafe_allow_html=True)


def _render_node_card(shared, title, topics, data_topic):
    st.markdown(f"### {title}")
    with shared["lock"]:
        msgs = [shared["latest"][t] for t in topics if t in shared["latest"]]
    if not msgs:
        st.info("Waiting for data...")
        return

    last_update = max(ts for _, ts in msgs)
    st.caption(f"Last update: {_time_str(last_update)}")

    payload = None
    if data_topic in shared["latest"]:
        payload = shared["latest"][data_topic][0]

    if payload is None:
        st.info("Waiting for data...")
        return

    m1, m2, m3, m4 = st.columns(4)
    return m1, m2, m3, m4, payload


def _render_env_card():
    metrics = _render_node_card(_shared_state(), "Environmental Node", ENV_TOPICS, TOPIC_ENV_DATA)
    if metrics is None:
        return
    m1, m2, m3, m4, payload = metrics

    temp = payload.get("temperature")
    hum = payload.get("humidity")
    light = payload.get("light")
    status = payload.get("status")
    rssi = payload.get("rssi")
    qos = payload.get("qos")

    m1.metric("Temperature", f"{temp} °C" if temp is not None else "—")
    m2.metric("Humidity", f"{hum} %" if hum is not None else "—")
    m3.metric("Light (raw LDR)", f"{light}" if light is not None else "—")
    m4.metric("RSSI", f"{rssi} dBm" if rssi is not None else "—")

    status_colors = {"SAFE": "#2e7d32", "WARNING": "#ef6c00", "DANGER": "#c62828"}
    status_html = _badge(status, status_colors.get(status, "#757575")) if status else _badge("—", "#757575")

    s1, s2, s3 = st.columns(3)
    s1.markdown(f"**Environmental Status**<br>{status_html}", unsafe_allow_html=True)
    s2.markdown(f"**Selected MQTT QoS**<br>{_badge(f'QoS {qos}', '#1565c0') if qos is not None else '—'}", unsafe_allow_html=True)
    s3.markdown(f"**Device Status**<br>{_badge('ONLINE', '#2e7d32')}", unsafe_allow_html=True)


def _render_sec_card():
    metrics = _render_node_card(_shared_state(), "Security Node", SEC_TOPICS, TOPIC_SEC_DATA)
    if metrics is None:
        return
    m1, m2, m3, m4, payload = metrics

    motion = payload.get("motion")
    relay = payload.get("relay")
    rssi = payload.get("rssi")
    qos = payload.get("qos")

    if motion is True:
        motion_html = _badge("DETECTED", "#c62828")
    elif motion is False:
        motion_html = _badge("CLEAR", "#2e7d32")
    else:
        motion_html = _badge("—", "#757575")

    if relay == "ON":
        relay_html = _badge("ON", "#c62828")
    elif relay == "OFF":
        relay_html = _badge("OFF", "#2e7d32")
    else:
        relay_html = _badge("—", "#757575")

    m1.markdown(f"**Motion**<br>{motion_html}", unsafe_allow_html=True)
    m2.markdown(f"**Relay**<br>{relay_html}", unsafe_allow_html=True)
    m3.metric("RSSI", f"{rssi} dBm" if rssi is not None else "—")
    m4.markdown(f"**Selected MQTT QoS**<br>{_badge(f'QoS {qos}', '#1565c0') if qos is not None else '—'}", unsafe_allow_html=True)

    s1, s2 = st.columns(2)
    s1.markdown(f"**Device Status**<br>{_badge('ONLINE', '#2e7d32')}", unsafe_allow_html=True)


def main():
    st.set_page_config(page_title="Adaptive MQTT Safety Monitoring", layout="wide")
    _get_mqtt_client()

    st.title("Adaptive MQTT Safety Monitoring")

    _render_mqtt_status()

    col1, col2 = st.columns(2)
    with col1:
        _render_env_card()
    with col2:
        _render_sec_card()

    st.markdown(
        '<p style="text-align:center;color:#9e9e9e;font-size:0.85rem;'
        'margin-top:2rem;">Developed by Moontasir Al Mansur at ULAB</p>',
        unsafe_allow_html=True,
    )

    time.sleep(REFRESH_SECONDS)
    st.rerun()


main()
