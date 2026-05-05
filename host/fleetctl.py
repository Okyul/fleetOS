#!/usr/bin/env python3
"""fleetctl — minimal Python CLI for the FleetOS POC.

Connects to HiveMQ Cloud over TLS using credentials in `.env` (or env vars)
and either watches devices come online (`status`) or pushes a command to a
specific device (`cmd`).

Usage:
    python fleetctl.py status                       # watch fleet/+/alive
    python fleetctl.py cmd <device-id> <payload>    # publish to fleet/<id>/cmd

Environment variables (or .env in this directory):
    HIVEMQ_HOST       cluster hostname, e.g. 'abc123.s1.eu.hivemq.cloud'
    HIVEMQ_PORT       broker TLS port, default 8883
    HIVEMQ_USERNAME   access credential username
    HIVEMQ_PASSWORD   access credential password

TLS: paho's `tls_set()` with no ca_certs argument uses the system default
CA bundle. HiveMQ Cloud's certs chain to public roots so this Just Works on
Linux/Mac/Windows.
"""

from __future__ import annotations

import argparse
import json
import os
import ssl
import sys
import time

import paho.mqtt.client as mqtt
from dotenv import load_dotenv


def _load_env() -> dict[str, str]:
    load_dotenv()  # picks up host/.env if present
    cfg = {
        "host": os.environ.get("HIVEMQ_HOST", ""),
        "port": int(os.environ.get("HIVEMQ_PORT", "8883")),
        "username": os.environ.get("HIVEMQ_USERNAME", ""),
        "password": os.environ.get("HIVEMQ_PASSWORD", ""),
    }
    missing = [k for k, v in cfg.items() if not v]
    if missing:
        sys.exit(f"missing required env vars: {', '.join(missing)} (see .env.example)")
    return cfg


def _make_client(cfg: dict, client_id: str) -> mqtt.Client:
    client = mqtt.Client(
        client_id=client_id,
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
    )
    client.username_pw_set(cfg["username"], cfg["password"])
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS_CLIENT)
    return client


def cmd_status(cfg: dict) -> int:
    """Subscribe to fleet/+/alive, print messages as devices report in."""
    client = _make_client(cfg, client_id="fleetctl-status")

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code != 0:
            print(f"connect failed: {reason_code}", file=sys.stderr)
            return
        print(f"connected to {cfg['host']}, subscribing fleet/+/alive")
        client.subscribe("fleet/+/alive", qos=1)

    def on_message(client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8", errors="replace"))
            pretty = json.dumps(payload, indent=None, separators=(",", ":"))
        except Exception:
            pretty = msg.payload.decode("utf-8", errors="replace")
        ts = time.strftime("%H:%M:%S")
        retained = " [retained]" if msg.retain else ""
        print(f"[{ts}] {msg.topic}{retained} {pretty}")

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(cfg["host"], cfg["port"], keepalive=60)
    print("watching... (ctrl-c to exit)")
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nbye")
    return 0


def cmd_send(cfg: dict, device_id: str, payload: str) -> int:
    """Publish payload to fleet/<device_id>/cmd. Used Day 3 to push OTA URLs."""
    client = _make_client(cfg, client_id="fleetctl-cmd")
    client.connect(cfg["host"], cfg["port"], keepalive=60)
    topic = f"fleet/{device_id}/cmd"
    info = client.publish(topic, payload, qos=1)
    info.wait_for_publish(timeout=10)
    print(f"published to {topic}: {payload}")
    client.disconnect()
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="fleetctl — FleetOS host CLI")
    sub = parser.add_subparsers(dest="action", required=True)

    sub.add_parser("status", help="watch devices report in on fleet/+/alive")

    p_cmd = sub.add_parser("cmd", help="publish a command to a specific device")
    p_cmd.add_argument("device_id", help="e.g. ece334a40d4c (12 lowercase hex chars)")
    p_cmd.add_argument("payload", help="raw payload string (Day 3+ uses JSON with a 'url' field)")

    args = parser.parse_args(argv)
    cfg = _load_env()

    if args.action == "status":
        return cmd_status(cfg)
    if args.action == "cmd":
        return cmd_send(cfg, args.device_id, args.payload)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
