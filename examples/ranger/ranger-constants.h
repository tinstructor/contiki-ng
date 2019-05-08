#ifndef RANGER_CONSTANTS_H_
#define RANGER_CONSTANTS_H_

#include "os/net/linkaddr.h"
#include "arch/dev/cc1200/cc1200-rf-cfg.h"
#include "ranger-types.h"

/*----------------------------------------------------------------------------*/
// Change these for your own use case
// Make sure that the interval is long enough to send and receive your messages

#define MAIN_INTERVAL           6*(CLOCK_SECOND/10)
#define CONTENT_SIZE            28
#define TX_POWER_DBM            14
#define CHANNEL                 26

#define AUTO_MEASURE_SECONDS    10
#define AUTO_MEASURE_INTERVAL   (CLOCK_SECOND * AUTO_MEASURE_SECONDS)
#define ENABLE_AUTO_MEASURE     1
#define ENABLE_CFG_HANDSHAKE    0
#define ENABLE_SEND_PIN         0
#define ENABLE_UART_INPUT       1
#define BURST_AMOUNT            3
#define UNIQUE_ID               UINT32_C(0x30695444)
#define RX_RECEIVE_LED          RGB_LED_GREEN
#define TX_SEND_LED             RGB_LED_RED

#define HANDSHAKE_TX_DELAY      2*(CLOCK_SECOND/10)+2*CLOCK_SECOND
#define HANDSHAKE_RX_DELAY      8*(CLOCK_SECOND/10)+1*CLOCK_SECOND

/*----------------------------------------------------------------------------*/

extern const cc1200_rf_cfg_t cc1200_868_2fsk_1_2kbps;
extern const cc1200_rf_cfg_t cc1200_868_2gfsk_19_2kbps;
extern const cc1200_rf_cfg_t cc1200_868_2gfsk_50kbps;
extern const cc1200_rf_cfg_t cc1200_868_2gfsk_100kbps;
extern const cc1200_rf_cfg_t cc1200_868_4gfsk_150kbps;
extern const cc1200_rf_cfg_t cc1200_868_4gfsk_200kbps;
extern const cc1200_rf_cfg_t cc1200_868_2gfsk_500kbps;
extern const cc1200_rf_cfg_t cc1200_868_4gfsk_1000kbps;

static const cc1200_rf_cfg_t * const rf_cfg_ptrs[] = {&cc1200_868_2fsk_1_2kbps,
                                                      &cc1200_868_2gfsk_19_2kbps,
                                                      &cc1200_868_2gfsk_50kbps,
                                                      &cc1200_868_2gfsk_100kbps,
                                                      &cc1200_868_4gfsk_150kbps,
                                                      &cc1200_868_4gfsk_200kbps,
                                                      &cc1200_868_2gfsk_500kbps,
                                                      &cc1200_868_4gfsk_1000kbps};

enum {RF_CFG_AMOUNT = sizeof(rf_cfg_ptrs)/sizeof(rf_cfg_ptrs[0]),};

// static const uint8_t rf_cfg_leds[] = {RGB_LED_CYAN,
//                                       RGB_LED_MAGENTA,
//                                       RGB_LED_WHITE,
//                                       RGB_LED_YELLOW,
//                                       RGB_LED_BLUE,
//                                       RGB_LED_RED,
//                                       RGB_LED_GREEN,
//                                       RGB_LED_CYAN,};

// static const uint8_t rx_tx_mode_leds[] = {RX_RECEIVE_LED,
//                                           TX_SEND_LED};

static const message empty_message;
static const handshake_delay_t empty_handshake_delay;
static const linkaddr_t empty_linkaddr;

// node labelled "gateway" has link-addr: 0012.4b00.09df.4dee
// static const linkaddr_t src_linkaddr = {{0x00, 0x12, 0x4b, 0x00, 0x09, 0xdf, 0x4d, 0xee}};

#endif /* RANGER_CONSTANTS_H_ */