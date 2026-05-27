# mcx-npu-rtos

A minimal preemptive RTOS for the **NXP FRDM-MCXN947** (MCXN947, ARM Cortex-M33), built bare-metal from the MCUXpresso `led_blinky` SDK example as a learning project.

## Hardware

- Board: FRDM-MCXN947
- MCU: MCXN947 (Cortex-M33, core 0)
- Clock: 12 MHz FRO (`BOARD_BootClockFRO12M`)

## Architecture

### Scheduler

Preemptive round-robin scheduler built on two ARM exception handlers:

- **SysTick** — fires at 1 kHz, sets `ICSR.PENDSVSET` to request a context switch.
- **PendSV** — naked-assembly handler. Saves R4–R11 (and S16–S31 when an FPU frame is live) onto the preempted task's PSP, calls a C++ scheduler core, then restores the next task's registers and switches `PSP`/`PSPLIM`.

PendSV runs at the lowest interrupt priority, so the context switch always executes after the SysTick handler returns.

### Tasks

Each task holds a `TaskContext { sp, sp_limit, exc_return }`, its entry point, stack size, state (`ACTIVE` / `PAUSED` / `DEAD`), and a pending signal slot.

Stacks are carved out of a shared 64 KB pool and painted with `0xDEADBEEF` for watermarking. The pool allocator reuses the slots of `DEAD` tasks when possible.

### Stack inspection

- `task_size(id)` — high-water mark, derived from how much paint has been overwritten.
- `estimate_task_empty_space(id)` — count of untouched (still-painted) cells.
- `task_stack_map(id, width)` — compact ASCII bar showing painted vs. touched buckets across the stack.

## Build

Requires the `SdkRootDirPath` environment variable, pointing at the MCUXpresso SDK root (toolchain expected at `$SdkRootDirPath/mcuxsdk/cmake/toolchain/armgcc.cmake`).

```bash
# configure
cmake --preset debug

# build (output: debug/mcx-npu-rtos_cm33_core0.elf)
cmake --build --preset debug

# release build
cmake --preset release && cmake --build --preset release
```

In VS Code, the default build task (`Ctrl+Shift+B`) runs the debug preset via the CMake extension.

## Flash & debug

Use the VS Code MCUXpresso extension (`mcuxpresso-debug` launch type) with LinkServer, targeting `MCXN947:FRDM-MCXN947`. Connect the board to the host via USB on the MCU-Link port (J17). The debug session stops at `main` on launch.

## Planned features

- Signal-driven task control (`task_signal()`) with `SIG_KILL` / `SIG_PAUSE` / `SIG_RESUME`
- Stack-overflow watchdog: tripwire check in PendSV, auto-`SIG_KILL` at the 90% threshold
- `/proc`-style VFS: `/proc/tasks`, `/proc/uptime`, `/proc/signals`
- Serial shell over UART: `ps`, `kill`, `pause`, `resume`, `cat`

## AI use

Developed with assistance from Claude (Anthropic) for design discussion, debugging, refactoring, code commenting, and commit message composition. Implementation and architectural decisions are mine.
