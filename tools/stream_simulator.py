#!/usr/bin/env python3
"""
stream_simulator.py — Enhanced EMG/EDA UART data simulator.

Outputs frames to stdout in the format:
    $EMG,EMG=0.72,EDA=0.31,TS=123456*

Supports multiple scenarios:
    normal   — low EMG, steady EDA (normal driving)
    braking  — high sustained EMG (emergency braking)
    noisy    — random noise injection, missing fields
    fault    — malformed frames and data gaps

Usage:
    python3 tools/stream_simulator.py | ./build/sim_emg_brake
    python3 tools/stream_simulator.py --scenario braking --duration 30
    python3 tools/stream_simulator.py --serial COM3
"""

import sys
import time
import random
import math
import argparse

try:
    import serial
except ImportError:
    serial = None


# ── Configuration ────────────────────────────────────────────────────

DEFAULT_RATE_HZ = 20       # Matches project spec (20 Hz simulation rate)
DEFAULT_DURATION = 60      # seconds
DEFAULT_BAUD = 9600


# ── Scenario generators ─────────────────────────────────────────────

def scenario_normal(t):
    """Normal driving — low EMG baseline with slight noise."""
    emg = 0.05 + 0.03 * math.sin(2 * math.pi * 0.5 * t) + random.gauss(0, 0.01)
    eda = 0.20 + 0.05 * math.sin(2 * math.pi * 0.1 * t) + random.gauss(0, 0.01)
    return max(0.0, emg), max(0.0, eda), True


def scenario_braking(t):
    """
    Emergency braking sequence:
        0–5s    normal driving
        5–6s    ramp up (muscle contraction begins)
        6–10s   sustained high EMG (braking intent)
        10–12s  ramp down (foot releasing)
        12–20s  return to normal
        20–25s  second braking event
        25+     normal
    """
    if t < 5.0:
        emg = 0.05 + random.gauss(0, 0.01)
    elif t < 6.0:
        progress = (t - 5.0) / 1.0
        emg = 0.05 + 0.65 * progress + random.gauss(0, 0.02)
    elif t < 10.0:
        emg = 0.70 + 0.10 * math.sin(2 * math.pi * 2 * t) + random.gauss(0, 0.03)
    elif t < 12.0:
        progress = (t - 10.0) / 2.0
        emg = 0.70 - 0.65 * progress + random.gauss(0, 0.02)
    elif t < 20.0:
        emg = 0.05 + random.gauss(0, 0.01)
    elif t < 21.0:
        progress = (t - 20.0) / 1.0
        emg = 0.05 + 0.75 * progress + random.gauss(0, 0.02)
    elif t < 25.0:
        emg = 0.80 + random.gauss(0, 0.03)
    else:
        emg = 0.05 + random.gauss(0, 0.01)

    eda = 0.20 + (0.30 if 5.0 < t < 12.0 or 20.0 < t < 26.0 else 0.0)
    eda += random.gauss(0, 0.02)

    return max(0.0, emg), max(0.0, eda), True


def scenario_noisy(t):
    """Noisy data — random spikes, occasional missing frames."""
    emg, eda, _ = scenario_braking(t)

    # 10% chance of noise spike
    if random.random() < 0.10:
        emg += random.uniform(0.3, 0.8)

    # 5% chance of missing frame
    if random.random() < 0.05:
        return 0.0, 0.0, False

    return max(0.0, emg), max(0.0, eda), True


def scenario_fault(t):
    """Sensor fault injection — malformed frames and data gaps."""
    emg, eda, _ = scenario_normal(t)

    # 15% chance of producing a malformed frame
    if random.random() < 0.15:
        return emg, eda, "malformed"

    # 10% chance of gap (no output)
    if random.random() < 0.10:
        return 0.0, 0.0, False

    return emg, eda, True


SCENARIOS = {
    "normal":  scenario_normal,
    "braking": scenario_braking,
    "noisy":   scenario_noisy,
    "fault":   scenario_fault,
    "full":    None,  # Combined: normal → braking → noisy → fault
}


# ── Frame formatting ─────────────────────────────────────────────────

def format_frame(emg, eda, ts_ms, valid):
    """Format a data frame string."""
    if valid == "malformed":
        # Produce various kinds of malformed frames
        patterns = [
            f"GARBAGE{random.randint(0, 9999)}",
            f"$EMG,EMG=,EDA={eda:.2f},TS={ts_ms}*",
            f"EMG={emg:.2f},EDA={eda:.2f}",
            f"$EMG,EMG={emg:.2f},TS={ts_ms}",
            "",
        ]
        return random.choice(patterns)

    if not valid:
        return None  # Skip this frame

    return f"$EMG,EMG={emg:.2f},EDA={eda:.2f},TS={ts_ms}*"


# ── Main ─────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="EMG/EDA UART stream simulator for brake intent detection."
    )
    parser.add_argument("--scenario", choices=SCENARIOS.keys(), default="braking",
                        help="Data scenario to simulate (default: braking)")
    parser.add_argument("--duration", type=float, default=DEFAULT_DURATION,
                        help=f"Duration in seconds (default: {DEFAULT_DURATION})")
    parser.add_argument("--rate", type=int, default=DEFAULT_RATE_HZ,
                        help=f"Samples per second (default: {DEFAULT_RATE_HZ})")
    parser.add_argument("--serial", type=str, default=None,
                        help="Serial port (e.g. COM3, /dev/ttyUSB0). If omitted, output to stdout.")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD,
                        help=f"Baud rate for serial output (default: {DEFAULT_BAUD})")
    args = parser.parse_args()

    # Output target
    ser = None
    if args.serial:
        if serial is None:
            print("❌ pyserial not installed. Run: pip install pyserial", file=sys.stderr)
            return 1
        ser = serial.Serial(args.serial, args.baud, timeout=1)
        print(f"✅ Connected to {args.serial} at {args.baud} baud.", file=sys.stderr)

    print(f"▶ Scenario: {args.scenario} | Duration: {args.duration}s | Rate: {args.rate} Hz",
          file=sys.stderr)

    # Determine scenario function
    if args.scenario == "full":
        # Full test: cycle through all scenarios
        segment_duration = args.duration / 4.0
        generators = [scenario_normal, scenario_braking, scenario_noisy, scenario_fault]
    else:
        generators = [SCENARIOS[args.scenario]]
        segment_duration = args.duration

    interval = 1.0 / args.rate
    start_time = time.time()
    sample_count = 0

    try:
        while True:
            elapsed = time.time() - start_time
            if elapsed >= args.duration:
                break

            # Select generator for "full" mode
            if args.scenario == "full":
                gen_idx = min(int(elapsed / segment_duration), len(generators) - 1)
                gen = generators[gen_idx]
            else:
                gen = generators[0]

            emg, eda, valid = gen(elapsed)
            ts_ms = int(elapsed * 1000)

            frame = format_frame(emg, eda, ts_ms, valid)
            if frame is not None:
                if ser:
                    ser.write((frame + "\n").encode("utf-8"))
                else:
                    print(frame, flush=True)

                sample_count += 1

            time.sleep(interval)

    except KeyboardInterrupt:
        print("\n🛑 Stopped by user.", file=sys.stderr)
    finally:
        if ser and ser.is_open:
            ser.close()

    print(f"✅ Done. Sent {sample_count} frames in {elapsed:.1f}s.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
