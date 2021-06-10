/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <string.h>

#include "contiki.h"

#include "sys/cooja_mt.h"
#include "lib/simEnvChange.h"

#include "net/packetbuf.h"
#include "net/netstack.h"
#include "sys/energest.h"

#include "dev/radio.h"
#include "dev/cooja-radio.h"

#if COOJA_WITH_TWOFACED
#include "sys/mutex.h"
#endif

/*
 * The maximum number of bytes this driver can accept from the MAC layer for
 * transmission or will deliver to the MAC layer after reception. Includes
 * the MAC header and payload, but not the FCS.
 */
#ifdef COOJA_RADIO_CONF_BUFSIZE
#define COOJA_RADIO_BUFSIZE COOJA_RADIO_CONF_BUFSIZE
#else
#define COOJA_RADIO_BUFSIZE 125
#endif

#define CCA_SS_THRESHOLD -95

const struct simInterface radio_interface;

/* COOJA */
char simReceiving = 0;
char simInDataBuffer[COOJA_RADIO_BUFSIZE];
int simInSize = 0;
rtimer_clock_t simLastPacketTimestamp = 0;
char simOutDataBuffer[COOJA_RADIO_BUFSIZE];
int simOutSize = 0;
char simRadioHWOn = 1;
int simSignalStrength = -100;
int simLastSignalStrength = -100;
char simPower = 100;
int simRadioChannel = 26;
int simLQI = 105;

#if COOJA_WITH_TWOFACED
const struct simInterface twofaced_radio_interface;

/* TWOFACED */
char simReceivingTwofaced = 0;
char simInDataBufferTwofaced[COOJA_RADIO_BUFSIZE];
int simInSizeTwofaced = 0;
rtimer_clock_t simLastPacketTimestampTwofaced = 0;
char simOutDataBufferTwofaced[COOJA_RADIO_BUFSIZE];
int simOutSizeTwofaced = 0;
char simRadioHWOnTwofaced = 1;
int simSignalStrengthTwofaced = -100;
int simLastSignalStrengthTwofaced = -100;
char simPowerTwofaced = 100;
int simRadioChannelTwofaced = 5;
int simLQITwofaced = 105;

/* A lock that prevents changing interfaces when innapropriate */
static volatile mutex_t rf_lock = MUTEX_STATUS_UNLOCKED;
/* A collection of all interface ids */
static const if_id_collection_t if_id_collection = { {COOJA_PRIMARY_IF_ID, COOJA_SECONDARY_IF_ID},
                                                     {COOJA_PRIMARY_IF_DR, COOJA_SECONDARY_IF_DR},
                                                     2 };
/* The twofaced interface state */
static uint8_t twofaced_rf_flags = 0x00;
/* The id of the currently selected interface */
static uint8_t sel_if_id;
/* The id of the next interface to be selected */
static uint8_t next_if_id;

/* The supported twofaced-rf flag bitmasks */
#define TWOFACED_RF_UPDATE_IF_VIA_ID    0x01
#define TWOFACED_RF_INITIALIZED         0x02
#endif

static const void *pending_data;

/* If we are in the polling mode, poll_mode is 1; otherwise 0 */
static int poll_mode = 0; /* default 0, disabled */
static int auto_ack = 0; /* AUTO_ACK is not supported; always 0 */
static int addr_filter = 0; /* ADDRESS_FILTER is not supported; always 0 */
static int send_on_cca = (COOJA_TRANSMIT_ON_CCA != 0);

PROCESS(cooja_radio_process, "cooja radio process");
/*---------------------------------------------------------------------------*/
static void
set_send_on_cca(uint8_t enable)
{
  send_on_cca = enable;
}
/*---------------------------------------------------------------------------*/
static void
set_frame_filtering(int enable)
{
  addr_filter = enable;
}
/*---------------------------------------------------------------------------*/
static void
set_auto_ack(int enable)
{
  auto_ack = enable;
}
/*---------------------------------------------------------------------------*/
static void
set_poll_mode(int enable)
{
  poll_mode = enable;
}
/*---------------------------------------------------------------------------*/
void
radio_set_channel(int channel)
{
  simRadioChannel = channel;
}
/*---------------------------------------------------------------------------*/
void
radio_set_txpower(unsigned char power)
{
  /* 1 - 100: Number indicating output power */
  simPower = power;
}
/*---------------------------------------------------------------------------*/
int
radio_signal_strength_last(void)
{
  return simLastSignalStrength;
}
/*---------------------------------------------------------------------------*/
int
radio_signal_strength_current(void)
{
  return simSignalStrength;
}
/*---------------------------------------------------------------------------*/
int
radio_LQI(void)
{
  return simLQI;
}
/*---------------------------------------------------------------------------*/
#if COOJA_WITH_TWOFACED
void
radio_set_channel_twofaced(int channel)
{
  simRadioChannelTwofaced = channel;
}
/*---------------------------------------------------------------------------*/
void
radio_set_txpower_twofaced(unsigned char power)
{
  /* 1 - 100: Number indicating output power */
  simPowerTwofaced = power;
}
/*---------------------------------------------------------------------------*/
int
radio_signal_strength_last_twofaced(void)
{
  return simLastSignalStrengthTwofaced;
}
/*---------------------------------------------------------------------------*/
int
radio_signal_strength_current_twofaced(void)
{
  return simSignalStrengthTwofaced;
}
/*---------------------------------------------------------------------------*/
int
radio_LQI_twofaced(void)
{
  return simLQITwofaced;
}
#endif
/*---------------------------------------------------------------------------*/
static int
radio_on(void)
{
  ENERGEST_ON(ENERGEST_TYPE_LISTEN);
#if COOJA_WITH_TWOFACED
#if MAC_CONF_WITH_TWOFACED
  simRadioHWOn = 1;
  simRadioHWOnTwofaced = 1;
#else
  if(sel_if_id == COOJA_PRIMARY_IF_ID) {
    simRadioHWOn = 1;
  } else {
    simRadioHWOnTwofaced = 1;
  }
#endif
#else
  simRadioHWOn = 1;
#endif
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
radio_off(void)
{
  ENERGEST_OFF(ENERGEST_TYPE_LISTEN);
#if COOJA_WITH_TWOFACED
#if MAC_CONF_WITH_TWOFACED
  simRadioHWOn = 0;
  simRadioHWOnTwofaced = 0;
#else
  if(sel_if_id == COOJA_PRIMARY_IF_ID) {
    simRadioHWOn = 0;
  } else {
    simRadioHWOnTwofaced = 0;
  }
#endif
#else
  simRadioHWOn = 0;
#endif
  return 1;
}
/*---------------------------------------------------------------------------*/
static void
doInterfaceActionsBeforeTick(void)
{
#if COOJA_WITH_TWOFACED
  // TODO
#else
  if(!simRadioHWOn) {
    simInSize = 0;
    return;
  }
  if(simReceiving) {
    simLastSignalStrength = simSignalStrength;
    return;
  }

  if(simInSize > 0) {
    process_poll(&cooja_radio_process);
  }
#endif
}
/*---------------------------------------------------------------------------*/
static void
doInterfaceActionsAfterTick(void)
{
}
/*---------------------------------------------------------------------------*/
static int
radio_read(void *buf, unsigned short bufsize)
{
#if COOJA_WITH_TWOFACED
  /* Check which interface is currently preferred and perform a
     read operation for that interface only */
  int tmp;
  if(sel_if_id == COOJA_PRIMARY_IF_ID) {
    tmp = simInSize;
    if(simInSize == 0) {
      return 0;
    }
    if(bufsize < simInSize) {
      simInSize = 0; /* rx flush */
      return 0;
    }

    memcpy(buf, simInDataBuffer, simInSize);
    simInSize = 0;
    if(!poll_mode) {
      packetbuf_set_attr(PACKETBUF_ATTR_RSSI, simSignalStrength);
      packetbuf_set_attr(PACKETBUF_ATTR_LINK_QUALITY, simLQI);
      packetbuf_set_attr(PACKETBUF_ATTR_INTERFACE_ID, COOJA_PRIMARY_IF_ID);
    }
  } else {
    tmp = simInSizeTwofaced;
    if(simInSizeTwofaced == 0) {
      return 0;
    }
    if(bufsize < simInSizeTwofaced) {
      simInSizeTwofaced = 0; /* rx flush */
      return 0;
    }

    memcpy(buf, simInDataBufferTwofaced, simInSizeTwofaced);
    simInSizeTwofaced = 0;
    if(!poll_mode) {
      packetbuf_set_attr(PACKETBUF_ATTR_RSSI, simSignalStrengthTwofaced);
      packetbuf_set_attr(PACKETBUF_ATTR_LINK_QUALITY, simLQITwofaced);
      packetbuf_set_attr(PACKETBUF_ATTR_INTERFACE_ID, COOJA_SECONDARY_IF_ID);
    }
  }
  return tmp;
#else
  int tmp = simInSize;

  if(simInSize == 0) {
    return 0;
  }
  if(bufsize < simInSize) {
    simInSize = 0; /* rx flush */
    return 0;
  }

  memcpy(buf, simInDataBuffer, simInSize);
  simInSize = 0;
  if(!poll_mode) {
    packetbuf_set_attr(PACKETBUF_ATTR_RSSI, simSignalStrength);
    packetbuf_set_attr(PACKETBUF_ATTR_LINK_QUALITY, simLQI);
  }

  return tmp;
#endif
}
/*---------------------------------------------------------------------------*/
static int
channel_clear(void)
{
#if COOJA_WITH_TWOFACED
  if(sel_if_id == COOJA_PRIMARY_IF_ID) {
    if(simSignalStrength > CCA_SS_THRESHOLD) {
      return 0;
    }
    return 1;
  } else {
    if(simSignalStrengthTwofaced > CCA_SS_THRESHOLD) {
      return 0;
    }
    return 1;
  }
#else
  if(simSignalStrength > CCA_SS_THRESHOLD) {
    return 0;
  }
  return 1;
#endif
}
/*---------------------------------------------------------------------------*/
static int
radio_send(const void *payload, unsigned short payload_len)
{
#if COOJA_WITH_TWOFACED
  int result;
  int radio_was_on = (sel_if_id == COOJA_PRIMARY_IF_ID) ? simRadioHWOn : simRadioHWOnTwofaced;

  if(payload_len > COOJA_RADIO_BUFSIZE) {
    return RADIO_TX_ERR;
  }
  if(payload_len == 0) {
    return RADIO_TX_ERR;
  }

  if((sel_if_id == COOJA_PRIMARY_IF_ID) && (simOutSize > 0)) {
    return RADIO_TX_ERR;
  }
  if((sel_if_id != COOJA_PRIMARY_IF_ID) && (simOutSizeTwofaced > 0)) {
    return RADIO_TX_ERR;
  }

  if(radio_was_on) {
    ENERGEST_SWITCH(ENERGEST_TYPE_LISTEN, ENERGEST_TYPE_TRANSMIT);
  } else {
    /* Turn on radio temporarily */
    if(sel_if_id == COOJA_PRIMARY_IF_ID) {
      simRadioHWOn = 1;
    } else {
      simRadioHWOnTwofaced = 1;
    }
    ENERGEST_ON(ENERGEST_TYPE_TRANSMIT);
  }

#if COOJA_SIMULATE_TURNAROUND
  simProcessRunValue = 1;
  cooja_mt_yield();
  if(payload_len > 3) {
    simProcessRunValue = 1;
    cooja_mt_yield();
  }
#endif /* COOJA_SIMULATE_TURNAROUND */

  /* Transmit on CCA */
  if(COOJA_TRANSMIT_ON_CCA && send_on_cca && !channel_clear()) {
    result = RADIO_TX_COLLISION;
  } else {
    /* Copy packet data to temporary storage */
    if(sel_if_id == COOJA_PRIMARY_IF_ID) {
      memcpy(simOutDataBuffer, payload, payload_len);
      simOutSize = payload_len;

      /* Transmit */
      while(simOutSize > 0) {
        cooja_mt_yield();
      }
    } else {
      memcpy(simOutDataBufferTwofaced, payload, payload_len);
      simOutSizeTwofaced = payload_len;

      /* Transmit */
      while(simOutSizeTwofaced > 0) {
        cooja_mt_yield();
      }
    }

    result = RADIO_TX_OK;
  }

  if(radio_was_on) {
    ENERGEST_SWITCH(ENERGEST_TYPE_TRANSMIT, ENERGEST_TYPE_LISTEN);
  } else {
    ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
  }

  if(sel_if_id == COOJA_PRIMARY_IF_ID) {
    simRadioHWOn = radio_was_on;
  } else {
    simRadioHWOnTwofaced = radio_was_on;
  }
  return result;
#else
  int result;
  int radio_was_on = simRadioHWOn;

  if(payload_len > COOJA_RADIO_BUFSIZE) {
    return RADIO_TX_ERR;
  }
  if(payload_len == 0) {
    return RADIO_TX_ERR;
  }
  if(simOutSize > 0) {
    return RADIO_TX_ERR;
  }

  if(radio_was_on) {
    ENERGEST_SWITCH(ENERGEST_TYPE_LISTEN, ENERGEST_TYPE_TRANSMIT);
  } else {
    /* Turn on radio temporarily */
    simRadioHWOn = 1;
    ENERGEST_ON(ENERGEST_TYPE_TRANSMIT);
  }

#if COOJA_SIMULATE_TURNAROUND
  simProcessRunValue = 1;
  cooja_mt_yield();
  if(payload_len > 3) {
    simProcessRunValue = 1;
    cooja_mt_yield();
  }
#endif /* COOJA_SIMULATE_TURNAROUND */

  /* Transmit on CCA */
  if(COOJA_TRANSMIT_ON_CCA && send_on_cca && !channel_clear()) {
    result = RADIO_TX_COLLISION;
  } else {
    /* Copy packet data to temporary storage */
    memcpy(simOutDataBuffer, payload, payload_len);
    simOutSize = payload_len;

    /* Transmit */
    while(simOutSize > 0) {
      cooja_mt_yield();
    }

    result = RADIO_TX_OK;
  }

  if(radio_was_on) {
    ENERGEST_SWITCH(ENERGEST_TYPE_TRANSMIT, ENERGEST_TYPE_LISTEN);
  } else {
    ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
  }

  simRadioHWOn = radio_was_on;
  return result;
#endif
}
/*---------------------------------------------------------------------------*/
static int
prepare_packet(const void *data, unsigned short len)
{
  if(len > COOJA_RADIO_BUFSIZE) {
    return RADIO_TX_ERR;
  }
  pending_data = data;
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
transmit_packet(unsigned short len)
{
  int ret = RADIO_TX_ERR;
  if(pending_data != NULL) {
    ret = radio_send(pending_data, len);
  }
  return ret;
}
/*---------------------------------------------------------------------------*/
static int
receiving_packet(void)
{
#if COOJA_WITH_TWOFACED
  if(sel_if_id == COOJA_PRIMARY_IF_ID) {
    return simReceiving;
  }
  return simReceivingTwofaced;
#else
  return simReceiving;
#endif
}
/*---------------------------------------------------------------------------*/
static int
pending_packet(void)
{
#if COOJA_WITH_TWOFACED
  if(sel_if_id == COOJA_PRIMARY_IF_ID) {
    return !simReceiving && simInSize > 0;
  }
  return !simReceivingTwofaced && simInSizeTwofaced > 0;
#else
  return !simReceiving && simInSize > 0;
#endif
}
/*---------------------------------------------------------------------------*/
#if COOJA_WITH_TWOFACED
static int
lock_interface(void)
{
  return mutex_try_lock(&rf_lock);
}
/*---------------------------------------------------------------------------*/
static void
unlock_interface(void)
{
  mutex_unlock(&rf_lock);
}
/*---------------------------------------------------------------------------*/
static int
channel_clear_all(void)
{
  return (simSignalStrength <= CCA_SS_THRESHOLD) && (simSignalStrengthTwofaced <= CCA_SS_THRESHOLD);
}
/*---------------------------------------------------------------------------*/
static int
receiving_packet_all(void)
{
  return simReceiving || simReceivingTwofaced;
}
/*---------------------------------------------------------------------------*/
static int
pending_packet_all(void)
{
  return (!simReceiving && simInSize > 0) || (!simReceivingTwofaced && simInSizeTwofaced > 0);
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_if_via_id(uint8_t if_id)
{
#if MAC_CONF_WITH_TWOFACED
  if(lock_interface()) {
    twofaced_rf_flags &= ~TWOFACED_RF_UPDATE_IF_VIA_ID;
    
    if(if_id == sel_if_id) {
      unlock_interface();
      return RADIO_RESULT_OK;
    }

    for(uint8_t i = 0; i < if_id_collection.size; i++) {
      if(if_id == if_id_collection.if_id_list[i]) {
          NETSTACK_MAC.off();
          sel_if_id = if_id_collection.if_id_list[i];
          NETSTACK_MAC.on();
          unlock_interface();
          return RADIO_RESULT_OK;
        }
    }

    /* If we've reached this point, there is no interface available with
       the specified id and so we must unlock the interface change lock
       and return RADIO_RESULT_INVALID_VALUE */
    unlock_interface();
    return RADIO_RESULT_INVALID_VALUE;
  }
  /* If we've reached this point, the interface lock is taken and so we
     must defer changing the selected interface to a later point in time
     by setting a flag and polling the cooja_radio_process */
  next_if_id = if_id;
  twofaced_rf_flags |= TWOFACED_RF_UPDATE_IF_VIA_ID;
  process_poll(&cooja_radio_process);
  /* We return RADIO_RESULT_OK because deferring an interface
     switch doesn't constitute an error */
  return RADIO_RESULT_OK;
#else /* MAC_CONF_WITH_TWOFACED */
  return RADIO_RESULT_ERROR;
#endif /* MAC_CONF_WITH_TWOFACED */
}
#endif
/*---------------------------------------------------------------------------*/
static void pollhandler(void);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cooja_radio_process, ev, data)
{
  PROCESS_POLLHANDLER(pollhandler());

  PROCESS_BEGIN();

  PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_EXIT);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static void
pollhandler(void)
{
  if(!poll_mode) {
    int len;
#if COOJA_WITH_TWOFACED && MAC_CONF_WITH_TWOFACED
    if(NETSTACK_MAC.lock_input()) {
      /* MAC input lock acquired */
#endif
    packetbuf_clear();
    len = radio_read(packetbuf_dataptr(), PACKETBUF_SIZE);
    if(len > 0) {
      packetbuf_set_datalen(len);
      NETSTACK_MAC.input();
    }
#if COOJA_WITH_TWOFACED && MAC_CONF_WITH_TWOFACED
      /* Releasing MAC input lock */
      NETSTACK_MAC.unlock_input();
    } else {
      /* Failed trying MAC input lock, polling process again */
      process_poll(&cooja_radio_process);
    }
#endif
  }

#if COOJA_WITH_TWOFACED
  if(twofaced_rf_flags & TWOFACED_RF_UPDATE_IF_VIA_ID) {
    set_if_via_id(next_if_id);
  }
#endif
}
/*---------------------------------------------------------------------------*/
static int
init(void)
{
#if COOJA_WITH_TWOFACED
  if(!(twofaced_rf_flags & TWOFACED_RF_INITIALIZED)) {
    sel_if_id = if_id_collection.if_id_list[0];
    twofaced_rf_flags |= TWOFACED_RF_INITIALIZED;
    process_start(&cooja_radio_process, NULL);
  }
#else
  process_start(&cooja_radio_process, NULL);
#endif
  return 1;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_value(radio_param_t param, radio_value_t *value)
{
  switch(param) {
  case RADIO_PARAM_RX_MODE:
    *value = 0;
    if(addr_filter) {
      *value |= RADIO_RX_MODE_ADDRESS_FILTER;
    }
    if(auto_ack) {
      *value |= RADIO_RX_MODE_AUTOACK;
    }
    if(poll_mode) {
      *value |= RADIO_RX_MODE_POLL_MODE;
    }
    return RADIO_RESULT_OK;
  case RADIO_PARAM_TX_MODE:
    *value = 0;
    if(send_on_cca) {
      *value |= RADIO_TX_MODE_SEND_ON_CCA;
    }
    return RADIO_RESULT_OK;
  case RADIO_PARAM_LAST_RSSI:
#if COOJA_WITH_TWOFACED
    if(sel_if_id == COOJA_PRIMARY_IF_ID) {
      *value = (radio_value_t)simSignalStrength;
    } else {
      *value = (radio_value_t)simSignalStrengthTwofaced;
    }
    return RADIO_RESULT_OK;
#else
    *value = simSignalStrength;
    return RADIO_RESULT_OK;
#endif
  case RADIO_PARAM_LAST_LINK_QUALITY:
#if COOJA_WITH_TWOFACED
    if(sel_if_id == COOJA_PRIMARY_IF_ID) {
      *value = (radio_value_t)simLQI;
    } else {
      *value = (radio_value_t)simLQITwofaced;
    }
    return RADIO_RESULT_OK;
#else
    *value = simLQI;
    return RADIO_RESULT_OK;
#endif
  case RADIO_PARAM_RSSI:
#if COOJA_WITH_TWOFACED
    if(sel_if_id == COOJA_PRIMARY_IF_ID) {
      *value = (radio_value_t)(-90 + simRadioChannel - 11);
    } else {
      *value = (radio_value_t)(-90 + simRadioChannelTwofaced);
    }
    return RADIO_RESULT_OK;
#else
    /* return a fixed value depending on the channel */
    *value = -90 + simRadioChannel - 11;
    return RADIO_RESULT_OK;
#endif
  case RADIO_CONST_MAX_PAYLOAD_LEN:
    *value = (radio_value_t)COOJA_RADIO_BUFSIZE;
    return RADIO_RESULT_OK;
#if COOJA_WITH_TWOFACED
  case RADIO_PARAM_CHANNEL:
    if(sel_if_id == COOJA_PRIMARY_IF_ID) {
      *value = (radio_value_t)simRadioChannel;
    } else {
      *value = (radio_value_t)simRadioChannelTwofaced;
    }
    return RADIO_RESULT_OK;
  case RADIO_CONST_MULTI_RF:
    *value = (radio_value_t)RADIO_MULTI_RF_EN;
    return RADIO_RESULT_OK;
  case RADIO_CONST_INTERFACE_ID:
    *value = (radio_value_t)sel_if_id;
    return RADIO_RESULT_OK;
  case RADIO_CONST_DATA_RATE:
    if(sel_if_id == COOJA_PRIMARY_IF_ID) {
      *value = (radio_value_t)COOJA_PRIMARY_IF_DR;
    } else {
      *value = (radio_value_t)COOJA_SECONDARY_IF_DR;
    }
    return RADIO_RESULT_OK;
#endif
  default:
    return RADIO_RESULT_NOT_SUPPORTED;
  }
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_value(radio_param_t param, radio_value_t value)
{
  switch(param) {
  case RADIO_PARAM_RX_MODE:
    if(value & ~(RADIO_RX_MODE_ADDRESS_FILTER |
        RADIO_RX_MODE_AUTOACK | RADIO_RX_MODE_POLL_MODE)) {
      return RADIO_RESULT_INVALID_VALUE;
    }

    /* Only disabling is acceptable for RADIO_RX_MODE_ADDRESS_FILTER */
    if ((value & RADIO_RX_MODE_ADDRESS_FILTER) != 0) {
      return RADIO_RESULT_NOT_SUPPORTED;
    }
    set_frame_filtering((value & RADIO_RX_MODE_ADDRESS_FILTER) != 0);

    /* Only disabling is acceptable for RADIO_RX_MODE_AUTOACK */
    if ((value & RADIO_RX_MODE_ADDRESS_FILTER) != 0) {
      return RADIO_RESULT_NOT_SUPPORTED;
    }
    set_auto_ack((value & RADIO_RX_MODE_AUTOACK) != 0);

    set_poll_mode((value & RADIO_RX_MODE_POLL_MODE) != 0);
    return RADIO_RESULT_OK;
  case RADIO_PARAM_TX_MODE:
    if(value & ~(RADIO_TX_MODE_SEND_ON_CCA)) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    set_send_on_cca((value & RADIO_TX_MODE_SEND_ON_CCA) != 0);
    return RADIO_RESULT_OK;
  case RADIO_PARAM_CHANNEL:
#if COOJA_WITH_TWOFACED
    if(sel_if_id == COOJA_PRIMARY_IF_ID) {
      if(value < 11 || value > 26) {
        return RADIO_RESULT_INVALID_VALUE;
      }
      radio_set_channel(value);
    } else {
      if(value < 0 || value > 10) {
        return RADIO_RESULT_INVALID_VALUE;
      }
      radio_set_channel_twofaced(value);
    }
    return RADIO_RESULT_OK;
#else
    if(value < 11 || value > 26) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    radio_set_channel(value);
    return RADIO_RESULT_OK;
#endif
#if COOJA_WITH_TWOFACED
  case RADIO_PARAM_SEL_IF_ID:
    return set_if_via_id((uint8_t)value);
#endif
  default:
    return RADIO_RESULT_NOT_SUPPORTED;
  }
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_object(radio_param_t param, void *dest, size_t size)
{
  if(param == RADIO_PARAM_LAST_PACKET_TIMESTAMP) {
    if(size != sizeof(rtimer_clock_t) || !dest) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    *(rtimer_clock_t *)dest = (rtimer_clock_t)simLastPacketTimestamp;
    return RADIO_RESULT_OK;
#if COOJA_WITH_TWOFACED
  } else if(param == RADIO_PARAM_LAST_PACKET_TIMESTAMP_COOJA_TWOFACED) {
    if(size != sizeof(rtimer_clock_t) || !dest) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    *(rtimer_clock_t *)dest = (rtimer_clock_t)simLastPacketTimestampTwofaced;
    return RADIO_RESULT_OK;
  } else if(param == RADIO_CONST_INTERFACE_ID_COLLECTION) {
    if(size == sizeof(if_id_collection_t)) {
      *(if_id_collection_t *)dest = if_id_collection;
      return RADIO_RESULT_OK;
    }
    return RADIO_RESULT_ERROR;
#endif
  }
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_object(radio_param_t param, const void *src, size_t size)
{
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
const struct radio_driver cooja_radio_driver =
{
    init,
    prepare_packet,
    transmit_packet,
    radio_send,
    radio_read,
    channel_clear,
    receiving_packet,
    pending_packet,
    radio_on,
    radio_off,
    get_value,
    set_value,
    get_object,
    set_object,
#if COOJA_WITH_TWOFACED
    lock_interface,
    unlock_interface,
    channel_clear_all,
    receiving_packet_all,
    pending_packet_all,
#else
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
#endif
    "cooja_radio_driver"
};
/*---------------------------------------------------------------------------*/
SIM_INTERFACE(radio_interface,
              doInterfaceActionsBeforeTick,
              doInterfaceActionsAfterTick);
#if COOJA_WITH_TWOFACED
SIM_INTERFACE(twofaced_radio_interface,
              doInterfaceActionsBeforeTick,
              doInterfaceActionsAfterTick);
#endif
