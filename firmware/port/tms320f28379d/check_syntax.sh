#!/bin/sh
# Синтаксическая проверка порта по заголовкам C2000Ware компилятором ПК.
# Использование: ./check_syntax.sh <путь к c2000ware-core-sdk>
set -e
C=${1:-vendor/c2000ware-core-sdk}
G=$(gcc -print-file-name=include)
SHIM=$(mktemp -d)
cat > $SHIM/stdint.h <<'H'
#ifndef SHIM_STDINT_H
#define SHIM_STDINT_H
typedef short int16_t; typedef unsigned short uint16_t;
typedef int int32_t; typedef unsigned int uint32_t;
typedef long long int64_t; typedef unsigned long long uint64_t;
typedef unsigned long uintptr_t;
#endif
H
cat > $SHIM/math.h <<'H'
float fmodf(float, float); float floorf(float); float ceilf(float); float fabsf(float);
H
# driverlib.h целиком содержит расширения C28x (can.h); для проверки подключаются нужные модули
cat > $SHIM/driverlib.h <<'H'
#include "inc/hw_memmap.h"
#include "adc.h"
#include "cpu.h"
#include "cputimer.h"
#include "epwm.h"
#include "eqep.h"
#include "gpio.h"
#include "interrupt.h"
#include "pin_map.h"
#include "sysctl.h"
H
# device.h копируется, чтобы его #include "driverlib.h" разрешался в сокращенный вариант
cp $C/device_support/f2837xd/common/include/device.h $SHIM/
status=0
for f in main.c bsp_f28379d.c; do
  out=$(gcc -fsyntax-only -std=c99 -Wall -Wextra -Wno-int-to-pointer-cast -Wno-unused-parameter -Wno-main \
      -nostdinc -I$SHIM -isystem $G -D__TMS320C28XX__ -D__TI_EABI__ -DCPU1 -D_LAUNCHXL_F28379D \
      -DSRM_MOTOR_8_6 -D__interrupt= -Dinterrupt= -D__cregister= -Dcregister= \
      -I. -I../../core -I$C/driverlib/f2837xd/driverlib -I$C/device_support/f2837xd/common/include \
      $f 2>&1 | grep -A3 -E "^(main|bsp_f28379d)\.c:|error" || true)
  if [ -n "$out" ]; then echo "$out"; echo "$f: есть замечания"; status=1; else echo "$f: ошибок и предупреждений нет"; fi
done
rm -rf $SHIM
exit $status
