TARGET = tfpcmcia

# Toolchain — WCH MRS toolchain. Check with:
#   ls $(MRS_TOOLCHAIN)/bin/riscv*
# Common binary names: riscv-none-embed-gcc (MRS v1.x) or riscv-none-elf-gcc (newer)
# Override from command line: make CC=riscv-none-elf-gcc
CC      = riscv-none-embed-gcc
OBJCOPY = riscv-none-embed-objcopy
SIZE    = riscv-none-embed-size

MCU_FLAGS = -march=rv32imafc -mabi=ilp32f -msmall-data-limit=8

CFLAGS  = $(MCU_FLAGS) -Os -std=gnu99 \
          -ffunction-sections -fdata-sections \
          -Wall -Wno-unused-function \
          -DUSE_STDPERIPH_DRIVER -DCH32V30x_D8C \
          -I User -I Core -I Peripheral/inc

LDFLAGS = $(MCU_FLAGS) \
          -T Ld/Link.ld \
          -Wl,--gc-sections \
          -Wl,--print-memory-usage \
          -nostartfiles \
          --specs=nosys.specs \
          -lc -lgcc

SRCS  = User/main.c \
        User/gpio.c \
        User/pcmcia.c \
        User/debug.c \
        User/ch32v30x_it.c \
        Core/core_riscv.c \
        Core/system_ch32v30x.c \
        Peripheral/src/ch32v30x_gpio.c \
        Peripheral/src/ch32v30x_rcc.c \
        Peripheral/src/ch32v30x_exti.c \
        Peripheral/src/ch32v30x_usart.c \
        Peripheral/src/ch32v30x_misc.c \
        Peripheral/src/ch32v30x_dbgmcu.c \
        Peripheral/src/ch32v30x_spi.c

ASRCS = Startup/startup_ch32v30x_D8C.S \
        User/boot_rom.S

OBJS  = $(SRCS:.c=.o) $(ASRCS:.S=.o)

.PHONY: all clean flash

all: $(TARGET).bin $(TARGET).hex
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(MCU_FLAGS) -c -o $@ $<

flash: $(TARGET).bin
	minichlink -w $< flash -b

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).hex
