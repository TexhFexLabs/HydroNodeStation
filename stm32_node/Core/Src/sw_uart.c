#include "sw_uart.h"

static uint32_t sw_uart_bit_cycles = 0U;
static uint32_t sw_uart_edge_guard_cycles = 0U;

static void SW_UART_WaitUntil(uint32_t target_cycles)
{
  while ((int32_t)(DWT->CYCCNT - target_cycles) < 0)
  {
  }
}

static void SW_UART_WaitUntilEdge(uint32_t target_cycles)
{
  const uint32_t early_target = target_cycles - sw_uart_edge_guard_cycles;

  while ((int32_t)(DWT->CYCCNT - early_target) < 0)
  {
  }
}

static void SW_UART_SetTxLevel(uint8_t high)
{
  if (high != 0U)
  {
    SW_UART_TX_PORT->BSRR = SW_UART_TX_PIN;
  }
  else
  {
    SW_UART_TX_PORT->BSRR = ((uint32_t)SW_UART_TX_PIN << 16U);
  }
}

void SW_UART_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = SW_UART_TX_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(SW_UART_TX_PORT, &GPIO_InitStruct);

  SW_UART_SetTxLevel(1U);

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  DWT->CYCCNT = 0U;

  sw_uart_bit_cycles = (SystemCoreClock + (SW_UART_BAUDRATE / 2U)) / SW_UART_BAUDRATE;
  sw_uart_edge_guard_cycles = sw_uart_bit_cycles / 16U;
  if (sw_uart_edge_guard_cycles < 32U)
  {
    sw_uart_edge_guard_cycles = 32U;
  }
}

void SW_UART_WriteByte(uint8_t b)
{
  uint32_t i;
  uint32_t edge_time;
  uint32_t primask;

  edge_time = DWT->CYCCNT;
  SW_UART_SetTxLevel(0U);
  edge_time += sw_uart_bit_cycles;

  for (i = 0U; i < 8U; i++)
  {
    SW_UART_WaitUntilEdge(edge_time);
    primask = __get_PRIMASK();
    __disable_irq();
    SW_UART_WaitUntil(edge_time);
    SW_UART_SetTxLevel((uint8_t)((b >> i) & 0x01U));
    if (primask == 0U)
    {
      __enable_irq();
    }
    edge_time += sw_uart_bit_cycles;
  }

  SW_UART_WaitUntilEdge(edge_time);
  primask = __get_PRIMASK();
  __disable_irq();
  SW_UART_WaitUntil(edge_time);
  SW_UART_SetTxLevel(1U);
  if (primask == 0U)
  {
    __enable_irq();
  }
  edge_time += sw_uart_bit_cycles;
  SW_UART_WaitUntil(edge_time);
}

void SW_UART_WriteString(const char *s)
{
  if (s == NULL)
  {
    return;
  }

  while (*s != '\0')
  {
    SW_UART_WriteByte((uint8_t)*s);
    s++;
  }
}
