/*
 * Copyright (c) 2015, SICS Swedish ICT.
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
 *
 * Authors: Simon Duquennoy <simonduq@sics.se>
 */

#include "contiki.h"
#include "sys/clock.h"
#include "net/packetbuf.h"
#include "net/nbr-table.h"
#include "net/link-stats.h"
#include "lib/list.h"
#include "lib/memb.h"
#include <stdio.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "Link Stats"
#define LOG_LEVEL LOG_LEVEL_MAC

/* Maximum value for the Tx count counter */
#define TX_COUNT_MAX                    32

/* Statistics with no update in FRESHNESS_EXPIRATION_TIMEOUT is not fresh */
#define FRESHNESS_EXPIRATION_TIME       (10 * 60 * (clock_time_t)CLOCK_SECOND)
/* Half time for the freshness counter */
#define FRESHNESS_HALF_LIFE             (15 * 60 * (clock_time_t)CLOCK_SECOND)
/* Statistics are fresh if the freshness counter is FRESHNESS_TARGET or more */
#define FRESHNESS_TARGET                 4
/* Maximum value for the freshness counter */
#define FRESHNESS_MAX                   16

/* EWMA (exponential moving average) used to maintain statistics over time */
#define EWMA_SCALE                     100
#define EWMA_ALPHA                      10
#define EWMA_BOOTSTRAP_ALPHA            25

/* ETX fixed point divisor. 128 is the value used by RPL (RFC 6551 and RFC 6719) */
#define ETX_DIVISOR                     LINK_STATS_ETX_DIVISOR
/* In case of no-ACK, add ETX_NOACK_PENALTY to the real Tx count, as a penalty */
#define ETX_NOACK_PENALTY               12
/* Initial ETX value */
#define ETX_DEFAULT                      2

/* Used for guessing metrics based on RSSI */
#define RSSI_HIGH                       -60
#define RSSI_LOW                        -90
#define RSSI_DIFF                       (RSSI_HIGH - RSSI_LOW)

/* Per-neighbor link statistics table */
NBR_TABLE(struct link_stats, link_stats);
MEMB(interface_memb, struct interface_list_entry, NBR_TABLE_MAX_NEIGHBORS * LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR);

/* Called at a period of FRESHNESS_HALF_LIFE */
struct ctimer periodic_timer;

/*---------------------------------------------------------------------------*/
/* Retrieve an entry from the interface list of a supplied
   link stats table entry based on the interface's id */
static struct interface_list_entry *
interface_list_entry_from_id(struct link_stats *stats, uint8_t if_id)
{
  struct interface_list_entry *ile = list_head(stats->interface_list);
  while(ile != NULL) {
    if(ile->if_id == if_id) {
      return ile;
    }
    ile = list_item_next(ile);
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/* Modify the wifsel flag to indicate wether or not preferred interface
   selection for a given neighbor (whos link-layer addr is supplied) is
   to be based on weights or not */
int
link_stats_modify_wifsel_flag(const linkaddr_t *lladdr, link_stats_wifsel_flag_t value)
{
  struct link_stats *stats;
  stats = nbr_table_get_from_lladdr(link_stats, lladdr);
  if(stats == NULL) {
    LOG_DBG("Could not find link stats table entry for ");
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_(", aborting wifsel flag modification\n");
    return 0;
  }
  stats->wifsel_flag = value;
  LOG_DBG("Wifsel flag for ");
  LOG_DBG_LLADDR(lladdr);
  LOG_DBG_(" modified to %d\n", stats->wifsel_flag);
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Modify the weight associated with a neighboring interface by
   supplying said neighbor's link-layer addr, the interface's ID
   and a weight. */
int
link_stats_modify_weight(const linkaddr_t *lladdr, uint8_t if_id, uint8_t weight)
{
  if(weight == 0) {
    LOG_DBG("Setting a weight of 0 is prohibited, aborting weight modification\n");
    return 0;
  }
  struct link_stats *stats;
  stats = nbr_table_get_from_lladdr(link_stats, lladdr);
  if(stats == NULL) {
    LOG_DBG("Could not find link stats table entry for ");
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_(", aborting weight modification\n");
    return 0;
  }
  struct interface_list_entry *ile;
  ile = interface_list_entry_from_id(stats, if_id);
  if(ile == NULL) {
    LOG_DBG("Could not find interface list entry for ");
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_(" and interface ID = %d, aborting weight modification\n", if_id);
    return 0;
  }
  ile->weight = weight;
  LOG_DBG("Weight for interface with ID = %d towards ", if_id);
  LOG_DBG_LLADDR(lladdr);
  LOG_DBG_(" changed to %d\n", weight);
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Modify the weight for all neighbors to which a connection with
   the interface of the given id exists. */
int
link_stats_modify_weights(uint8_t if_id, uint8_t weight)
{
  if(weight == 0) {
    LOG_DBG("Setting a weight of 0 is prohibited, aborting weight modification\n");
    return 0;
  }
  struct link_stats *stats;
  stats = nbr_table_head(link_stats);
  while(stats != NULL) {
    const linkaddr_t *lladdr = link_stats_get_lladdr(stats);
    link_stats_modify_weight(lladdr, if_id, weight);
    stats = nbr_table_next(link_stats, stats);
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Select the preferred interface of the neighbor corresponding
   to the supplied link-layer address. */
int
link_stats_select_pref_interface(const linkaddr_t *lladdr)
{
  struct link_stats *stats;
  stats = nbr_table_get_from_lladdr(link_stats, lladdr);
  if(stats == NULL) {
    LOG_DBG("Could not find link stats table entry for ");
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_(", aborting preferred interface selection\n");
    return 0;
  }
  if(stats->wifsel_flag) {
    LOG_DBG("Preferred interface selection is weight-based for ");
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_("\n");
  } else {
    LOG_DBG("Preferred interface selection is not weight-based for ");
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_("\n");
  }
  struct interface_list_entry *ile, *pref_ile;
  pref_ile = list_head(stats->interface_list);
  ile = list_item_next(pref_ile);
  /* TODO make this iteration much more efficient */
  while(ile != NULL) {
    uint32_t pref_if_metric;
    uint32_t if_metric;
    if(LINK_STATS_WORSE_THAN_THRESH(ile->inferred_metric) == LINK_STATS_WORSE_THAN_THRESH(pref_ile->inferred_metric)) {
      if(LINK_STATS_WORSE_THAN_THRESH(ile->inferred_metric)) {
        /* Both interfaces are down */
        if(LINK_STATS_WORSE_THAN_THRESH(LINK_STATS_METRIC_THRESHOLD - 1)) {
          /* If worse than threshold is defined as "< threshold", use placeholder values */
          pref_if_metric = LINK_STATS_METRIC_PLACEHOLDER;
          if_metric = LINK_STATS_METRIC_PLACEHOLDER;
        } else {
          /* If worse than threshold is defines as "> threshold", use actual values */
          pref_if_metric = pref_ile->inferred_metric;
          if_metric = ile->inferred_metric;
        }
      } else {
        /* Both interfaces are up, use actual values */
        pref_if_metric = pref_ile->inferred_metric;
        if_metric = ile->inferred_metric;
      }
      /* Divide by weights if wifsel flag is set */
      if(stats->wifsel_flag) {
        /* Increase precision to 4 decimal points */
        pref_if_metric *= 10000;
        if_metric *= 10000;
        /* If weight is zero, use default weight */
        uint8_t pref_if_weight = pref_ile->weight ? pref_ile->weight : LINK_STATS_DEFAULT_WEIGHT;
        uint8_t if_weight = ile->weight ? ile->weight : LINK_STATS_DEFAULT_WEIGHT;
        /* Integer division rounded to nearest */
        pref_if_metric = (pref_if_metric + pref_if_weight / 2) / pref_if_weight;
        if_metric = (if_metric + if_weight / 2) / if_weight;
      }
      /* If metric of next interface is better than metric of pref if, new pref if */
      pref_ile = (if_metric < pref_if_metric) ? ile : pref_ile;
    } else if(LINK_STATS_WORSE_THAN_THRESH(pref_ile->inferred_metric)) {
      /* The next if is better simply because it is up and the currently pref if is down! */
      pref_ile = ile;
    }
    ile = list_item_next(ile);
  }
  LOG_DBG("Setting preferred interface for ");
  LOG_DBG_LLADDR(lladdr);
  LOG_DBG_(" to interface with ID = %d (previously ID = %d)\n",
           pref_ile->if_id, stats->pref_if_id);
  stats->pref_if_id = pref_ile->if_id;
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Select the preferred interface for all neighbors. */
int 
link_stats_select_pref_interfaces(void)
{
  struct link_stats *stats;
  stats = nbr_table_head(link_stats);
  while(stats != NULL) {
    const linkaddr_t *lladdr = link_stats_get_lladdr(stats);
    link_stats_select_pref_interface(lladdr);
    stats = nbr_table_next(link_stats, stats);
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Update the normalized metric stored in the link stats table
   entry corresponding to the supplied link-layer address. Note
   that this function does not check the defer flag status, since
   that's the responsibility of the routing protocol and pertains
   only to the preferred parent anyway. */
int
link_stats_update_norm_metric(const linkaddr_t *lladdr)
{
  struct link_stats *stats;
  stats = nbr_table_get_from_lladdr(link_stats, lladdr);
  if(stats == NULL) {
    LOG_DBG("Could not find link stats table entry for ");
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_(", aborting normalized metric update\n");
    return 0;
  }
  struct interface_list_entry *ile;
  ile = list_head(stats->interface_list);
  uint8_t num_if = 0;
  uint32_t numerator = 0;
  uint16_t denominator = 0;
  while(ile != NULL) {
    uint32_t inferred_metric;
    uint8_t weight;
    inferred_metric = LINK_STATS_WORSE_THAN_THRESH(ile->inferred_metric) ? LINK_STATS_METRIC_PLACEHOLDER : ile->inferred_metric;
    weight = ile->weight ? ile->weight : LINK_STATS_DEFAULT_WEIGHT;
    numerator += (inferred_metric * weight);
    denominator += weight;
    num_if++;
    ile = list_item_next(ile);
  }
  if(num_if > LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR) {
    LOG_DBG("Num ifaces found > LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR ");
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_(", aborting normalized metric update\n");
    return 0;
  }
  uint8_t num_if_left = LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR - num_if;
  numerator += (num_if_left * LINK_STATS_METRIC_PLACEHOLDER * LINK_STATS_DEFAULT_WEIGHT);
  denominator += (num_if_left * LINK_STATS_DEFAULT_WEIGHT);
  /* Never divide by zero or the universe might implode */
  denominator = denominator ? denominator : 1;
  /* Integer division but rounded to nearest integer */
  stats->normalized_metric = (numerator + denominator / 2) / denominator;
  LOG_DBG("Normalized metric for ");
  LOG_DBG_LLADDR(lladdr);
  LOG_DBG_(" updated to %d\n", stats->normalized_metric);
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Check if metric normalization should be deferred according
   to the current defer flag status of each interface of the
   neighbor corresponding to the supplied link-layer address */
int
link_stats_is_defer_required(const linkaddr_t *lladdr)
{
  struct link_stats *stats;
  stats = nbr_table_get_from_lladdr(link_stats, lladdr);
  if(stats == NULL) {
    LOG_DBG("Could not find link stats table entry for ");
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_(", aborting check of defer requirement\n");
    return -1;
  }
  struct interface_list_entry *ile;
  ile = list_head(stats->interface_list);
  uint8_t num_def = 0;
  while(ile != NULL) {
    if(ile->defer_flag) {
      num_def++;
    }
    ile = list_item_next(ile);
  }
  /* If an interface is not in a neighbor's interface list at this
     point, it has not gone down but it is has simply never been
     available (yet) at all. Hence, it makes sense to return 1 when
     the number of set defer flags is less than nominal but greater
     than zero. */
  if(num_def > 0 && num_def < LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR) {
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/* Reset the defer flag of each interface list entry of the
   link stats table entry corresponding to the supplied link-
   layer address */
int
link_stats_reset_defer_flags(const linkaddr_t *lladdr)
{
  struct link_stats *stats;
  stats = nbr_table_get_from_lladdr(link_stats, lladdr);
  if(stats == NULL) {
    LOG_DBG("Could not find link stats table entry for ");
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_(", aborting defer flag reset\n");
    return 0;
  }
  struct interface_list_entry *ile;
  ile = list_head(stats->interface_list);
  while(ile != NULL) {
    ile->defer_flag = LINK_STATS_DEFER_FLAG_FALSE;
    ile = list_item_next(ile);
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Returns the neighbor's link stats */
const struct link_stats *
link_stats_from_lladdr(const linkaddr_t *lladdr)
{
  return nbr_table_get_from_lladdr(link_stats, lladdr);
}
/*---------------------------------------------------------------------------*/
/* Returns the neighbor's address given a link stats item */
const linkaddr_t *
link_stats_get_lladdr(const struct link_stats *stat)
{
  return nbr_table_get_lladdr(link_stats, stat);
}
/*---------------------------------------------------------------------------*/
/* Are the statistics fresh? */
int
link_stats_is_fresh(const struct link_stats *stats)
{
  return (stats != NULL)
      && clock_time() - stats->last_tx_time < FRESHNESS_EXPIRATION_TIME
      && stats->freshness >= FRESHNESS_TARGET;
}
/*---------------------------------------------------------------------------*/
#if LINK_STATS_INIT_ETX_FROM_RSSI
uint16_t
guess_etx_from_rssi(const struct link_stats *stats)
{
  if(stats != NULL) {
    if(stats->rssi == 0) {
      return ETX_DEFAULT * ETX_DIVISOR;
    } else {
      /* A rough estimate of PRR from RSSI, as a linear function where:
       *      RSSI >= -60 results in PRR of 1
       *      RSSI <= -90 results in PRR of 0
       * prr = (bounded_rssi - RSSI_LOW) / (RSSI_DIFF)
       * etx = ETX_DIVOSOR / ((bounded_rssi - RSSI_LOW) / RSSI_DIFF)
       * etx = (RSSI_DIFF * ETX_DIVOSOR) / (bounded_rssi - RSSI_LOW)
       * */
#define ETX_INIT_MAX 3
      uint16_t etx;
      int16_t bounded_rssi = stats->rssi;
      bounded_rssi = MIN(bounded_rssi, RSSI_HIGH);
      bounded_rssi = MAX(bounded_rssi, RSSI_LOW + 1);
      etx = RSSI_DIFF * ETX_DIVISOR / (bounded_rssi - RSSI_LOW);
      return MIN(etx, ETX_INIT_MAX * ETX_DIVISOR);
    }
  }
  return 0xffff;
}
#endif /* LINK_STATS_INIT_ETX_FROM_RSSI */
/*---------------------------------------------------------------------------*/
/* Guess the link-quality level [1-7] from the RSSI of a received packet 
   that currently still resides in the packet buffer or return 0 if the
   given status != MAC_TX_OK. */
uint16_t
guess_lql_from_rssi(int status)
{
  if(status == MAC_TX_OK) {
    uint16_t lql;
    int16_t bounded_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    bounded_rssi = MIN(bounded_rssi, RSSI_HIGH);
    bounded_rssi = MAX(bounded_rssi, RSSI_LOW + 1);
    lql = 7 - ((((bounded_rssi - RSSI_LOW) * 6) + RSSI_DIFF / 2) / RSSI_DIFF);
    LOG_DBG("RSSI mapped to LQL = %d\n", lql);
    return lql;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/* Packet sent callback. Updates stats for transmissions to lladdr */
void
link_stats_packet_sent(const linkaddr_t *lladdr, int status, int numtx)
{
  struct interface_list_entry *ile;
  struct link_stats *stats;
#if !LINK_STATS_ETX_FROM_PACKET_COUNT
  uint16_t packet_etx;
  uint8_t ewma_alpha;
#endif /* !LINK_STATS_ETX_FROM_PACKET_COUNT */

  if(status != MAC_TX_OK && status != MAC_TX_NOACK) {
    /* Do not penalize the ETX when collisions or transmission errors occur. */
    return;
  }

  stats = nbr_table_get_from_lladdr(link_stats, lladdr);
  if(stats == NULL) {
    /* If transmission failed, do not add the neighbor, as the neighbor might not exist anymore */
    if(status != MAC_TX_OK) {
      return;
    }

    /* Add the neighbor */
    stats = nbr_table_add_lladdr(link_stats, lladdr, NBR_TABLE_REASON_LINK_STATS, NULL);
    if(stats != NULL) {
#if LINK_STATS_INIT_ETX_FROM_RSSI
      stats->etx = guess_etx_from_rssi(stats);
#else /* LINK_STATS_INIT_ETX_FROM_RSSI */
      stats->etx = ETX_DEFAULT * ETX_DIVISOR;
#endif /* LINK_STATS_INIT_ETX_FROM_RSSI */
    } else {
      return; /* No space left, return */
    }
    LIST_STRUCT_INIT(stats, interface_list);
  }

  uint8_t if_id = packetbuf_attr(PACKETBUF_ATTR_INTERFACE_ID);
  ile = interface_list_entry_from_id(stats, if_id);
  if(ile != NULL) {
    /* Update the existing ile */
    LOG_DBG("Interface with ID = %d already in interface list of ", if_id);
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_("\n");
    uint16_t old_metric = ile->inferred_metric;
    /* Set inferred metric to worse than threshold if no ACK was received */
    ile->inferred_metric = LINK_STATS_INFERRED_METRIC_FUNC(status);
    LOG_DBG("Updated metric to %d (previously %d) for interface with ID = %d of ",
            ile->inferred_metric,
            old_metric,
            if_id);
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_("\n");
    /* When an inferred metric is not updated, or when it is but it doesn't
       cross the metric threshold in any direction, the link-layer may not
       update the corresponding defer flag */
    if(old_metric != ile->inferred_metric) {
      if(LINK_STATS_WORSE_THAN_THRESH(old_metric) &&
        !LINK_STATS_WORSE_THAN_THRESH(ile->inferred_metric)) {
        ile->defer_flag = LINK_STATS_DEFER_FLAG_FALSE;
        LOG_DBG("Defer flag of interface with ID = %d of ", if_id);
        LOG_DBG_LLADDR(lladdr);
        LOG_DBG_(" reset because metric crossed threshold\n");
      } else if(!LINK_STATS_WORSE_THAN_THRESH(old_metric) &&
                LINK_STATS_WORSE_THAN_THRESH(ile->inferred_metric)) {
        ile->defer_flag = LINK_STATS_DEFER_FLAG_TRUE;
        LOG_DBG("Defer flag of interface with ID = %d of ", if_id);
        LOG_DBG_LLADDR(lladdr);
        LOG_DBG_(" set because metric crossed threshold\n");
      }
      /* It makes no sense to re-select the preferred interface if there's
         no change in inferred metric for the given interface (represented
         by the ile) */
      link_stats_select_pref_interface(lladdr);
    }
  } else {
    if(list_length(stats->interface_list) < LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR) {
      /* Create new ile and add to interface list */
      ile = memb_alloc(&interface_memb);
      if(ile != NULL) {
        ile->if_id = if_id;
        /* Set inferred metric to worse than threshold if no ACK was received */
        ile->inferred_metric = LINK_STATS_INFERRED_METRIC_FUNC(status);
        ile->weight = LINK_STATS_DEFAULT_WEIGHT;
        list_add(stats->interface_list, ile);
        LOG_DBG("Added interface with ID = %d (metric = %d) to interface list of ", if_id, ile->inferred_metric);
        LOG_DBG_LLADDR(lladdr);
        LOG_DBG_("\n");
        /* REVIEW maybe it's a good idea to reset all defer flags upon
           adding a newly discovered interface to a neighbor's interface
           list? Maybe it's a better idea to force an update and leave
           the defer flags as is? We're going with the latter for now */
        link_stats_update_norm_metric(lladdr);
        link_stats_select_pref_interface(lladdr);
      } else {
        LOG_DBG("Could not allocate interface list entry\n");
        return;
      }
    }
  }

  /* Update last timestamp and freshness */
  /* FIXME this is problematic when running multi-rf because freshness should then be interface-scope instead of
     neighbor-scope. The problem with all of this is that stats->last_tx_time and stats->freshness apply to an entire
     neighbor (represented by the link-stats table entry in which they're stored) instead of just on interface to said
     neighbor and hence, these values are updated each time a unicast packet is sent (if status was MAC_TX_OK or, if
     link-stats table entry already existed, MAC_TX_NOACK) regardless of the interface over which it was sent. However,
     since selection of the probing target relies on these values and, in turn, probes are the primary way to keep the
     inferred metrics of all interfaces of a parent up to date (since probes are unicast over each interface), it
     could happen that the inferred metrics of a parent's non-preferred interfaces are hardly ever updated because the
     freshness mechanism indicates that we have recently transmitted a (unicast) packet to said parent and thus the
     metric for said parent would theoretically be up to date. While this is accurate for the legacy implementation of
     the link-stats module, since most packets sent to a parent (or any other neighbor for that matter) are only sent
     over its preferred interface, freshness of a neighbor('s metrics) is no longer guaranteed this way (at least not
     for all interfaces). This problem is worst for neighbors to which we send the most packets, which means that it
     is mostly problematic for the preferred parent */
  stats->last_tx_time = clock_time();
  stats->freshness = MIN(stats->freshness + numtx, FRESHNESS_MAX);

#if LINK_STATS_PACKET_COUNTERS
  /* Update paket counters */
  stats->cnt_current.num_packets_tx += numtx;
  if(status == MAC_TX_OK) {
    stats->cnt_current.num_packets_acked++;
  }
#endif

  /* Add penalty in case of no-ACK */
  if(status == MAC_TX_NOACK) {
    numtx += ETX_NOACK_PENALTY;
  }

#if LINK_STATS_ETX_FROM_PACKET_COUNT
  /* Compute ETX from packet and ACK count */
  /* Halve both counter after TX_COUNT_MAX */
  if(stats->tx_count + numtx > TX_COUNT_MAX) {
    stats->tx_count /= 2;
    stats->ack_count /= 2;
  }
  /* Update tx_count and ack_count */
  stats->tx_count += numtx;
  if(status == MAC_TX_OK) {
    stats->ack_count++;
  }
  /* Compute ETX */
  if(stats->ack_count > 0) {
    stats->etx = ((uint16_t)stats->tx_count * ETX_DIVISOR) / stats->ack_count;
  } else {
    stats->etx = (uint16_t)MAX(ETX_NOACK_PENALTY, stats->tx_count) * ETX_DIVISOR;
  }
#else /* LINK_STATS_ETX_FROM_PACKET_COUNT */
  /* Compute ETX using an EWMA */

  /* ETX used for this update */
  packet_etx = numtx * ETX_DIVISOR;
  /* ETX alpha used for this update */
  ewma_alpha = link_stats_is_fresh(stats) ? EWMA_ALPHA : EWMA_BOOTSTRAP_ALPHA;

  /* Compute EWMA and update ETX */
  stats->etx = ((uint32_t)stats->etx * (EWMA_SCALE - ewma_alpha) +
      (uint32_t)packet_etx * ewma_alpha) / EWMA_SCALE;
#endif /* LINK_STATS_ETX_FROM_PACKET_COUNT */
}
/*---------------------------------------------------------------------------*/
/* Packet input callback. Updates statistics for receptions on a given link */
void
link_stats_input_callback(const linkaddr_t *lladdr)
{
  struct interface_list_entry *ile;
  struct link_stats *stats;
  int16_t packet_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

  stats = nbr_table_get_from_lladdr(link_stats, lladdr);
  if(stats == NULL) {
    /* Add the neighbor */
    stats = nbr_table_add_lladdr(link_stats, lladdr, NBR_TABLE_REASON_LINK_STATS, NULL);
    if(stats != NULL) {
      /* Initialize */
      stats->rssi = packet_rssi;
#if LINK_STATS_INIT_ETX_FROM_RSSI
      stats->etx = guess_etx_from_rssi(stats);
#else /* LINK_STATS_INIT_ETX_FROM_RSSI */
      stats->etx = ETX_DEFAULT * ETX_DIVISOR;
#endif /* LINK_STATS_INIT_ETX_FROM_RSSI */
#if LINK_STATS_PACKET_COUNTERS
      stats->cnt_current.num_packets_rx = 1;
#endif
    } else {
      return; /* No space left, return */
    }
    LIST_STRUCT_INIT(stats, interface_list);
  }

  uint8_t if_id = packetbuf_attr(PACKETBUF_ATTR_INTERFACE_ID);
  ile = interface_list_entry_from_id(stats, if_id);
  if(ile != NULL) {
    /* Update the existing ile */
    LOG_DBG("Interface with ID = %d already in interface list of ", if_id);
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_("\n");
    uint16_t old_metric = ile->inferred_metric;
    ile->inferred_metric = LINK_STATS_INFERRED_METRIC_FUNC(MAC_TX_OK);
    LOG_DBG("Updated metric to %d (previously %d) for interface with ID = %d of ",
            ile->inferred_metric,
            old_metric,
            if_id);
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_("\n");
    /* When an inferred metric is not updated, or when it is but it doesn't
       cross the metric threshold in any direction, the link-layer may not
       update the corresponding defer flag */
    if(old_metric != ile->inferred_metric) {
      if(LINK_STATS_WORSE_THAN_THRESH(old_metric) &&
         !LINK_STATS_WORSE_THAN_THRESH(ile->inferred_metric)) {
        ile->defer_flag = LINK_STATS_DEFER_FLAG_FALSE;
        LOG_DBG("Defer flag of interface with ID = %d of ", if_id);
        LOG_DBG_LLADDR(lladdr);
        LOG_DBG_(" reset because metric crossed threshold\n");
      } else if(!LINK_STATS_WORSE_THAN_THRESH(old_metric) &&
                LINK_STATS_WORSE_THAN_THRESH(ile->inferred_metric)) {
        ile->defer_flag = LINK_STATS_DEFER_FLAG_TRUE;
        LOG_DBG("Defer flag of interface with ID = %d of ", if_id);
        LOG_DBG_LLADDR(lladdr);
        LOG_DBG_(" set because metric crossed threshold\n");
      }
      /* It makes no sense to re-select the preferred interface if there's
         no change in inferred metric for the given interface (represented
         by the ile) */
      link_stats_select_pref_interface(lladdr);
    }
  } else {
    if(list_length(stats->interface_list) < LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR) {
      /* Create new ile and add to interface list */
      ile = memb_alloc(&interface_memb);
      if(ile != NULL) {
        ile->if_id = if_id;
        ile->inferred_metric = LINK_STATS_INFERRED_METRIC_FUNC(MAC_TX_OK);
        ile->weight = LINK_STATS_DEFAULT_WEIGHT;
        list_add(stats->interface_list, ile);
        LOG_DBG("Added interface with ID = %d (metric = %d) to interface list of ", if_id, ile->inferred_metric);
        LOG_DBG_LLADDR(lladdr);
        LOG_DBG_("\n");
        /* REVIEW maybe it's a good idea to reset all defer flags upon
           adding a newly discovered interface to a neighbor's interface
           list? Maybe it's a better idea to force an update and leave
           the defer flags as is? We're going with the latter for now */
        link_stats_update_norm_metric(lladdr);
        link_stats_select_pref_interface(lladdr);
      } else {
        LOG_DBG("Could not allocate interface list entry\n");
        return;
      }
    }
  }

  /* Update RSSI EWMA */
  stats->rssi = ((int32_t)stats->rssi * (EWMA_SCALE - EWMA_ALPHA) +
      (int32_t)packet_rssi * EWMA_ALPHA) / EWMA_SCALE;

#if LINK_STATS_PACKET_COUNTERS
  stats->cnt_current.num_packets_rx++;
#endif
}
/*---------------------------------------------------------------------------*/
#if LINK_STATS_PACKET_COUNTERS
/*---------------------------------------------------------------------------*/
static void
print_and_update_counters(void)
{
  struct link_stats *stats;

  for(stats = nbr_table_head(link_stats); stats != NULL;
      stats = nbr_table_next(link_stats, stats)) {

    struct link_packet_counter *c = &stats->cnt_current;

    LOG_INFO("num packets: tx=%u ack=%u rx=%u to=",
             c->num_packets_tx, c->num_packets_acked, c->num_packets_rx);
    LOG_INFO_LLADDR(link_stats_get_lladdr(stats));
    LOG_INFO_("\n");

    stats->cnt_total.num_packets_tx += stats->cnt_current.num_packets_tx;
    stats->cnt_total.num_packets_acked += stats->cnt_current.num_packets_acked;
    stats->cnt_total.num_packets_rx += stats->cnt_current.num_packets_rx;
    memset(&stats->cnt_current, 0, sizeof(stats->cnt_current));
  }
}
/*---------------------------------------------------------------------------*/
#endif /* LINK_STATS_PACKET_COUNTERS */
/*---------------------------------------------------------------------------*/
/* Periodic timer called at a period of FRESHNESS_HALF_LIFE */
static void
periodic(void *ptr)
{
  /* Age (by halving) freshness counter of all neighbors */
  struct link_stats *stats;
  ctimer_reset(&periodic_timer);
  for(stats = nbr_table_head(link_stats); stats != NULL; stats = nbr_table_next(link_stats, stats)) {
    stats->freshness >>= 1;
  }

#if LINK_STATS_PACKET_COUNTERS
  print_and_update_counters();
#endif
}
/*---------------------------------------------------------------------------*/
/* Resets link-stats module */
void
link_stats_reset(void)
{
  struct link_stats *stats;
  stats = nbr_table_head(link_stats);
  while(stats != NULL) {
    while(list_head(stats->interface_list) != NULL) {
      struct interface_list_entry *ile = list_head(stats->interface_list);
      list_remove(stats->interface_list, ile);
      memb_free(&interface_memb, ile);
    }
    nbr_table_remove(link_stats, stats);
    stats = nbr_table_next(link_stats, stats);
  }
}
/*---------------------------------------------------------------------------*/
/* Initializes link-stats module */
void
link_stats_init(void)
{
  nbr_table_register(link_stats, NULL);
  memb_init(&interface_memb);
  ctimer_set(&periodic_timer, FRESHNESS_HALF_LIFE, periodic, NULL);
}
