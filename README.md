# stm32l476-firmware

Firmware for an STM32L476-based board using a **simple cooperative “round robin” scheduler**.  
Each “system” is a small module with a `*_controller()` function that is called once per scheduler tick.

This repo also includes a lightweight **CAN parameter database** driven by a **DBC file**, plus a CAN system that:
- **Creates all DBC parameters in RAM at startup**
- **Decodes incoming CAN frames into parameters**
- Provides a safe API for **other systems to request parameter changes** that will be **transmitted on CAN** on the CAN system’s next turn
- Exposes two global flags: `pending_inbox` and `pending_outbox`

---

## Quick start

### Building
Use your existing STM32CubeIDE / Make setup (this project is already configured for it).

### Running
`Core/Src/main.c` initializes HAL + platform drivers, registers systems with the round robin scheduler, and then calls:

- `RR_Scheduler_Tick()` in an infinite loop

---

## Project architecture

### Folder layout (high level)
- `Core/` – Cube/HAL entrypoints (main, interrupts, etc.)
- `Platform/` – board/platform drivers (clock, gpio, can, uart)
- `App/`
  - `Inc/` – public headers
  - `Src/` – application modules (CAN params, config, etc.)
  - `systems/` – “systems” that run in the round robin loop
  - `dbc/` – DBC file + generated C text blob
  - `rr/` – round robin scheduler implementation
- `tools/` – helper scripts (not used on-target)

---

## The round robin scheduler

The scheduler is intentionally simple and cooperative:
- There is **no preemption**
- Each controller function should run quickly and return
- Every loop iteration calls each registered controller exactly once, in order

### Scheduler API (global functions)

Declared in `App/Inc/rr_scheduler.h` and implemented in `App/rr/rr_scheduler.c`:

- `void RR_Scheduler_Init(void);`  
  Clears the controller list.

- `bool RR_AddController(rr_controller_t controller);`  
  Adds a controller to the list if it is not already present.

- `bool RR_RemoveController(rr_controller_t controller);`  
  Removes a controller from the list if present.

- `void RR_Scheduler_Tick(void);`  
  Calls each registered controller once.

### Where systems are registered
In `Core/Src/main.c`:

```c
RR_Scheduler_Init();
RR_AddController(can_system_controller);
RR_AddController(pcb_led_system_controller);

while (1)
{
  RR_Scheduler_Tick();
}
```

This is the “exact step” that makes a system run.

---

## How to write your own system (step-by-step)

1) **Create a header** in `App/Inc/`  
Example: `my_system.h`
```c
#ifndef MY_SYSTEM_H
#define MY_SYSTEM_H

void my_system_controller(void);

#endif
```

2) **Create the implementation** in `App/systems/`  
Example: `my_system.c`
```c
#include "my_system.h"

static uint8_t s_inited = 0U;

static void init_once(void)
{
  /* one-time init */
}

void my_system_controller(void)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_once();
  }

  /* do small amount of work and return */
}
```

3) **Include your header** in `Core/Src/main.c`

4) **Register it** with the scheduler:
```c
RR_AddController(my_system_controller);
```

That’s it. Your system will now be called once per tick.

### System design guidelines
- Keep controller runtime short (no long loops, no blocking waits)
- Use static state + `init_once()` to do one-time initialization
- If you need periodic behavior, track time via counters/timestamps inside your controller

---

## CAN architecture overview

The CAN stack is split into 3 pieces:

1) **Platform CAN driver**  
`Platform/Inc/can.h`, `Platform/Src/can.c` initialize bxCAN (`hcan1`).

2) **CAN parameters database** (`can_params`)  
A RAM-backed table of named parameters addressed by strings like `"MESSAGE.SIGNAL"`.

3) **CAN system** (`can_system`)  
Parses the DBC, creates parameters, decodes RX frames into parameters, and sends TX frames based on requested parameter changes.

---

## CAN parameter database (`can_params`)

Parameters are referenced by full names:  
`"MESSAGE.SIGNAL"` (example: `SERVO_PCB_C.led_status`)

### Public CanParams API (global functions)

Declared in `App/Inc/can_params.h` and implemented in `App/Src/can_params.c`:

- `bool CanParams_IsValid(const char* full_name);`  
  Returns true if the parameter exists **and has been set at least once** (i.e., “valid”).

- `bool CanParams_GetBool(const char* full_name, bool* out_value);`  
- `bool CanParams_GetInt32(const char* full_name, int32_t* out_value);`  
- `bool CanParams_GetFloat(const char* full_name, float* out_value);`  
  Read a parameter **only if it exists and is valid**. Returns `false` if not valid.

- `bool CanParams_SetBool(const char* full_name, bool value);`  
- `bool CanParams_SetInt32(const char* full_name, int32_t value);`  
- `bool CanParams_SetFloat(const char* full_name, float value);`  
  Sets a parameter value and marks it valid.  
  (Note: for CAN TX you typically use the `CanSystem_Set*()` APIs instead; see below.)

### Internal-only helpers
Used by the CAN system to avoid echoing RX updates back onto the bus:
- `CanParams__Reset()`
- `CanParams__Create()`
- `CanParams__UpdateBool/Int32/Float()`

**Important:** `CanParams__Update*()` updates values from RX **without** requesting a TX.

---

## CAN system (`can_system`)

The CAN system runs in the round robin and does:

1) **First call only**
   - Parses the DBC text (`App/dbc/can_dbc_text.c`)
   - Creates *all* DBC parameters in RAM via `CanParams__Create(...)`
   - Creates two extra global parameters:
     - `pending_inbox` (bool)
     - `pending_outbox` (bool)
   - Applies CAN ID filters
   - Starts CAN and enables FIFO0 RX notification

2) **Every tick**
   - Drains RX FIFO0 and decodes frames into parameters
   - Updates `pending_inbox`
   - If `pending_outbox == true`, sends any pending dirty messages and clears it

### CAN system controller
- `void can_system_controller(void);`  
  Registered in the round robin. Owns parsing, RX, TX, and pending flags.

### The two global CAN flags
These are parameters accessible through `CanParams_GetBool()` / `CanParams_SetBool()`:

- `pending_inbox`  
  Set to `true` if **any incoming CAN frame** caused **any parameter** to change during the last tick.  
  If the CAN system runs again and no parameters changed, it will be set back to `false`.

- `pending_outbox`  
  Set to `true` when any system requests a parameter change through the `CanSystem_Set*()` APIs.  
  The CAN system clears it after transmitting all pending messages.

### External TX request API (global functions)

Declared in `App/Inc/can_system.h` and implemented in `App/systems/can_system.c`:

- `bool CanSystem_SetBool(const char* full_name, bool value);`
- `bool CanSystem_SetInt32(const char* full_name, int32_t value);`
- `bool CanSystem_SetFloat(const char* full_name, float value);`

Use these when you want to **send a signal on CAN**.

What they do:
1) Validate the parameter exists in the DBC-parsed table
2) Update the parameter value (marks it valid)
3) Mark the owning CAN message “dirty”
4) Set `pending_outbox = true`

On the CAN system’s next scheduler tick, it will:
- Encode the appropriate message(s)
- Send them using `HAL_CAN_AddTxMessage`
- Clear `pending_outbox`

### Important TX rule: no RX echo
Incoming CAN updates use `CanParams__Update*()` (internal) so they **do not** mark messages dirty.  
That prevents the CAN system from “echoing” received frames back out.

---

## CAN RX filtering (allowlist)

Config lives in:

- `App/Src/can_config.c`
- `App/Inc/can_config.h`

Edit `g_can_rx_id_filter[]` to restrict which **standard** arbitration IDs will be decoded into parameters.

Rules:
- If `g_can_rx_id_filter_count == 0` (empty list): accept **all** standard IDs
- If non-empty: only those IDs are decoded

The CAN system will:
1) Try to apply the allowlist as **hardware filters** (IDLIST mode) as far as filter banks allow
2) Enforce the allowlist again in **software**, so extra IDs never become parameters even if hardware banks run out

---

## DBC workflow (how to contribute protocols)

### Source of truth
- `App/dbc/file.dbc` is the DBC you edit.

### Generated file
- `App/dbc/can_dbc_text.c` is auto-generated.
- **Do not hand-edit** `can_dbc_text.c`.

Generation is done by:
- `tools/dbc_to_c.py`

Example command (from repo root):
```bash
python tools/dbc_to_c.py App/dbc/file.dbc App/dbc/can_dbc_text.c
```

### Naming convention (parameters)
Each DBC signal becomes one parameter named:

`<MessageName>.<SignalName>`

Example:
- Message: `SERVO_PCB_C`
- Signal: `led_status`
- Parameter: `SERVO_PCB_C.led_status`

### Multiplexing support
This firmware supports classic DBC multiplexing tokens in `SG_` lines:
- `M` for the multiplexor signal
- `m17M` for a signal active only when mux == 17

The CAN system will:
- Read the mux value first
- Only decode signals whose `mXXM` matches the mux value
- When transmitting, force the mux field and encode only the active mux page

**Tip:** For mux messages, always define the mux signal explicitly in the DBC (example: `SG_ cmd M : ...`).

### After editing the DBC
1) Update `App/dbc/file.dbc`
2) Run `tools/dbc_to_c.py ...` to regenerate `App/dbc/can_dbc_text.c`
3) Rebuild firmware
4) Test using your CAN tool (CANable, etc.)

---

## Example: controlling the onboard PCB LED via CAN

Your DBC defines (example):
- Message ID: `0x080` (`SERVO_PCB_C`)
- Multiplexor: `cmd`
- LED signal: `led_status` active when `cmd == 0x11` (17)

To command the LED over CAN, send:
- **LED ON**: `080#1101000000000000`
- **LED OFF**: `080#1100000000000000`

(8-byte payload is recommended for consistent tools.)

The LED system reads:
- `SERVO_PCB_C.led_status`

and updates the physical LED GPIO accordingly.

---

## Common pitfalls and troubleshooting

- **No parameters update at all**
  - Ensure the DBC was regenerated into `can_dbc_text.c`
  - Ensure CAN filters aren’t blocking your ID (`can_config.c`)
  - Ensure the sender is using **standard ID** (not extended) if your DBC expects standard
  - Ensure bitrate matches the platform CAN configuration

- **Muxed signals don’t update**
  - Confirm the mux signal is defined with `M`
  - Confirm the signal is defined with `mXXM`
  - Confirm you are sending the mux value (`cmd`) correctly in the payload

- **TX doesn’t occur**
  - Confirm you used `CanSystem_Set*()` (not `CanParams_Set*()`) for outgoing requests
  - Confirm `pending_outbox` becomes true (you can check it through `CanParams_GetBool`)

---

## Notes for contributors

- Keep systems small and deterministic.
- Prefer using the CAN parameter database as the interface between systems.
- Avoid adding blocking behavior in controller functions.
- Treat the DBC as the contract: if it changes, regenerate `can_dbc_text.c` and update any system parameter names accordingly.
