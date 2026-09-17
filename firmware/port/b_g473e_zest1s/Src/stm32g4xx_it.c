/* Обработчики прерываний */
#include "stm32g4xx_hal.h"
#include "bsp.h"
extern ADC_HandleTypeDef hadc1;

void NMI_Handler(void)        { }
void HardFault_Handler(void)  { bsp_pwm_off(); for (;;) { } }
void MemManage_Handler(void)  { bsp_pwm_off(); for (;;) { } }
void BusFault_Handler(void)   { bsp_pwm_off(); for (;;) { } }
void UsageFault_Handler(void) { bsp_pwm_off(); for (;;) { } }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }
void SysTick_Handler(void)    { HAL_IncTick(); }
void ADC1_2_IRQHandler(void)  { HAL_ADC_IRQHandler(&hadc1); }
