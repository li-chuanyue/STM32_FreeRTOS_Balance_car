STM32F103RC + FreeRTOS 入门模板

工程文件：User\F103_FreeRTOS_Template.uvprojx
目标芯片：STM32F103RC（Cortex-M3）
工具链：Keil MDK / ARM Compiler 6.22
FreeRTOS：Kernel V11.3.1，GCC ARM_CM3 移植层，heap_4

当前模板只验证调度器和两个周期任务，不会驱动电机或传感器。
TaskA_Count 每约 500 ms 加一，TaskB_Count 每约 1000 ms 加一。
在 Keil 的 Watch 窗口观察这两个全局变量，可以确认任务正在运行。

使用步骤：
1. 双击打开 User\F103_FreeRTOS_Template.uvprojx。
2. 编译并下载到 STM32F103RC 板卡。
3. 运行后在 Watch 窗口添加 TaskA_Count、TaskB_Count。

旧平衡车工程没有被修改。建议先确认本模板可以正常启动调度器，
再由自己逐步添加 GPIO、串口、传感器和控制任务。
