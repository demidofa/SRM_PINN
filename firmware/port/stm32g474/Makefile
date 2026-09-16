# Сборка прошивки для NUCLEO-G474RE: make VENDOR=<папка с cmsis-device-g4, stm32g4xx-hal-driver, CMSIS_5>
# Исходники ST и ARM не входят в архив дисциплины; их можно получить командой make vendor.
VENDOR ?= vendor
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
SIZE    = $(PREFIX)size
OBJCOPY = $(PREFIX)objcopy
MCU     = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS    = -DSTM32G474xx -DUSE_HAL_DRIVER
INC     = -IInc -I../../core -I$(VENDOR)/stm32g4xx-hal-driver/Inc \
          -I$(VENDOR)/cmsis-device-g4/Include -I$(VENDOR)/CMSIS_5/CMSIS/Core/Include
CFLAGS  = $(MCU) $(DEFS) $(INC) -O2 -g -std=c11 -Wall -Wextra -ffunction-sections -fdata-sections
LDFLAGS = $(MCU) -TSTM32G474RETx_FLASH.ld --specs=nano.specs --specs=nosys.specs \
          -Wl,--gc-sections -Wl,-Map=build/srm_fw.map -Wl,--print-memory-usage -lm
HAL = $(addprefix $(VENDOR)/stm32g4xx-hal-driver/Src/stm32g4xx_, hal.c hal_cortex.c hal_rcc.c hal_rcc_ex.c \
      hal_gpio.c hal_pwr.c hal_pwr_ex.c hal_flash.c hal_flash_ex.c hal_tim.c hal_tim_ex.c hal_adc.c hal_adc_ex.c \
      hal_exti.c hal_dma.c ll_adc.c)
SRC = Src/main.c Src/syscalls.c Src/bsp_stm32g4.c Src/stm32g4xx_hal_msp.c Src/stm32g4xx_it.c Src/system_stm32g4xx.c \
      ../../core/srm_ctrl.c ../../core/srm_lut.c ../../core/srm_lut_data.c $(HAL)
OBJ = $(patsubst %.c,build/%.o,$(notdir $(SRC))) build/startup_stm32g474xx.o
vpath %.c $(sort $(dir $(SRC)))

all: build/srm_fw.elf build/srm_fw.bin
	$(SIZE) build/srm_fw.elf

build/%.o: %.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/startup_stm32g474xx.o: Src/startup_stm32g474xx.s | build
	$(CC) $(MCU) -c $< -o $@
build/srm_fw.elf: $(OBJ) STM32G474RETx_FLASH.ld
	$(CC) $(OBJ) $(LDFLAGS) -o $@
build/srm_fw.bin: build/srm_fw.elf
	$(OBJCOPY) -O binary $< $@
build:
	mkdir -p build

vendor:
	mkdir -p $(VENDOR) && cd $(VENDOR) && \
	git clone --depth 1 https://github.com/STMicroelectronics/cmsis-device-g4.git && \
	git clone --depth 1 https://github.com/STMicroelectronics/stm32g4xx-hal-driver.git && \
	git clone --depth 1 --filter=blob:none --sparse https://github.com/ARM-software/CMSIS_5.git && \
	cd CMSIS_5 && git sparse-checkout set CMSIS/Core/Include

flash: build/srm_fw.bin
	st-flash write build/srm_fw.bin 0x08000000

clean:
	rm -rf build
.PHONY: all clean vendor flash
