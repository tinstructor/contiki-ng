/*
 * Copyright (c) 2021, Ghent University.
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki-NG operating system.
 */

/**
 * \file
 *      The twofaced-rf driver source file for zoul devices
 * \author
 *      Robbe Elsas <robbe.elsas@ugent.be>
 */

#include "twofaced-rf.h"
#include "dev/radio/twofaced-rf/twofaced-rf-types.h"
#include "dev/radio/twofaced-rf/twofaced-rf-conf.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "sys/mutex.h"

#include <string.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "twofaced-rf"
#define LOG_LEVEL LOG_LEVEL_NONE

/*---------------------------------------------------------------------------*/
/* Constants */
/*---------------------------------------------------------------------------*/
/* The supported interface drivers */
extern const struct radio_driver cc2538_rf_driver;
extern const struct radio_driver cc1200_driver;
static const struct radio_driver *const available_interfaces[] = TWOFACED_RF_AVAILABLE_IFS;
/* The supported twofaced-rf flag bitmasks */
#define TWOFACED_RF_UPDATE_IF_VIA_DESC  0x01
#define TWOFACED_RF_UPDATE_IF_VIA_ID    0x02
#define TWOFACED_RF_INITIALIZED         0x04
/*---------------------------------------------------------------------------*/
/* Variables */
/*---------------------------------------------------------------------------*/
/* The currently selected interface */
static const struct radio_driver *selected_interface;
/* Lowest reported max payload length of all drivers. Initialized in init() */
static uint16_t max_payload_len;
/* A lock that prevents changing interfaces when innapropriate */
static volatile mutex_t rf_lock = MUTEX_STATUS_UNLOCKED;
/* The twofaced-rf driver's state */
static uint8_t twofaced_rf_flags = 0x00;
/* The descriptor of the next interface to be selected */
static char next_if_desc[32];
/* The id of the next interface to be selected */
static uint8_t next_if_id;
/* A collection of all interface ids, technically a variable but only
   to be modified once, i.e., in init() */
static if_id_collection_t if_id_collection = { .size = 0 };
/*---------------------------------------------------------------------------*/
/* The twofaced radio driver exported to Contiki-NG */
/*---------------------------------------------------------------------------*/
const struct radio_driver twofaced_rf_driver = {
  init,
  prepare,
  transmit,
  send,
  read,
  channel_clear,
  receiving_packet,
  pending_packet,
  on,
  off,
  get_value,
  set_value,
  get_object,
  set_object,
  lock_interface,
  unlock_interface,
  channel_clear_all,
  receiving_packet_all,
  pending_packet_all,
  "twofaced_rf_driver"
};
/*---------------------------------------------------------------------------*/
/* Prototypes for internal driver functions */
/*---------------------------------------------------------------------------*/
/**
 * @brief Set the currently selected interface.
 *
 * @param descriptor pointer to a radio driver descriptor string
 * @param size length of the supplied radio driver descriptor string + 1
 * @return radio_result_t
 */
static radio_result_t set_if_via_desc(const char *descriptor, size_t size);
/*---------------------------------------------------------------------------*/
/**
 * @brief Set the currently selected interface.
 *
 * @param if_id identifier of the interface to select
 * @return radio_result_t
 */
static radio_result_t set_if_via_id(uint8_t if_id);
/*---------------------------------------------------------------------------*/
/* Processes and related functionality */
/*---------------------------------------------------------------------------*/
static void pollhandler(void);
/*---------------------------------------------------------------------------*/
PROCESS(twofaced_rf_process, "twofaced radio driver");
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(twofaced_rf_process, ev, data)
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
  if(twofaced_rf_flags & TWOFACED_RF_UPDATE_IF_VIA_DESC) {
    set_if_via_desc(next_if_desc, strlen(next_if_desc) + 1);
  }

  if(twofaced_rf_flags & TWOFACED_RF_UPDATE_IF_VIA_ID) {
    set_if_via_id(next_if_id);
  }
}
/*---------------------------------------------------------------------------*/
/* Internal driver functions */
/*---------------------------------------------------------------------------*/
static radio_result_t
set_if_via_desc(const char *descriptor, size_t size)
{
  /* REVIEW for now, this preprocessor check is good enough to verify that
     the above MAC layer even knows about the multi-rf capabilities of this PHY
     layer abstraction. Otherwise it doesn't make sense to allow switching
     radio interface because a MAC layer that has no knowledge about these
     capabilities presumably doesn't bother to acquire an rf lock before
     preparing a packet and unlocking the rf lock after receiving an ACK
     (under normal circumstances). In the future, a more robust mechanism
     is required here. */
#if MAC_CONF_WITH_TWOFACED
  if(size > (sizeof(next_if_desc) / sizeof(next_if_desc[0]))) {
    LOG_DBG("Interface descriptor too large, aborting interface selection\n");
    return RADIO_RESULT_INVALID_VALUE;
  }
  if(lock_interface()) {
    LOG_DBG("RF lock acquired by set_if_via_desc()\n");

    LOG_DBG("Unsetting interface update flag\n");
    twofaced_rf_flags &= ~TWOFACED_RF_UPDATE_IF_VIA_DESC;

    if(size < strlen("") + 1) {
      unlock_interface();
      LOG_DBG("Unlocking RF lock held by set_if_via_desc(), no descriptor\n");
      return RADIO_RESULT_INVALID_VALUE;
    }

    if(!strcmp(descriptor, selected_interface->driver_descriptor)) {
      unlock_interface();
      LOG_DBG("Unlocking RF lock held by set_if_via_desc(), interface already selected\n");
      return RADIO_RESULT_OK;
    }

    for(uint8_t i = 0; i < sizeof(available_interfaces) /
        sizeof(available_interfaces[0]); i++) {
      if(!strcmp(descriptor, available_interfaces[i]->driver_descriptor)) {
        NETSTACK_MAC.off();
        selected_interface = available_interfaces[i];
        NETSTACK_MAC.on();
        unlock_interface();
        LOG_DBG("Unlocking RF lock held by set_if_via_desc(), interface set\n");
        return RADIO_RESULT_OK;
      }
    }
    unlock_interface();
    LOG_DBG("Unlocking RF lock held by set_if_via_desc(), unknown descriptor\n");
    return RADIO_RESULT_INVALID_VALUE;
  }
  LOG_DBG("Could not switch interface, interfaces are locked\n");
  LOG_DBG("Deferring interface switch\n");
  strcpy(next_if_desc, descriptor);
  LOG_DBG("Setting interface update flag\n");
  twofaced_rf_flags |= TWOFACED_RF_UPDATE_IF_VIA_DESC;
  process_poll(&twofaced_rf_process);
  /* We return RADIO_RESULT_OK because deferring an interface
     switch doesn't constitute an error */
  return RADIO_RESULT_OK;
#else /* MAC_CONF_WITH_TWOFACED */
  return RADIO_RESULT_ERROR;
#endif /* MAC_CONF_WITH_TWOFACED */
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_if_via_id(uint8_t if_id)
{
  /* REVIEW for now, this preprocessor check is good enough to verify that
     the above MAC layer even knows about the multi-rf capabilities of this PHY
     layer abstraction. Otherwise it doesn't make sense to allow switching
     radio interface because a MAC layer that has no knowledge about these
     capabilities presumably doesn't bother to acquire an rf lock before
     preparing a packet and unlocking the rf lock after receiving an ACK
     (under normal circumstances). In the future, a more robust mechanism
     is required here. */
#if MAC_CONF_WITH_TWOFACED
  if(lock_interface()) {
    LOG_DBG("RF lock acquired by set_if_via_id()\n");

    LOG_DBG("Unsetting interface update flag\n");
    twofaced_rf_flags &= ~TWOFACED_RF_UPDATE_IF_VIA_ID;

    radio_value_t temp_if_id;
    if(selected_interface->get_value(RADIO_CONST_INTERFACE_ID,
                                     &temp_if_id) == RADIO_RESULT_OK) {
      if(if_id == temp_if_id) {
        unlock_interface();
        LOG_DBG("Unlocking RF lock held by set_if_via_id(), interface already selected\n");
        return RADIO_RESULT_OK;
      }
    }

    for(uint8_t i = 0; i < sizeof(available_interfaces) /
        sizeof(available_interfaces[0]); i++) {
      if(available_interfaces[i]->get_value(RADIO_CONST_INTERFACE_ID,
                                            &temp_if_id) == RADIO_RESULT_OK) {
        if(if_id == temp_if_id) {
          NETSTACK_MAC.off();
          selected_interface = available_interfaces[i];
          NETSTACK_MAC.on();
          unlock_interface();
          LOG_DBG("Unlocking RF lock held by set_if_via_id(), interface set\n");
          return RADIO_RESULT_OK;
        }
      }
    }
    unlock_interface();
    LOG_DBG("Unlocking RF lock held by set_if_via_id(), unknown id\n");
    return RADIO_RESULT_INVALID_VALUE;
  }
  LOG_DBG("Could not switch interface, interfaces are locked\n");
  LOG_DBG("Deferring interface switch\n");
  next_if_id = if_id;
  LOG_DBG("Setting interface update flag\n");
  twofaced_rf_flags |= TWOFACED_RF_UPDATE_IF_VIA_ID;
  process_poll(&twofaced_rf_process);
  /* We return RADIO_RESULT_OK because deferring an interface
     switch doesn't constitute an error */
  return RADIO_RESULT_OK;
#else /* MAC_CONF_WITH_TWOFACED */
  return RADIO_RESULT_ERROR;
#endif /* MAC_CONF_WITH_TWOFACED */
}
/*---------------------------------------------------------------------------*/
/* Radio driver functions */
/*---------------------------------------------------------------------------*/
static int
init(void)
{
  if(!(twofaced_rf_flags & TWOFACED_RF_INITIALIZED)) {
    LOG_DBG("Initializing %s ...\n", twofaced_rf_driver.driver_descriptor);

    uint8_t num_if = sizeof(available_interfaces) / sizeof(available_interfaces[0]);

    if(num_if < 1) {
      LOG_DBG("Not enough interfaces available, aborting init.\n");
      return 0;
    }

    for(uint8_t i = 0; i < num_if; i++) {

      radio_value_t reported_max_payload_len = 0;
      radio_value_t radio_rx_mode;
      radio_value_t def_chan;
      radio_value_t if_id;

      /* Initialize underlying radio driver */
      if(!available_interfaces[i]->init()) {
        if(!strcmp(available_interfaces[i]->driver_descriptor, "")) {
          LOG_DBG("Failed to init() underlying radio driver\n");
        } else {
          LOG_DBG("Failed to init() underlying radio driver (%s)\n",
                  available_interfaces[i]->driver_descriptor);
        }
        return 0;
      }

      /* Check if underlying radio driver correctly reports max payload len */
      if(available_interfaces[i]->get_value(RADIO_CONST_MAX_PAYLOAD_LEN,
                                            &reported_max_payload_len) != RADIO_RESULT_OK) {
        if(!strcmp(available_interfaces[i]->driver_descriptor, "")) {
          LOG_DBG("Failed to retrieve max payload len of underlying radio driver\n");
        } else {
          LOG_DBG("Failed to retrieve max payload len of underlying radio driver (%s)\n",
                  available_interfaces[i]->driver_descriptor);
        }
        LOG_DBG("Setting max_payload_len to 0\n");
        max_payload_len = 0;
      } else if(reported_max_payload_len < max_payload_len || i == 0) {
        /* If max payload len reported correctly, check if smaller than
           current max_payload_len and set accordingly */
        LOG_INFO("Updated max_payload length from %d to %d\n",
                 max_payload_len, reported_max_payload_len);
        max_payload_len = (uint16_t)reported_max_payload_len;
      }

      /* Check if underlying radio driver allows retrieving the rx_mode */
      if(available_interfaces[i]->get_value(RADIO_PARAM_RX_MODE,
                                            &radio_rx_mode) != RADIO_RESULT_OK) {
        if(!strcmp(available_interfaces[i]->driver_descriptor, "")) {
          LOG_DBG("Failed to retrieve rx mode of underlying radio driver\n");
        } else {
          LOG_DBG("Failed to retrieve rx mode of underlying radio driver (%s)\n",
                  available_interfaces[i]->driver_descriptor);
        }
        return 0;
      } else {
        /* Enable hardware ACKs */
        radio_rx_mode |= RADIO_RX_MODE_AUTOACK;
        /* Disable poll mode */
        radio_rx_mode &= ~RADIO_RX_MODE_POLL_MODE;
        if(available_interfaces[i]->set_value(RADIO_PARAM_RX_MODE, radio_rx_mode) != RADIO_RESULT_OK) {
          if(!strcmp(available_interfaces[i]->driver_descriptor, "")) {
            LOG_DBG("Failed to enable hardware ACKs / disable poll mode of underlying radio driver\n");
          } else {
            LOG_DBG("Failed to enable hardware ACKs / disable poll mode of underlying radio driver (%s)\n",
                    available_interfaces[i]->driver_descriptor);
          }
          return 0;
        }
      }

      /* Check if the underlying radio driver correctly reports its default channel*/
      if(available_interfaces[i]->get_value(RADIO_CONST_DEFAULT_CHANNEL, &def_chan) != RADIO_RESULT_OK) {
        if(!strcmp(available_interfaces[i]->driver_descriptor, "")) {
          LOG_DBG("Failed to retrieve default channel of underlying radio driver\n");
        } else {
          LOG_DBG("Failed to retrieve default channel of underlying radio driver (%s)\n",
                  available_interfaces[i]->driver_descriptor);
        }
        return 0;
      }

      /* Check if the underlying radio driver correctly reports its interface id */
      if(available_interfaces[i]->get_value(RADIO_CONST_INTERFACE_ID, &if_id) != RADIO_RESULT_OK) {
        if(!strcmp(available_interfaces[i]->driver_descriptor, "")) {
          LOG_DBG("Failed to retrieve interface id of underlying radio driver\n");
        } else {
          LOG_DBG("Failed to retrieve interface id of underlying radio driver (%s)\n",
                  available_interfaces[i]->driver_descriptor);
        }
        return 0;
      } else if(if_id_collection.size < (sizeof(if_id_collection.if_id_list) / sizeof(if_id_collection.if_id_list[0]))) {
        uint8_t is_unique = true;
        for(uint8_t j = 0; j < if_id_collection.size; j++) {
          if(if_id_collection.if_id_list[j] == if_id) {
            is_unique = false;
            break;
          }
        }
        if(is_unique) {
          LOG_DBG("Adding interface with ID = %d to collection\n", (uint8_t)if_id);
          if_id_collection.if_id_list[if_id_collection.size] = (uint8_t)if_id;
          if_id_collection.size++;
        } else {
          LOG_DBG("Interface with ID = %d already in collection, not added\n", (uint8_t)if_id);
        }
      } else {
        LOG_DBG("Too damn many interfaces with a valid ID!\n");
        return 0;
      }
    }

    selected_interface = available_interfaces[0];
    twofaced_rf_flags |= TWOFACED_RF_INITIALIZED;
    process_start(&twofaced_rf_process, NULL);
  } else {
    LOG_DBG("%s already initialized\n", twofaced_rf_driver.driver_descriptor);
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
prepare(const void *payload, unsigned short payload_len)
{
  return selected_interface->prepare(payload, payload_len);
}
/*---------------------------------------------------------------------------*/
static int
transmit(unsigned short transmit_len)
{
  return selected_interface->transmit(transmit_len);
}
/*---------------------------------------------------------------------------*/
static int
send(const void *payload, unsigned short payload_len)
{
  if(!prepare(payload, payload_len)) {
    return transmit(payload_len);
  }

  return RADIO_TX_ERR;
}
/*---------------------------------------------------------------------------*/
static int
read(void *buf, unsigned short buf_len)
{
  return selected_interface->read(buf, buf_len);
}
/*---------------------------------------------------------------------------*/
static int
channel_clear(void)
{
  return selected_interface->channel_clear();
}
/*---------------------------------------------------------------------------*/
static int
receiving_packet(void)
{
  return selected_interface->receiving_packet();
}
/*---------------------------------------------------------------------------*/
static int
pending_packet(void)
{
  return selected_interface->pending_packet();
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
#if MAC_CONF_WITH_TWOFACED
  uint8_t is_on = 1;
  for(uint8_t i = 0; i < sizeof(available_interfaces) /
      sizeof(available_interfaces[0]); i++) {
    is_on = is_on && available_interfaces[i]->on();
  }
  return is_on;
#else /* MAC_CONF_WITH_TWOFACED */
  return selected_interface->on();
#endif /* MAC_CONF_WITH_TWOFACED */
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
#if MAC_CONF_WITH_TWOFACED
  uint8_t is_off = 1;
  for(uint8_t i = 0; i < sizeof(available_interfaces) /
      sizeof(available_interfaces[0]); i++) {
    is_off = is_off && available_interfaces[i]->off();
  }
  return is_off;
#else /* MAC_CONF_WITH_TWOFACED */
  return selected_interface->off();
#endif /* MAC_CONF_WITH_TWOFACED */
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_value(radio_param_t param, radio_value_t *value)
{
  if(!value) {
    return RADIO_RESULT_INVALID_VALUE;
  }

  switch(param) {
  case RADIO_CONST_MULTI_RF:
    if((sizeof(available_interfaces) / sizeof(available_interfaces[0])) > 1) {
      *value = RADIO_MULTI_RF_EN;
    } else {
      *value = RADIO_MULTI_RF_DIS;
    }
    return RADIO_RESULT_OK;
  case RADIO_CONST_MAX_PAYLOAD_LEN:
    *value = (radio_value_t)max_payload_len;
    return RADIO_RESULT_OK;
  case RADIO_PARAM_CHANNEL:
  default:
    return selected_interface->get_value(param, value);
  }
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_value(radio_param_t param, radio_value_t value)
{
  switch(param) {
  case RADIO_PARAM_SEL_IF_DESC:
  case RADIO_PARAM_64BIT_ADDR:
    return RADIO_RESULT_NOT_SUPPORTED;
  case RADIO_PARAM_RX_MODE:
    if((value & RADIO_RX_MODE_POLL_MODE) != 0) {
      LOG_DBG("Setting the underlying radio in poll mode is not allowed!\n");
      return RADIO_RESULT_NOT_SUPPORTED;
    } else {
      return selected_interface->set_value(param, value);
    }
  case RADIO_PARAM_PAN_ID:
  case RADIO_PARAM_16BIT_ADDR:
    for(uint8_t i = 0; i < sizeof(available_interfaces) /
        sizeof(available_interfaces[0]); i++) {
      available_interfaces[i]->set_value(param, value);
    }
    return RADIO_RESULT_OK;
  case RADIO_PARAM_SEL_IF_ID:
    return set_if_via_id((uint8_t)value);
  case RADIO_PARAM_CHANNEL:
  default:
    return selected_interface->set_value(param, value);
  }
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_object(radio_param_t param, void *dest, size_t size)
{
  switch(param) {
  case RADIO_PARAM_SEL_IF_DESC:
    if(size < strlen(selected_interface->driver_descriptor) + 1) {
      return RADIO_RESULT_ERROR;
    }
    strcpy((char *)dest, selected_interface->driver_descriptor);
    return RADIO_RESULT_OK;
  case RADIO_CONST_INTERFACE_ID_COLLECTION:
    if(size == sizeof(if_id_collection_t)) {
      if(if_id_collection.size > 0) {
        *(if_id_collection_t *)dest = if_id_collection;
        return RADIO_RESULT_OK;
      }
      return RADIO_RESULT_NOT_SUPPORTED;
    }
    return RADIO_RESULT_ERROR;
  default:
    return selected_interface->get_object(param, dest, size);
  }
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_object(radio_param_t param, const void *src, size_t size)
{
  switch(param) {
  case RADIO_PARAM_PAN_ID:
  case RADIO_PARAM_16BIT_ADDR:
  case RADIO_PARAM_CHANNEL:
  case RADIO_PARAM_SEL_IF_ID:
    return RADIO_RESULT_NOT_SUPPORTED;
  case RADIO_PARAM_SEL_IF_DESC:
    return set_if_via_desc((char *)src, size);
  case RADIO_PARAM_64BIT_ADDR:
    for(uint8_t i = 0; i < sizeof(available_interfaces) /
        sizeof(available_interfaces[0]); i++) {
      available_interfaces[i]->set_object(param, src, size);
    }
    return RADIO_RESULT_OK;
  default:
    return selected_interface->set_object(param, src, size);
  }
}
/*---------------------------------------------------------------------------*/
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
  uint8_t is_clear = 1;
  for(uint8_t i = 0; i < sizeof(available_interfaces) /
      sizeof(available_interfaces[0]); i++) {
    is_clear = is_clear && available_interfaces[i]->channel_clear();
  }
  return is_clear;
}
/*---------------------------------------------------------------------------*/
static int
receiving_packet_all(void)
{
  uint8_t is_receiving = 0;
  for(uint8_t i = 0; i < sizeof(available_interfaces) /
      sizeof(available_interfaces[0]); i++) {
    is_receiving = is_receiving || available_interfaces[i]->receiving_packet();
  }
  return is_receiving;
}
/*---------------------------------------------------------------------------*/
static int
pending_packet_all(void)
{
  uint8_t is_pending = 0;
  for(uint8_t i = 0; i < sizeof(available_interfaces) /
      sizeof(available_interfaces[0]); i++) {
    is_pending = is_pending || available_interfaces[i]->pending_packet();
  }
  return is_pending;
}