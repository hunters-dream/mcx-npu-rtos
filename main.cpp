#include "board.h"
#include "app.h"
#include "fsl_debug_console.h"

 
#include "rtos.hpp"

void task_led_signal(void)
{
    while (1)
    {
        GPIO_PortToggle(BOARD_LED_GPIO, 1u << BOARD_LED_GPIO_PIN);
    }
}





void task_growing_stack(void)
{
    while (1)
    {
        PRINTF("stale");
        for (volatile int i = 0; i < 100000000; i++);
    }
}

void task_monitor(void)
{
    while (1)
    {
        for (uint32_t id = 0; id < get_task_count(); id++)
        {
            task_stack_map(id, 512);


        }


        for (volatile int i = 0; i < 2000000; i++);
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


extern  void task_monitor(void);

int main(void)
{
    /* Board pin init */
    BOARD_InitHardware();
    BOARD_InitDebugConsole();

    PRINTF("Init\r\n");


    uint32_t led_desired_stack_size = 128u;

    rtos_task_init(reinterpret_cast<void *>(task_led_signal), led_desired_stack_size);
    rtos_task_init(reinterpret_cast<void*> (task_monitor),1024u );

    StartScheduler();
}
