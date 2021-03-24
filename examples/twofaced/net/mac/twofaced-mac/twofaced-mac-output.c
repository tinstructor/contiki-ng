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
static void packet_sent(struct neighbor_queue *nq, struct packet_queue *pq,
                        int status, int num_tx);
/*---------------------------------------------------------------------------*/
static void transmit_from_queue(void *ptr);
/*---------------------------------------------------------------------------*/
static struct neighbor_queue *
neighbor_queue_from_addr(const linkaddr_t *laddr)
{
  struct neighbor_queue *nq = list_head(neighbor_list);
  while(nq != NULL) {
    if(linkaddr_cmp(&nq->laddr, laddr)) {
      return nq;
    }
    nq = list_item_next(nq);
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
create_frame(void)
{
  packetbuf_set_attr(PACKETBUF_ATTR_FRAME_TYPE, FRAME802154_DATAFRAME);
  return NETSTACK_FRAMER.create();
}
/*---------------------------------------------------------------------------*/
static int
send_one_packet(struct neighbor_queue *nq, struct packet_queue *pq)
{
  int ret;
  int last_sent_ok = 0;

  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);

  if(create_frame() < 0) {
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

  packet_sent(nq, pq, ret, 1);
  return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static void
transmit_from_queue(void *ptr)
{
  struct neighbor_queue *nq = ptr;
  if(nq) {
    struct packet_queue *pq = list_head(nq->packet_queue);
    if(pq != NULL) {
      LOG_INFO("preparing packet for ");
      LOG_INFO_LLADDR(&nq->laddr);
      LOG_INFO_(", seqno %u, tx %u, queue %d\n",
                queuebuf_attr(pq->qbuf, PACKETBUF_ATTR_MAC_SEQNO),
                nq->num_tx, list_length(nq->packet_queue));
      /* Send first packet in the neighbor queue */
      queuebuf_to_packetbuf(pq->qbuf);
      send_one_packet(nq, pq);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
schedule_transmission(struct neighbor_queue *nq)
{
  clock_time_t delay;
  int backoff_exponent; /* BE in IEEE 802.15.4 */

  backoff_exponent = MIN(nq->num_col + TWOFACED_MAC_MIN_BE,
                         TWOFACED_MAC_MAX_BE);

  /* Compute max delay cfr. IEEE 802.15.4: 2^BE-1 backoff periods  */
  delay = ((1 << backoff_exponent) - 1) * backoff_period();
  if(delay > 0) {
    /* Pick a time for next transmission */
    delay = random_rand() % delay;
  }

  LOG_DBG("scheduling transmission in %u ticks, NB=%u, BE=%u\n",
          (unsigned)delay, nq->num_col, backoff_exponent);
  ctimer_set(&nq->tx_timer, delay, transmit_from_queue, nq);
}
/*---------------------------------------------------------------------------*/
static void
free_packet(struct neighbor_queue *nq, struct packet_queue *p, int status)
{
  if(p != NULL) {
    /* Remove packet from queue and deallocate */
    list_remove(nq->packet_queue, p);

    queuebuf_free(p->qbuf);
    memb_free(&metadata_memb, p->ptr);
    memb_free(&packet_memb, p);
    LOG_DBG("free_queued_packet, queue length %d, free packets %d\n",
            list_length(nq->packet_queue), memb_numfree(&packet_memb));
    if(list_head(nq->packet_queue) != NULL) {
      /* There is a next packet. We reset current tx information */
      nq->num_tx = 0;
      nq->num_col = 0;
      /* Schedule next transmissions */
      schedule_transmission(nq);
    } else {
      /* This was the last packet in the queue, we free the neighbor */
      ctimer_stop(&nq->tx_timer);
      list_remove(neighbor_list, nq);
      memb_free(&neighbor_memb, nq);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
tx_done(int status, struct packet_queue *pq, struct neighbor_queue *nq)
{
  mac_callback_t sent_callback;
  struct qbuf_metadata *metadata;
  void *ptr;
  uint8_t num_tx;

  metadata = (struct qbuf_metadata *)pq->ptr;
  sent_callback = metadata->sent_callback;
  ptr = metadata->ptr;
  num_tx = nq->num_tx;

  LOG_INFO("packet sent to ");
  LOG_INFO_LLADDR(&nq->laddr);
  LOG_INFO_(", seqno %u, status %u, tx %u, coll %u\n",
            packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO),
            status, nq->num_tx, nq->num_col);

  free_packet(nq, pq, status);
  mac_call_sent_callback(sent_callback, ptr, status, num_tx);
}
/*---------------------------------------------------------------------------*/
static void
rexmit(struct packet_queue *pq, struct neighbor_queue *nq)
{
  schedule_transmission(nq);
  /* This is needed to correctly attribute energy that we spent
     transmitting this packet. */
  queuebuf_update_attr_from_packetbuf(pq->qbuf);
}
/*---------------------------------------------------------------------------*/
static void
collision(struct packet_queue *pq, struct neighbor_queue *nq, int num_tx)
{
  struct qbuf_metadata *metadata;

  metadata = (struct qbuf_metadata *)pq->ptr;

  nq->num_col += num_tx;

  if(nq->num_col > TWOFACED_MAC_MAX_BACKOFF) {
    nq->num_col = 0;
    /* Increment to indicate a next retry */
    nq->num_tx++;
  }

  if(nq->num_tx >= metadata->max_tx) {
    tx_done(MAC_TX_COLLISION, pq, nq);
  } else {
    rexmit(pq, nq);
  }
}
/*---------------------------------------------------------------------------*/
static void
noack(struct packet_queue *pq, struct neighbor_queue *nq, int num_tx)
{
  struct qbuf_metadata *metadata;

  metadata = (struct qbuf_metadata *)pq->ptr;

  nq->num_col = 0;
  nq->num_tx += num_tx;

  if(nq->num_tx >= metadata->max_tx) {
    tx_done(MAC_TX_NOACK, pq, nq);
  } else {
    rexmit(pq, nq);
  }
}
/*---------------------------------------------------------------------------*/
static void
tx_ok(struct packet_queue *pq, struct neighbor_queue *nq, int num_tx)
{
  nq->num_col = 0;
  nq->num_tx += num_tx;
  tx_done(MAC_TX_OK, pq, nq);
}
/*---------------------------------------------------------------------------*/
static void
packet_sent(struct neighbor_queue *nq, struct packet_queue *pq,
            int status, int num_tx)
{
  assert(nq != NULL);
  assert(pq != NULL);

  if(pq->ptr == NULL) {
    LOG_WARN("packet sent: no metadata\n");
    return;
  }

  LOG_INFO("tx to ");
  LOG_INFO_LLADDR(&nq->laddr);
  LOG_INFO_(", seqno %u, status %u, tx %u, coll %u\n",
            packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO),
            status, nq->num_tx, nq->num_col);

  switch(status) {
  case MAC_TX_OK:
    tx_ok(pq, nq, num_tx);
    break;
  case MAC_TX_NOACK:
    noack(pq, nq, num_tx);
    break;
  case MAC_TX_COLLISION:
    collision(pq, nq, num_tx);
    break;
  case MAC_TX_DEFERRED:
    break;
  default:
    tx_done(status, pq, nq);
    break;
  }
}
/*---------------------------------------------------------------------------*/
/* Mac output functions */
/*---------------------------------------------------------------------------*/
void
twofaced_mac_output(mac_callback_t sent_callback, void *ptr)
{
  struct packet_queue *pq;
  struct neighbor_queue *nq;
  static uint8_t initialized = 0;
  static uint8_t seqno;
  const linkaddr_t *laddr = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

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
  /* Non-beacon-enabled mode only, all frames are dataframes */
  packetbuf_set_attr(PACKETBUF_ATTR_FRAME_TYPE, FRAME802154_DATAFRAME);

  /* Look for an existing neighbor list entry */
  nq = neighbor_queue_from_addr(laddr);
  if(nq == NULL) {
    /* Allocate memory for a new neighbor queue */
    nq = memb_alloc(&neighbor_memb);
    if(nq != NULL) {
      /* Init newly allocated neighbor queue */
      linkaddr_copy(&nq->laddr, laddr);
      nq->num_tx = 0;
      nq->num_col = 0;
      /* Init packet queue of new neighbor queue */
      LIST_STRUCT_INIT(nq, packet_queue);
      /* Add new entry (= new neighbor queue) to neighbor list */
      list_add(neighbor_list, nq);
    }
  }

  if(nq != NULL) {
    /* Add packet to the packet queue of neighbor list entry (= neighbor queue) */
    if(list_length(nq->packet_queue) < TWOFACED_MAC_MAX_PACKET_PER_NEIGHBOR) {
      pq = memb_alloc(&packet_memb);
      if(pq != NULL) {
        pq->ptr = memb_alloc(&metadata_memb);
        if(pq->ptr != NULL) {
          pq->qbuf = queuebuf_new_from_packetbuf();
          if(pq->qbuf != NULL) {
            struct qbuf_metadata *metadata = (struct qbuf_metadata *)pq->ptr;
            /* Neighbor and packet successfully allocated */
            metadata->max_tx = packetbuf_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS);
            if(metadata->max_tx == 0) {
              /* If not set by the application, use the default value */
              metadata->max_tx = TWOFACED_MAC_MAX_FRAME_RETRIES + 1;
            }
            metadata->sent_callback = sent_callback;
            metadata->ptr = ptr;
            list_add(nq->packet_queue, pq);

            LOG_INFO("sending to ");
            LOG_INFO_LLADDR(laddr);
            LOG_INFO_(", len %u, seqno %u, queue length %d, free packets %d\n",
                      packetbuf_datalen(),
                      packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO),
                      list_length(nq->packet_queue), memb_numfree(&packet_memb));
            /* If pq is the first packet in the neighbor's queue, send asap */
            if(list_head(nq->packet_queue) == pq) {
              schedule_transmission(nq);
            }
            return;
          }
          memb_free(&metadata_memb, pq->ptr);
          LOG_WARN("could not allocate queuebuf, dropping packet\n");
        }
        memb_free(&packet_memb, pq);
        LOG_WARN("could not allocate queuebuf, dropping packet\n");
      }
      /* The packet allocation failed. Remove and free neighbor entry if empty. */
      if(list_length(nq->packet_queue) == 0) {
        list_remove(neighbor_list, nq);
        memb_free(&neighbor_memb, nq);
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
