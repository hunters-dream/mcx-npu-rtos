# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires `SdkRootDirPath` environment variable pointing to the MCUXpresso SDK root (the toolchain file is at `$SdkRootDirPath/mcuxsdk/cmake/toolchain/armgcc.cmake`).

```bash
# Configure
cmake --preset debug

# Build (output: debug/mcx-npu-rtos_cm33_core0.elf and .bin)
cmake --build --preset debug

# Release build
cmake --preset release && cmake --build --preset release
```

VS Code: the default build task (`Ctrl+Shift+B`) runs the debug preset via the CMake extension.

## Flash / Debug

Flashing uses the VS Code MCUXpresso extension (`mcuxpresso-debug` launch type) with LinkServer targeting `MCXN947:FRDM-MCXN947`. Connect the board via USB to the MCU-Link port (J17). The debug configuration stops at `main` on launch.

## Architecture

### Scheduler (`source/rtos.cpp` / `rtos.hpp`)

Preemptive round-robin scheduler built on two ARM exception handlers:

- **`SysTick_Handler`** — runs at 1 kHz (`SystemCoreClock / 1000`), pends PendSV by setting `SCB->ICSR.PENDSVSET`.
- **`PendSV_Handler`** — naked assembly handler; saves FPU regs (S16–S31, only if FPU frame active) and general-purpose regs (R4–R11) onto the current task's PSP, calls `PendSV_Handler_Main`, then restores the next task's registers and switches PSP + PSPLIM.
- **`PendSV_Handler_Main`** — C++ function; saves SP/SPLIM/EXC_RETURN into the current `Task`, advances `current_task_id` (wraps at `task_count`), returns a pointer to the next task's `TaskContext`.

PendSV is set to the lowest priority (0xFF); SysTick is higher (0x11), so the context switch always runs after the SysTick handler returns.

### Task model (`source/task.hpp`)

Each `Task` holds a `TaskContext { uint32_t *sp, *sp_limit, excReturn }` plus entry point, stack size, ID, `TaskState` (ACTIVE / PAUSED / DEAD), and `TaskSignal` (NONE / SIG_KILL / SIG_PAUSE / SIG_RESUME).

Stack layout at task creation: `rtos_task_init()` carves a region from `psp_stack[]`, places a fake exception frame at the top (`XPSR_THREAD`, function address, `TerminateTask` as LR), and paints the remaining words with `0xDEADBEEF` for watermarking.

### Stack pool

`static uint32_t psp_stack[16384]` — shared pool for all task stacks (64 KB total). `find_stack_region()` first tries to reuse a DEAD task's slot (if `stack_size` fits), otherwise bumps the high-water pointer. Returns `nullptr` if the pool is exhausted.

### Watermarking

- `estimate_task_empty_space(id)` — counts leading `0xDEADBEEF` words from `sp_limit` upward.
- `task_size(id)` — returns words actually consumed (painted region minus untouched prefix).

### Entry point (`main.cpp`)

Creates tasks with `rtos_task_init(fn, stack_words)`, then calls `StartScheduler()` (which sets interrupt priorities, configures SysTick, and spins forever — tasks run via exceptions from that point).

### Board support (`frdmmcxn947_cm33_core0/`)

- `cm33_core0/hardware_init.c` — `BOARD_InitHardware()`: enables GPIO0 clock, calls `BOARD_InitPins()`, sets 12 MHz FRO clock, inits red LED (active-low, GPIO0 pin 10).
- `led_blinky/pin_mux.c` — auto-generated pin-mux config; regenerate via MCUXpresso Config Tools / `.mex` file, do not edit by hand.
- `frdmmcxn947/board.h` — LED macros (`LED_RED_INIT`, `LED_RED_ON`, `LED_RED_OFF`), `LOGIC_LED_ON = 0`.

## Key constants

| Constant | Value | Meaning |
|---|---|---|
| `MAX_NUMBER_OF_TASKS` | 32 | Hard task limit |
| `PSP_STACK_POOL_SIZE` | 16384 | Pool size in `uint32_t` words (64 KB) |
| `STACK_SIZE` | 2048 | Default stack size (words) |
| `NORM_STACK_FRAME_SIZE` | 50 | Words reserved at stack top for initial exception frame |
| `NUM_CALLEE_REGS` | 24 | Callee-saved words pushed by PendSV (R4–R11 + S16–S31) |
| `STACK_PAINT` | `0xDEADBEEF` | Watermark fill value |
| `EXC_RETURN_THREAD_S_PSP` | `0xFFFFFFED` | EXC_RETURN: thread mode, secure, PSP, FPU frame |

## Planned features (not yet implemented)

Signals (`task_signal()`), watchdog (auto-kill at 90% stack usage), `/proc` VFS (`/proc/tasks`, `/proc/uptime`, `/proc/signals`), and a serial shell (`ps`, `kill`, `pause`, `resume`, `cat`). See the global `~/.claude/CLAUDE.md` for the full feature spec.
