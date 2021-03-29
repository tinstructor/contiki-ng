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

  /* TODO even though we just checked the multi-rf capabilities of the radio driver,
     make sure that the interface lock / unlock function pointers aren't NULL */

  /* Check that the radio can correctly report its max supported payload */
  if(NETSTACK_RADIO.get_value(RADIO_CONST_MAX_PAYLOAD_LEN, &radio_max_payload_len) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support getting RADIO_CONST_MAX_PAYLOAD_LEN. Abort init.\n");
    return;
  }

  /* TODO make sure the underlying radio(s) support hardware ACKs
     and enable them if necessary */

  /* TODO make sure the underlying radios are not operating in poll
     mode and disable said mode if necessary */

  twofaced_mac_output_init();
  on();
}
/*---------------------------------------------------------------------------*/
static void
send(mac_callback_t sent_callback, void *ptr)
{
  twofaced_mac_output(sent_callback, ptr);
}
/*---------------------------------------------------------------------------*/
static void
input(void)
{
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
      LOG_INFO_(", seqno %u, len %u\n", packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO), packetbuf_datalen());
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
