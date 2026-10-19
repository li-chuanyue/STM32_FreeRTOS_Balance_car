#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f10x.h"

/* CPU 与时钟 */
#define configCPU_CLOCK_HZ                      ((uint32_t)SystemCoreClock)
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configSYSTICK_CLOCK_HZ                  (configCPU_CLOCK_HZ)

/* 调度器与任务 */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)
#define configMAX_TASK_NAME_LEN                 16
#define configIDLE_SHOULD_YIELD                 1
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configSTACK_DEPTH_TYPE                  uint16_t

/* 内存：F103RC 有 48 KB SRAM；heap_4 从中提供动态分配。 */
#define configTOTAL_HEAP_SIZE                   ((size_t)(12 * 1024))
#define configAPPLICATION_ALLOCATED_HEAP        0

/* Cortex-M3 中断优先级（STM32F1 使用 4 个优先级位）。 */
#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY         (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* 只启用模板需要的内核功能。 */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_TIMERS                        0
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

/* vTaskDelay 等常用 API */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_xTaskGetSchedulerState          1

/* FreeRTOS 使用 Cortex-M3 标准异常入口。 */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#define configASSERT(x) do { if ((x) == 0) { taskDISABLE_INTERRUPTS(); for (;;) { } } } while (0)

#endif /* FREERTOS_CONFIG_H */
