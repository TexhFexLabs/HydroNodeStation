#ifndef LINK_CHECK_H
#define LINK_CHECK_H
#include <stdint.h>
#include <stdbool.h>
/* Daily LinkCheckReq: detects a lost server context or a dead gateway that
 * unconfirmed uplinks never reveal. Three unanswered days in a row trigger
 * a rejoin. Times are MCU monotonic seconds. */
#define LINK_CHECK_INTERVAL_S   86400U
#define LINK_CHECK_REJOIN_DAYS  3U
typedef struct { uint32_t due_at; uint8_t missed; uint8_t rejoined_after; bool rejoining; } link_check_t;
/* After every join. Returns the unanswered days that caused the rejoin
 * (0 if this join was not a link-check rejoin). */
uint8_t LinkCheck_Joined(link_check_t *lc, uint32_t now);
bool LinkCheck_Due(const link_check_t *lc, uint32_t now);
void LinkCheck_Requested(link_check_t *lc, uint32_t now);
/* Returns true when the station must leave the network and rejoin. */
bool LinkCheck_Result(link_check_t *lc, bool answered);
#endif
