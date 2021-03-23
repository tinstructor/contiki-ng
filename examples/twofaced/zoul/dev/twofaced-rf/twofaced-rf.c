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
#include "sys/mutex.h"

#include <string.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "twofaced-rf"
#define LOG_LEVEL LOG_LEVEL_DBG

/*---------------------------------------------------------------------------*/
/* Constants */
/*---------------------------------------------------------------------------*/
/* The supported interface drivers */
extern const struct radio_driver cc2538_rf_driver;
extern const struct radio_driver cc1200_driver;
static const struct radio_driver *const available_interfaces[] = TWOFACED_RF_AVAILABLE_IFS;
/*---------------------------------------------------------------------------*/
/* Variables */
/*---------------------------------------------------------------------------*/
/* The currently selected interface */
static const struct radio_driver *selected_interface;
/* Lowest reported max payload length of all drivers. Initialized in init() */
static uint16_t max_payload_len;
/* A lock that prevents changing interfaces when innapropriate */
static volatile twofaced_rf_lock_t rf_lock = { .owner = TWOFACED_RF_NO_OWNER };
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
  "twofaced_rf_driver"
};
/*---------------------------------------------------------------------------*/
/* Internal driver functions */
/*---------------------------------------------------------------------------*/
/* NOTE add internal radio driver functions here as required */
/*---------------------------------------------------------------------------*/
/* Radio driver functions */
/*---------------------------------------------------------------------------*/
static int
init(void)
{
  for(uint8_t i = 0; i < sizeof(available_interfaces) /
      sizeof(available_interfaces[0]); i++) {

    radio_value_t reported_max_payload_len = 0;

    if(!available_interfaces[i]->init()) {
      if(!strcmp(available_interfaces[i]->driver_descriptor, "")) {
        LOG_DBG("Failed to init() underlying radio driver\n");
      } else {
        LOG_DBG("Failed to init() underlying radio driver (%s)\n",
                available_interfaces[i]->driver_descriptor);
      }
      return 0; /* REVIEW does one failed driver init() warrant abort? */
    } else {
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
        LOG_INFO("Updated max_payload length from %d to %d\n",
                 max_payload_len, reported_max_payload_len);
        max_payload_len = (uint16_t)reported_max_payload_len;
      }
    }
  }

  selected_interface = available_interfaces[0];

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
prepare(const void *payload, unsigned short payload_len)
{
  if(mutex_try_lock(&rf_lock.lock)) {
    rf_lock.owner = TWOFACED_RF_PREPARE;
    LOG_DBG("RF lock acquired by prepare()\n");
    if(selected_interface->prepare(payload, payload_len) == 1) {
      rf_lock.owner = TWOFACED_RF_NO_OWNER;
      mutex_unlock(&rf_lock.lock);
      LOG_DBG("Unlocking RF lock held by prepare(), copy was unsuccessful\n");
    } else {
      return 0;
    }
  } else {
    LOG_DBG("Could not prepare packet, interfaces are locked\n");
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
transmit(unsigned short transmit_len)
{
  uint8_t ret = RADIO_TX_ERR;

  if(mutex_try_lock(&rf_lock.lock)) {
    /* REVIEW is this case really necessary? */
    rf_lock.owner = TWOFACED_RF_TRANSMIT;
    LOG_DBG("RF lock acquired by transmit()\n");
    ret = selected_interface->transmit(transmit_len);
    rf_lock.owner = TWOFACED_RF_NO_OWNER;
    mutex_unlock(&rf_lock.lock);
    LOG_DBG("Unlocking RF lock held by transmit()\n");
  } else if(rf_lock.owner == TWOFACED_RF_PREPARE) {
    /*
     * NOTE this branch may very rarely erroneously evaluate as true. Say
     * someone acquired the lock, set its owner to TWOFACED_RF_PREPARE and
     * didn't change its owner to TWOFACED_RF_NO_OWNER before unlocking.
     * Then, if and only if, someone else were to lock the lock but forgets
     * to change the owner from TWOFACED_RF_PREPARE (which should have been
     * TWOFACED_RF_NO_OWNER in the first place) to itself, this branch could
     * be entered. THIS MUST NOT HAPPEN!!!
     */
    ret = selected_interface->transmit(transmit_len);
    rf_lock.owner = TWOFACED_RF_NO_OWNER;
    mutex_unlock(&rf_lock.lock);
    LOG_DBG("Unlocking RF lock held by prepare() after tx attempt\n");
  } else {
    LOG_DBG("Could not transmit packet, interfaces are locked\n");
  }

  return ret;
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
  return selected_interface->on();
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
  return selected_interface->off();
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
  case RADIO_PARAM_SEL_IF:
  case RADIO_PARAM_64BIT_ADDR:
    return RADIO_RESULT_NOT_SUPPORTED;
  case RADIO_PARAM_PAN_ID:
  case RADIO_PARAM_16BIT_ADDR:
    for(uint8_t i = 0; i < sizeof(available_interfaces) /
        sizeof(available_interfaces[0]); i++) {
      available_interfaces[i]->set_value(param, value);
    }
    return RADIO_RESULT_OK;
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
  case RADIO_PARAM_SEL_IF:
    if(size < strlen(selected_interface->driver_descriptor) + 1) {
      return RADIO_RESULT_ERROR;
    }
    strcpy((char *)dest, selected_interface->driver_descriptor);
    return RADIO_RESULT_OK;
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
    return RADIO_RESULT_NOT_SUPPORTED;
  case RADIO_PARAM_SEL_IF:
    if(mutex_try_lock(&rf_lock.lock)) {
      rf_lock.owner = TWOFACED_RF_SET_OBJECT;
      LOG_DBG("RF lock acquired by set_object()\n");

      if(size < strlen("") + 1) {
        rf_lock.owner = TWOFACED_RF_NO_OWNER;
        mutex_unlock(&rf_lock.lock);
        LOG_DBG("Unlocking RF lock held by set_object(), no descriptor\n");
        return RADIO_RESULT_INVALID_VALUE;
      }

      if(!strcmp((char *)src, selected_interface->driver_descriptor)) {
        rf_lock.owner = TWOFACED_RF_NO_OWNER;
        mutex_unlock(&rf_lock.lock);
        LOG_DBG("Unlocking RF lock held by set_object(), interface already selected\n");
        return RADIO_RESULT_OK;
      }

      for(uint8_t i = 0; i < sizeof(available_interfaces) /
          sizeof(available_interfaces[0]); i++) {
        if(!strcmp((char *)src, available_interfaces[i]->driver_descriptor)) {
          NETSTACK_MAC.off();
          selected_interface = available_interfaces[i];
          NETSTACK_MAC.on();
          /*
           * TODO we should check if setting the channel of the new iface
           * is successful before continuing and if not, doing something
           * about it.
           */
          selected_interface->set_value(RADIO_PARAM_CHANNEL, TWOFACED_RF_DEFAULT_CHANNEL);
          LOG_DBG("Set channel to %i after switching to new interface\n", TWOFACED_RF_DEFAULT_CHANNEL);
          rf_lock.owner = TWOFACED_RF_NO_OWNER;
          mutex_unlock(&rf_lock.lock);
          LOG_DBG("Unlocking RF lock held by set_object(), interface set\n");
          return RADIO_RESULT_OK;
        }
      }
      rf_lock.owner = TWOFACED_RF_NO_OWNER;
      mutex_unlock(&rf_lock.lock);
      LOG_DBG("Unlocking RF lock held by set_object(), unknown descriptor\n");
      return RADIO_RESULT_INVALID_VALUE;
    } else {
      LOG_DBG("Could not switch interface, interfaces are locked\n");
    }
    return RADIO_RESULT_ERROR;
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
