#include "cc1200-rf-cfg.h"
#include "cc1200-const.h"

/* Base frequency in kHz */
#define RF_CFG_CHAN_CENTER_F0           863125
/* Channel spacing in Hz */
#define RF_CFG_CHAN_SPACING             300000
/* The minimum channel */
#define RF_CFG_MIN_CHANNEL              0
/* The maximum channel */
#define RF_CFG_MAX_CHANNEL              21
/* The maximum output power in dBm */
#define RF_CFG_MAX_TXPOWER              CC1200_CONST_TX_POWER_MAX
/* The carrier sense level used for CCA in dBm */
#define RF_CFG_CCA_THRESHOLD            (-91)
/* The RSSI offset in dBm */
#define RF_CFG_RSSI_OFFSET              (-81)
/* The bitrate in bps */
#define RF_CFG_BITRATE			            100000
/*---------------------------------------------------------------------------*/
static const char rf_cfg_descriptor[] = "868MHz 2-GFSK 100 kbps";
/*---------------------------------------------------------------------------*/
/*
 * Register settings exported from SmartRF Studio using the standard template
 * "trxEB RF Settings Performance Line".
 */

// Address Config = No address check 
// Bit Rate = 100 
// Carrier Frequency = 867.999878 
// Deviation = 49.896240 
// Device Address = 0 
// Manchester Enable = false 
// Modulation Format = 2-GFSK 
// Packet Bit Length = 0 
// Packet Length = 255 
// Packet Length Mode = Variable 
// RX Filter BW = 208.333333 
// Symbol rate = 100 
// Whitening = false 

static const registerSetting_t preferredSettings[]= 
{
  {CC1200_IOCFG2,            0x06},
  {CC1200_SYNC_CFG1,         0x48},
  {CC1200_SYNC_CFG0,         0x23},
  {CC1200_DEVIATION_M,       0x47},
  {CC1200_MODCFG_DEV_E,      0x0C},
  {CC1200_DCFILT_CFG,        0x4B},
  {CC1200_PREAMBLE_CFG0,     0x8A},
  {CC1200_IQIC,              0xD8},
  {CC1200_CHAN_BW,           0x08},
  {CC1200_MDMCFG1,           0x42},
  {CC1200_MDMCFG0,           0x05},
  {CC1200_SYMBOL_RATE2,      0xA4},
  {CC1200_SYMBOL_RATE1,      0x7A},
  {CC1200_SYMBOL_RATE0,      0xE1},
  {CC1200_AGC_REF,           0x2A},
  {CC1200_AGC_CS_THR,        0xF6},
  {CC1200_AGC_CFG1,          0x12},
  {CC1200_AGC_CFG0,          0x80},
  {CC1200_FIFO_CFG,          0x00},
  {CC1200_FS_CFG,            0x12},
  {CC1200_PKT_CFG2,          0x20},
  {CC1200_PKT_CFG0,          0x20},
  {CC1200_PKT_LEN,           0xFF},
  {CC1200_IF_MIX_CFG,        0x1C},
  {CC1200_TOC_CFG,           0x03},
  {CC1200_MDMCFG2,           0x02},
  {CC1200_FREQ2,             0x56},
  {CC1200_FREQ1,             0xCC},
  {CC1200_FREQ0,             0xCC},
  {CC1200_IF_ADC1,           0xEE},
  {CC1200_IF_ADC0,           0x10},
  {CC1200_FS_DIG1,           0x04},
  {CC1200_FS_DIG0,           0x50},
  {CC1200_FS_CAL1,           0x40},
  {CC1200_FS_CAL0,           0x0E},
  {CC1200_FS_DIVTWO,         0x03},
  {CC1200_FS_DSM0,           0x33},
  {CC1200_FS_DVC1,           0xF7},
  {CC1200_FS_DVC0,           0x0F},
  {CC1200_FS_PFD,            0x00},
  {CC1200_FS_PRE,            0x6E},
  {CC1200_FS_REG_DIV_CML,    0x1C},
  {CC1200_FS_SPARE,          0xAC},
  {CC1200_FS_VCO0,           0xB5},
  {CC1200_IFAMP,             0x09},
  {CC1200_XOSC5,             0x0E},
  {CC1200_XOSC1,             0x03},
};

/*---------------------------------------------------------------------------*/
/* Global linkage: symbol name must be different in each exported file! */
const cc1200_rf_cfg_t cc1200_868_2gfsk_100kbps = {
  .cfg_descriptor = rf_cfg_descriptor,
  .register_settings = preferredSettings,
  .size_of_register_settings = sizeof(preferredSettings),
  .tx_pkt_lifetime = (RTIMER_SECOND),
  .tx_rx_turnaround = (RTIMER_SECOND / 100),
  .chan_center_freq0 = RF_CFG_CHAN_CENTER_F0,
  .chan_spacing = RF_CFG_CHAN_SPACING,
  .min_channel = RF_CFG_MIN_CHANNEL,
  .max_channel = RF_CFG_MAX_CHANNEL,
  .max_txpower = RF_CFG_MAX_TXPOWER,
  .cca_threshold = RF_CFG_CCA_THRESHOLD,
  .rssi_offset = RF_CFG_RSSI_OFFSET,
};
