#!/usr/bin/env python3
"""Render actual big-endian RGB565 LCD dumps as PNG/APNG (stdlib only)."""
import argparse
from pathlib import Path
import struct
import zlib


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def rgb_frame(data):
    rgb = bytearray()
    for i in range(0, len(data), 2):
        c = data[i] << 8 | data[i + 1]
        rgb.extend((((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31))
    return b''.join(b'\0' + rgb[y * 720:(y + 1) * 720] for y in range(240))


def render(source, destination, fps=20):
    raw = Path(source).read_bytes()
    stride = 240 * 240 * 2
    if not raw or len(raw) % stride:
        raise ValueError('Expected one or more complete 240x240 RGB565 frames')
    frames = len(raw) // stride
    result = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 240, 240, 8, 2, 0, 0, 0))
    if frames > 1:
        result += chunk(b'acTL', struct.pack('>II', frames, 0))
    sequence = 0
    for n in range(frames):
        if frames > 1:
            result += chunk(b'fcTL', struct.pack('>IIIIIHHBB', sequence, 240, 240, 0, 0, 1, fps, 0, 0))
            sequence += 1
        compressed = zlib.compress(rgb_frame(raw[n * stride:(n + 1) * stride]))
        if n == 0:
            result += chunk(b'IDAT', compressed)
        else:
            result += chunk(b'fdAT', struct.pack('>I', sequence) + compressed)
            sequence += 1
    Path(destination).write_bytes(result + chunk(b'IEND', b''))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source')
    parser.add_argument('destination')
    parser.add_argument('--fps', type=int, default=20)
    args = parser.parse_args()
    render(args.source, args.destination, args.fps)
