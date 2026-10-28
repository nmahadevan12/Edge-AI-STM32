TARGET = firmware

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# USE_FPU=1 (default): Cortex-M4 hardware FPU. main.c enables it at startup.
# USE_FPU=0: soft-float fallback.   Run:  make USE_FPU=0
USE_FPU ?= 1
ifeq ($(USE_FPU),1)
MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
else
MCU = -mcpu=cortex-m4 -mthumb
endif

BUILD_DIR = build

# -O0 on purpose: main.c's delay-loop timing sets the sample rate, so keep it
# at the same optimisation level the training clips were recorded with.
# -MMD -MP makes headers (wake_word_model.h, ...) trigger rebuilds.
CFLAGS  = $(MCU) -O0 -Wall -g -DSTM32L475xx -Isrc -MMD -MP \
          -ffunction-sections -fdata-sections
LDFLAGS = $(MCU) -Tstm32l475vg.ld -nostartfiles \
          --specs=nano.specs --specs=nosys.specs \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--gc-sections -lm

C_SOURCES   = src/main.c src/wake_features.c
ASM_SOURCES = startup_stm32l475xx.s

OBJECTS  = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES:.s=.o)))

.PHONY: all clean flash

all: $(BUILD_DIR)/$(TARGET).elf

# Feature code has no timing loops, so it can be optimised (much faster inference).
$(BUILD_DIR)/wake_features.o: CFLAGS += -O2

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

# Explicit reset run — "program ... reset" can leave the core halted
flash: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
	  -c "program $(BUILD_DIR)/$(TARGET).elf verify; reset run; shutdown"

-include $(OBJECTS:.o=.d)