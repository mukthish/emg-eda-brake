# ──────────────────────────────────────────────────────────────────────
# EMG-EDA Brake Intent Detection — Makefile
#
# Targets:
#   make sim      — build PC simulator  (gcc)
#   make stm32    — build STM32 binary  (arm-none-eabi-gcc)
#   make clean    — remove build artifacts
#
# Usage:
#   make sim
#   python3 tools/stream_simulator.py --scenario braking | ./build/sim_emg_brake
# ──────────────────────────────────────────────────────────────────────

# ── Directories ──────────────────────────────────────────────────────

SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build

# ── Common sources (platform-independent core) ───────────────────────

CORE_SRC = $(SRC_DIR)/system_shell.c \
           $(SRC_DIR)/supervisor.c   \
           $(SRC_DIR)/input_acq.c    \
           $(SRC_DIR)/signal_proc.c  \
           $(SRC_DIR)/safety_manager.c \
           $(SRC_DIR)/output_manager.c

# ══════════════════════════════════════════════════════════════════════
# PC SIMULATOR TARGET
# ══════════════════════════════════════════════════════════════════════

SIM_DIR  = targets/sim
SIM_SRC  = $(SIM_DIR)/main.c $(SIM_DIR)/hal_sim.c
SIM_OUT  = $(BUILD_DIR)/sim_emg_brake

CC       = gcc
CFLAGS   = -Wall -Wextra -Werror -std=c11 -D_GNU_SOURCE -I$(INC_DIR) -O2
LDFLAGS  = -lm

.PHONY: sim
sim: $(SIM_OUT)

$(SIM_OUT): $(CORE_SRC) $(SIM_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(CORE_SRC) $(SIM_SRC) $(LDFLAGS)
	@echo "✅ Simulator built: $@"
	@echo "   Run: python3 tools/stream_simulator.py | $@"

# ══════════════════════════════════════════════════════════════════════
# STM32F4 TARGET
# ══════════════════════════════════════════════════════════════════════

STM32_DIR = targets/stm32
STM32_SRC = $(STM32_DIR)/main.c $(STM32_DIR)/hal_stm32.c
STM32_OUT = $(BUILD_DIR)/stm32_emg_brake.elf

ARM_CC     = arm-none-eabi-gcc
ARM_CFLAGS = -Wall -Wextra -std=c11 -I$(INC_DIR) -I$(STM32_DIR) \
             -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
             -DSTM32F407xx -Os -ffunction-sections -fdata-sections
ARM_LDFLAGS = -lm -lc -lnosys -Wl,--gc-sections \
              -specs=nosys.specs -specs=nano.specs

.PHONY: stm32
stm32: $(STM32_OUT)

$(STM32_OUT): $(CORE_SRC) $(STM32_SRC) | $(BUILD_DIR)
	$(ARM_CC) $(ARM_CFLAGS) -o $@ $(CORE_SRC) $(STM32_SRC) $(ARM_LDFLAGS)
	@echo "✅ STM32 ELF built: $@"

# ══════════════════════════════════════════════════════════════════════
# UTILITIES
# ══════════════════════════════════════════════════════════════════════

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "🧹 Cleaned build artifacts."

.PHONY: help
help:
	@echo "EMG-EDA Brake Intent Detection — Build Targets"
	@echo ""
	@echo "  make sim      Build PC simulator (gcc)"
	@echo "  make stm32    Build STM32F4 binary (arm-none-eabi-gcc)"
	@echo "  make clean    Remove build artifacts"
	@echo ""
	@echo "Run simulator:"
	@echo "  python3 tools/stream_simulator.py | ./build/sim_emg_brake"
