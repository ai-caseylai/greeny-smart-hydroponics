# Kids Activity Tracker — CH582M BLE 5.0 Wristband
#
# Toolchain: RISC-V GCC (bundled with MounRiver Studio or standalone)
# SDK:       CH583 EVT from WCH
# Flash:     wchisp (pip3 install wchisp)
#
# Usage:
#   make              Build firmware
#   make flash        Build + flash via USB
#
# Paths auto-detected. Override with:
#   WCH_TOOLCHAIN=/path/to/riscv-gcc/bin
#   WCH_SDK=/path/to/CH583EVT
#
# --- Manual Install (no MounRiver needed) ---
# 1. Download RISC-V GCC: https://www.wch.cn/downloads/MRS_Toolchain_MAC_v140_x64.html
#    Extract → WCH_TOOLCHAIN=/path/to/toolchain/bin
# 2. Download CH583 SDK: https://www.wch.cn/downloads/CH583EVT_ZIP.html
#    Extract → WCH_SDK=/path/to/CH583EVT
# 3. pip3 install wchisp
# 4. make && make flash

TARGET      = wristband
SRC         = src/main.c
ELF         = $(TARGET).elf
BIN         = $(TARGET).bin
HEX         = $(TARGET).hex

# ============================================================
# Toolchain Detection
# ============================================================
MRS_TOOLCHAIN ?= /Applications/MounRiver_Studio/toolchain/RISC-V_Embedded_GCC/bin
WCH_TOOLCHAIN ?= $(MRS_TOOLCHAIN)

CC      = $(WCH_TOOLCHAIN)/riscv-none-embed-gcc
OBJCOPY = $(WCH_TOOLCHAIN)/riscv-none-embed-objcopy
SIZE    = $(WCH_TOOLCHAIN)/riscv-none-embed-size

# ============================================================
# SDK Detection
# ============================================================
MRS_SDK  ?= /Applications/MounRiver_Studio/workspace/WCH/CH583/EVT
WCH_SDK  ?= $(MRS_SDK)

# Common SDK sub-paths (try to auto-find)
SDK_INC ?= $(WCH_SDK)/include
SDK_SRC ?= $(WCH_SDK)/src
SDK_BLE ?= $(WCH_SDK)/BLE_LIB
SDK_LD  ?= $(WCH_SDK)/Ld/CH583.ld
SDK_STARTUP ?= $(SDK_SRC)/startup_ch583.S

# ============================================================
# Flags
# ============================================================
ARCH_FLAGS  = -march=rv32imac -mabi=ilp32 -mcmodel=medany
OPT_FLAGS   = -Os -ffunction-sections -fdata-sections -fno-common
WARN_FLAGS  = -Wall -Wextra

CFLAGS  = $(ARCH_FLAGS) $(OPT_FLAGS) $(WARN_FLAGS)
CFLAGS += -I$(SDK_INC) -I$(SDK_BLE)
CFLAGS += -DCH583 -D__riscv__

LDFLAGS = -T $(SDK_LD) -Wl,--gc-sections -nostartfiles
LDFLAGS += -L$(SDK_BLE) -lCH583BLE

# ============================================================
# Targets
# ============================================================
.PHONY: all clean flash check info

all: check $(BIN) $(HEX)
	@$(SIZE) $(ELF)
	@echo ""
	@echo "  Build: OK → $(BIN) ($(HEX))"
	@echo "  Flash: make flash"

info:
	@echo "=== CH582 Build Info ==="
	@echo "Toolchain: $(WCH_TOOLCHAIN)"
	@echo "  CC:       $(CC)"
	@echo "SDK:       $(WCH_SDK)"
	@echo "Target:    $(TARGET)"
	@echo ""

check:
	@test -x "$(CC)" || (echo "ERROR: RISC-V GCC not found at $(CC)"; echo "Set WCH_TOOLCHAIN=/path/to/gcc/bin or install MounRiver Studio"; exit 1)
	@test -f "$(SDK_STARTUP)" || (echo "ERROR: CH583 EVT SDK not found at $(WCH_SDK)"; echo "Set WCH_SDK=/path/to/CH583EVT or download from wch.cn"; exit 1)
	@test -x "$$(which wchisp)" || (echo "ERROR: wchisp not found. Run: pip3 install wchisp"; exit 1)

$(ELF): $(SRC) $(SDK_STARTUP)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SDK_STARTUP) $(SRC)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

flash: $(BIN)
	@echo "Flashing via wchisp (USB)..."
	wchisp flash $(BIN)
	@echo "Done."

clean:
	rm -f *.elf *.bin *.hex *.map *.o
