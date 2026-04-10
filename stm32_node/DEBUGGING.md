# STM32WLE5 / SMTC Modem Stack — Debugging Post-Mortem

## Symptom

Board boots (LED blinks), but no serial output and no LoRaWAN join. The firmware hangs silently during `smtc_modem_init()`.

---

## Root Cause

**RNG peripheral clock source was set to LSE (32.768 kHz) — way too slow.**

The STM32WL RNG requires a clock of at least `AHB_CLK / 32`. With AHB at 48 MHz that is **1.5 MHz minimum**. LSE runs at 32.768 kHz, which is ~46× too slow. The hardware clock-error-detection (CED) fires immediately, `HAL_RNG_GenerateRandomNumber` returns `HAL_ERROR`, and `Error_Handler()` spins forever.

### The buggy line

`stm32_node/Core/Src/rng.c`, inside `HAL_RNG_MspInit`:

```c
// WRONG — 32.768 kHz, below the 1.5 MHz minimum
PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_LSE;
```

### The fix

```c
// CORRECT — MSI at 48 MHz, well above the minimum
PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_MSI;
```

Valid fast clock sources for the STM32WL RNG: **MSI (48 MHz)** or **HSI (16 MHz)**.  
Do NOT use LSE (32.768 kHz) or LSI (32 kHz) — both are too slow.

Also update `stm32_node.ioc` → `RNGClockSelection` to match, so CubeMX does not regenerate the wrong value.

---

## How It Was Found — The Debugging Method

The SMTC modem stack has no early serial output and the hang happens deep inside middleware, so classic printf-debugging was needed before the UART trace system initialises.

### The `boot_print()` helper

A tiny polling-mode UART transmit wrapper was added to `Core/Src/main.c`:

```c
void boot_print(const char *msg) {
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 100);
}
```

Declared in `Core/Inc/main.h`. It works before `UTIL_ADV_TRACE` is ready because it is fully blocking.

### Binary-search strategy

Add `boot_print("... step X\r\n")` before and after a suspected call. Flash, read serial. The last printed line before silence = the hang is inside (or just after) that call. Bisect inward until you reach one function.

### Call chain traced

```
smtc_modem_init()                          [smtc_modem.c]
  └─ ral_reset / ral_init / ral_set_sleep  → OK
  └─ modem_context_init_light()            [modem_core.c]
       └─ lorawan_api_init()               [lorawan_api.c]
       |    └─ lr1mac_core_init            → OK
       |    └─ lr1mac_class_c_init         → OK
       └─ lorawan_api_dr_strategy_set()    ← HANGS HERE
            └─ lr1mac_core_dr_strategy_set()
                 └─ smtc_real_get_next_tx_dr()
                      └─ smtc_modem_hal_get_random_nb_in_range()  [inline, smtc_modem_hal.h]
                           └─ HandlerCallbacks->GetRandomValue()
                                └─ HAL_RNG_GenerateRandomNumber()
                                     └─ Error_Handler()  ← infinite loop
```

### Files that had temporary instrumentation added

All of these should be **reverted / cleaned** once the fix is confirmed:

- `Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem.c`
- `Middlewares/Third_Party/SubGHz_Phy/lorawan/smtc_ral/src/ral_sx126x.c`
- `Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_utilities/modem_core.c`
- `Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_api/lorawan_api.c`

Each had `#include "main.h"` added at the top and `boot_print(...)` calls inserted at function entry/exit points.

---

## Other Fixes Applied in the Same Session

### Fix 1 — SUBGHZ not initialised before smtc_modem_init

The SUBGHZ peripheral was not initialised before `smtc_modem_init()` was called.

**Fix:** Add `MX_SUBGHZ_Init()` explicitly before `smtc_modem_init()` in `LoRaWAN/App/lora_app.c` inside `LoRaWAN_Init()`. CubeMX does not wire this automatically for the SMTC modem variant.

---

## Bug 2 — JOIN sent but JOINFAIL (RX windows never close)

### Symptom

Device boots, serial output appears, JOIN REQUESTs are transmitted (confirmed on Helium with excellent RSSI/SNR), but JOINFAIL is reported ~240 s after each TX attempt. Log shows:

```
rp_callback (line 488): ERROR   ← Radio Planner FAILSAFE
```

### Root Cause

**`SUBGHZ_Radio_IRQn` was never enabled in the NVIC.**

The CubeMX `.ioc` has `NVIC.SUBGHZ_Radio_IRQn=true\:0\:0\:...`, but `Core/Src/subghz.c` was not (re)generated correctly — `HAL_SUBGHZ_MspInit` only enables the peripheral clock, missing the two NVIC lines.

Without `HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn)`, the radio's DIO1 interrupt (EXTI44) never reaches the CPU, so `SUBGHZ_Radio_IRQHandler` never fires. The radio planner's `radio_irq_flag` is never set for `RX_DONE` / `RX_TIMEOUT`, so the RX task stays in `RP_TASK_STATE_RUNNING` forever. After 128 s the Radio Planner failsafe aborts it and reports JOINFAIL.

Note: `SMTC_MODEM_HAL_PANIC` is defined as a log-only macro (no reset), so the failsafe fires and the modem retries — producing the observed 240 s JOINFAIL cycle.

### The buggy code

`Core/Src/subghz.c`, inside `HAL_SUBGHZ_MspInit`:

```c
// WRONG — NVIC lines missing; radio IRQs silently lost
__HAL_RCC_SUBGHZSPI_CLK_ENABLE();
```

### The fix

```c
// CORRECT — enables SUBGHZ_Radio_IRQn so SUBGHZ_Radio_IRQHandler fires
__HAL_RCC_SUBGHZSPI_CLK_ENABLE();
HAL_NVIC_SetPriority(SUBGHZ_Radio_IRQn, 0, 0);
HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
```

This matches the working `software/` project (`LoRaWAN_End_Node/Core/Src/subghz.c`, lines 61–62) and what CubeMX generates when the NVIC line is ticked in the `.ioc`.

---

## Hardware Notes (Wio E5 mini)

| Signal      | Pin  | Notes                        |
|-------------|------|------------------------------|
| USART1 TX   | PB6  | 115200, 8N1                  |
| USART1 RX   | PB7  |                              |
| LED         | PB5  |                              |
| TCXO enable | PB0  | Must be driven high at boot  |
| RF switch   | PA4  | RX path                      |
| RF switch   | PA5  | TX path                      |

---

## Quick Checklist for "Boots but no serial / no join"

1. Is USART1 (PB6/PB7) initialised and baud correct? Check with `boot_print` as early as possible.
2. Is `MX_SUBGHZ_Init()` called before `smtc_modem_init()`?
3. Is the RNG clock source a *fast* source (MSI or HSI)?  Check `rng.c` → `HAL_RNG_MspInit` → `RngClockSelection`.
4. Is the TCXO enabled before radio init? Check `radio_board_if.c` → `RADIO_BOARD_IoInit`.
5. Is `Error_Handler` being called silently? Add a `boot_print` or a breakpoint inside it.
6. **JOIN REQUEST sent but JOINFAIL after ~240 s?** Check `Core/Src/subghz.c` → `HAL_SUBGHZ_MspInit` — must have `HAL_NVIC_SetPriority(SUBGHZ_Radio_IRQn, 0, 0)` AND `HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn)` after `__HAL_RCC_SUBGHZSPI_CLK_ENABLE()`.
