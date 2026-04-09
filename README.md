# EMG-EDA Brake Intent Detection

An embedded safety subsystem that detects **emergency braking intent** using calf muscle EMG (and optional EDA) signals and triggers an emergency brake request. Built as a redundant, safety-oriented intent detection channel for driver assistance.

> **Course:** CS G523 — Software for Embedded Systems

---

## Architecture

The firmware uses a **coordinated safety pipeline** with a central supervisor state machine:

```
Input Acquisition → Signal Processing → Safety Manager → Supervisor → Output
    (UART/BLE)       (Filter/RMS)       (Threshold)     (State Machine)  (Brake GPIO)
```

### System States

```
StartupSafe → Idle → IntentPending → IntentConfirmed → Recovery
                ↕                         ↕
            SoftFault                  HardFault
```

| State | Meaning |
|-------|---------|
| **StartupSafe** | Boot state — waiting for valid data |
| **Idle** | Normal monitoring — data valid, no braking intent |
| **IntentPending** | EMG above threshold — awaiting confirmation |
| **IntentConfirmed** | Sustained EMG — **brake request ASSERTED** |
| **Recovery** | EMG dropping — cooldown before returning to Idle |
| **SoftFault** | Data missing/corrupt — recoverable |
| **HardFault** | Sensor fault — requires manual intervention |

### Signal Processing Parameters

Derived from [Ju et al. 2021, IEEE ACCESS](https://doi.org/10.1109/ACCESS.2021.3114341):

| Parameter | Value |
|-----------|-------|
| Bandpass filter | 4th-order Butterworth, 15–90 Hz |
| Linear envelope | 2nd-order Butterworth LPF @ 2 Hz |
| Detection window | 1 second |
| Step size | 60 ms |
| RMS threshold | Baseline × 2.5 |
| Confirmation | 3 consecutive windows (~180 ms) |
| Cooldown | 2.0 s after brake de-assert |

---

## Prerequisites

### PC Simulator (Linux/macOS)

- **GCC** (C11 support)
- **Python 3** (for the stream simulator)
- **Make**

```bash
# Ubuntu/Debian
sudo apt install build-essential python3
```

### STM32 Target (optional)

- **arm-none-eabi-gcc** toolchain
- STM32F4 Discovery board

```bash
# Ubuntu/Debian
sudo apt install gcc-arm-none-eabi
```

---

## Building

### PC Simulator

```bash
make sim
```

Produces `build/sim_emg_brake`.

### STM32 Target

```bash
make stm32
```

Produces `build/stm32_emg_brake.elf`.

### Clean

```bash
make clean
```

---

## Running the Simulator

The PC simulator reads UART-format EMG data frames from **stdin**. The included Python script generates simulated data and pipes it in.

### Basic Usage

```bash
python3 tools/stream_simulator.py --scenario braking | ./build/sim_emg_brake
```

### Simulator Options

```
python3 tools/stream_simulator.py [OPTIONS]
```

| Option | Default | Description |
|--------|---------|-------------|
| `--scenario` | `braking` | Data scenario (see below) |
| `--duration` | `60` | Duration in seconds |
| `--rate` | `20` | Samples per second |
| `--serial` | — | Serial port (e.g. `/dev/ttyUSB0`) instead of stdout |
| `--baud` | `9600` | Baud rate for serial output |

### Scenarios

| Scenario | Description |
|----------|-------------|
| `normal` | Low EMG baseline with slight noise (normal driving) |
| `braking` | Two emergency braking events with ramp-up/down and recovery |
| `noisy` | Braking scenario with random noise spikes and missing frames |
| `fault` | Malformed frames and data gaps (sensor fault injection) |
| `full` | Cycles through all four scenarios sequentially |

### Example: 30-second braking test

```bash
python3 tools/stream_simulator.py --scenario braking --duration 30 | ./build/sim_emg_brake
```

### Example: Full test suite

```bash
python3 tools/stream_simulator.py --scenario full --duration 60 | ./build/sim_emg_brake
```

### Reading the Output

Output goes to **stderr** and includes timestamped logs:

```
[   20480] [SUP] Idle -> IntentPending              # EMG crossed threshold
[   20600] [SUP] IntentPending -> IntentConfirmed    # 3 windows confirmed
[   20600] [OUT] *** BRAKE ASSERTED ***              # Brake GPIO active
[   24626] [SUP] IntentConfirmed -> Recovery         # EMG dropped
[   24626] [OUT] brake de-asserted                   # Brake GPIO released
```

Key log tags:
- `[SYS]` — System shell lifecycle events
- `[SUP]` — Supervisor state transitions
- `[OUT]` — Brake assert/de-assert
- `[INP]` — Input acquisition errors
- `[SAF]` — Safety manager faults
- `[LOG]` — Structured log entries
- `[HAL-SIM]` — Simulated hardware events (GPIO, LED)

### Serial Port Mode (for STM32 testing)

To send data to a real STM32 over a USB-UART adapter:

```bash
python3 tools/stream_simulator.py --scenario braking --serial /dev/ttyUSB0
```

---

## UART Frame Format

The simulator outputs frames in this format:

```
$EMG,EMG=0.72,EDA=0.31,TS=123456*
```

| Field | Description |
|-------|-------------|
| `$EMG` | Frame start delimiter |
| `EMG=` | Normalised EMG amplitude (0.0 – 1.0) |
| `EDA=` | Normalised EDA level (0.0 – 1.0, optional) |
| `TS=` | Timestamp in milliseconds |
| `*` | Frame end delimiter |

---

## Project Structure

```
emg-eda-brake/
├── Makefile                # Build: make sim / make stm32
├── README.md               # This file
│
├── include/                # Module interfaces (no HW-specific code)
│   ├── hal.h               # Hardware Abstraction Layer
│   ├── system_shell.h      # Boot + scheduler
│   ├── supervisor.h        # State machine
│   ├── input_acq.h         # UART/BLE input
│   ├── signal_proc.h       # Signal processing
│   ├── safety_manager.h    # Intent + safety
│   └── output_manager.h    # Brake + logging
│
├── src/                    # Core logic (platform-independent)
│   ├── system_shell.c
│   ├── supervisor.c
│   ├── input_acq.c
│   ├── signal_proc.c
│   ├── safety_manager.c
│   └── output_manager.c
│
├── targets/
│   ├── sim/                # PC simulator
│   │   ├── main.c
│   │   └── hal_sim.c
│   └── stm32/              # STM32F4 Discovery
│       ├── main.c
│       ├── hal_stm32.c
│       └── stm32f4xx.h
│
└── tools/
    └── stream_simulator.py # Data generator
```

---

## Team

| Member | Module |
|--------|--------|
| Mukthish | Supervisor / State Manager |
| Devansh | Input Acquisition + UART/BLE |
| Mahesh | Signal Processing + Feature Engine |
| Bhagavath | Intent + Safety Manager |
| Abhishek | Output + Observability + Brake Driver |
