#!/usr/bin/env python3
import argparse
import csv
from dataclasses import dataclass

@dataclass
class Frame:
    timestamp_ms: int
    bus: str
    can_id: int
    data: bytes

def load_csv(path):
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            payload = bytes.fromhex(row["data"].replace(" ", ""))
            yield Frame(
                timestamp_ms=int(row["timestamp_ms"]),
                bus=row["bus"].strip().upper(),
                can_id=int(row["id"], 0),
                data=payload,
            )

def decode(frame):
    if frame.bus == "C1" and frame.can_id == 0x4B2 and len(frame.data) >= 4:
        b = frame.data
        oil_raw = ((b[0] & 1) << 7) | ((b[1] >> 1) & 0x7F)
        temp_raw = ((b[2] & 0x3F) << 2) | ((b[3] >> 6) & 0x03)
        return {
            "oil_pressure_bar": oil_raw * 0.1,
            "oil_temperature_c": float(temp_raw),
        }

    if frame.bus == "C1" and frame.can_id == 0x2ED and len(frame.data) >= 7:
        return {"shift_urgency": frame.data[6] & 0x03}

    return None

def main():
    ap = argparse.ArgumentParser(description="AlfaRaceX CSV CAN replay decoder")
    ap.add_argument("capture")
    args = ap.parse_args()

    for frame in load_csv(args.capture):
        decoded = decode(frame)
        if decoded:
            print(frame.timestamp_ms, frame.bus, hex(frame.can_id), decoded)

if __name__ == "__main__":
    main()
