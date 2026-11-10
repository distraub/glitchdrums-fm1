#!/usr/bin/env python3
"""Build the native package without requiring the browser/Emscripten toolchain.
Uses JIELI_TOOLCHAIN. All loader, memory and MMIO safety checks remain enabled.
"""
import build

if __name__ == "__main__":
    cfg = build.config()
    build.build_site()
    try:
        notes = build.build_fm1(cfg)
    except build.BuildError:
        # Preserve compiler context in CI when the safety checker refuses a build.
        dis = build.FW / "firmware.dis"
        if dis.exists():
            lines = dis.read_text().splitlines()
            print("Disassembly header:\n" + "\n".join(lines[:20]), flush=True)
            for i, line in enumerate(lines):
                if any(v in line for v in tuple(str(v) for x in (0xffc3ff89, 0xffc56781, 0xffc08000) for v in (x, x-(1<<32))) + ("ffc3ff89", "ffc56781", "ffc08000")):
                    print("\nSafety diagnostic context:\n" + "\n".join(lines[max(0,i-5):i+4]), flush=True)
        raise
    print("Native package built:", "; ".join(notes))
