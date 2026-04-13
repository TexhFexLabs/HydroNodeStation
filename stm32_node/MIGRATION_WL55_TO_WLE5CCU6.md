# Migration: STM32WL55JCI6 → STM32WLE5CCU6

Custom board mit **BALFHB-WL-02D3** (Balun) und **BGS12SN6E6327XTSA1** (RF-Switch, CTL an PC13).

---

## Warum der Code auf dem Wio-E5 (WLE5JC) bereits läuft

Der WL55 und WLE5 teilen denselben CM4-Peripherieblock (GPIO, UART, RTC, SubGHz).
Das `STM32WL55xx`-Define aktiviert zusätzlich CM0+-, IPCC- und DMA-Security-Register –
die auf dem WLE5 nicht existieren, aber solange sie nie angesprochen werden, passiert nichts.
Das BSP `STM32WLxx_LoRa_E5_mini` ist von Anfang an für das Wio-E5 (WLE5JC) geschrieben.

Für den **WLE5CCU6 auf einer Custom-Platine** müssen trotzdem zwei Dinge geändert werden:
1. Das Build-System sauber auf `STM32WLE5xx` umstellen
2. Den RF-Switch-BSP für den BGS12SN6 (1 CTL-Pin) anpassen

---

## Paketunterschied: UFBGA73 → UFQFPN48

| | WL55JCI6 / WLE5JC | WLE5CCU6 |
|---|---|---|
| Package | UFBGA73 (73 Balls) | UFQFPN48 (48 Pins) |
| Port C | PC0–PC15 | **nur PC13–PC15** |
| Port B | PB0–PB15 | PB0–PB12 (**PB13–PB15 nicht verfügbar**) |
| Port A | PA0–PA15 | PA0–PA15 |

Aktuell genutzte Pins und Status auf UFQFPN48:

| Pin | Funktion | Status |
|-----|----------|--------|
| PA0 | BUT1 | OK |
| PA1 | BUT2 | OK |
| PA2 | USART TX | OK |
| PA3 | USART RX | OK |
| PB9 | LED2 | OK |
| PB11 | LED3 | OK |
| PB12 | PROB1 | OK |
| PB13 | PROB2 | **nicht verfügbar** – bei Bedarf auf PB0–PB9 remappen |
| PB15 | LED1 | **nicht verfügbar** – bei Bedarf auf freien PB-Pin remappen |
| PC6 | BUT3 | **nicht verfügbar** – auf PA/PB remappen |
| PC13 | RF-Switch CTL | OK (neue Funktion) |

---

## Änderung 1: CMake Define

**Datei:** `cmake/stm32cubemx/CMakeLists.txt`

```cmake
# Vorher:
set(MX_Defines_Syms
    NUMBER_OF_STACKS=1
    SX126X
    ENDNODE
    CORE_CM4
    USE_HAL_DRIVER
    STM32WL55xx          # <-- ändern
    $<$<CONFIG:Debug>:DEBUG>
)

# Nachher:
set(MX_Defines_Syms
    NUMBER_OF_STACKS=1
    SX126X
    ENDNODE
    CORE_CM4
    USE_HAL_DRIVER
    STM32WLE5xx          # <-- geändert
    $<$<CONFIG:Debug>:DEBUG>
)
```

---

## Änderung 2: Startup-Datei

**Datei:** `cmake/stm32cubemx/CMakeLists.txt`

```cmake
# Vorher:
${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32wl55xx_cm4.s

# Nachher:
${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32wle5xx.s
```

Die Datei `startup_stm32wle5xx.s` muss aus dem **STM32CubeWL-Package** geholt oder
über **STM32CubeMX** (neues Projekt mit WLE5CCU6 anlegen) generiert werden.
Sie liegt im CubeWL-Package unter:
`Drivers/CMSIS/Device/ST/STM32WLxx/Source/Templates/gcc/startup_stm32wle5xx.s`

---

## Änderung 3: BSP RF-Switch Header

**Datei:** `Drivers/BSP/STM32WLxx_LoRa_E5_mini/stm32wlxx_LoRa_E5_mini_radio.h`

Der Wio-E5 nutzt **zwei CTL-Pins** (PA4 + PA5). Das neue Custom-Board hat den
**BGS12SN6E6327XTSA1** (SPDT, ein CTL-Pin) an **PC13**.

```c
// Vorher (2-Pin, Wio-E5):
#define RF_SW_CTRL1_PIN                          GPIO_PIN_4
#define RF_SW_CTRL1_GPIO_PORT                    GPIOA
#define RF_SW_CTRL1_GPIO_CLK_ENABLE()            __HAL_RCC_GPIOA_CLK_ENABLE()
#define RF_SW_RX_GPIO_CLK_DISABLE()              __HAL_RCC_GPIOA_CLK_DISABLE()

#define RF_SW_CTRL2_PIN                          GPIO_PIN_5
#define RF_SW_CTRL2_GPIO_PORT                    GPIOA
#define RF_SW_CTRL2_GPIO_CLK_ENABLE()            __HAL_RCC_GPIOA_CLK_ENABLE()
#define RF_SW_CTRL2_GPIO_CLK_DISABLE()           __HAL_RCC_GPIOA_CLK_DISABLE()

// Nachher (1-Pin, BGS12SN6 an PC13):
/* BGS12SN6E6327: SPDT, ein CTL-Pin
 * CTL=LOW  -> RFC zu RF2 (RX-Pfad)
 * CTL=HIGH -> RFC zu RF1 (TX-Pfad)
 * Polarität anhand PCB-Layout / Schaltplan des BALFHB-WL-02D3 verifizieren */
#define RF_SW_CTRL_PIN                           GPIO_PIN_13
#define RF_SW_CTRL_GPIO_PORT                     GPIOC
#define RF_SW_CTRL_GPIO_CLK_ENABLE()             __HAL_RCC_GPIOC_CLK_ENABLE()
#define RF_SW_CTRL_GPIO_CLK_DISABLE()            __HAL_RCC_GPIOC_CLK_DISABLE()
```

---

## Änderung 4: BSP RF-Switch Source

**Datei:** `Drivers/BSP/STM32WLxx_LoRa_E5_mini/stm32wlxx_LoRa_E5_mini_radio.c`

### BSP_RADIO_Init()

```c
// Vorher:
int32_t BSP_RADIO_Init(void)
{
  GPIO_InitTypeDef gpio_init_structure = {0};

  RF_SW_CTRL1_GPIO_CLK_ENABLE();

  gpio_init_structure.Pin   = RF_SW_CTRL1_PIN;
  gpio_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio_init_structure.Pull  = GPIO_NOPULL;
  gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(RF_SW_CTRL1_GPIO_PORT, &gpio_init_structure);

  gpio_init_structure.Pin = RF_SW_CTRL2_PIN;
  HAL_GPIO_Init(RF_SW_CTRL2_GPIO_PORT, &gpio_init_structure);

  HAL_GPIO_WritePin(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_RESET);

  return BSP_ERROR_NONE;
}

// Nachher:
int32_t BSP_RADIO_Init(void)
{
  GPIO_InitTypeDef gpio_init_structure = {0};

  RF_SW_CTRL_GPIO_CLK_ENABLE();

  gpio_init_structure.Pin   = RF_SW_CTRL_PIN;
  gpio_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio_init_structure.Pull  = GPIO_NOPULL;
  gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(RF_SW_CTRL_GPIO_PORT, &gpio_init_structure);

  HAL_GPIO_WritePin(RF_SW_CTRL_GPIO_PORT, RF_SW_CTRL_PIN, GPIO_PIN_RESET);

  return BSP_ERROR_NONE;
}
```

### BSP_RADIO_DeInit()

```c
// Vorher:
int32_t BSP_RADIO_DeInit(void)
{
  RF_SW_RX_GPIO_CLK_DISABLE();
  HAL_GPIO_WritePin(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_RESET);
  HAL_GPIO_DeInit(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN);
  HAL_GPIO_DeInit(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN);
  return BSP_ERROR_NONE;
}

// Nachher:
int32_t BSP_RADIO_DeInit(void)
{
  HAL_GPIO_WritePin(RF_SW_CTRL_GPIO_PORT, RF_SW_CTRL_PIN, GPIO_PIN_RESET);
  HAL_GPIO_DeInit(RF_SW_CTRL_GPIO_PORT, RF_SW_CTRL_PIN);
  RF_SW_CTRL_GPIO_CLK_DISABLE();
  return BSP_ERROR_NONE;
}
```

### BSP_RADIO_ConfigRFSwitch()

```c
// Vorher (2-Pin-Logik für Wio-E5):
int32_t BSP_RADIO_ConfigRFSwitch(BSP_RADIO_Switch_TypeDef Config)
{
  switch (Config)
  {
    case RADIO_SWITCH_OFF:
      HAL_GPIO_WritePin(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_RESET);
      break;
    case RADIO_SWITCH_RX:
      HAL_GPIO_WritePin(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_SET);
      HAL_GPIO_WritePin(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_RESET);
      break;
    case RADIO_SWITCH_RFO_LP:
      HAL_GPIO_WritePin(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_SET);
      HAL_GPIO_WritePin(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_SET);
      break;
    case RADIO_SWITCH_RFO_HP:
      HAL_GPIO_WritePin(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_SET);
      break;
    default:
      break;
  }
  return BSP_ERROR_NONE;
}

// Nachher (1-Pin-Logik für BGS12SN6 an PC13):
int32_t BSP_RADIO_ConfigRFSwitch(BSP_RADIO_Switch_TypeDef Config)
{
  switch (Config)
  {
    case RADIO_SWITCH_OFF:
      HAL_GPIO_WritePin(RF_SW_CTRL_GPIO_PORT, RF_SW_CTRL_PIN, GPIO_PIN_RESET);
      break;
    case RADIO_SWITCH_RX:
      /* CTL=LOW -> RX-Pfad (RFC zu RF2) */
      HAL_GPIO_WritePin(RF_SW_CTRL_GPIO_PORT, RF_SW_CTRL_PIN, GPIO_PIN_RESET);
      break;
    case RADIO_SWITCH_RFO_LP:
    case RADIO_SWITCH_RFO_HP:
      /* CTL=HIGH -> TX-Pfad (RFC zu RF1) */
      HAL_GPIO_WritePin(RF_SW_CTRL_GPIO_PORT, RF_SW_CTRL_PIN, GPIO_PIN_SET);
      break;
    default:
      break;
  }
  return BSP_ERROR_NONE;
}
```

> **Wichtig:** Falls nach dem ersten Test kein RX oder kein TX funktioniert,
> sind `GPIO_PIN_RESET` und `GPIO_PIN_SET` in `RADIO_SWITCH_RX` / `RADIO_SWITCH_RFO_HP`
> zu tauschen. Abhängig davon, ob RF1 oder RF2 auf der Platine zum TX-Port des BALFHB-WL-02D3 geht.

---

## Änderung 5: STM32CubeMX (optional, für Neugenerierung)

1. **File → New Project** → `STM32WLE5CCU6` auswählen
2. Peripherals identisch konfigurieren (SUBGHZ, USART, GPIO, RTC, ADC, DMA)
3. PC6 (BUT3) auf verfügbaren Pin verlegen (PC6 nicht im UFQFPN48)
4. **Project Manager** → gleichen Projektnamen und Pfad wie bisher
5. Code generieren → `startup_stm32wle5xx.s` und neues Linker-Script werden erzeugt

> Alternativ: Startup und Linker-Script direkt aus dem STM32CubeWL-Package kopieren
> (`Drivers/CMSIS/Device/ST/STM32WLxx/Source/Templates/`)

---

## Zusammenfassung der geänderten Dateien

| Datei | Was ändert sich |
|-------|----------------|
| `cmake/stm32cubemx/CMakeLists.txt` | `STM32WL55xx` → `STM32WLE5xx`, Startup-Dateiname |
| `Drivers/BSP/.../stm32wlxx_LoRa_E5_mini_radio.h` | RF-Switch-Defines: 2 Pins → 1 Pin (PC13) |
| `Drivers/BSP/.../stm32wlxx_LoRa_E5_mini_radio.c` | Init/DeInit/ConfigRFSwitch vereinfacht |
| `startup_stm32wle5xx.s` | Neue Datei aus CubeMX/CubeWL-Package |
| `Core/Inc/main.h` | BUT3 von PC6 auf verfügbaren Pin (falls genutzt) |
