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
 *      A MAC protocol implementation that works together with DRiPL and PO
 * \author
 *      Robbe Elsas <robbe.elsas@ugent.be>
 */

#include "twofaced-mac.h"
#include "twofaced-mac-conf.h"
#include "twofaced-mac-output.h"
#include "net/netstack.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/tcpip.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/mac/mac-sequence.h"
#include "sys/mutex.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "twofaced-mac"
#define LOG_LEVEL LOG_LEVEL_MAC

#if LLSEC802154_ENABLED
#error "The twofaced MAC layer doesn't support IEEE 802.15.4 link-layer security (yet)!"
#endif /* LLSEC802154_ENABLED */

/*---------------------------------------------------------------------------*/
/* Constants */
/*---------------------------------------------------------------------------*/
#define TWOFACED_MAC_MAX_HEADER 21
/*---------------------------------------------------------------------------*/
/* Variables */
/*---------------------------------------------------------------------------*/
/* A lock that prevents calling the input function when inappropriate */
static volatile mutex_t input_lock = MUTEX_STATUS_UNLOCKED;
/* The mac callback to call in intercept_callback() */
static mac_callback_t twofaced_mac_sent_callback;
/* The ID of the selected interface prior to all-interfaces tx attempt */
static volatile radio_value_t selected_if_id = 0;
/*---------------------------------------------------------------------------*/
/* The twofaced mac driver exported to Contiki-NG */
/*---------------------------------------------------------------------------*/
const struct mac_driver twofaced_mac_driver = {
  "twofaced_mac",
  init,
  send,
  input,
  on,
  off,
  max_payload,
  lock_input,
  unlock_input,
};
/*---------------------------------------------------------------------------*/
/* Internal driver functions and prototypes */
/*---------------------------------------------------------------------------*/
/* NOTE add internal mac driver functions and prototypes here as required */
static void
intercept_callback(void *ptr, int status, int num_tx)
{
  /* TODO perform checks and only select new interface if
     not already selected */
  NETSTACK_RADIO.set_value(RADIO_PARAM_SEL_IF_ID, selected_if_id);
  mac_call_sent_callback(twofaced_mac_sent_callback, ptr, status, num_tx);
}
/*---------------------------------------------------------------------------*/
/* Mac driver functions */
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  radio_value_t radio_multi_rf = RADIO_MULTI_RF_DIS;
  radio_value_t radio_max_payload_len = 0;

  /* Check that the radio driver is multi-rf enabled */
  if(NETSTACK_RADIO.get_value(RADIO_CONST_MULTI_RF, &radio_multi_rf) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support getting RADIO_CONST_MULTI_RF. Abort init.\n");
    return;
  } else if(radio_multi_rf != RADIO_MULTI_RF_EN) {
    LOG_ERR("! radio does not support multiple concurrent interfaces. Abort init.\n");
    return;
  }

  if(NETSTACK_RADIO.lock_interface == NULL || NETSTACK_RADIO.unlock_interface == NULL) {
    LOG_ERR("! radio does not support locking / unlocking interfaces. Abort init.\n");
    return;
  }

  if(NETSTACK_RADIO.pending_packet_all == NULL || NETSTACK_RADIO.receiving_packet_all == NULL) {
    LOG_ERR("! radio does not support pending / receiving check on all interfaces. Abort init.\n");
    return;
  }

  if(NETSTACK_RADIO.channel_clear_all == NULL) {
    LOG_ERR("! radio does not support channel clear check on all interfaces. Abort init.\n");
    return;
  }

  /* Check that the radio can correctly report its max supported payload */
  if(NETSTACK_RADIO.get_value(RADIO_CONST_MAX_PAYLOAD_LEN, &radio_max_payload_len) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support getting RADIO_CONST_MAX_PAYLOAD_LEN. Abort init.\n");
    return;
  }

  twofaced_mac_output_init();
  /* Turns on all underlying radios when used in conjunction
     with a twofaced_rf_driver (platform-specific) */
  on();
}
/*---------------------------------------------------------------------------*/
static void
send(mac_callback_t sent_callback, void *ptr)
{
  radio_value_t if_id = 0;
  NETSTACK_RADIO.get_value(RADIO_CONST_INTERFACE_ID, &if_id);
  if(packetbuf_attr(PACKETBUF_ATTR_INTERFACE_ID) == 0) {
    packetbuf_set_attr(PACKETBUF_ATTR_INTERFACE_ID, if_id);
  }
  if(packetbuf_attr(PACKETBUF_ATTR_ALL_INTERFACES)) {
    LOG_DBG("Attempting tx on all interfaces with valid ID\n");
    if_id_collection_t if_id_collection;
    if(NETSTACK_RADIO.get_object(RADIO_CONST_INTERFACE_ID_COLLECTION, &if_id_collection,
                                 sizeof(if_id_collection)) == RADIO_RESULT_OK) {
      LOG_DBG("Found %d interfaces with valid ID\n", if_id_collection.size);
      if(if_id_collection.size > 0) {
        struct queuebuf *qbuf = queuebuf_new_from_packetbuf();
        for(uint8_t i = 0; i < if_id_collection.size; i++) {
          queuebuf_to_packetbuf(qbuf);
          packetbuf_set_attr(PACKETBUF_ATTR_INTERFACE_ID, if_id_collection.if_id_list[i]);
          if(i == if_id_collection.size - 1) {
            /* Intercept the callback after the last packet from the all-interfaces
               tx attempt in order to reset the interface to the one that was selected
               prior to the all-interfaces tx attempt */
            /* REVIEW this approach is not ideal because if the selected interface changes
               in between actual transmissions of an all-interfaces tx attempt, then it will
               be reset to the selected interface prior to the all-interfaces tx attempt instead
               of the newly selected interface */
            twofaced_mac_sent_callback = sent_callback;
            selected_if_id = if_id;
            twofaced_mac_output(&intercept_callback, ptr);
          } else {
            twofaced_mac_output(sent_callback, ptr);
          }
        }
        queuebuf_free(qbuf);
      } else {
        LOG_DBG("Found no interfaces with valid ID, attempting tx on default interface\n");
        twofaced_mac_output(sent_callback, ptr);
      }
    } else {
      LOG_DBG("Found no interfaces with valid ID, attempting tx on default interface\n");
      twofaced_mac_output(sent_callback, ptr);
    }
  } else {
    twofaced_mac_output(sent_callback, ptr);
  }
}
/*---------------------------------------------------------------------------*/
static void
input(void)
{
  LOG_DBG("Packet received on interface with ID = %d\n",
          packetbuf_attr(PACKETBUF_ATTR_INTERFACE_ID));
  if(packetbuf_datalen() == TWOFACED_MAC_ACK_LEN) {
    /* Ignore ack packets */
    LOG_DBG("ignored ack\n");
  } else if(NETSTACK_FRAMER.parse() < 0) {
    LOG_ERR("failed to parse %u\n", packetbuf_datalen());
  } else if(!linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                          &linkaddr_node_addr) && !packetbuf_holds_broadcast()) {
    LOG_WARN("not for us\n");
  } else if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_SENDER), &linkaddr_node_addr)) {
    LOG_WARN("frame from ourselves\n");
  } else {
    int duplicate = 0;

    /* Check for duplicate packet. */
    duplicate = mac_sequence_is_duplicate();
    if(duplicate) {
      /* Drop the packet. */
      LOG_WARN("drop duplicate link layer packet from ");
      LOG_WARN_LLADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
      LOG_WARN_(", seqno %u\n", packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO));
    } else {
      mac_sequence_register_seqno();
    }

    if(!duplicate) {
      LOG_INFO("received packet from ");
      LOG_INFO_LLADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
      LOG_INFO_(", seqno %u, len %u\n", packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO),
                packetbuf_datalen());
      NETSTACK_NETWORK.input();
    }
  }
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  return NETSTACK_RADIO.on();
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
  return NETSTACK_RADIO.off();
}
/*---------------------------------------------------------------------------*/
static int
max_payload(void)
{
  radio_value_t radio_max_payload_len;
  int framer_hdr_len;

  if(NETSTACK_RADIO.get_value(RADIO_CONST_MAX_PAYLOAD_LEN, &radio_max_payload_len)
     == RADIO_RESULT_NOT_SUPPORTED) {
    LOG_DBG("Failed to retrieve max payload length from radio driver\n");
    return 0;
  }

  framer_hdr_len = NETSTACK_FRAMER.length();

  if(framer_hdr_len < 0) {
    LOG_DBG("Framer returned error, assuming max header length\n");
    framer_hdr_len = TWOFACED_MAC_MAX_HEADER;
  }

  return MIN(radio_max_payload_len, PACKETBUF_SIZE) - framer_hdr_len;
}
/*---------------------------------------------------------------------------*/
static int
lock_input(void)
{
  return mutex_try_lock(&input_lock);
}
/*---------------------------------------------------------------------------*/
static void
unlock_input(void)
{
  mutex_unlock(&input_lock);
}
