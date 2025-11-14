#include "bsp.h"
#include "stm32h7xx_it.h"
#include "hw_def.h"


/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
#ifndef _USE_HW_RTOS
void SVC_Handler(void)
{
}
#endif

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{

}

/**
  * @brief This function handles Pendable request for system service.
  */
#ifndef _USE_HW_RTOS
void PendSV_Handler(void)
{
}
#endif

#ifdef _USE_HW_RTOS
extern void osSystickHandler(void);

void SysTick_Handler(void)
{
  osSystickHandler();
}
#else
extern void swtimerISR(void);

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
  swtimerISR();
}
#endif

