#ifndef RANGER_CONSTANTS_H_
#define RANGER_CONSTANTS_H_

#include "os/net/linkaddr.h"
#include "arch/dev/cc1200/cc1200-rf-cfg.h"
#include "ranger-types.h"

/*----------------------------------------------------------------------------*/
// Change these for your own use case
// Make sure that the interval is long enough to send and receive your messages

#define MAIN_INTERVAL           6*(CLOCK_SECOND/10)
#define TX_DURATION             MAIN_INTERVAL*100
#define CONTENT_SIZE            28
#define TX_POWER_DBM            14
#define CHANNEL                 1

#define ENABLE_CFG_REQ          0
#define ENABLE_SEND_PIN         0
#define ENABLE_UART_INPUT       0
#define BURST_AMOUNT            3
#define UNIQUE_ID               UINT32_C(0x30695444)
#define RX_LED                  RGB_LED_GREEN
#define TX_LED                  RGB_LED_RED
#define CFG_REQ_DELAY           8*(CLOCK_SECOND/10)+1*CLOCK_SECOND

/*----------------------------------------------------------------------------*/

extern const cc1200_rf_cfg_t cc1200_868_2fsk_1_2kbps;
//extern const cc1200_rf_cfg_t cc1200_868_2gfsk_19_2kbps;
extern const cc1200_rf_cfg_t cc1200_868_2gfsk_50kbps;
//extern const cc1200_rf_cfg_t cc1200_868_2gfsk_100kbps;
//extern const cc1200_rf_cfg_t cc1200_868_4gfsk_150kbps;
extern const cc1200_rf_cfg_t cc1200_868_2gfsk_200kbps;
//extern const cc1200_rf_cfg_t cc1200_868_2gfsk_200kbps; //FIXME: works, but not as expected
//extern const cc1200_rf_cfg_t cc1200_868_2gfsk_500kbps;
extern const cc1200_rf_cfg_t cc1200_868_4gfsk_1000kbps;

static const cc1200_rf_cfg_t * const rf_cfg_ptrs[] = {&cc1200_868_2fsk_1_2kbps,
                                                      //&cc1200_868_2gfsk_19_2kbps,
                                                      &cc1200_868_2gfsk_50kbps,
                                                      //&cc1200_868_2gfsk_100kbps,
                                                      //&cc1200_868_4gfsk_150kbps,
                                                      &cc1200_868_2gfsk_200kbps,
                                                      //&cc1200_868_4gfsk_200kbps, //FIXME: works, but not as expected
                                                      //&cc1200_868_2gfsk_500kbps,
                                                      &cc1200_868_4gfsk_1000kbps};

enum {RF_CFG_AMOUNT = sizeof(rf_cfg_ptrs)/sizeof(rf_cfg_ptrs[0]),};

static const uint8_t rf_cfg_leds[] = {RGB_LED_CYAN,
                                      RGB_LED_YELLOW,
                                      RGB_LED_MAGENTA,
                                      RGB_LED_BLUE,
                                      RGB_LED_WHITE,
                                      RGB_LED_RED,
                                      RGB_LED_GREEN,
                                      RGB_LED_MAGENTA,
                                      RGB_LED_BLUE,};

static const message empty_message;
static const rf_cfg_delay_t empty_rf_cfg_delay;
static const linkaddr_t empty_linkaddr;

// node labelled "gateway" has link-addr: 0012.4b00.09df.4dee
// static const linkaddr_t src_linkaddr = {{0x00, 0x12, 0x4b, 0x00, 0x09, 0xdf, 0x4d, 0xee}};

/*----------------------------------------------------------------------------*/

// have a look at p.83 of SWRU346B (1 byte = 2 nibbles)
static const float num_preamble_nibbles[] = {0,1,2,3,4,6,8,10,12,14,16,24,48,60,0,0};
static const uint8_t preamble_words[] = {0xAA,0x55,0x33,0xCC};

// have a look at p.30 and p.84 of SWRU346B
static const uint8_t decimation_factors[] = {12,24,48,0};

// have a look at p.94 of SWRU346B
static const cc1200_crc_cfg_t crc_configurations[] = {{},{0xC002,0xFFFF},{0x8810,0x0000},{0x77EF,0x1D0F}};

// have a look at p.35 and p.80 of SWRU346B
static const uint32_t sync_word_masks[] = {0x00000000,0x000007FF,0x0000FFFF,0x0003FFFF,0x00FFFFFF,0xFFFFFFFF,0xFFFF0000,0xFFFFFFFF};

#define SYNC_MODE_16_H  0b110
#define SYNC_MODE_16_D  0b111

/*----------------------------------------------------------------------------*/

#ifndef XTAL_FREQ_KHZ
#define XTAL_FREQ_KHZ   40000
#endif /* XTAL_FREQ_KHZ */

#endif /* RANGER_CONSTANTS_H_ */