#!/usr/bin/env python3
"""Minimal serial-side reproducer for the CFG timeout incident.

Usage examples:
  python scripts/repro_cfg_timeout.py --port /dev/cu.wchusbserial5B5E0200881 --command get_capabilities
  python scripts/repro_cfg_timeout.py --port /dev/cu.wchusbserial5B5E0200881 --command get_capabilities --wait-for-boot
  python scripts/repro_cfg_timeout.py --port /dev/cu.wchusbserial5B5E0200881 --command persist --wait-for-boot
"""

from __future__ import annotations

import argparse
import json
import sys
import time

import serial


BOOT_MARKERS = (
    b"Returned from app_main()",
    b"USB host stack ready",
)


def build_request(command: str, request_id: int) -> str:
    if command == "get_capabilities":
        payload = {}
    elif command == "persist":
        payload = {
            "mapping_document": {
                "version": 1,
                "global": {
                    "scale": 1,
                    "deadzone": 0.08,
                    "clamp_min": -1,
                    "clamp_max": 1,
                },
                "axes": [
                    {
                        "target": "move_x",
                        "source_index": 0,
                        "scale": 1,
                        "deadzone": 0.08,
                        "invert": False,
                    }
                ],
                "buttons": [
                    {
                        "target": "action_a",
                        "source_index": 0,
                    }
                ],
            },
            "profile_id": 1,
        }
    else:
        raise ValueError(f"unsupported command: {command}")

    envelope = {
        "protocol_version": 2,
        "request_id": request_id,
        "command": f"config.{command}",
        "payload": payload,
        "integrity": "CFG1",
    }
    return "@CFG:" + json.dumps(envelope, separators=(",", ":")) + "\n"


def read_until(ser: serial.Serial, deadline: float) -> bytes:
    chunks: list[bytes] = []
    while time.time() < deadline:
        chunk = ser.read(4096)
        if chunk:
            chunks.append(chunk)
            sys.stdout.buffer.write(chunk)
            sys.stdout.flush()
    return b"".join(chunks)


def read_until_boot_complete(ser: serial.Serial, deadline: float) -> tuple[bytes, bool]:
    chunks: list[bytes] = []
    while time.time() < deadline:
        chunk = ser.read(4096)
        if chunk:
            chunks.append(chunk)
            sys.stdout.buffer.write(chunk)
            sys.stdout.flush()
            combined = b"".join(chunks)
            if any(marker in combined for marker in BOOT_MARKERS):
                return combined, True
    return b"".join(chunks), False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument(
        "--command",
        choices=("get_capabilities", "persist"),
        default="get_capabilities",
    )
    parser.add_argument("--baud-rate", type=int, default=115200)
    parser.add_argument("--request-id", type=int, default=9401)
    parser.add_argument("--wait-for-boot", action="store_true")
    parser.add_argument("--boot-timeout", type=float, default=8.0)
    parser.add_argument("--response-timeout", type=float, default=4.0)
    parser.add_argument("--settle-delay", type=float, default=0.5)
    args = parser.parse_args()

    frame = build_request(args.command, args.request_id)
    summary: dict[str, object] = {
        "port": args.port,
        "command": f"config.{args.command}",
        "request_id": args.request_id,
        "frame_len": len(frame),
        "wait_for_boot": args.wait_for_boot,
    }

    print(f"OPEN {args.port}")
    ser = serial.Serial(
        port=args.port,
        baudrate=args.baud_rate,
        timeout=0.2,
        write_timeout=1,
    )
    try:
        try:
            ser.setDTR(False)
            ser.setRTS(False)
            summary["signals_deasserted"] = True
        except Exception as exc:  # pragma: no cover - hardware dependent
            print(f"SIGNAL_WARN {exc}")
            summary["signals_deasserted"] = False

        boot_bytes = b""
        boot_complete = False
        if args.wait_for_boot:
            boot_bytes, boot_complete = read_until_boot_complete(
                ser, time.time() + args.boot_timeout
            )
            print(f"\nBOOT_PHASE_DONE {boot_complete}")
            time.sleep(args.settle_delay)
            ser.reset_input_buffer()
        summary["boot_bytes"] = len(boot_bytes)
        summary["boot_complete"] = boot_complete

        ser.write(frame.encode())
        ser.flush()
        print("REQUEST_SENT")

        response_bytes = read_until(ser, time.time() + args.response_timeout)
        print("\nRESPONSE_PHASE_DONE")
        summary["response_bytes"] = len(response_bytes)
        summary["has_cfg_frame"] = b"@CFG:" in response_bytes
        print("SUMMARY " + json.dumps(summary, sort_keys=True))
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    raise SystemExit(main())
