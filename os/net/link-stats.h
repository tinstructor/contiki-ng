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

#ifndef LINK_STATS_H_
#define LINK_STATS_H_

#include "net/linkaddr.h"
#include "lib/list.h"
#include "dev/radio.h"

/* ETX fixed point divisor. 128 is the value used by RPL (RFC 6551 and RFC 6719) */
#ifdef LINK_STATS_CONF_ETX_DIVISOR
#define LINK_STATS_ETX_DIVISOR LINK_STATS_CONF_ETX_DIVISOR
#else /* LINK_STATS_CONF_ETX_DIVISOR */
#define LINK_STATS_ETX_DIVISOR                   128
#endif /* LINK_STATS_CONF_ETX_DIVISOR */

/* Option to infer the initial ETX from the RSSI of previously received packets. */
#ifdef LINK_STATS_CONF_INIT_ETX_FROM_RSSI
#define LINK_STATS_INIT_ETX_FROM_RSSI LINK_STATS_CONF_INIT_ETX_FROM_RSSI
#else /* LINK_STATS_CONF_INIT_ETX_FROM_RSSI */
#define LINK_STATS_INIT_ETX_FROM_RSSI              1
#endif /* LINK_STATS_CONF_INIT_ETX_FROM_RSSI */

/* Option to use packet and ACK count for ETX estimation, instead of EWMA */
#ifdef LINK_STATS_CONF_ETX_FROM_PACKET_COUNT
#define LINK_STATS_ETX_FROM_PACKET_COUNT LINK_STATS_CONF_ETX_FROM_PACKET_COUNT
#else /* LINK_STATS_CONF_ETX_FROM_PACKET_COUNT */
#define LINK_STATS_ETX_FROM_PACKET_COUNT           0
#endif /* LINK_STATS_ETX_FROM_PACKET_COUNT */

/* Store and periodically print packet counters? */
#ifdef LINK_STATS_CONF_PACKET_COUNTERS
#define LINK_STATS_PACKET_COUNTERS LINK_STATS_CONF_PACKET_COUNTERS
#else /* LINK_STATS_CONF_PACKET_COUNTERS */
#define LINK_STATS_PACKET_COUNTERS           0
#endif /* LINK_STATS_PACKET_COUNTERS */

/* The maximum number of interfaces per neighbor */
#ifdef LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR
#if LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR <= RADIO_MAX_INTERFACES
#define LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR
#else /* LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR <= RADIO_MAX_INTERFACES */
#error "LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR exceeds RADIO_MAX_INTERFACES"
#endif /* LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR <= RADIO_MAX_INTERFACES */
#else /* LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR */
#define LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR    1U
#endif /* LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR */

/* The metric threshold value "below" which a physical link is considered down.
   Below is defined as the outcome of `LINK_STATS_WORSE_THAN_THRESH( x )` */
#ifdef LINK_STATS_CONF_METRIC_THRESHOLD
#if LINK_STATS_CONF_METRIC_THRESHOLD > 0U
#define LINK_STATS_METRIC_THRESHOLD LINK_STATS_CONF_METRIC_THRESHOLD
#else /* LINK_STATS_CONF_METRIC_THRESHOLD > 0U */
#error "Metric threshold must be greater than 0"
#endif /* LINK_STATS_CONF_METRIC_THRESHOLD > 0U */
#else /* LINK_STATS_CONF_METRIC_THRESHOLD */
#ifdef LINK_STATS_CONF_WITH_ETX
#define LINK_STATS_METRIC_THRESHOLD               0x0300U
#else /* LINK_STATS_CONF_WITH_ETX */
#define LINK_STATS_METRIC_THRESHOLD               1U
#endif /* LINK_STATS_CONF_WITH_ETX */
#endif /* LINK_STATS_METRIC_THRESHOLD */

/* The metric placeholder used in normalization when an inferred
   metric is worse than the metric threshold */
#ifdef LINK_STATS_CONF_METRIC_PLACEHOLDER
#define LINK_STATS_METRIC_PLACEHOLDER LINK_STATS_CONF_METRIC_PLACEHOLDER
#else /* LINK_STATS_CONF_METRIC_PLACEHOLDER */
#ifdef LINK_STATS_CONF_WITH_ETX
#define LINK_STATS_METRIC_PLACEHOLDER             0x0400U
#else /* LINK_STATS_CONF_WITH_ETX */
#define LINK_STATS_METRIC_PLACEHOLDER             7U
#endif /* LINK_STATS_CONF_WITH_ETX */
#endif /* LINK_STATS_METRIC_PLACEHOLDER */

/* Define what it means for x to be worse than the metric threshold */
#ifdef LINK_STATS_CONF_WORSE_THAN_THRESH
#define LINK_STATS_WORSE_THAN_THRESH LINK_STATS_CONF_WORSE_THAN_THRESH
#else /* LINK_STATS_CONF_WORSE_THAN_THRESH */
#ifdef LINK_STATS_CONF_WITH_ETX
#define LINK_STATS_WORSE_THAN_THRESH( x )         ((x)>LINK_STATS_METRIC_THRESHOLD)
#else /* LINK_STATS_CONF_WITH_ETX */
#define LINK_STATS_WORSE_THAN_THRESH( x )         ((x)<LINK_STATS_METRIC_THRESHOLD)
#endif /* LINK_STATS_CONF_WITH_ETX */
#endif /* LINK_STATS_WORSE_THAN_THRESH */

/* Function used to retrieve inferred metric */
#ifdef LINK_STATS_CONF_INFERRED_METRIC_FUNC
#define LINK_STATS_INFERRED_METRIC_FUNC LINK_STATS_CONF_INFERRED_METRIC_FUNC
#else /* LINK_STATS_CONF_INFERRED_METRIC_FUNC */
#ifdef LINK_STATS_CONF_WITH_ETX
#define LINK_STATS_INFERRED_METRIC_FUNC( ile, status, numtx, mi_flag )  get_interface_etx(ile, status, numtx, mi_flag)
#else /* LINK_STATS_CONF_WITH_ETX */
#define LINK_STATS_INFERRED_METRIC_FUNC( ile, status, numtx, mi_flag )  guess_interface_lql_from_rssi(ile, status)
#endif /* LINK_STATS_CONF_WITH_ETX */
#endif /* LINK_STATS_INFERRED_METRIC_FUNC */

/* The default weight assigned to a neighboring interface */
#ifdef LINK_STATS_CONF_DEFAULT_WEIGHT
#if LINK_STATS_CONF_DEFAULT_WEIGHT > 0U
#define LINK_STATS_DEFAULT_WEIGHT LINK_STATS_CONF_DEFAULT_WEIGHT
#else /* LINK_STATS_CONF_DEFAULT_WEIGHT > 0U */
#error "A default interface weight of 0 is not allowed!"
#endif /* LINK_STATS_CONF_DEFAULT_WEIGHT > 0U */
#else /* LINK_STATS_CONF_DEFAULT_WEIGHT */
#define LINK_STATS_DEFAULT_WEIGHT                 1U
#endif /* LINK_STATS_DEFAULT_WEIGHT */

typedef uint16_t link_packet_stat_t;

struct link_packet_counter {
  /* total attempts to transmit unicast packets */
  link_packet_stat_t num_packets_tx;
  /* total ACKs for unicast packets */
  link_packet_stat_t num_packets_acked;
  /* total number of unicast and broadcast packets received */
  link_packet_stat_t num_packets_rx;
};

typedef enum {
  LINK_STATS_WIFSEL_FLAG_FALSE,
  LINK_STATS_WIFSEL_FLAG_TRUE
} link_stats_wifsel_flag_t;

typedef enum {
  LINK_STATS_METRIC_INIT_FLAG_FALSE,
  LINK_STATS_METRIC_INIT_FLAG_TRUE
} link_stats_metric_init_flag_t;

/* All statistics of a given link */
struct link_stats {
  clock_time_t last_tx_time;  /* Last Tx timestamp */
  uint16_t etx;               /* ETX using ETX_DIVISOR as fixed point divisor */
  int16_t rssi;               /* RSSI (received signal strength) */
  uint8_t freshness;          /* Freshness of the statistics */
#if LINK_STATS_ETX_FROM_PACKET_COUNT
  uint8_t tx_count;           /* Tx count, used for ETX calculation */
  uint8_t ack_count;          /* ACK count, used for ETX calculation */
#endif /* LINK_STATS_ETX_FROM_PACKET_COUNT */

#if LINK_STATS_PACKET_COUNTERS
  struct link_packet_counter cnt_current; /* packets in the current period */
  struct link_packet_counter cnt_total;   /* packets in total */
#endif

  uint16_t normalized_metric;           /* Weighted average metric accross interfaces */
  uint8_t pref_if_id;                   /* ID of the preferred interface towards a neighbor */
  link_stats_wifsel_flag_t wifsel_flag; /* Flag indicating if preferred interface selection is weighted */
  LIST_STRUCT(interface_list);          /* List of interfaces and metrics + flags */
};

typedef enum {
  LINK_STATS_DEFER_FLAG_FALSE,
  LINK_STATS_DEFER_FLAG_TRUE
} link_stats_defer_flag_t;

/* An entry in the interface list of a link stats table entry */
struct interface_list_entry {
  struct interface_list_entry *next;
  uint8_t if_id;                        /* Identifier of the interface */
  uint16_t inferred_metric;             /* Inferred metric of physical link */
  link_stats_defer_flag_t defer_flag;   /* The weighted averaging defer flag */
  uint8_t weight;                       /* The weight associated with a neighboring interface */
  clock_time_t last_tx_time;            /* Last tx timestamp for this interface */
  uint8_t freshness;                    /* Freshness of the statistics of this interface */
  int16_t rssi;                         /* RSSI (received signal strength) */
#if LINK_STATS_ETX_FROM_PACKET_COUNT
  uint8_t tx_count;                     /* Tx count, used for ETX calculation */
  uint8_t ack_count;                    /* ACK count, used for ETX calculation */
#endif /* LINK_STATS_ETX_FROM_PACKET_COUNT */
};

/* Modify the wifsel flag to indicate wether or not preferred interface
   selection for a given neighbor (whos link-layer addr is supplied) is
   to be based on weights or not */
int link_stats_modify_wifsel_flag(const linkaddr_t *lladdr, link_stats_wifsel_flag_t value);
/* Modify the weight associated with a neighboring interface by
   supplying said neighbor's link-layer addr, the interface's ID
   and a weight. */
int link_stats_modify_weight(const linkaddr_t *lladdr, uint8_t if_id, uint8_t weight);
/* Modify the weight for all neighbors to which a connection with
   the interface of the given id exists. */
int link_stats_modify_weights(uint8_t if_id, uint8_t weight);
/* Select the preferred interface of the neighbor corresponding
   to the supplied link-layer address. */
int link_stats_select_pref_interface(const linkaddr_t *lladdr);
/* Select the preferred interface for all neighbors. */
int link_stats_select_pref_interfaces(void);
/* Update the normalized metric stored in the link stats table
   entry corresponding to the supplied link-layer address. Note
   that this function does not check the defer flag status, since
   that's the responsibility of the routing protocol and pertains
   only to the preferred parent anyway. */
int link_stats_update_norm_metric(const linkaddr_t *lladdr);
/* Check if metric normalization should be deferred according
   to the current defer flag status of each interface of the
   neighbor corresponding to the supplied link-layer address */
int link_stats_is_defer_required(const linkaddr_t *lladdr);
/* Reset the defer flag of each interface list entry of the
   link stats table entry corresponding to the supplied link-
   layer address */
int link_stats_reset_defer_flags(const linkaddr_t *lladdr);
/* Returns the neighbor's link statistics */
const struct link_stats *link_stats_from_lladdr(const linkaddr_t *lladdr);
/* Returns the address of the neighbor */
const linkaddr_t *link_stats_get_lladdr(const struct link_stats *);
/* Are the statistics fresh? */
int link_stats_is_fresh(const struct link_stats *stats);
/* Are the statistics fresh for interface? */
int link_stats_interface_is_fresh(const struct interface_list_entry *ile);
/* Resets link-stats module */
void link_stats_reset(void);
/* Initializes link-stats module */
void link_stats_init(void);
/* Packet sent callback. Updates statistics for transmissions on a given link */
void link_stats_packet_sent(const linkaddr_t *lladdr, int status, int numtx);
/* Packet input callback. Updates statistics for receptions on a given link */
void link_stats_input_callback(const linkaddr_t *lladdr);

#endif /* LINK_STATS_H_ */
