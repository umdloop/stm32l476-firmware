# stm32l476-firmware — Motherboard Firmware (Round‑Robin)

Target MCU: **STM32L476RGTX**  
Programmer: **STLINK‑V3MINIE**  
Board: custom motherboard (external clock + CAN transceiver)

## Goal

Separate the repo into:

- **Platform/**: hardware bring‑up + ST “plumbing” (interrupts, MSP, system init, syscalls)
- **App/**: cooperative round‑robin scheduler + systems (application behavior)
- **Core/**: minimal entrypoint (`main.c`) + ST config headers in `Core/Inc/`

## Architecture rules

- `Core/Src/main.c` does **only**:
  - HAL + clock init
  - calls `MX_*_Init()` for mandatory peripherals
  - starts the app scheduler
- Each system is its **own file** under `App/systems/`
- Scheduler is under `App/rr/`
- App headers live in `App/Inc/`
- Platform headers live in `Platform/Inc/`

## Directory tree (expected)

```
<ProjectRoot>/
├─ stm32l476-firmware.ioc
├─ .project
├─ .cproject                  (update to compile App/ and Platform/)
├─ Core/
│  ├─ Inc/                    (ST config headers)
│  │  ├─ main.h
│  │  ├─ stm32l4xx_hal_conf.h
│  │  └─ stm32l4xx_it.h
│  └─ Src/
│     └─ main.c               (entry point)
├─ Platform/
│  ├─ Inc/
│  │  ├─ gpio.h
│  │  ├─ can.h
│  │  └─ usart.h
│  └─ Src/
│     ├─ gpio.c
│     ├─ can.c
│     ├─ usart.c
│     ├─ system_stm32l4xx.c
│     ├─ stm32l4xx_it.c
│     ├─ stm32l4xx_hal_msp.c
│     ├─ syscalls.c
│     └─ sysmem.c
├─ App/
│  ├─ Inc/
│  │  ├─ app_config.h
│  │  ├─ rr_scheduler.h
│  │  └─ blink_system.h
│  ├─ rr/
│  │  └─ rr_scheduler.c
│  └─ systems/
│     └─ blink_system.c
├─ Drivers/                   (STM32 HAL + CMSIS)
├─ Startup/
├─ *.ld
└─ README.md
```

## Build note (important)

After reorganizing, `.cproject` must include:

- **Source folders**: `App`, `Platform` (in addition to `Core` and `Drivers`)
- **Include paths**: `../App/Inc`, `../Platform/Inc` (in addition to `../Core/Inc` and Drivers paths)

I provide an updated `.cproject` you can copy from `cproject_updated.xml` output in this chat (or I can paste it).

## Current system

- **BlinkSystem**: toggles LED on **PC5** at `BLINK_PERIOD_MS` using `HAL_GetTick()` (non‑blocking).
