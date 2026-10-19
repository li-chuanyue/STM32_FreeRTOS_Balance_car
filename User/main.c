#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

/* 这两个变量可在 Keil 的 Watch 窗口中观察任务是否持续运行。 */
volatile uint32_t TaskA_Count = 0;
volatile uint32_t TaskB_Count = 0;

static void TaskA(void *argument)
{
    (void)argument;
	
    for (;;)
    {
        TaskA_Count++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void TaskB(void *argument)
{
    (void)argument;

    for (;;)
    {
        TaskB_Count++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
	/* 硬件初始化 */
	/* 创建任务 */
	/* 启动调度器 */

    if (xTaskCreate(TaskA, "TaskA", 256, NULL, 2, NULL) != pdPASS)
    {
        for (;;) { }
    }

    if (xTaskCreate(TaskB, "TaskB", 256, NULL, 1, NULL) != pdPASS)
    {
        for (;;) { }
    }

    vTaskStartScheduler();
    for (;;);	/* 调度器启动失败执行内容 */
}
