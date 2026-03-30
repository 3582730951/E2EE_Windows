#!/usr/bin/env python3
from __future__ import annotations

import argparse
import math
import struct
import zlib
from pathlib import Path


def read_png_rgba(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a png: {path}")
    offset = 8
    width = height = 0
    chunks = bytearray()
    while offset < len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        chunk_data = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if chunk_type == b"IHDR":
            width, height = struct.unpack(">II", chunk_data[:8])
        elif chunk_type == b"IDAT":
            chunks.extend(chunk_data)
        elif chunk_type == b"IEND":
            break
    raw = zlib.decompress(bytes(chunks))
    stride = width * 4
    pixels = bytearray()
    row_len = stride + 1
    prev = bytearray(stride)
    for row in range(height):
        chunk = raw[row * row_len : (row + 1) * row_len]
        filter_type = chunk[0]
        scan = bytearray(chunk[1:])
        if filter_type == 1:
            for i in range(stride):
                left = scan[i - 4] if i >= 4 else 0
                scan[i] = (scan[i] + left) & 0xFF
        elif filter_type == 2:
            for i in range(stride):
                scan[i] = (scan[i] + prev[i]) & 0xFF
        elif filter_type == 3:
            for i in range(stride):
                left = scan[i - 4] if i >= 4 else 0
                scan[i] = (scan[i] + ((left + prev[i]) // 2)) & 0xFF
        elif filter_type == 4:
            for i in range(stride):
                left = scan[i - 4] if i >= 4 else 0
                up = prev[i]
                up_left = prev[i - 4] if i >= 4 else 0
                p = left + up - up_left
                pa = abs(p - left)
                pb = abs(p - up)
                pc = abs(p - up_left)
                predictor = left if pa <= pb and pa <= pc else (up if pb <= pc else up_left)
                scan[i] = (scan[i] + predictor) & 0xFF
        prev = scan
        pixels.extend(scan)
    return width, height, bytes(pixels)


def rms_diff(first: bytes, second: bytes) -> float:
    total = 0.0
    for a, b in zip(first, second):
        delta = a - b
        total += delta * delta
    return math.sqrt(total / max(1, len(first)))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--actual", required=True)
    parser.add_argument("--golden", required=True)
    parser.add_argument("--threshold", type=float, default=4.0)
    parser.add_argument("--allow-missing-golden", action="store_true")
    args = parser.parse_args()

    actual = Path(args.actual)
    golden = Path(args.golden)
    if not actual.exists():
        print(f"ERROR: missing actual capture: {actual}")
        return 1
    if not golden.exists():
        if args.allow_missing_golden:
            print(f"Skipping golden diff; missing golden: {golden}")
            return 0
        print(f"ERROR: missing golden capture: {golden}")
        return 1

    actual_w, actual_h, actual_pixels = read_png_rgba(actual)
    golden_w, golden_h, golden_pixels = read_png_rgba(golden)
    if (actual_w, actual_h) != (golden_w, golden_h):
        print(
            "ERROR: golden size mismatch: "
            f"actual={actual_w}x{actual_h} golden={golden_w}x{golden_h}"
        )
        return 1
    diff = rms_diff(actual_pixels, golden_pixels)
    if diff > args.threshold:
        print(f"ERROR: golden diff rms {diff:.2f} exceeds threshold {args.threshold:.2f}")
        return 1
    print(f"Windows smoke golden diff passed with rms {diff:.2f}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
