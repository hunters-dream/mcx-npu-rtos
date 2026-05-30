#include "board.h"
#include "app.h"
#include "fsl_debug_console.h"
#include "rtos.hpp"

void task_led_signal(void)
{
    while (1)
    {
        GPIO_PortToggle(BOARD_LED_GPIO, 1u << BOARD_LED_GPIO_PIN);
        for (volatile int i = 0; i < 500000; i++);
    }
}

void task_monitor(void)
{
    while (1)
    {
        for (uint32_t id = 0; id < get_task_count(); id++)
            task_stack_map(id, 64);
        PRINTF("\r\n");
        for (volatile int i = 0; i < 250000; i++);
    }
}

void npu_task(void)
{
   // Inference_Init();  // one-time setup

    while (1)
    {
       // Inference_Run();  // blocks during inference, scheduler runs other tasks
    }
}

static volatile bool keep_eating = true;

__attribute__((noinline)) void stack_eater(void)
{
    volatile uint32_t buf[16];
    (void)buf; //suppress warning
    for (volatile int i = 0; i < 300000; i++);
    if (keep_eating)
        stack_eater();
}

int main(void)
{
    BOARD_InitHardware();
    BOARD_InitDebugConsole();
    PRINTF("Init\r\n");



    rtos_task_init("led", reinterpret_cast<void*>(task_led_signal), 128u);
    rtos_task_init("sysmonitor",  reinterpret_cast<void*>(task_monitor), 256u);
    rtos_task_init("npu",  reinterpret_cast<void*>(npu_task), 128u);
    rtos_task_init("stack_eater", reinterpret_cast<void*>(stack_eater), 512u);

    StartScheduler();
}
