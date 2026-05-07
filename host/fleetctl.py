#!/usr/bin/env python3
"""fleetctl — Python CLI for the FleetOS POC.

Subcommands:
    status                            watch fleet/+/{alive,status}
    cmd     <device-id> <payload>     publish to fleet/<device-id>/cmd
    upload  <local-bin> [<key>]       upload .bin to R2, print public URL
    ota     <device-id> <version>     full pipeline: ensure firmware-builds/
                                      fleetos-v<version>.bin is on R2, then
                                      publish its URL to <device-id>/cmd

Environment (via host/.env):
    HIVEMQ_HOST / HIVEMQ_PORT / HIVEMQ_USERNAME / HIVEMQ_PASSWORD
    R2_ACCOUNT_ID / R2_BUCKET / R2_ACCESS_KEY_ID / R2_SECRET_ACCESS_KEY
    R2_PUBLIC_URL_PREFIX

R2 vars are only required for `upload` and `ota`. `status` and `cmd` work
without them.
"""

from __future__ import annotations

import argparse
import json
import os
import ssl
import sys
import time
from pathlib import Path

import paho.mqtt.client as mqtt
from dotenv import load_dotenv


# ----------------------------------------------------------------------------
# config
# ----------------------------------------------------------------------------

def _load_mqtt_env() -> dict:
    load_dotenv()
    cfg = {
        "host": os.environ.get("HIVEMQ_HOST", ""),
        "port": int(os.environ.get("HIVEMQ_PORT", "8883")),
        "username": os.environ.get("HIVEMQ_USERNAME", ""),
        "password": os.environ.get("HIVEMQ_PASSWORD", ""),
    }
    missing = [k for k, v in cfg.items() if not v]
    if missing:
        sys.exit(f"missing required MQTT env vars: {', '.join(missing)} (see .env.example)")
    return cfg


def _load_r2_env() -> dict:
    load_dotenv()
    cfg = {
        "account_id":     os.environ.get("R2_ACCOUNT_ID", ""),
        "bucket":         os.environ.get("R2_BUCKET", ""),
        "access_key_id":  os.environ.get("R2_ACCESS_KEY_ID", ""),
        "secret_key":     os.environ.get("R2_SECRET_ACCESS_KEY", ""),
        "public_prefix":  os.environ.get("R2_PUBLIC_URL_PREFIX", "").rstrip("/"),
    }
    missing = [k for k, v in cfg.items() if not v]
    if missing:
        sys.exit(
            f"missing required R2 env vars: {', '.join(missing)} (see .env.example)\n"
            "Create an R2 API token at: Cloudflare → R2 → Manage API Tokens"
        )
    return cfg


def _make_mqtt_client(cfg: dict, client_id: str) -> mqtt.Client:
    client = mqtt.Client(
        client_id=client_id,
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
    )
    client.username_pw_set(cfg["username"], cfg["password"])
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS_CLIENT)
    return client


def _make_r2_client(cfg: dict):
    # boto3 is only imported when R2 commands are used so users without an R2
    # token can still run status/cmd without installing boto3.
    import boto3
    from botocore.config import Config
    return boto3.client(
        "s3",
        endpoint_url=f"https://{cfg['account_id']}.r2.cloudflarestorage.com",
        aws_access_key_id=cfg["access_key_id"],
        aws_secret_access_key=cfg["secret_key"],
        # R2 does not use AWS regions; "auto" is the documented value.
        region_name="auto",
        config=Config(signature_version="s3v4"),
    )


# ----------------------------------------------------------------------------
# subcommands
# ----------------------------------------------------------------------------

def cmd_status(cfg: dict) -> int:
    client = _make_mqtt_client(cfg, client_id="fleetctl-status")

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code != 0:
            print(f"connect failed: {reason_code}", file=sys.stderr)
            return
        print(f"connected to {cfg['host']}, subscribing fleet/+/alive + fleet/+/status")
        client.subscribe([("fleet/+/alive", 1), ("fleet/+/status", 1)])

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
    client = _make_mqtt_client(cfg, client_id="fleetctl-cmd")
    client.connect(cfg["host"], cfg["port"], keepalive=60)
    topic = f"fleet/{device_id}/cmd"
    info = client.publish(topic, payload, qos=1)
    info.wait_for_publish(timeout=10)
    print(f"published to {topic}: {payload}")
    client.disconnect()
    return 0


def cmd_upload(local_path: Path, key: str | None) -> tuple[int, str]:
    """Upload a local file to R2. Returns (exit_code, public_url)."""
    if not local_path.exists():
        sys.exit(f"file not found: {local_path}")
    r2 = _load_r2_env()
    s3 = _make_r2_client(r2)
    object_key = key or local_path.name

    size = local_path.stat().st_size
    print(f"uploading {local_path} ({size:,} bytes) → r2://{r2['bucket']}/{object_key}")
    s3.upload_file(
        str(local_path),
        r2["bucket"],
        object_key,
        ExtraArgs={"ContentType": "application/octet-stream"},
    )
    public_url = f"{r2['public_prefix']}/{object_key}"
    print(f"public URL: {public_url}")
    return 0, public_url


def cmd_ota(cfg: dict, device_id: str, version: str) -> int:
    """Find firmware-builds/fleetos-v<version>.bin, upload, then publish.

    The whole demo in one command. Looks two levels up to find the project
    root (this script lives at host/fleetctl.py).
    """
    repo_root = Path(__file__).resolve().parent.parent
    bin_path = repo_root / "firmware-builds" / f"fleetos-v{version}.bin"
    if not bin_path.exists():
        sys.exit(
            f"no binary at {bin_path}\n"
            f"build first:  ./tools/release.sh {version} <half-period-ms>"
        )

    _, url = cmd_upload(bin_path, key=bin_path.name)
    payload = json.dumps({"url": url})
    return cmd_send(cfg, device_id, payload)


# ----------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------

def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="fleetctl — FleetOS host CLI")
    sub = parser.add_subparsers(dest="action", required=True)

    sub.add_parser("status", help="watch fleet/+/{alive,status}")

    p_cmd = sub.add_parser("cmd", help="publish a command to a specific device")
    p_cmd.add_argument("device_id", help="e.g. ece334a40d4c")
    p_cmd.add_argument("payload", help="raw payload string (JSON for OTA)")

    p_upload = sub.add_parser("upload", help="upload a local .bin to R2")
    p_upload.add_argument("file", type=Path)
    p_upload.add_argument("key", nargs="?", default=None,
                          help="object key (defaults to filename)")

    p_ota = sub.add_parser("ota",
        help="end-to-end: upload firmware-builds/fleetos-v<version>.bin and push URL")
    p_ota.add_argument("device_id", help="e.g. ece334a40d4c")
    p_ota.add_argument("version", help="e.g. 3.0.2")

    args = parser.parse_args(argv)

    if args.action == "status":
        return cmd_status(_load_mqtt_env())
    if args.action == "cmd":
        return cmd_send(_load_mqtt_env(), args.device_id, args.payload)
    if args.action == "upload":
        rc, _ = cmd_upload(args.file, args.key)
        return rc
    if args.action == "ota":
        return cmd_ota(_load_mqtt_env(), args.device_id, args.version)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
