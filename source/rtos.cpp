#include <cstdint>
#include <cstddef>
#include <new>
#include "rtos.hpp"
#include "fsl_device_registers.h"
/*******************************************************************************
 * Variables
 ******************************************************************************/

static uint32_t psp_stack[PSP_STACK_POOL_SIZE];
uint32_t *PSPBase = psp_stack;

static Task tasks[MAX_NUMBER_OF_TASKS];

static uint8_t  current_task_id = 0;
static uint32_t task_count      = 0;
static bool     first_switch    = true;


extern "C" void TerminateTask(void)
{
    __builtin_trap(); // crash if any task was finished
}

void rtos_task_init(void * function, uint32_t stack_size)
{
    uint32_t * stack_base =  find_stack_region(stack_size);
    if (stack_base == nullptr)
    {
        PRINTF("rtos: out of stack memory\r\n");
        __builtin_trap();
    }
  
    uint32_t id = task_count;
    new (&tasks[id]) Task(function, stack_size, id); //placement new
    task_count++;


    tasks[id].setSPLimit(stack_base);
    tasks[id].setSP(stack_base + stack_size - NORM_STACK_FRAME_SIZE);
    tasks[id].setExcReturn(EXC_RETURN_THREAD_S_PSP);

    //write the fake frame
    // Thumb mode (16-bit instruction set) = on
    // active exception = none
    // conditioning flags = clear
    tasks[id].getSP()[STK_FRAME_XPSR]     = XPSR_THREAD;
    tasks[id].getSP()[STK_FRAME_RET_ADDR] = reinterpret_cast<uint32_t>(function);
    tasks[id].getSP()[STK_FRAME_LR]       = reinterpret_cast<uint32_t>(TerminateTask);



    // paint the stack with a known value for watermarking
    for (uint32_t *i = tasks[id].getSPLimit(); i < tasks[id].getSP(); i++)
    {
        *i = STACK_PAINT;
    }



}


uint32_t * find_stack_region(uint32_t stack_size)
{
    uint32_t * region = PSPBase;


    //find if we can replace dead task
    for(uint32_t i = 0; i < task_count; i++)
    {

        if(tasks[i].getState() == TaskState::DEAD &&  tasks[i].getStackSize() >= stack_size)
        {
            return tasks[i].getSPLimit();
        }

    }



    for (uint32_t i = 0; i < task_count; i++)
    {
        uint32_t *task_end = tasks[i].getSPLimit() + tasks[i].getStackSize();
        if (task_end > region)
            region = task_end;
    }


    //no empty space
    if (region + stack_size > PSPBase + PSP_STACK_POOL_SIZE)
        return nullptr;

    return region;

}



extern "C" TaskContext *PendSV_Handler_Main(uint32_t *sp, uint32_t *sp_lim, uint32_t exc_Return)
{

    if (first_switch)
    {
        first_switch = false;
        return tasks[current_task_id].getContext();
    }

    tasks[current_task_id].setSP(sp);
    tasks[current_task_id].setSPLimit(sp_lim);
    tasks[current_task_id].setExcReturn(exc_Return);

    current_task_id++;
    if (current_task_id >= task_count) current_task_id = 0;

    return tasks[current_task_id].getContext();
}



extern "C" __attribute__((naked)) void PendSV_Handler(void)
{
    __asm(
        "TST      LR,     #0x4                \n"
        "ITTEE    EQ                          \n"
        "MRSEQ    R0,     MSP                 \n"
        "MRSEQ    R1,     MSPLIM              \n"
        "MRSNE    R0,     PSP                 \n"
        "MRSNE    R1,     PSPLIM              \n"
        "MOV      R2,     LR                  \n"

        "STMDB    R0!,    {R4-R11}            \n"
        "TST      LR,     #0x10               \n"
        "IT       EQ                          \n"
        "VSTMDBEQ R0!,    {S16-S31}           \n"

        "BL       PendSV_Handler_Main         \n"
        "LDR      LR,     [R0, #8]            \n"
        "LDR      R1,     [R0, #4]            \n"
        "LDR      R0,     [R0, #0]            \n"

        "TST      LR,     #0x10               \n"
        "IT       EQ                          \n"
        "VLDMIAEQ R0!,    {S16-S31}           \n"
        "LDMIA    R0!,    {R4-R11}            \n"

        "TST      LR,     #0x4                \n"
        "ITT      NE                          \n"
        "MSRNE    PSPLIM, R1                  \n"
        "MSRNE    PSP,    R0                  \n"
        "BX       LR                          \n"
    );
}

extern "C" void SysTick_Handler(void)
{
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    __DSB();
}

void StartScheduler(void)
{
    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    NVIC_SetPriority(SysTick_IRQn, 0x11);
    SysTick_Config(SystemCoreClock / 1000);
    while (1);
}




bool task_in_danger(uint32_t task_id)
{
    uint32_t *sp        = tasks[task_id].getSP();
    uint32_t *limit     = tasks[task_id].getSPLimit();
    uint32_t  size      = tasks[task_id].getStackSize();
    uint32_t  danger    = size * DANGER_THRESHOLD_PCT / 100;

    return (uint32_t)(sp - limit) <= danger;
}


size_t task_size(uint32_t task_id)
{
    size_t untouched     = estimate_task_empty_space(task_id);
    size_t total_painted = tasks[task_id].getStackSize() - NORM_STACK_FRAME_SIZE;
    return total_painted - untouched;
}

uint32_t get_task_count(void)
{
    return task_count;
}

void task_stack_map(uint32_t task_id, uint32_t width)
{
    if (task_id >= task_count) return;

    uint32_t *base  = tasks[task_id].getSPLimit();
    uint32_t  total = tasks[task_id].getStackSize() - NORM_STACK_FRAME_SIZE;

    if (width == 0)     width = 64;
    if (width > 128)    width = 128;
    if (width > total)  width = total;

    char buf[130];
    buf[0] = '[';

    for (uint32_t b = 0; b < width; b++)
    {
        uint32_t *start = base + (b       * total) / width;
        uint32_t *end   = base + ((b + 1) * total) / width;

        bool touched = false;
        for (uint32_t *p = start; p < end; p++)
        {
            if (*p != STACK_PAINT) { touched = true; break; }
        }
        buf[1 + b] = touched ? 'X' : '-';
    }

    buf[1 + width] = ']';
    buf[2 + width] = '\0';

    PRINTF("task %u %s\r\n", task_id, buf);
    PRINTF("STATUS: %u \r\n", task_in_danger(task_id));
}

