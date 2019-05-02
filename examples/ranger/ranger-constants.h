#ifndef RANGER_CONSTANTS_H_
#define RANGER_CONSTANTS_H_

#include "os/net/linkaddr.h"
#include "ranger-types.h"

/*----------------------------------------------------------------------------*/
// Change these for your own use case
// Make sure that the interval is long enough to send and receive your messages

#define MAIN_INTERVAL_SECONDS   1
#define MAIN_INTERVAL           (CLOCK_SECOND * MAIN_INTERVAL_SECONDS)
#define CONTENT_SIZE            28
#define TX_POWER_DBM            14
#define CHANNEL                 26

#define ENABLE_CFG_HANDSHAKE    1
#define ENABLE_SEND_PIN         0
#define UNIQUE_ID               UINT32_C(0x30695444)
#define RX_RECEIVE_LEDS         LEDS_GREEN
#define TX_SEND_LEDS            LEDS_RED

/*----------------------------------------------------------------------------*/

extern const cc1200_rf_cfg_t cc1200_868_fsk_1_2kbps;
extern const cc1200_rf_cfg_t cc1200_802154g_863_870_fsk_50kbps;
extern const cc1200_rf_cfg_t cc1200_868_4gfsk_1000kbps;

static const cc1200_rf_cfg_t * const rf_cfg_ptrs[] = {&cc1200_868_fsk_1_2kbps, 
                                                      &cc1200_802154g_863_870_fsk_50kbps,
                                                      &cc1200_868_4gfsk_1000kbps};

enum {RF_CFG_AMOUNT = sizeof(rf_cfg_ptrs)/sizeof(rf_cfg_ptrs[0]),};

static const message empty_message;

static const linkaddr_t empty_linkaddr;
// node labelled "gateway" has link-addr: 0012.4b00.09df.4dee
// static const linkaddr_t src_linkaddr = {{0x00, 0x12, 0x4b, 0x00, 0x09, 0xdf, 0x4d, 0xee}};

#endif /* RANGER_CONSTANTS_H_ */