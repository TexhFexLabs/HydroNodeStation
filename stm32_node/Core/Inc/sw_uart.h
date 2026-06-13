#ifndef __SW_UART_H__
#define __SW_UART_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define SW_UART_TX_PORT       GPIOA
#define SW_UART_TX_PIN        GPIO_PIN_6
#define SW_UART_TX_PIN_INDEX  6U

/* TX only: the node never receives data from the PC.
 * PA7 (former RX) is wired to the BQ25185 charger CE pin, see bq25185.h. */

#define SW_UART_BAUDRATE      1200U

void SW_UART_Init(void);
void SW_UART_WriteByte(uint8_t b);
void SW_UART_WriteString(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* __SW_UART_H__ */
