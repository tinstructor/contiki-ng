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
 *      Output function definitions for the twofaced MAC protocol
 * \author
 *      Robbe Elsas <robbe.elsas@ugent.be>
 */

#include "twofaced-mac-conf.h"
#include "twofaced-mac-types.h"
#include "twofaced-mac-output.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/netstack.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "net/mac/framer/frame802154.h"
#include "net/mac/framer/framer-802154.h"
#include "sys/clock.h"
#include "sys/rtimer.h"
#include "lib/assert.h"
#include "lib/random.h"
#include "dev/watchdog.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "twofaced-mac"
#define LOG_LEVEL LOG_LEVEL_MAC

/*---------------------------------------------------------------------------*/
/* Constants */
/*---------------------------------------------------------------------------*/
#define MAX_QUEUED_PACKETS QUEUEBUF_NUM
/*---------------------------------------------------------------------------*/
/* Variables */
/*---------------------------------------------------------------------------*/
MEMB(neighbor_memb, struct neighbor_queue, TWOFACED_MAC_MAX_NEIGHBOR_QUEUES);
MEMB(packet_memb, struct packet_queue, MAX_QUEUED_PACKETS);
MEMB(metadata_memb, struct qbuf_metadata, MAX_QUEUED_PACKETS);
LIST(neighbor_list);
/*---------------------------------------------------------------------------*/
/* Internal output functions and prototypes */
/*---------------------------------------------------------------------------*/
static void packet_sent(struct neighbor_queue *n, struct packet_queue *q,
                        int status, int num_transmissions);
/*---------------------------------------------------------------------------*/
static void transmit_from_queue(void *ptr);
/*---------------------------------------------------------------------------*/
static struct neighbor_queue *
neighbor_queue_from_addr(const linkaddr_t *addr)
{
  struct neighbor_queue *n = list_head(neighbor_list);
  while(n != NULL) {
    if(linkaddr_cmp(&n->addr, addr)) {
      return n;
    }
    n = list_item_next(n);
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
static clock_time_t
backoff_period(void)
{
#if CONTIKI_TARGET_COOJA
  /* Multiply by 20 to compensate for coarse-grained radio medium with Cooja motes */
  return MAX(20 * CLOCK_SECOND / 3125, 1);
#else /* CONTIKI_TARGET_COOJA */
  /* Use the default aUnitBackoffPeriod of IEEE 802.15.4 */
  return MAX(CLOCK_SECOND / 3125, 1);
#endif /* CONTIKI_TARGET_COOJA */
}
/*---------------------------------------------------------------------------*/
static int
send_one_packet(struct neighbor_queue *n, struct packet_queue *q)
{
  int ret;
  int last_sent_ok = 0;

  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);

  if(twofaced_mac_output_create_frame() < 0) {
    /* Failed to allocate space for headers */
    LOG_ERR("failed to create packet, seqno: %d\n",
            packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO));
    ret = MAC_TX_ERR_FATAL;
  } else {
    int is_broadcast;
    uint8_t dsn;
    dsn = ((uint8_t *)packetbuf_hdrptr())[2] & 0xff;

    NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen());

    is_broadcast = packetbuf_holds_broadcast();

    if(NETSTACK_RADIO.receiving_packet() ||
       (!is_broadcast && NETSTACK_RADIO.pending_packet())) {

      /* Currently receiving a packet over air or the radio has
         already received a packet that needs to be read before
         sending with auto ack. */
      ret = MAC_TX_COLLISION;
    } else {
      radio_result_t foo = NETSTACK_RADIO.transmit(packetbuf_totlen());
      RTIMER_BUSYWAIT(RTIMER_SECOND / 200);
      switch(foo) {
      case RADIO_TX_OK:
        if(is_broadcast) {
          ret = MAC_TX_OK;
        } else {
          /* Check for ack */

          /* Wait for max TWOFACED_MAC_ACK_WAIT_TIME */
          RTIMER_BUSYWAIT_UNTIL(NETSTACK_RADIO.pending_packet(),
                                TWOFACED_MAC_ACK_WAIT_TIME);

          ret = MAC_TX_NOACK;
          if(NETSTACK_RADIO.receiving_packet() ||
             NETSTACK_RADIO.pending_packet() ||
             NETSTACK_RADIO.channel_clear() == 0) {
            int len;
            uint8_t ackbuf[TWOFACED_MAC_ACK_LEN];

            /* Wait an additional TWOFACED_MAC_AFTER_ACK_DETECTED_WAIT_TIME
               to complete reception */
            RTIMER_BUSYWAIT_UNTIL(NETSTACK_RADIO.pending_packet(),
                                  TWOFACED_MAC_AFTER_ACK_DETECTED_WAIT_TIME);

            if(NETSTACK_RADIO.pending_packet()) {
              len = NETSTACK_RADIO.read(ackbuf, TWOFACED_MAC_ACK_LEN);
              if(len == TWOFACED_MAC_ACK_LEN && ackbuf[2] == dsn) {
                /* Ack received */
                LOG_DBG("ACK received\n");
                ret = MAC_TX_OK;
              } else {
                if(ackbuf[2] != dsn) {
                  LOG_DBG("NOACK: dsn %d doesn't match expected (%d)\n",
                          ackbuf[2], dsn);
                } else {
                  LOG_DBG("NOACK: len %d doesn't match expected (%d)\n",
                          len, TWOFACED_MAC_ACK_LEN);
                }
                /* Not an ack or ack not for us: collision */
                ret = MAC_TX_COLLISION;
              }
            } else {
              LOG_DBG("NOACK: TWOFACED_MAC_AFTER_ACK_DETECTED_WAIT_TIME = %d exceeded\n",
                      TWOFACED_MAC_AFTER_ACK_DETECTED_WAIT_TIME);
            }
          } else {
            LOG_DBG("NOACK: TWOFACED_MAC_ACK_WAIT_TIME = %d exceeded\n",
                    TWOFACED_MAC_ACK_WAIT_TIME);
          }
        }
        break;
      case RADIO_TX_COLLISION:
        ret = MAC_TX_COLLISION;
        break;
      default:
        ret = MAC_TX_ERR;
        break;
      }
    }
  }
  if(ret == MAC_TX_OK) {
    last_sent_ok = 1;
  }

  packet_sent(n, q, ret, 1);
  return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static void
transmit_from_queue(void *ptr)
{
  struct neighbor_queue *n = ptr;
  if(n) {
    struct packet_queue *q = list_head(n->packet_queue);
    if(q != NULL) {
      LOG_INFO("preparing packet for ");
      LOG_INFO_LLADDR(&n->addr);
      LOG_INFO_(", seqno %u, tx %u, queue %d\n",
                queuebuf_attr(q->buf, PACKETBUF_ATTR_MAC_SEQNO),
                n->transmissions, list_length(n->packet_queue));
      /* Send first packet in the neighbor queue */
      queuebuf_to_packetbuf(q->buf);
      send_one_packet(n, q);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
schedule_transmission(struct neighbor_queue *n)
{
  clock_time_t delay;
  int backoff_exponent; /* BE in IEEE 802.15.4 */

  backoff_exponent = MIN(n->collisions + TWOFACED_MAC_MIN_BE,
                         TWOFACED_MAC_MAX_BE);

  /* Compute max delay cfr. IEEE 802.15.4: 2^BE-1 backoff periods  */
  delay = ((1 << backoff_exponent) - 1) * backoff_period();
  if(delay > 0) {
    /* Pick a time for next transmission */
    delay = random_rand() % delay;
  }

  LOG_DBG("scheduling transmission in %u ticks, NB=%u, BE=%u\n",
          (unsigned)delay, n->collisions, backoff_exponent);
  ctimer_set(&n->transmit_timer, delay, transmit_from_queue, n);
}
/*---------------------------------------------------------------------------*/
static void
free_packet(struct neighbor_queue *n, struct packet_queue *p, int status)
{
  if(p != NULL) {
    /* Remove packet from queue and deallocate */
    list_remove(n->packet_queue, p);

    queuebuf_free(p->buf);
    memb_free(&metadata_memb, p->ptr);
    memb_free(&packet_memb, p);
    LOG_DBG("free_queued_packet, queue length %d, free packets %d\n",
            list_length(n->packet_queue), memb_numfree(&packet_memb));
    if(list_head(n->packet_queue) != NULL) {
      /* There is a next packet. We reset current tx information */
      n->transmissions = 0;
      n->collisions = 0;
      /* Schedule next transmissions */
      schedule_transmission(n);
    } else {
      /* This was the last packet in the queue, we free the neighbor */
      ctimer_stop(&n->transmit_timer);
      list_remove(neighbor_list, n);
      memb_free(&neighbor_memb, n);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
tx_done(int status, struct packet_queue *q, struct neighbor_queue *n)
{
  mac_callback_t sent_callback;
  struct qbuf_metadata *metadata;
  void *cptr;
  uint8_t ntx;

  metadata = (struct qbuf_metadata *)q->ptr;
  sent_callback = metadata->sent_callback;
  cptr = metadata->cptr;
  ntx = n->transmissions;

  LOG_INFO("packet sent to ");
  LOG_INFO_LLADDR(&n->addr);
  LOG_INFO_(", seqno %u, status %u, tx %u, coll %u\n",
            packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO),
            status, n->transmissions, n->collisions);

  free_packet(n, q, status);
  mac_call_sent_callback(sent_callback, cptr, status, ntx);
}
/*---------------------------------------------------------------------------*/
static void
rexmit(struct packet_queue *q, struct neighbor_queue *n)
{
  schedule_transmission(n);
  /* This is needed to correctly attribute energy that we spent
     transmitting this packet. */
  queuebuf_update_attr_from_packetbuf(q->buf);
}
/*---------------------------------------------------------------------------*/
static void
collision(struct packet_queue *q, struct neighbor_queue *n,
          int num_transmissions)
{
  struct qbuf_metadata *metadata;

  metadata = (struct qbuf_metadata *)q->ptr;

  n->collisions += num_transmissions;

  if(n->collisions > TWOFACED_MAC_MAX_BACKOFF) {
    n->collisions = 0;
    /* Increment to indicate a next retry */
    n->transmissions++;
  }

  if(n->transmissions >= metadata->max_transmissions) {
    tx_done(MAC_TX_COLLISION, q, n);
  } else {
    rexmit(q, n);
  }
}
/*---------------------------------------------------------------------------*/
static void
noack(struct packet_queue *q, struct neighbor_queue *n, int num_transmissions)
{
  struct qbuf_metadata *metadata;

  metadata = (struct qbuf_metadata *)q->ptr;

  n->collisions = 0;
  n->transmissions += num_transmissions;

  if(n->transmissions >= metadata->max_transmissions) {
    tx_done(MAC_TX_NOACK, q, n);
  } else {
    rexmit(q, n);
  }
}
/*---------------------------------------------------------------------------*/
static void
tx_ok(struct packet_queue *q, struct neighbor_queue *n, int num_transmissions)
{
  n->collisions = 0;
  n->transmissions += num_transmissions;
  tx_done(MAC_TX_OK, q, n);
}
/*---------------------------------------------------------------------------*/
static void
packet_sent(struct neighbor_queue *n, struct packet_queue *q,
            int status, int num_transmissions)
{
  assert(n != NULL);
  assert(q != NULL);

  if(q->ptr == NULL) {
    LOG_WARN("packet sent: no metadata\n");
    return;
  }

  LOG_INFO("tx to ");
  LOG_INFO_LLADDR(&n->addr);
  LOG_INFO_(", seqno %u, status %u, tx %u, coll %u\n",
            packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO),
            status, n->transmissions, n->collisions);

  switch(status) {
  case MAC_TX_OK:
    tx_ok(q, n, num_transmissions);
    break;
  case MAC_TX_NOACK:
    noack(q, n, num_transmissions);
    break;
  case MAC_TX_COLLISION:
    collision(q, n, num_transmissions);
    break;
  case MAC_TX_DEFERRED:
    break;
  default:
    tx_done(status, q, n);
    break;
  }
}
/*---------------------------------------------------------------------------*/
/* Mac output functions */
/*---------------------------------------------------------------------------*/
void
twofaced_mac_output(mac_callback_t sent_callback, void *ptr)
{
  struct packet_queue *q;
  struct neighbor_queue *n;
  static uint8_t initialized = 0;
  static uint8_t seqno;
  const linkaddr_t *addr = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

  if(!initialized) {
    initialized = 1;
    /* Initialize the sequence number to a random value as per 802.15.4. */
    seqno = random_rand();
  }

  if(seqno == 0) {
    /* PACKETBUF_ATTR_MAC_SEQNO cannot be zero, due to a pecuilarity
       in framer-802154.c. */
    seqno++;
  }
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_SEQNO, seqno++);
  packetbuf_set_attr(PACKETBUF_ATTR_FRAME_TYPE, FRAME802154_DATAFRAME);

  /* Look for the neighbor entry */
  n = neighbor_queue_from_addr(addr);
  if(n == NULL) {
    /* Allocate a new neighbor entry */
    n = memb_alloc(&neighbor_memb);
    if(n != NULL) {
      /* Init neighbor entry */
      linkaddr_copy(&n->addr, addr);
      n->transmissions = 0;
      n->collisions = 0;
      /* Init packet queue for this neighbor */
      LIST_STRUCT_INIT(n, packet_queue);
      /* Add neighbor to the neighbor list */
      list_add(neighbor_list, n);
    }
  }

  if(n != NULL) {
    /* Add packet to the neighbor's queue */
    if(list_length(n->packet_queue) < TWOFACED_MAC_MAX_PACKET_PER_NEIGHBOR) {
      q = memb_alloc(&packet_memb);
      if(q != NULL) {
        q->ptr = memb_alloc(&metadata_memb);
        if(q->ptr != NULL) {
          q->buf = queuebuf_new_from_packetbuf();
          if(q->buf != NULL) {
            struct qbuf_metadata *metadata = (struct qbuf_metadata *)q->ptr;
            /* Neighbor and packet successfully allocated */
            metadata->max_transmissions = packetbuf_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS);
            if(metadata->max_transmissions == 0) {
              /* If not set by the application, use the default value */
              metadata->max_transmissions = TWOFACED_MAC_MAX_FRAME_RETRIES + 1;
            }
            metadata->sent_callback = sent_callback;
            metadata->cptr = ptr;
            list_add(n->packet_queue, q);

            LOG_INFO("sending to ");
            LOG_INFO_LLADDR(addr);
            LOG_INFO_(", len %u, seqno %u, queue length %d, free packets %d\n",
                      packetbuf_datalen(),
                      packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO),
                      list_length(n->packet_queue), memb_numfree(&packet_memb));
            /* If q is the first packet in the neighbor's queue, send asap */
            if(list_head(n->packet_queue) == q) {
              schedule_transmission(n);
            }
            return;
          }
          memb_free(&metadata_memb, q->ptr);
          LOG_WARN("could not allocate queuebuf, dropping packet\n");
        }
        memb_free(&packet_memb, q);
        LOG_WARN("could not allocate queuebuf, dropping packet\n");
      }
      /* The packet allocation failed. Remove and free neighbor entry if empty. */
      if(list_length(n->packet_queue) == 0) {
        list_remove(neighbor_list, n);
        memb_free(&neighbor_memb, n);
      }
    } else {
      LOG_WARN("Neighbor queue full\n");
    }
    LOG_WARN("could not allocate packet, dropping packet\n");
  } else {
    LOG_WARN("could not allocate neighbor, dropping packet\n");
  }
  mac_call_sent_callback(sent_callback, ptr, MAC_TX_ERR, 1);
}
/*---------------------------------------------------------------------------*/
void
twofaced_mac_output_init(void)
{
  memb_init(&packet_memb);
  memb_init(&metadata_memb);
  memb_init(&neighbor_memb);
}
/*---------------------------------------------------------------------------*/
int
twofaced_mac_output_create_frame(void)
{
  packetbuf_set_attr(PACKETBUF_ATTR_FRAME_TYPE, FRAME802154_DATAFRAME);
  return NETSTACK_FRAMER.create();
}
/*---------------------------------------------------------------------------*/
int
twofaced_mac_output_parse_frame(void)
{
  return NETSTACK_FRAMER.parse();
}
