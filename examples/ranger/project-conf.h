
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/*******************************************************************************
 * Change these for your own use case
 * Make sure that the interval is long enough to send and receive your messages
 */
#define MAIN_INTERVAL_SECONDS 1
#define MAIN_INTERVAL (CLOCK_SECOND * MAIN_INTERVAL_SECONDS)
#define UDP_PORT 10011 /* https://en.wikipedia.org/wiki/List_of_TCP_and_UDP_port_numbers */
#define CONTENT_SIZE 40
#define TX_POWER_DBM 14
#define CHANNEL 26
#define SEND_MESSAGE_TYPE USE_RANDOM_ASCII_MESSAGE
#define ENABLE_SEND_PIN 1

/******************************************************************************/
/* sha1("martijn.saelens@ugent.be") = 6689cb27c07b5f3c8f5ca3a39786b1368e05f70a
 */
#undef IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID               0x6689

/******************************************************************************/
#define LOG_CONF_LEVEL_MAIN LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RPL LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_6LOWPAN LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_NULLNET LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_FRAMER LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_6TOP LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_COAP LOG_LEVEL_INFO

/******************************************************************************/
#undef NETSTACK_CONF_RADIO
#ifndef RF_WIFI_2_4_GHZ /* If NOT defined RF_WIFI_2_4_GHZ */
#   define NETSTACK_CONF_RADIO              cc1200_driver
#   define RF_STRING                        "CC1200 SubGHz"
#   define CC1200_CONF_RF_CFG               cc1200_802154g_863_870_fsk_50kbps
// #   define CC1200_CONF_RF_CFG               cc1200_868_fsk_1_2kbps
#   define ANTENNA_SW_SELECT_DEF_CONF       ANTENNA_SW_SELECT_SUBGHZ
#   define CC1200_CONF_USE_GPIO2            0
#   define CC1200_CONF_USE_RX_WATCHDOG      0
#else  /* If defined RF_WIFI_2_4_GHZ */
#   define NETSTACK_CONF_RADIO              cc2538_rf_driver
#   define RF_STRING                        "CC2538 2.4GHz"
#   define ANTENNA_SW_SELECT_DEF_CONF       ANTENNA_SW_SELECT_2_4GHZ
#endif /* RF_WIFI_2_4_GHZ */

#endif /* PROJECT_CONF_H_ */

