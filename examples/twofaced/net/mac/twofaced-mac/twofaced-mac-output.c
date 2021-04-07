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
/* NOTE looking at twofaced-mac-types.h you can see that each entry in the
   following neighbor_list is of the type neighbor_queue, and so, each entry
   in the neighbor_list contains: information that allows one to identify the
   neighbor to which the entry is tied (by means of its link-layer address),
   a timer indicating when the next transmission to said neighbor is supposed
   to take place, some statistical information, and finally, a "packet list"
   if you will. This packet list is unfortunately called the packet_queue,
   which is confusing because that's also the type of each element in this
   "packet list". Anyhow, each entry is said list represents a packet to be
   transmitted to the given neighbor. */
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
/**
 * @brief Callback function for the tx scheduling ctimer.
 *
 * This function is called when the ctimer in `schedule_tx()` expires.
 * It copies the contents of the queue buffer stored in the first entry
 * of the "packet list" stored in the supplied neighbor_list entry to
 * the packet buffer before calling `send_one_packet()`.
 *
 * @param ptr an entry in the neighbor_list
 */
static void tx_from_packet_queue(void *ptr);
/*---------------------------------------------------------------------------*/
/**
 * @brief Retrieve an entry from the neighbor_list.
 *
 * @param laddr the link-layer addr of the neighbor represented by a
 *              neighbor list entry
 * @return struct neighbor_queue* an existing entry in the neighbor_list,
 *                                or NULL otherwise
 */
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
/**
 * @brief Create an IEEE 802.15.4 dataframe in the packet buffer.
 *
 * @return int -1 if failed, length of frame header otherwise
 */
static int
create_frame(void)
{
  packetbuf_set_attr(PACKETBUF_ATTR_FRAME_TYPE, FRAME802154_DATAFRAME);
  return NETSTACK_FRAMER.create();
}
/*---------------------------------------------------------------------------*/
/**
 * @brief Sends a single packet to a given neighbor.
 *
 * Prior to calling this function, the packet must be loaded
 * into the packet buffer, presumably by copying it from the
 * queue buffer of the supplied "packet list" entry.
 *
 * @param nq an entry in the neighbor_list
 * @param pq an entry in the "packet list" of the neighbor list entry
 * @return int a MAC_TX_ return value defined in mac.h
 */
static int
send_one_packet(struct neighbor_queue *nq, struct packet_queue *pq)
{
  int ret;
  int last_sent_ok = 0;
  struct qbuf_metadata *metadata;
  radio_value_t if_id;

  /* We require the metadata of the packet to be sent because it contains
     a flag that indicates whether or not the packet must be transmitted
     across a single (i.e., the selected) interface (when the flag is 0)
     or all available interfaces (when the flag is 1) */
  metadata = (struct qbuf_metadata *)pq->metadata;

  if(metadata->if_id != 0) {
    if(NETSTACK_RADIO.get_value(RADIO_CONST_INTERFACE_ID, &if_id) == RADIO_RESULT_OK) {
      if(if_id != metadata->if_id) {
        if(NETSTACK_RADIO.set_value(RADIO_PARAM_SEL_IF_ID, metadata->if_id) == RADIO_RESULT_OK) {
          LOG_DBG("Selected interface with ID = %d (previously %d)\n", metadata->if_id, if_id);
        } else {
          LOG_DBG("Failed selecting interface with ID = %d, keeping current (ID = %d)\n",
                  metadata->if_id, if_id);
        }
      } else {
        LOG_DBG("Interface with ID = %d already selected\n", if_id);
      }
    } else {
      if(NETSTACK_RADIO.set_value(RADIO_PARAM_SEL_IF_ID, metadata->if_id) == RADIO_RESULT_OK) {
        LOG_DBG("Selected interface with ID = %d\n", metadata->if_id);
      } else {
        LOG_DBG("Failed selecting interface with ID = %d, keeping current\n", metadata->if_id);
      }
    }
  } else {
    LOG_DBG("Selecting interface with ID = 0 is not allowed here\n");
  }

  /* When initializing this MAC layer, we must have made sure that the
     underlying radio driver is multi-rf capable and its interface locking /
     unlocking function pointers aren't NULL. Hence we don't need to repeat
     those checks here. */
  if(NETSTACK_RADIO.lock_interface()) {
    LOG_DBG("RF lock acquired before preparing packet\n");

    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
    packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);

    if(create_frame() < 0) {
      /* Failed to allocate space for headers */
      LOG_ERR("failed to create packet, seqno: %d\n",
              packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO));
      ret = MAC_TX_ERR_FATAL;

      LOG_DBG("Unlocking RF lock before tx attempt\n");
      NETSTACK_RADIO.unlock_interface();
    } else {
      int is_broadcast;
      uint8_t dsn;
      dsn = ((uint8_t *)packetbuf_hdrptr())[2] & 0xff;

      NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen());

      is_broadcast = packetbuf_holds_broadcast();

      if(NETSTACK_RADIO.receiving_packet_all() ||
         (!is_broadcast && NETSTACK_RADIO.pending_packet_all())) {

        /* Currently receiving a packet over air or the radio has
           already received a packet that needs to be read before
           sending with auto ack. */
        ret = MAC_TX_COLLISION;

        LOG_DBG("Unlocking RF lock before tx attempt\n");
        NETSTACK_RADIO.unlock_interface();
      } else {
        radio_result_t tx_res = RADIO_TX_ERR;
        /* The reason why we retrieve the idd directly from NETSTACK_RADIO here
           is because it's the safest bet and the interfaces are locked anyway */
        if(NETSTACK_RADIO.get_value(RADIO_CONST_INTERFACE_ID, &if_id) == RADIO_RESULT_OK) {
          LOG_DBG("Attempting tx on interface with ID = %d\n", if_id);
        }
        tx_res = NETSTACK_RADIO.transmit(packetbuf_totlen());
        RTIMER_BUSYWAIT(RTIMER_SECOND / 200);
        switch(tx_res) {
        case RADIO_TX_OK:
          if(is_broadcast) {
            ret = MAC_TX_OK;
          } else {
            /* Check for ack */

            /* Wait for max TWOFACED_MAC_ACK_WAIT_TIME */
            /* REVIEW should we not check for NETSTACK_RADIO.receiving_packet()
               instead? In the very worst case we could even check whether the
               channel is clear with NETSTACK_RADIO.channel_clear() */
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
                  LOG_DBG("ACK received on interface with ID = %d\n",
                          packetbuf_attr(PACKETBUF_ATTR_INTERFACE_ID));
                  ret = MAC_TX_OK;
                } else {
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
        LOG_DBG("Unlocking RF lock after tx attempt\n");
        NETSTACK_RADIO.unlock_interface();
      }
    }
    if(ret == MAC_TX_OK) {
      last_sent_ok = 1;
    }
  } else {
    LOG_DBG("Could not acquire RF lock: deferring transmission\n");
    ret = MAC_TX_DEFERRED;
  }

  packet_sent(nq, pq, ret, 1);
  return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static void
tx_from_packet_queue(void *ptr)
{
  /* The supplied pointer points to the neighbor list entry of which the
     packet_queue or (less confusingly) "packet list" is a member */
  struct neighbor_queue *nq = ptr;
  if(nq) {
    /* Retrieve the first entry in the neighbor list entry's "packet list" */
    struct packet_queue *pq = list_head(nq->packet_queue);
    if(pq != NULL) {
      LOG_INFO("preparing packet for ");
      LOG_INFO_LLADDR(&nq->laddr);
      LOG_INFO_(", seqno %u, tx %u, queue %d\n",
                queuebuf_attr(pq->qbuf, PACKETBUF_ATTR_MAC_SEQNO),
                nq->num_tx, list_length(nq->packet_queue));
      /* Send first packet in the neighbor list entry's "packet list" */
      queuebuf_to_packetbuf(pq->qbuf);
      send_one_packet(nq, pq);
    }
  }
}
/*---------------------------------------------------------------------------*/
/**
 * @brief Schedule a packet transmission.
 *
 * @param nq an entry in the neighbor_list representing the neighbor
 *           to which a packet must be transmitted some time in the
 *           near future
 */
static void
schedule_tx(struct neighbor_queue *nq)
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
  ctimer_set(&nq->tx_timer, delay, tx_from_packet_queue, nq);
}
/*---------------------------------------------------------------------------*/
static void
free_packet(struct neighbor_queue *nq, struct packet_queue *pq, int status)
{
  if(pq != NULL) {
    /* Remove entry from "packet list" and deallocate */
    list_remove(nq->packet_queue, pq);

    queuebuf_free(pq->qbuf);
    memb_free(&metadata_memb, pq->metadata);
    memb_free(&packet_memb, pq);
    LOG_DBG("free_packet, queue length %d, free packets %d\n",
            list_length(nq->packet_queue), memb_numfree(&packet_memb));
    if(list_head(nq->packet_queue) != NULL) {
      /* There is a next packet. We reset current tx information */
      nq->num_tx = 0;
      nq->num_col = 0;
      /* Schedule next transmissions */
      schedule_tx(nq);
    } else {
      /* This was the last packet in the "packet list", freeing neighbor list entry */
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

  metadata = (struct qbuf_metadata *)pq->metadata;
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
retx(struct packet_queue *pq, struct neighbor_queue *nq)
{
  schedule_tx(nq);
  /* This is needed to correctly attribute energy that we spent
     transmitting this packet. */
  queuebuf_update_attr_from_packetbuf(pq->qbuf);
}
/*---------------------------------------------------------------------------*/
static void
collision(struct packet_queue *pq, struct neighbor_queue *nq, int num_tx)
{
  struct qbuf_metadata *metadata;

  metadata = (struct qbuf_metadata *)pq->metadata;

  nq->num_col += num_tx;

  if(nq->num_col > TWOFACED_MAC_MAX_BACKOFF) {
    nq->num_col = 0;
    /* Increment to indicate a next retry */
    nq->num_tx++;
  }

  if(nq->num_tx >= metadata->max_tx) {
    tx_done(MAC_TX_COLLISION, pq, nq);
  } else {
    retx(pq, nq);
  }
}
/*---------------------------------------------------------------------------*/
static void
noack(struct packet_queue *pq, struct neighbor_queue *nq, int num_tx)
{
  struct qbuf_metadata *metadata;

  metadata = (struct qbuf_metadata *)pq->metadata;

  nq->num_col = 0;
  nq->num_tx += num_tx;

  if(nq->num_tx >= metadata->max_tx) {
    tx_done(MAC_TX_NOACK, pq, nq);
  } else {
    retx(pq, nq);
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
packet_sent(struct neighbor_queue *nq, struct packet_queue *pq, int status,
            int num_tx)
{
  assert(nq != NULL);
  assert(pq != NULL);

  if(pq->metadata == NULL) {
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
    retx(pq, nq);
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
    /* PACKETBUF_ATTR_MAC_SEQNO can't be zero due to a peculiarity
       in `os/net/mac/framer/framer-802154.c` */
    seqno++;
  }
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_SEQNO, seqno++);
  /* Non-beacon-enabled mode only, i.e., all frames are dataframes */
  packetbuf_set_attr(PACKETBUF_ATTR_FRAME_TYPE, FRAME802154_DATAFRAME);

  /* Look for an existing neighbor list entry via the neighbor's
     link-layer address (i.e., its MAC addr) */
  nq = neighbor_queue_from_addr(laddr);
  if(nq == NULL) {
    /* There was no pre-existing entry found. Allocate memory
       block for a new neighbor list entry */
    nq = memb_alloc(&neighbor_memb);
    if(nq != NULL) {
      /* Initialize newly allocated neighbor list entry */
      linkaddr_copy(&nq->laddr, laddr);
      nq->num_tx = 0;
      nq->num_col = 0;
      /* Initialize "packet list" (i.e., the parameter called
         packet_queue with elements of type struct packetqueue)
         of new neighbor list entry */
      LIST_STRUCT_INIT(nq, packet_queue);
      /* Add newly allocated and initialized entry
         to neighbor list */
      list_add(neighbor_list, nq);
    }
  }

  if(nq != NULL) {
    /* There either was a pre-existing entry in the neighbor list
       or we just successfully created a new entry. If the "packet
       list" of this neighbor list entry contains less than a fixed
       number of entries, we add a new entry */
    if(list_length(nq->packet_queue) < TWOFACED_MAC_MAX_PACKET_PER_NEIGHBOR) {
      /* Allocate memory block for a new "packet list" entry */
      pq = memb_alloc(&packet_memb);
      if(pq != NULL) {
        /* Allocate memory block for metadata tied to new
           "packet list" entry */
        pq->metadata = memb_alloc(&metadata_memb);
        if(pq->metadata != NULL) {
          /* Copy the contents of the packet buffer to the
             queue buffer of the new "packet list" entry */
          pq->qbuf = queuebuf_new_from_packetbuf();
          if(pq->qbuf != NULL) {
            /* Initialize newly allocated metadata tied
               to new "packet list" entry */
            struct qbuf_metadata *metadata = (struct qbuf_metadata *)pq->metadata;
            metadata->max_tx = packetbuf_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS);
            if(metadata->max_tx == 0) {
              /* If not set by the application, use the default value */
              metadata->max_tx = TWOFACED_MAC_MAX_FRAME_RETRIES + 1;
            }
            metadata->if_id = packetbuf_attr(PACKETBUF_ATTR_INTERFACE_ID);
            metadata->sent_callback = sent_callback;
            metadata->ptr = ptr; /* REVIEW what does this even point to? */
            /* Add entry to the "packet list" of neighbor list entry */
            list_add(nq->packet_queue, pq);

            LOG_INFO("sending to ");
            LOG_INFO_LLADDR(laddr);
            LOG_INFO_(", len %u, seqno %u, queue length %d, free packets %d\n",
                      packetbuf_datalen(),
                      packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO),
                      list_length(nq->packet_queue), memb_numfree(&packet_memb));
            /* If pq is the first packet in the neighbor's queue, schedule ASAP.
               Otherwise, scheduling is performed automatically after successful
               transmission of the previous packet in the queue */
            if(list_head(nq->packet_queue) == pq) {
              schedule_tx(nq);
            }
            return;
          }
          memb_free(&metadata_memb, pq->metadata);
          LOG_WARN("could not allocate queuebuf, dropping packet\n");
        }
        memb_free(&packet_memb, pq);
        LOG_WARN("could not allocate metadata, dropping packet\n");
      }
      /* The packet allocation failed. Remove and free neighbor
         list entry if "packet list" empty */
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
