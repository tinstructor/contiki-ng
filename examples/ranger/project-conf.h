#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#undef  IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID   0x6689

//#define LOG_CONF_LEVEL_MAC          LOG_LEVEL_DBG
//#define LOG_CONF_LEVEL_RANGER       LOG_LEVEL_INFO
//#define LOG_CONF_LEVEL_RANGER_NET   LOG_LEVEL_INFO

#define NETSTACK_CONF_NETWORK   ranger_net_driver

#define BUTTON_HAL_CONF_WITH_DESCRIPTION    1

#undef NETSTACK_CONF_RADIO
#ifndef RADIO_2400MHZ
    #define NETSTACK_CONF_RADIO             cc1200_driver
    #define CC1200_CONF_RF_CFG              cc1200_802154g_863_870_fsk_50kbps
    //#define CC1200_CONF_RF_CFG              cc1200_868_fsk_1_2kbps
    #define ANTENNA_SW_SELECT_DEF_CONF      ANTENNA_SW_SELECT_SUBGHZ
    #define CC1200_CONF_USE_GPIO2           0
    #define CC1200_CONF_USE_RX_WATCHDOG     0

    #define CSMA_CONF_ACK_WAIT_TIME                 (RTIMER_SECOND / 200)
    #define CSMA_CONF_AFTER_ACK_DETECTED_WAIT_TIME  (RTIMER_SECOND / 1500)
#else
    #define NETSTACK_CONF_RADIO              cc2538_rf_driver
    #define ANTENNA_SW_SELECT_DEF_CONF       ANTENNA_SW_SELECT_2_4GHZ
#endif

#endif
