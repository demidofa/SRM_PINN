/* Минимальный запуск для QEMU mps2-an386 (Cortex-M4F) с вводом-выводом через semihosting. */
#include <stdint.h>
extern void _start(void);
extern uint32_t __stack;
volatile unsigned long g_systick_wraps = 0;

#define SCB_CPACR   (*(volatile uint32_t *)0xE000ED88)
#define SYST_CSR    (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR    (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR    (*(volatile uint32_t *)0xE000E018)
#define SYST_RELOAD 0x00FFFFFFu

void Reset_Handler(void)
{
    SCB_CPACR |= (0xFu << 20);           /* разрешение FPU до первой операции с плавающей точкой */
    SYST_RVR = SYST_RELOAD; SYST_CVR = 0; SYST_CSR = 0x7u;   /* SysTick от тактовой частоты процессора */
    _start();
}
void SysTick_Handler(void) { g_systick_wraps++; }
static void Default_Handler(void) { for (;;) { } }

/* При запуске QEMU с -icount shift=0 одна команда соответствует 1 нс виртуального времени,
   а SysTick на mps2-an386 тактируется частотой 25 МГц: один отсчет — 40 команд. */
unsigned long qemu_insn_now(void)
{
    unsigned long w1, w2; uint32_t v;
    do { w1 = g_systick_wraps; v = SYST_CVR; w2 = g_systick_wraps; } while (w1 != w2);
    return (unsigned long)((w1 * (SYST_RELOAD + 1ull) + (SYST_RELOAD - v)) * 40ull);
}

__attribute__((section(".isr_vector"), used))
void (*const vectors[16])(void) = {
    (void (*)(void))&__stack, Reset_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, 0, 0, 0, 0,
    Default_Handler, Default_Handler, 0, Default_Handler, SysTick_Handler
};
