# Standalone GNU Make build (no STM32CubeIDE makefiles)

APP ?= lorawan
BUILD_ROOT ?= build

PROJECT_NAME := LoRaWAN_End_Node
PROJECT_DIR := .
CORE_INC := $(PROJECT_DIR)/Core/Inc
CORE_INC_DIRS := $(shell find $(CORE_INC) -type d | LC_ALL=C sort)
APP_DEFS := -DDEBUG
LDSCRIPT := toolchain/ldscripts/STM32WLE5JCIX_FLASH.ld

INCLUDES := \
	$(foreach d,$(CORE_INC_DIRS),-I$(d)) \
	-I$(PROJECT_DIR)/LoRaWAN/App \
	-I$(PROJECT_DIR)/LoRaWAN/Target \
	-IDrivers/STM32WLxx_HAL_Driver/Inc \
	-IDrivers/STM32WLxx_HAL_Driver/Inc/Legacy \
	-IUtilities/trace/adv_trace \
	-IUtilities/misc \
	-IUtilities/sequencer \
	-IUtilities/timer \
	-IUtilities/lpm/tiny_lpm \
	-IMiddlewares/Third_Party/LoRaWAN/LmHandler/Packages \
	-IMiddlewares/Third_Party/SubGHz_Phy \
	-IMiddlewares/Third_Party/SubGHz_Phy/stm32_radio_driver \
	-IDrivers/CMSIS/Device/ST/STM32WLxx/Include \
	-IMiddlewares/Third_Party/LoRaWAN/Crypto \
	-IMiddlewares/Third_Party/LoRaWAN/Mac/Region \
	-IMiddlewares/Third_Party/LoRaWAN/Mac \
	-IMiddlewares/Third_Party/LoRaWAN/LmHandler \
	-IMiddlewares/Third_Party/LoRaWAN/Utilities \
	-IDrivers/CMSIS/Include \
	-IDrivers/BSP/STM32WLxx_LoRa_E5_mini

APP_CORE_SRCS := $(shell find $(PROJECT_DIR)/Core/Src -type f -name '*.c' | LC_ALL=C sort)

APP_SPECIFIC_SRCS := \
	$(PROJECT_DIR)/LoRaWAN/App/app_lorawan.c \
	$(PROJECT_DIR)/LoRaWAN/App/lora_app.c \
	$(PROJECT_DIR)/LoRaWAN/App/lora_info.c \
	$(PROJECT_DIR)/LoRaWAN/Target/radio_board_if.c \
	Drivers/BSP/STM32WLxx_LoRa_E5_mini/stm32wlxx_LoRa_E5_mini.c \
	Drivers/BSP/STM32WLxx_LoRa_E5_mini/stm32wlxx_LoRa_E5_mini_radio.c \
	Middlewares/Third_Party/LoRaWAN/LmHandler/LmHandler.c \
	Middlewares/Third_Party/LoRaWAN/LmHandler/NvmDataMgmt.c \
	Middlewares/Third_Party/LoRaWAN/LmHandler/Packages/LmhpCompliance.c \
	Middlewares/Third_Party/LoRaWAN/Mac/LoRaMac.c \
	Middlewares/Third_Party/LoRaWAN/Mac/LoRaMacAdr.c \
	Middlewares/Third_Party/LoRaWAN/Mac/LoRaMacClassB.c \
	Middlewares/Third_Party/LoRaWAN/Mac/LoRaMacCommands.c \
	Middlewares/Third_Party/LoRaWAN/Mac/LoRaMacConfirmQueue.c \
	Middlewares/Third_Party/LoRaWAN/Mac/LoRaMacCrypto.c \
	Middlewares/Third_Party/LoRaWAN/Mac/LoRaMacParser.c \
	Middlewares/Third_Party/LoRaWAN/Mac/LoRaMacSerializer.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/Region.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionAS923.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionAU915.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionBaseUS.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionCN470.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionCN779.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionCommon.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionEU433.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionEU868.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionIN865.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionKR920.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionRU864.c \
	Middlewares/Third_Party/LoRaWAN/Mac/Region/RegionUS915.c \
	Middlewares/Third_Party/LoRaWAN/Crypto/cmac.c \
	Middlewares/Third_Party/LoRaWAN/Crypto/lorawan_aes.c \
	Middlewares/Third_Party/LoRaWAN/Crypto/soft-se.c \
	Middlewares/Third_Party/LoRaWAN/Utilities/utilities.c \
	Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver/radio.c \
	Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver/radio_driver.c \
	Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver/radio_fw.c \
	Utilities/trace/adv_trace/stm32_adv_trace.c \
	Utilities/lpm/tiny_lpm/stm32_lpm.c \
	Utilities/misc/stm32_mem.c \
	Utilities/sequencer/stm32_seq.c \
	Utilities/misc/stm32_systime.c \
	Utilities/timer/stm32_timer.c \
	Utilities/misc/stm32_tiny_sscanf.c \
	Utilities/misc/stm32_tiny_vsnprintf.c

STARTUP_SRCS := \
	toolchain/startup/startup_stm32wle5jcix.s

HAL_SRCS := \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_adc.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_adc_ex.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_cortex.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_dma.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_dma_ex.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_exti.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_flash.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_flash_ex.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_gpio.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_i2c.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_i2c_ex.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_pwr.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_pwr_ex.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_rcc.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_rcc_ex.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_rtc.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_rtc_ex.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_subghz.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_tim.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_tim_ex.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_uart.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_uart_ex.c \
	Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_ll_adc.c

ifneq ($(APP),lorawan)
$(error Unsupported APP='$(APP)'. Use APP=lorawan)
endif

BUILD_DIR ?= $(BUILD_ROOT)/$(APP)

# Optional override. If empty, toolchain is searched in PATH or auto-detected.
MACOS_ARM_TOOLCHAIN := /Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin
CUBEIDE_TOOLCHAIN := /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.macosaarch64_1.0.0.202602081740/tools/bin

ifeq ($(OS),Windows_NT)
TOOLCHAIN_BIN ?=
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
TOOLCHAIN_BIN ?= $(MACOS_ARM_TOOLCHAIN)
else
TOOLCHAIN_BIN ?=
endif
endif

ifeq ($(strip $(TOOLCHAIN_BIN)),)
ifneq ($(wildcard $(CUBEIDE_TOOLCHAIN)/arm-none-eabi-gcc),)
TOOLCHAIN_BIN := $(CUBEIDE_TOOLCHAIN)
endif
endif

ifeq ($(strip $(TOOLCHAIN_BIN)),)
CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
OBJDUMP := arm-none-eabi-objdump
SIZE := arm-none-eabi-size
else
CC := $(TOOLCHAIN_BIN)/arm-none-eabi-gcc
OBJCOPY := $(TOOLCHAIN_BIN)/arm-none-eabi-objcopy
OBJDUMP := $(TOOLCHAIN_BIN)/arm-none-eabi-objdump
SIZE := $(TOOLCHAIN_BIN)/arm-none-eabi-size
endif

MCU_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
DEFS := $(APP_DEFS) -DCORE_CM4 -DSTM32WLE5xx -DUSE_HAL_DRIVER
COMMON_FLAGS := -Og -g3 -ffunction-sections -fdata-sections -Wall -fstack-usage

CFLAGS := $(MCU_FLAGS) -std=gnu11 $(DEFS) $(COMMON_FLAGS)
ASFLAGS := $(MCU_FLAGS) $(DEFS) $(COMMON_FLAGS) -x assembler-with-cpp

LDFLAGS := $(MCU_FLAGS) -T$(LDSCRIPT) --specs=nosys.specs --specs=nano.specs -Wl,-Map=$(BUILD_DIR)/$(PROJECT_NAME).map -Wl,--gc-sections -static -Wl,--start-group -lc -lm -Wl,--end-group
ALL_C_SRCS := $(APP_CORE_SRCS) $(APP_SPECIFIC_SRCS) $(HAL_SRCS)
ALL_S_SRCS := $(STARTUP_SRCS)

OBJS := \
	$(addprefix $(BUILD_DIR)/,$(ALL_C_SRCS:.c=.o)) \
	$(addprefix $(BUILD_DIR)/,$(ALL_S_SRCS:.s=.o))

DEPS := $(OBJS:.o=.d)

ELF := $(BUILD_DIR)/$(PROJECT_NAME).elf
MAP := $(BUILD_DIR)/$(PROJECT_NAME).map
LIST := $(BUILD_DIR)/$(PROJECT_NAME).list
HEX_BUILD := $(BUILD_DIR)/$(PROJECT_NAME).hex
BIN_BUILD := $(BUILD_DIR)/$(PROJECT_NAME).bin
HEX_ROOT := $(PROJECT_NAME).hex

.PHONY: all build hex clean rebuild size list bin help print-vars purge-cubeide lorawan

all: build

build: $(HEX_ROOT)

lorawan:
	$(MAKE) APP=lorawan build

hex: $(HEX_ROOT)

size: $(ELF)
	$(SIZE) $(ELF)

list: $(LIST)

bin: $(BIN_BUILD)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -MF"$(@:.o=.d)" -MT"$@" -c "$<" -o "$@"

$(BUILD_DIR)/%.o: %.s
	@mkdir -p "$(dir $@)"
	$(CC) $(ASFLAGS) $(INCLUDES) -MMD -MP -MF"$(@:.o=.d)" -MT"$@" -c "$<" -o "$@"

$(ELF): $(OBJS) $(LDSCRIPT)
	@mkdir -p "$(dir $@)"
	$(CC) -o "$@" $(OBJS) $(LDFLAGS)

$(HEX_BUILD): $(ELF)
	$(OBJCOPY) -O ihex "$<" "$@"

$(BIN_BUILD): $(ELF)
	$(OBJCOPY) -O binary "$<" "$@"

$(LIST): $(ELF)
	$(OBJDUMP) -h -S "$<" > "$@"

$(HEX_ROOT): $(HEX_BUILD)
	cp "$<" "$@"
	@echo "HEX ready: $(HEX_ROOT)"

clean:
	rm -rf "$(BUILD_ROOT)/lorawan"
	rm -f "LoRaWAN_End_Node.hex"

rebuild: clean build

purge-cubeide:
ifeq ($(CONFIRM),YES)
	rm -rf "$(PROJECT_DIR)/STM32CubeIDE" ".metadata"
	@echo "Removed CubeIDE project artifacts. Standalone make remains available."
else
	@echo "Refusing to delete CubeIDE artifacts without confirmation."
	@echo "Run: make purge-cubeide CONFIRM=YES"
	@exit 1
endif

print-vars:
	@echo "APP=$(APP)"
	@echo "PROJECT_NAME=$(PROJECT_NAME)"
	@echo "PROJECT_DIR=$(PROJECT_DIR)"
	@echo "CC=$(CC)"
	@echo "BUILD_DIR=$(BUILD_DIR)"
	@echo "LDSCRIPT=$(LDSCRIPT)"
	@echo "HEX_ROOT=$(HEX_ROOT)"

help:
	@echo "Standalone targets:"
	@echo "  make / make build                    Build selected APP and create $(HEX_ROOT)"
	@echo "  make APP=lorawan build               Build LoRaWAN_End_Node.hex"
	@echo "  make lorawan                         Shortcut for APP=lorawan build"
	@echo "  make hex            Same as build"
	@echo "  make clean          Remove standalone build artifacts"
	@echo "  make rebuild        Clean and build"
	@echo "  make size           Show ELF size"
	@echo "  make list           Generate disassembly list file"
	@echo "  make bin            Generate BIN file"
	@echo "  make purge-cubeide CONFIRM=YES"
	@echo ""
	@echo "Optional override:"
	@echo "  make TOOLCHAIN_BIN=<path-to-bin>"
	@echo "  make BUILD_ROOT=build_custom APP=lorawan build"

-include $(DEPS)
