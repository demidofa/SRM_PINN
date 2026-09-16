# Проверка ядра прошивки на эмулируемом Cortex-M4F: make run (нужны arm-none-eabi-gcc и qemu-system-arm)
MOTOR ?= 6_4
MDEF = $(if $(filter 8_6,$(MOTOR)),-DSRM_MOTOR_8_6,)
MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
SRC = startup_m4.c ../test_firmware.c ../plant.c ../../core/srm_ctrl.c ../../core/srm_lut.c ../../core/srm_lut_data.c
test_m4_$(MOTOR).elf: $(SRC) mps2_an386.ld
	arm-none-eabi-gcc $(MCU) -O2 -std=c11 -D_DEFAULT_SOURCE -DQEMU_TARGET $(MDEF) -I../../core -I.. \
	  $(SRC) -T mps2_an386.ld --specs=rdimon.specs -lm -o $@
run: test_m4_$(MOTOR).elf
	qemu-system-arm -machine mps2-an386 -nographic -monitor none -serial none \
	  -semihosting-config enable=on,target=native -icount shift=0 -kernel test_m4_$(MOTOR).elf
	python3 ../compare_python.py --motor $(MOTOR) --m4
clean:
	rm -f test_m4_*.elf *.csv
