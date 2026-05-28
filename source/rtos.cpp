#include <cstdint>
#include <cstddef>
#include <new>
#include "rtos.hpp"
#include "fsl_device_registers.h"


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

void rtos_task_init(const char * name, void * function, uint32_t stack_size)
{
    uint32_t * stack_base =  find_stack_region(stack_size);
    if (stack_base == nullptr)
    {
        PRINTF("rtos: out of stack memory\r\n");
        __builtin_trap();
    }
  
    uint32_t id = task_count;
    new (&tasks[id]) Task(name,function, stack_size, id); //placement new
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


    //find if a dead task can be replaced
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

    //store the context of outgoing task on it's stack
    tasks[current_task_id].setSP(sp);
    tasks[current_task_id].setSPLimit(sp_lim);
    tasks[current_task_id].setExcReturn(exc_Return);

    uint32_t start = current_task_id;

    do {
        current_task_id++;
        if (current_task_id >= task_count) current_task_id = 0;
    } while (tasks[current_task_id].getState() != TaskState::ACTIVE
         && current_task_id != start);

    if (tasks[current_task_id].getState() != TaskState::ACTIVE) {
        __builtin_trap();
    }





    return tasks[current_task_id].getContext(); // store the pointer of the next task in R0
}



extern "C" __attribute__((naked)) void PendSV_Handler(void)
{
    __asm(
        "TST      LR,     #0x4                \n" //set EQ if we came from MSP, NE if we came from PSP.
        "ITTEE    EQ                          \n" //conditionally execute up to four instructions without branches
        "MRSEQ    R0,     MSP                 \n" //  (only if EQ)
        "MRSEQ    R1,     MSPLIM              \n" //  (only if EQ)
        "MRSNE    R0,     PSP                 \n" //  (only if NE)
        "MRSNE    R1,     PSPLIM              \n" //  (only if NE)
        "MOV      R2,     LR                  \n" //  Set the third argument for pendSV_Handler

        "STMDB    R0!,    {R4-R11}            \n" // stores R4 through R11 onto the stack (calee-saved regs)
        "TST      LR,     #0x10               \n" // test if FPU was in use
        "IT       EQ                          \n"
        "VSTMDBEQ R0!,    {S16-S31}           \n" // stores S16-S31 calee-saved FPU regs

        "BL       PendSV_Handler_Main         \n"
        "LDR      LR,     [R0, #8]            \n" //
        "LDR      R1,     [R0, #4]            \n" // load incoming sp/splim/exc_return
        "LDR      R0,     [R0, #0]            \n" // 

        "TST      LR,     #0x10               \n"
        "IT       EQ                          \n"
        "VLDMIAEQ R0!,    {S16-S31}           \n" //pop FPU regs
        "LDMIA    R0!,    {R4-R11}            \n" //pop R4-R11

        "TST      LR,     #0x4                \n" //set EQ if we came from MSP, NE if we came from PSP.
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

static void idle_task(void)
{
    while (1) {
        __WFI();   // wait for interrupt — sleeps until next SysTick 
    }
}



void StartScheduler(void)
{
    rtos_task_init("idle_task",reinterpret_cast<void*>(idle_task),128);

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

    char name_col[32];
    const char *name = tasks[task_id].GetName();
    uint32_t n = 0;
    while (n < 31 && name[n]) { name_col[n] = name[n]; n++; }
    while (n < 31)             { name_col[n++] = ' '; }
    name_col[31] = '\0';

    PRINTF("%s %2u %s\r\n", name_col, task_id, buf);
}

