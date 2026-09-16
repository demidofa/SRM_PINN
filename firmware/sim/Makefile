# Сборка и запуск проверки прошивки на ПК.
#   make test              — учебная модель 6/4
#   make test MOTOR=8_6    — четырехфазный ВИД 8/6 лабораторного стенда
CC ?= gcc
MOTOR ?= 6_4
MDEF = $(if $(filter 8_6,$(MOTOR)),-DSRM_MOTOR_8_6,)
CFLAGS = -O2 -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -I../core $(MDEF)
SRC = test_firmware.c plant.c ../core/srm_ctrl.c ../core/srm_lut.c ../core/srm_lut_data.c

test: test_firmware_$(MOTOR)
	./test_firmware_$(MOTOR)
	python3 compare_python.py --motor $(MOTOR)

test_firmware_$(MOTOR): $(SRC) ../core/*.h plant.h
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm

clean:
	rm -f test_firmware_* *.csv
