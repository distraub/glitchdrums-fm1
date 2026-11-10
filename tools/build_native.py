#!/usr/bin/env python3
"""Build the native package without requiring the browser/Emscripten toolchain.
Uses JIELI_TOOLCHAIN. All loader, memory and MMIO safety checks remain enabled.
"""
import build

if __name__ == "__main__":
    cfg = build.config()
    build.build_site()
    notes = build.build_fm1(cfg)
    print("Native package built:", "; ".join(notes))
