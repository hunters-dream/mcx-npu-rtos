#ifndef RTOS_HPP_
#define RTOS_HPP_

#include "task.hpp"
#include <cstddef>
#include <cstdint>
#include "fsl_debug_console.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

constexpr uint32_t STACK_SIZE              = 2048u;
constexpr uint32_t NORM_STACK_FRAME_SIZE   = 50u;          // reserved space for a register snapshot in words
constexpr uint32_t EXC_RETURN_THREAD_S_PSP = 0xFFFFFFEDu; // return to thread mode in secure state, use PSP on return
constexpr uint32_t XPSR_THREAD             = 0x01000000u; // resume in Thumb mode, no exceptions
constexpr uint32_t NUM_CALLEE_REGS         = 24u;
constexpr uint32_t STK_FRAME_XPSR         = 7u + NUM_CALLEE_REGS; // 31 - the 8th slot of the hardware frame
constexpr uint32_t STK_FRAME_RET_ADDR     = 6u + NUM_CALLEE_REGS;
constexpr uint32_t STK_FRAME_LR           = 5u + NUM_CALLEE_REGS;
constexpr uint32_t STACK_PAINT            = 0xDEADBEEFu; // value used to fill the stack for watermarking

constexpr uint32_t DANGER_THRESHOLD_PCT = 10; //percent
constexpr uint32_t TRIPWIRE_STEP      = 4;

constexpr int     MAX_NUMBER_OF_TASKS     = 32;

constexpr uint32_t PSP_STACK_POOL_SIZE = 16384u; //memory budget











// Called from assembly — must not be name-mangled
extern "C" {
    void          TerminateTask(void);
    TaskContext * PendSV_Handler_Main(uint32_t *sp, uint32_t *sp_lim, uint32_t exc_Return);
}

void rtos_task_init(void * function, uint32_t stack_size);
void StartScheduler(void);



// measure the stack peak usage
size_t estimate_task_empty_space(uint32_t task_id);

// calculate task size
size_t task_size(uint32_t task_id);

// print an ASCII map of the task's painted region: '-' = still 0xDEADBEEF, 'X' = touched.
// width is the number of characters in the bar (each char covers a chunk of the stack).
void task_stack_map(uint32_t task_id, uint32_t width = 64);

uint32_t get_task_count(void);

//check if stack size is healthy
void resize_stack(uint32_t task_id);



//TODO
void task_signal(uint32_t task_id, TaskSignal signal);


//finds the free bottom(splim) entry 
uint32_t * find_stack_region(uint32_t stack_size);

#endif /* RTOS_HPP_ */
