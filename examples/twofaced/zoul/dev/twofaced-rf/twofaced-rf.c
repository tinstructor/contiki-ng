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
#include "net/netstack.h"
#include "net/packetbuf.h"

#include <string.h>

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
PROCESS(twofaced_rf_process, "twofaced radio driver");
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(twofaced_rf_process, ev, data)
{
  PROCESS_BEGIN();

  PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_EXIT);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/* Radio driver functions */
/*---------------------------------------------------------------------------*/
static int
init(void)
{
  for(uint8_t i = 0; i < sizeof(available_interfaces) /
      sizeof(available_interfaces[0]); i++) {
    available_interfaces[i]->init();
  }

  selected_interface = available_interfaces[0];

  process_start(&twofaced_rf_process, NULL);

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
  prepare(payload, payload_len);
  return transmit(payload_len);
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
  default:
    return selected_interface->get_value(param, value);
  }
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_value(radio_param_t param, radio_value_t value)
{
  switch(param) {
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
  case RADIO_PARAM_SEL_IF:
    if(size < strlen("") + 1) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    for(uint8_t i = 0; i < sizeof(available_interfaces) /
        sizeof(available_interfaces[0]); i++) {
      if((!strcmp((char *)src, available_interfaces[i]->driver_descriptor))) {
        selected_interface = available_interfaces[i];
        return RADIO_RESULT_OK;
      }
    }
    return RADIO_RESULT_INVALID_VALUE;
  default:
    return selected_interface->set_object(param, src, size);
  }
}
/*---------------------------------------------------------------------------*/
/* Internal driver functions */
/*---------------------------------------------------------------------------*/
static void
reset(void)
{
}