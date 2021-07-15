/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
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
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Logic for Directed Acyclic Graphs in RPL.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 * Contributors: George Oikonomou <oikonomou@users.sourceforge.net> (multicast)
 */

/**
 * \addtogroup uip
 * @{
 */

#include "contiki.h"
#include "net/link-stats.h"
#include "net/routing/rpl-classic/rpl.h"
#include "net/routing/rpl-classic/rpl-private.h"
#include "net/routing/rpl-classic/rpl-dag-root.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-nd6.h"
#include "net/ipv6/uip-ds6-nbr.h"
#include "net/nbr-table.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "sys/ctimer.h"
#include "sys/log.h"
#include "dev/radio.h"

#if RPL_WEIGHTED_INTERFACES
#include <math.h>
#endif

#include <limits.h>
#include <string.h>

#define LOG_MODULE "RPL"
#define LOG_LEVEL LOG_LEVEL_RPL

/* A configurable function called after every RPL parent switch */
#ifdef RPL_CALLBACK_PARENT_SWITCH
void RPL_CALLBACK_PARENT_SWITCH(rpl_parent_t *old, rpl_parent_t *new);
#endif /* RPL_CALLBACK_PARENT_SWITCH */

/*---------------------------------------------------------------------------*/
extern rpl_of_t rpl_of0, rpl_mrhof, rpl_poof, rpl_driplof;
static rpl_of_t * const objective_functions[] = RPL_SUPPORTED_OFS;

/*---------------------------------------------------------------------------*/
/* RPL definitions. */

#ifndef RPL_CONF_GROUNDED
#define RPL_GROUNDED                    0
#else
#define RPL_GROUNDED                    RPL_CONF_GROUNDED
#endif /* !RPL_CONF_GROUNDED */

/*---------------------------------------------------------------------------*/
/* Per-parent RPL information */
NBR_TABLE_GLOBAL(rpl_parent_t, rpl_parents);
/*---------------------------------------------------------------------------*/
/* Allocate instance table. */
rpl_instance_t instance_table[RPL_MAX_INSTANCES];
rpl_instance_t *default_instance;
/*---------------------------------------------------------------------------*/
/* Pointer to the instance we are currently poisoning if any, NULL otherwise */
rpl_instance_t *poisoning_instance;
/*---------------------------------------------------------------------------*/
/* A collection of interface IDs together with their weight */
#if RPL_WEIGHTED_INTERFACES
static rpl_ifw_collection_t rpl_ifw_collection  = { .size = 0 };
#endif
/*---------------------------------------------------------------------------*/
void
rpl_print_neighbor_list(void)
{
  if(default_instance != NULL && default_instance->current_dag != NULL &&
      default_instance->of != NULL) {
    int curr_dio_interval = default_instance->dio_intcurrent;
    int curr_rank = default_instance->current_dag->rank;
    rpl_parent_t *p = nbr_table_head(rpl_parents);
    clock_time_t clock_now = clock_time();

    LOG_DBG("RPL: MOP %u OCP %u rank %u dioint %u, nbr count %u\n",
        default_instance->mop, default_instance->of->ocp, curr_rank, curr_dio_interval, uip_ds6_nbr_num());
    while(p != NULL) {
      const struct link_stats *stats = rpl_get_parent_link_stats(p);
      uip_ipaddr_t *parent_addr = rpl_parent_get_ipaddr(p);
      LOG_DBG("RPL: nbr ");
      LOG_DBG_6ADDR(parent_addr);
      LOG_DBG_(" %5u, %5u => %5u -- %c%c%c",
          p->rank,
          rpl_get_parent_link_metric(p),
          rpl_rank_via_parent(p),
          rpl_parent_is_fresh(p) ? 'f' : (rpl_parent_is_stale(p) ? 's' : 'u'),
          p == default_instance->current_dag->preferred_parent ? 'p' : ' ',
          !(p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE) && p->dag == default_instance->current_dag ? 'e' : ' '
      );
      if(stats != NULL) {
        struct interface_list_entry *ile;
        ile = list_head(stats->interface_list);
        while(ile != NULL) {
          LOG_DBG_(" (ID: %u, fcnt: %2u, ltx: %u)", ile->if_id, ile->freshness, (unsigned)((clock_now - ile->last_tx_time) / CLOCK_SECOND));
          ile = list_item_next(ile);
        }
      }
      LOG_DBG_("\n");
      p = nbr_table_next(rpl_parents, p);
    }
    LOG_DBG("RPL: end of list\n");
  }
}
/*---------------------------------------------------------------------------*/
uip_ds6_nbr_t *
rpl_get_nbr(rpl_parent_t *parent)
{
  const linkaddr_t *lladdr = rpl_get_parent_lladdr(parent);
  if(lladdr != NULL) {
    return uip_ds6_nbr_ll_lookup((const uip_lladdr_t *)lladdr);
  } else {
    return NULL;
  }
}
/*---------------------------------------------------------------------------*/
static void
nbr_callback(void *ptr)
{
  rpl_remove_parent(ptr);
}

void
rpl_dag_init(void)
{
  nbr_table_register(rpl_parents, (nbr_table_callback *)nbr_callback);
}
/*---------------------------------------------------------------------------*/
rpl_parent_t *
rpl_get_parent(const uip_lladdr_t *addr)
{
  rpl_parent_t *p = nbr_table_get_from_lladdr(rpl_parents, (const linkaddr_t *)addr);
  return p;
}
/*---------------------------------------------------------------------------*/
rpl_rank_t
rpl_get_parent_rank(uip_lladdr_t *addr)
{
  rpl_parent_t *p = nbr_table_get_from_lladdr(rpl_parents, (linkaddr_t *)addr);
  if(p != NULL) {
    return p->rank;
  } else {
    return RPL_INFINITE_RANK;
  }
}
/*---------------------------------------------------------------------------*/
uint16_t
rpl_get_parent_link_metric(rpl_parent_t *p)
{
  if(p != NULL && p->dag != NULL) {
    rpl_instance_t *instance = p->dag->instance;
    if(instance != NULL && instance->of != NULL && instance->of->parent_link_metric != NULL) {
      return instance->of->parent_link_metric(p);
    }
  }
  return 0xffff;
}
/*---------------------------------------------------------------------------*/
rpl_rank_t
rpl_rank_via_parent(rpl_parent_t *p)
{
  if(p != NULL && p->dag != NULL) {
    rpl_instance_t *instance = p->dag->instance;
    if(instance != NULL && instance->of != NULL && instance->of->rank_via_parent != NULL) {
      return instance->of->rank_via_parent(p);
    }
  }
  return RPL_INFINITE_RANK;
}
/*---------------------------------------------------------------------------*/
/* Retrieve the rank to be advertised in DIO messages for the given DAG. The
   value of blame is set to the linkaddr of the parent responsible for the
   returned rank (if blame != NULL). */
rpl_rank_t
rpl_rank_via_dag(rpl_dag_t *dag, linkaddr_t *blame)
{
  if(dag != NULL) {
    rpl_instance_t *instance = dag->instance;
    if(instance != NULL && instance->of != NULL) {
      if(instance->of->rank_via_dag != NULL) {
        return instance->of->rank_via_dag(dag, blame);
      } else if(instance->of->rank_via_parent != NULL && dag->preferred_parent != NULL) {
        if(blame != NULL) {
          *blame = *rpl_get_parent_lladdr(dag->preferred_parent);
        }
        return instance->of->rank_via_parent(dag->preferred_parent);
      }
    }
  }
  return RPL_INFINITE_RANK;
}
/*---------------------------------------------------------------------------*/
const linkaddr_t *
rpl_get_parent_lladdr(rpl_parent_t *p)
{
  return nbr_table_get_lladdr(rpl_parents, p);
}
/*---------------------------------------------------------------------------*/
uip_ipaddr_t *
rpl_parent_get_ipaddr(rpl_parent_t *p)
{
  const linkaddr_t *lladdr = rpl_get_parent_lladdr(p);
  if(lladdr == NULL) {
    return NULL;
  }
  return uip_ds6_nbr_ipaddr_from_lladdr((uip_lladdr_t *)lladdr);
}
/*---------------------------------------------------------------------------*/
const struct link_stats *
rpl_get_parent_link_stats(rpl_parent_t *p)
{
  const linkaddr_t *lladdr = rpl_get_parent_lladdr(p);
  return link_stats_from_lladdr(lladdr);
}
/*---------------------------------------------------------------------------*/
/* True if all of p's interfaces have fresh statistics, false otherwise. */
int
rpl_parent_is_fresh(rpl_parent_t *p)
{
  const struct link_stats *stats = rpl_get_parent_link_stats(p);
  if(stats == NULL) {
    return 0;
  }
  struct interface_list_entry *ile;
  ile = list_head(stats->interface_list);
  while(ile != NULL) {
    if(!link_stats_interface_is_fresh(ile)) {
      return 0;
    }
    ile = list_item_next(ile);
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
/* True if none of p's interfaces has fresh statistics, false otherwise. */
int 
rpl_parent_is_stale(rpl_parent_t *p)
{
  const struct link_stats *stats = rpl_get_parent_link_stats(p);
  if(stats == NULL) {
    return 1;
  }
  struct interface_list_entry *ile;
  ile = list_head(stats->interface_list);
  while(ile != NULL) {
    if(link_stats_interface_is_fresh(ile)) {
      return 0;
    }
    ile = list_item_next(ile);
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
int
rpl_parent_is_reachable(rpl_parent_t *p) {
  if(p == NULL || p->dag == NULL || p->dag->instance == NULL || p->dag->instance->of == NULL) {
    return 0;
  } else {
#if UIP_ND6_SEND_NS
    /* Exclude links to a neighbor that is not reachable at a NUD level */
    if(rpl_get_nbr(p) == NULL) {
      return 0;
    }
#endif /* UIP_ND6_SEND_NS */
    /* If we don't have fresh link information, assume the parent is reachable. */
    return rpl_parent_is_stale(p) || p->dag->instance->of->parent_has_usable_link(p);
  }
}
/*---------------------------------------------------------------------------*/
/* Reset all defer flags of all parents */
void
rpl_reset_defer_flags(void)
{
  rpl_parent_t *p;

  p = nbr_table_head(rpl_parents);
  while(p != NULL) {
    const linkaddr_t *lladdr = rpl_get_parent_lladdr(p);
    if(lladdr != NULL) {
      LOG_DBG("Resetting all defer flags for parent ");
      LOG_DBG_LLADDR(lladdr);
      LOG_DBG_("\n");
      link_stats_reset_defer_flags(lladdr);
    }
    p = nbr_table_next(rpl_parents, p);
  }
}
/*---------------------------------------------------------------------------*/
/* Execute the normalized metric update logic for all parents in the rpl_parents 
   global neighbor table. Note that the defer flags are only checked if a parent
   is the preferred parent of the current dag of the default instance. For all other
   parents, metric normalization is performed regardless of its defer flags. One
   can also pass a flag that indicates whether or not the defer flags of each parent
   should be reset or not. */
void
rpl_exec_norm_metric_logic(rpl_reset_defer_t reset_defer)
{
  rpl_parent_t *p;

  p = nbr_table_head(rpl_parents);
  if(p != NULL) {
    LOG_DBG("Executing normalized metric logic\n");
  }
  while(p != NULL) {
    const linkaddr_t *lladdr = rpl_get_parent_lladdr(p);
    if(lladdr != NULL) {
      if(p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE) {
        LOG_DBG("Non-eligible");
      } else {
        LOG_DBG("Eligible");
      }
      /* TODO move away from using default instance */
      if(default_instance != NULL && default_instance->current_dag != NULL &&
         p == default_instance->current_dag->preferred_parent) {
        LOG_DBG_(" parent ");
        LOG_DBG_LLADDR(lladdr);
        LOG_DBG_(" is preferred for current DAG ");
        LOG_DBG_6ADDR(&default_instance->current_dag->dag_id);
        LOG_DBG_(", checking defer flags\n");
        if(!link_stats_is_defer_required(lladdr)) {
          LOG_DBG("Deferral is not required, updating normalized metric\n");
          link_stats_update_norm_metric(lladdr);
        } else {
          LOG_DBG("Deferring normalized metric update\n");
        }
      } else {
        LOG_DBG_(" parent ");
        LOG_DBG_LLADDR(lladdr);
        LOG_DBG_(" is not preferred for current DAG, updating normalized metric\n");
        link_stats_update_norm_metric(lladdr);
      }
      if(reset_defer) {
        LOG_DBG("Resetting all defer flags for ");
        LOG_DBG_LLADDR(lladdr);
        LOG_DBG_("\n");
        link_stats_reset_defer_flags(lladdr);
      }
    }
    p = nbr_table_next(rpl_parents, p);
  }
}
/*---------------------------------------------------------------------------*/
/* Set the interface weights of the given parent or of all neighbors
   (not just parents) if the supplied parent pointer == NULL. */
int
rpl_set_interface_weights(rpl_parent_t *p)
{
#if RPL_WEIGHTED_INTERFACES
#if RPL_MAX_INSTANCES == 1
  /* Abort execution if we are root because weights are useless in that case anyway */
  if(default_instance != NULL && default_instance->current_dag != NULL &&
     default_instance->current_dag->rank == ROOT_RANK(default_instance)) {
    LOG_DBG("Not setting interface weights because we are root!\n");
    return 0;
  }
#endif
  if(p != NULL) {
    const linkaddr_t *lladdr = rpl_get_parent_lladdr(p);
    if(lladdr == NULL) {
      return 0;
    }
    LOG_DBG("Attempting weight modification for %u interfaces of ", rpl_ifw_collection.size);
    LOG_DBG_LLADDR(lladdr);
    LOG_DBG_("\n");
    for(uint8_t i = 0; i < rpl_ifw_collection.size; i++) {
      uint8_t if_id = rpl_ifw_collection.if_id_list[i];
      uint8_t weight = rpl_ifw_collection.weights[i];
      link_stats_modify_weight(lladdr, if_id, weight);
    }
  } else {
    for(uint8_t i = 0; i < rpl_ifw_collection.size; i++) {
      uint8_t if_id = rpl_ifw_collection.if_id_list[i];
      uint8_t weight = rpl_ifw_collection.weights[i];
      LOG_DBG("Setting the weight of all neighboring interfaces with ID = %d to %d\n", if_id, weight);
      link_stats_modify_weights(if_id, weight);
    }
  }
  return 1;
#else
  return 0;
#endif
}
/*---------------------------------------------------------------------------*/
#if RPL_WEIGHTED_INTERFACES
/* Set the stored interface weight for all interfaces with the supplied id to the
   supplied weight. Returns 1 if weight was updated and 0 otherwise. */
static uint8_t
update_interface_weight(uint8_t if_id, uint8_t weight)
{
  /* Look for an existing entry in rpl_ifw_collection */
  for(uint8_t i = 0; i < rpl_ifw_collection.size; i++) {
    if(rpl_ifw_collection.if_id_list[i] == if_id) {
      LOG_DBG("Found ID = %d in RPL interface weight collection,", if_id);
      if(rpl_ifw_collection.weights[i] != weight) {
        LOG_DBG_(" updating weight to %d (previously %d)\n", weight, rpl_ifw_collection.weights[i]);
        /* REVIEW we could either make traffic density a moving average or better yet
          since we're here implies that we have stored a previous weight value, we might
          as well use an EWMA filter to calculate and assign a new weight */
        rpl_ifw_collection.weights[i] = weight;
        return 1;
      }
      LOG_DBG_(" not updating weight because still %d\n", weight);
      return 0;
    }
  }
  /* If no entry exists, create one (if possible) */
  if(rpl_ifw_collection.size < RADIO_MAX_INTERFACES) {
    rpl_ifw_collection.if_id_list[rpl_ifw_collection.size] = if_id;
    rpl_ifw_collection.weights[rpl_ifw_collection.size] = weight;
    rpl_ifw_collection.size++;
    LOG_DBG("Added new entry to RPL interface weight collection for ID = %d with weight %d\n",
            if_id, weight);
    return 1;
  }
  LOG_DBG("Failed adding new entry to RPL interface weight collection for ID = %d, too many entries\n",
          if_id);
  return 0;
}
#endif
/*---------------------------------------------------------------------------*/
/* Recalculate the weight for all types of interfaces.
   Returns 1 if any weights were updated and 0 otherwise. */ 
int
rpl_recalculate_interface_weights(void)
{
#if RPL_WEIGHTED_INTERFACES
#if RPL_MAX_INSTANCES == 1
  /* Abort execution if we are root because weights are useless in that case anyway */
  if(default_instance != NULL && default_instance->current_dag != NULL &&
     default_instance->current_dag->rank == ROOT_RANK(default_instance)) {
    LOG_DBG("Not recalculating interface weights because we are root!\n");
    return 0;
  }
#endif
  LOG_DBG("Recalculating interface weights\n");
  uint16_t ntp = num_tx_preferred;
  LOG_DBG("Transmitted %u packets to preferred parent in current RPL_IF_WEIGHTS_WINDOW\n", ntp);
  /* We now know how many packets we have successfully transmitted towards
     our preferred parent during the current RPL_IF_WEIGHTS_WINDOW period. We must
     now somehow translate this absolute number into a into a weight for each type
     of interface we possess. This latter calculation will rely at least partly on
     the data rate of each interface type. */
  uint16_t period = (RPL_IF_WEIGHTS_WINDOW / CLOCK_SECOND);
  /* REVIEW should we make density a moving average accross a given number
     of RPL_IF_WEIGHTS_WINDOWs? */
  double density = ((double)ntp / (double)period) * 240.0; /* Density = per 4 minutes */
  if_id_collection_t if_id_collection;
  if(NETSTACK_RADIO.get_object(RADIO_CONST_INTERFACE_ID_COLLECTION, &if_id_collection,
                               sizeof(if_id_collection)) == RADIO_RESULT_OK) {
    if(if_id_collection.size > RADIO_MAX_INTERFACES) {
      LOG_DBG("Size of if_id collection exceeds RADIO_MAX_INTERFACES. Aborting weight recalculation.\n");
      return 0;
    }
    uint8_t weights_updated = 0;
    for(uint8_t i = 0; i < if_id_collection.size; i++) {
      /* At this point we assume each if_id appears at most once in
         the if_id_collection's if_id_list. However, we won't check this
         here as this is the responsibility of the creator of said list,
         i.e., it's the responsibility of the radio driver */
      uint8_t weight;
      uint8_t if_id = if_id_collection.if_id_list[i];
      uint16_t data_rate = if_id_collection.data_rates[i];
      double exponent = (density * (double)data_rate) / 8197.7;
      double precise_weight = pow(2.0, exponent); /* Approaches 255 for density * data_rate = 65535 */
      weight = (uint8_t)(precise_weight + 0.5);
      weights_updated |= update_interface_weight(if_id, weight);
    }
    return weights_updated;
  }
  LOG_DBG("Could not retrieve if_id collection from radio driver. Aborting weight recalculation.\n");
#endif
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
rpl_set_preferred_parent(rpl_dag_t *dag, rpl_parent_t *p)
{
  if(dag != NULL && dag->preferred_parent != p && (p == NULL || !(p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE))) {
    LOG_INFO("rpl_set_preferred_parent ");
    if(p != NULL) {
      LOG_INFO_6ADDR(rpl_parent_get_ipaddr(p));
    } else {
      LOG_INFO_("NULL");
    }
    LOG_INFO_(" used to be ");
    if(dag->preferred_parent != NULL) {
      LOG_INFO_6ADDR(rpl_parent_get_ipaddr(dag->preferred_parent));
    } else {
      LOG_INFO_("NULL");
    }
    LOG_INFO_("\n");

#ifdef RPL_CALLBACK_PARENT_SWITCH
    RPL_CALLBACK_PARENT_SWITCH(dag->preferred_parent, p);
#endif /* RPL_CALLBACK_PARENT_SWITCH */

    /* Always keep the preferred parent locked, so it remains in the
     * neighbor table. */
    nbr_table_unlock(rpl_parents, dag->preferred_parent);
    nbr_table_lock(rpl_parents, p);
    dag->preferred_parent = p;
  } else if(p != NULL && (p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE)) {
    LOG_INFO("rpl_set_preferred_parent ");
    LOG_INFO_6ADDR(rpl_parent_get_ipaddr(p));
    LOG_INFO_(" not eligible\n");
  }
}
/*---------------------------------------------------------------------------*/
/* Greater-than function for the lollipop counter.                      */
/*---------------------------------------------------------------------------*/
static int
lollipop_greater_than(int a, int b)
{
  /* Check if we are comparing an initial value with an old value */
  if(a > RPL_LOLLIPOP_CIRCULAR_REGION && b <= RPL_LOLLIPOP_CIRCULAR_REGION) {
    return (RPL_LOLLIPOP_MAX_VALUE + 1 + b - a) > RPL_LOLLIPOP_SEQUENCE_WINDOWS;
  }
  /* Otherwise check if a > b and comparable => ok, or
     if they have wrapped and are still comparable */
  return (a > b && (a - b) < RPL_LOLLIPOP_SEQUENCE_WINDOWS) ||
    (a < b && (b - a) > (RPL_LOLLIPOP_CIRCULAR_REGION + 1-
			 RPL_LOLLIPOP_SEQUENCE_WINDOWS));
}
/*---------------------------------------------------------------------------*/
/* Remove DAG parents with a rank that is at least the same as minimum_rank. */
static void
remove_parents(rpl_dag_t *dag, rpl_rank_t minimum_rank)
{
  rpl_parent_t *p;

  LOG_INFO("Removing parents (minimum rank %u)\n", minimum_rank);

  p = nbr_table_head(rpl_parents);
  while(p != NULL) {
    if(dag == p->dag && p->rank >= minimum_rank) {
      rpl_remove_parent(p);
    }
    p = nbr_table_next(rpl_parents, p);
  }
}
/*---------------------------------------------------------------------------*/
static void
nullify_parents(rpl_dag_t *dag, rpl_rank_t minimum_rank)
{
  rpl_parent_t *p;

  LOG_INFO("Nullifying parents (minimum rank %u)\n", minimum_rank);

  p = nbr_table_head(rpl_parents);
  while(p != NULL) {
    if(dag == p->dag && p->rank >= minimum_rank) {
      rpl_nullify_parent(p);
    }
    p = nbr_table_next(rpl_parents, p);
  }
}
/*---------------------------------------------------------------------------*/
static int
should_refresh_routes(rpl_instance_t *instance, rpl_dio_t *dio, rpl_parent_t *p)
{
  /* if MOP is set to no downward routes no DAO should be sent */
  if(instance->mop == RPL_MOP_NO_DOWNWARD_ROUTES) {
    return 0;
  }
  /* check if the new DTSN is more recent */
  return p == instance->current_dag->preferred_parent &&
    (lollipop_greater_than(dio->dtsn, p->dtsn));
}
/*---------------------------------------------------------------------------*/
int
rpl_acceptable_rank(rpl_dag_t *dag, rpl_rank_t rank)
{
  /* See RFC6550 Section 6.7.6. */
  /* See RFC6550 Section 8.2.2.4. */
  return rank != RPL_INFINITE_RANK &&
    ((dag->instance->max_rankinc == 0) ||
     DAG_RANK(rank, dag->instance) <= DAG_RANK(dag->min_rank + dag->instance->max_rankinc, dag->instance));
}
/*---------------------------------------------------------------------------*/
static rpl_dag_t *
get_dag(uint8_t instance_id, uip_ipaddr_t *dag_id)
{
  rpl_instance_t *instance;
  rpl_dag_t *dag;
  int i;

  instance = rpl_get_instance(instance_id);
  if(instance == NULL) {
    return NULL;
  }

  for(i = 0; i < RPL_MAX_DAG_PER_INSTANCE; ++i) {
    dag = &instance->dag_table[i];
    if(dag->used && uip_ipaddr_cmp(&dag->dag_id, dag_id)) {
      return dag;
    }
  }

  return NULL;
}
/*---------------------------------------------------------------------------*/
rpl_dag_t *
rpl_set_root(uint8_t instance_id, uip_ipaddr_t *dag_id)
{
  rpl_dag_t *dag;
  rpl_instance_t *instance;
  uint8_t version;
  int i;

  version = RPL_LOLLIPOP_INIT;
  instance = rpl_get_instance(instance_id);
  if(instance != NULL) {
    for(i = 0; i < RPL_MAX_DAG_PER_INSTANCE; ++i) {
      dag = &instance->dag_table[i];
      if(dag->used) {
        if(uip_ipaddr_cmp(&dag->dag_id, dag_id)) {
          version = dag->version;
          RPL_LOLLIPOP_INCREMENT(version);
        } else {
          if(dag == dag->instance->current_dag) {
            LOG_INFO("Dropping a joined DAG when setting this node as root\n");
            rpl_set_default_route(instance, NULL);
            dag->instance->current_dag = NULL;
          } else {
            LOG_INFO("Dropping a DAG when setting this node as root\n");
          }
          rpl_free_dag(dag);
        }
      }
    }
  }

  dag = rpl_alloc_dag(instance_id, dag_id);
  if(dag == NULL) {
    LOG_ERR("Failed to allocate a DAG\n");
    return NULL;
  }

  instance = dag->instance;

  dag->version = version;
  dag->joined = 1;
  dag->grounded = RPL_GROUNDED;
  dag->preference = RPL_PREFERENCE;
  instance->mop = RPL_MOP_DEFAULT;
  instance->of = rpl_find_of(RPL_OF_OCP);
  if(instance->of == NULL) {
    LOG_WARN("OF with OCP %u not supported\n", RPL_OF_OCP);
    return NULL;
  }

  rpl_set_preferred_parent(dag, NULL);

  memcpy(&dag->dag_id, dag_id, sizeof(dag->dag_id));

  instance->dio_intdoubl = RPL_DIO_INTERVAL_DOUBLINGS;
  instance->dio_intmin = RPL_DIO_INTERVAL_MIN;
  /* The current interval must differ from the minimum interval in order to
     trigger a DIO timer reset. */
  instance->dio_intcurrent = RPL_DIO_INTERVAL_MIN +
    RPL_DIO_INTERVAL_DOUBLINGS;
  instance->dio_redundancy = RPL_DIO_REDUNDANCY;
  instance->max_rankinc = RPL_MAX_RANKINC;
  instance->min_hoprankinc = RPL_MIN_HOPRANKINC;
  instance->default_lifetime = RPL_DEFAULT_LIFETIME;
  instance->lifetime_unit = RPL_DEFAULT_LIFETIME_UNIT;

  dag->rank = ROOT_RANK(instance);

  if(instance->current_dag != dag && instance->current_dag != NULL) {
    /* Remove routes installed by DAOs. */
    if(RPL_IS_STORING(instance)) {
      rpl_remove_routes(instance->current_dag);
    }

    instance->current_dag->joined = 0;
  }

  instance->current_dag = dag;
  instance->dtsn_out = RPL_LOLLIPOP_INIT;
  instance->of->update_metric_container(instance);
  default_instance = instance;

  LOG_INFO("Node set to be a DAG root with DAG ID ");
  LOG_INFO_6ADDR(&dag->dag_id);
  LOG_INFO_("\n");

  LOG_ANNOTATE("#A root=%u\n", dag->dag_id.u8[sizeof(dag->dag_id) - 1]);

  rpl_reset_dio_timer(instance);

  return dag;
}
/*---------------------------------------------------------------------------*/
int
rpl_repair_root(uint8_t instance_id)
{
  rpl_instance_t *instance;

  instance = rpl_get_instance(instance_id);
  if(instance == NULL ||
     instance->current_dag->rank != ROOT_RANK(instance)) {
    LOG_WARN("rpl_repair_root triggered but not root\n");
    return 0;
  }
  RPL_STAT(rpl_stats.root_repairs++);

  RPL_LOLLIPOP_INCREMENT(instance->current_dag->version);
  RPL_LOLLIPOP_INCREMENT(instance->dtsn_out);
  LOG_INFO("rpl_repair_root initiating global repair with version %d\n", instance->current_dag->version);
  rpl_reset_dio_timer(instance);
  return 1;
}
/*---------------------------------------------------------------------------*/
static void
set_ip_from_prefix(uip_ipaddr_t *ipaddr, rpl_prefix_t *prefix)
{
  memset(ipaddr, 0, sizeof(uip_ipaddr_t));
  memcpy(ipaddr, &prefix->prefix, (prefix->length + 7) / 8);
  uip_ds6_set_addr_iid(ipaddr, &uip_lladdr);
}
/*---------------------------------------------------------------------------*/
static void
check_prefix(rpl_prefix_t *last_prefix, rpl_prefix_t *new_prefix)
{
  uip_ipaddr_t ipaddr;
  uip_ds6_addr_t *rep;

  if(last_prefix != NULL && new_prefix != NULL &&
     last_prefix->length == new_prefix->length &&
     uip_ipaddr_prefixcmp(&last_prefix->prefix, &new_prefix->prefix, new_prefix->length) &&
     last_prefix->flags == new_prefix->flags) {
    /* Nothing has changed. */
    return;
  }

  if(last_prefix != NULL) {
    set_ip_from_prefix(&ipaddr, last_prefix);
    rep = uip_ds6_addr_lookup(&ipaddr);
    if(rep != NULL) {
      LOG_DBG("removing global IP address ");
      LOG_DBG_6ADDR(&ipaddr);
      LOG_DBG_("\n");
      uip_ds6_addr_rm(rep);
    }
  }

  if(new_prefix != NULL) {
    set_ip_from_prefix(&ipaddr, new_prefix);
    if(uip_ds6_addr_lookup(&ipaddr) == NULL) {
      LOG_DBG("adding global IP address ");
      LOG_DBG_6ADDR(&ipaddr);
      LOG_DBG_("\n");
      uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
    }
  }
}
/*---------------------------------------------------------------------------*/
int
rpl_set_prefix(rpl_dag_t *dag, uip_ipaddr_t *prefix, unsigned len)
{
  rpl_prefix_t last_prefix;
  uint8_t last_len = dag->prefix_info.length;

  if(len > 128) {
    return 0;
  }
  if(dag->prefix_info.length != 0) {
    memcpy(&last_prefix, &dag->prefix_info, sizeof(rpl_prefix_t));
  }
  memset(&dag->prefix_info.prefix, 0, sizeof(dag->prefix_info.prefix));
  memcpy(&dag->prefix_info.prefix, prefix, (len + 7) / 8);
  dag->prefix_info.length = len;
  dag->prefix_info.flags = UIP_ND6_RA_FLAG_AUTONOMOUS;
  LOG_INFO("Prefix set - will announce this in DIOs\n");
  if(dag->rank != ROOT_RANK(dag->instance)) {
    /* Autoconfigure an address if this node does not already have an address
       with this prefix. Otherwise, update the prefix */
    if(last_len == 0) {
      LOG_INFO("rpl_set_prefix - prefix NULL\n");
      check_prefix(NULL, &dag->prefix_info);
    } else {
      LOG_INFO("rpl_set_prefix - prefix NON-NULL\n");
      check_prefix(&last_prefix, &dag->prefix_info);
    }
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
int
rpl_set_default_route(rpl_instance_t *instance, uip_ipaddr_t *from)
{
  if(instance->def_route != NULL) {
    LOG_DBG("Removing default route through ");
    LOG_DBG_6ADDR(&instance->def_route->ipaddr);
    LOG_DBG_("\n");
    uip_ds6_defrt_rm(instance->def_route);
    instance->def_route = NULL;
  }

  if(from != NULL) {
    LOG_DBG("Adding default route through ");
    LOG_DBG_6ADDR(from);
    LOG_DBG_("\n");
    instance->def_route = uip_ds6_defrt_add(from,
        RPL_DEFAULT_ROUTE_INFINITE_LIFETIME ? 0 : RPL_LIFETIME(instance, instance->default_lifetime));
    if(instance->def_route == NULL) {
      return 0;
    }
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
rpl_instance_t *
rpl_alloc_instance(uint8_t instance_id)
{
  rpl_instance_t *instance, *end;

  for(instance = &instance_table[0], end = instance + RPL_MAX_INSTANCES;
      instance < end; ++instance) {
    if(instance->used == 0) {
      memset(instance, 0, sizeof(*instance));
      instance->instance_id = instance_id;
      instance->def_route = NULL;
      instance->used = 1;
#if RPL_WITH_PROBING
      rpl_schedule_probing(instance);
#endif /* RPL_WITH_PROBING */
      return instance;
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
rpl_dag_t *
rpl_alloc_dag(uint8_t instance_id, uip_ipaddr_t *dag_id)
{
  rpl_dag_t *dag, *end;
  rpl_instance_t *instance;

  instance = rpl_get_instance(instance_id);
  if(instance == NULL) {
    instance = rpl_alloc_instance(instance_id);
    if(instance == NULL) {
      RPL_STAT(rpl_stats.mem_overflows++);
      return NULL;
    }
  }

  for(dag = &instance->dag_table[0], end = dag + RPL_MAX_DAG_PER_INSTANCE; dag < end; ++dag) {
    if(!dag->used) {
      memset(dag, 0, sizeof(*dag));
      dag->used = 1;
      dag->rank = RPL_INFINITE_RANK;
      dag->min_rank = RPL_INFINITE_RANK;
      dag->instance = instance;
      return dag;
    }
  }

  RPL_STAT(rpl_stats.mem_overflows++);
  return NULL;
}
/*---------------------------------------------------------------------------*/
void
rpl_set_default_instance(rpl_instance_t *instance)
{
  default_instance = instance;
}
/*---------------------------------------------------------------------------*/
rpl_instance_t *
rpl_get_default_instance(void)
{
  return default_instance;
}
/*---------------------------------------------------------------------------*/
void
rpl_free_instance(rpl_instance_t *instance)
{
  rpl_dag_t *dag;
  rpl_dag_t *end;

  LOG_INFO("Leaving the instance %u\n", instance->instance_id);

  /* Remove any DAG inside this instance */
  for(dag = &instance->dag_table[0], end = dag + RPL_MAX_DAG_PER_INSTANCE; dag < end; ++dag) {
    if(dag->used) {
      rpl_free_dag(dag);
    }
  }

  rpl_set_default_route(instance, NULL);

#if RPL_WITH_PROBING
  ctimer_stop(&instance->probing_timer);
#endif /* RPL_WITH_PROBING */
  ctimer_stop(&instance->dio_timer);
  ctimer_stop(&instance->dao_timer);
  ctimer_stop(&instance->dao_lifetime_timer);

  if(default_instance == instance) {
    default_instance = NULL;
  }

  instance->used = 0;
}
/*---------------------------------------------------------------------------*/
void
rpl_free_dag(rpl_dag_t *dag)
{
  if(dag->joined) {
    LOG_INFO("Leaving the DAG ");
    LOG_INFO_6ADDR(&dag->dag_id);
    LOG_INFO_("\n");
    dag->joined = 0;

    /* Remove routes installed by DAOs. */
    if(RPL_IS_STORING(dag->instance)) {
      rpl_remove_routes(dag);
    }
    /* Stop the DAO retransmit timer */
#if RPL_WITH_DAO_ACK
    ctimer_stop(&dag->instance->dao_retransmit_timer);
#endif /* RPL_WITH_DAO_ACK */

   /* Remove autoconfigured address */
    if((dag->prefix_info.flags & UIP_ND6_RA_FLAG_AUTONOMOUS)) {
      check_prefix(&dag->prefix_info, NULL);
    }

    remove_parents(dag, 0);
  }
  dag->used = 0;
}
/*---------------------------------------------------------------------------*/
rpl_parent_t *
rpl_add_parent(rpl_dag_t *dag, rpl_dio_t *dio, uip_ipaddr_t *addr)
{
  rpl_parent_t *p = NULL;
  /* Is the parent known by ds6? Drop this request if not.
   * Typically, the parent is added upon receiving a DIO. */
  const uip_lladdr_t *lladdr = uip_ds6_nbr_lladdr_from_ipaddr(addr);

  LOG_DBG("rpl_add_parent lladdr %p ", lladdr);
  LOG_DBG_6ADDR(addr);
  LOG_DBG_("\n");
  if(lladdr != NULL) {
    /* Add parent in rpl_parents - again this is due to DIO */
    p = nbr_table_add_lladdr(rpl_parents, (linkaddr_t *)lladdr,
                             NBR_TABLE_REASON_RPL_DIO, dio);
    if(p == NULL) {
      LOG_DBG("rpl_add_parent p NULL\n");
    } else {
      p->dag = dag;
      p->rank = dio->rank;
      p->dtsn = dio->dtsn;
#if RPL_WITH_MC
      memcpy(&p->mc, &dio->mc, sizeof(p->mc));
#endif /* RPL_WITH_MC */
#if RPL_WEIGHTED_INTERFACES
      LOG_DBG("Scheduling interface weighting for ");
      LOG_DBG_LLADDR((const linkaddr_t *)lladdr);
      LOG_DBG_(" %.3f seconds from now\n", (float)RPL_IF_WEIGHTS_DELAY / CLOCK_SECOND);
      rpl_schedule_interface_weighting(p);
#endif /* RPL_WEIGHTED_INTERFACES */
      link_stats_reset_defer_flags((const linkaddr_t *)lladdr);
      link_stats_update_norm_metric((const linkaddr_t *)lladdr);
    }
  }

  return p;
}
/*---------------------------------------------------------------------------*/
static rpl_parent_t *
find_parent_any_dag_any_instance(uip_ipaddr_t *addr)
{
  uip_ds6_nbr_t *ds6_nbr = uip_ds6_nbr_lookup(addr);
  const uip_lladdr_t *lladdr = uip_ds6_nbr_get_ll(ds6_nbr);
  return nbr_table_get_from_lladdr(rpl_parents, (linkaddr_t *)lladdr);
}
/*---------------------------------------------------------------------------*/
rpl_parent_t *
rpl_find_parent(rpl_dag_t *dag, uip_ipaddr_t *addr)
{
  rpl_parent_t *p = find_parent_any_dag_any_instance(addr);
  if(p != NULL && p->dag == dag) {
    return p;
  } else {
    return NULL;
  }
}
/*---------------------------------------------------------------------------*/
static rpl_dag_t *
find_parent_dag(rpl_instance_t *instance, uip_ipaddr_t *addr)
{
  rpl_parent_t *p = find_parent_any_dag_any_instance(addr);
  if(p != NULL) {
    return p->dag;
  } else {
    return NULL;
  }
}
/*---------------------------------------------------------------------------*/
rpl_parent_t *
rpl_find_parent_any_dag(rpl_instance_t *instance, uip_ipaddr_t *addr)
{
  rpl_parent_t *p = find_parent_any_dag_any_instance(addr);
  if(p && p->dag && p->dag->instance == instance) {
    return p;
  } else {
    return NULL;
  }
}
/*---------------------------------------------------------------------------*/
rpl_dag_t *
rpl_select_dag(rpl_instance_t *instance, rpl_parent_t *p)
{
  rpl_parent_t *last_parent;
  rpl_dag_t *dag, *end, *best_dag;
  rpl_rank_t old_rank;

  old_rank = instance->current_dag->rank;
  last_parent = instance->current_dag->preferred_parent;

  if(instance->current_dag->rank != ROOT_RANK(instance)) {
    /* Select a new preferred parent. This causes the rank to be recomputed
       for all parents before comparing said computed ranks and choosing the
       best candidate as the new preferred parent, that is, for the dag of
       which the supplied parent p is part. Note that p->dag does not always
       equal instance->current_dag. */
    rpl_select_parent(p->dag);
  }

  /* Iterate through the dag_table of the supplied instance (first RPL_MAX_DAG_PER_INSTANCE
     positions in dag_table only) and if a dag is used, has a preferred parent and the
     rank we advertise in said dag is not RPL_INFINITE_RANK, then we either set best_dag
     to said dag if best_dag is NULL, or we compare the dag to the one currently stored
     in best_dag and overwrite best_dag if the dag in the current iteration is better (as
     determined by the OF). Note that many OFs largely base the output of best_dag() on
     the ranks of the supplied dags and so it must be noted that we're selecting the best
     dag prior to adjusting the ranks we advertise in said dags (which we only do for the
     best dag after we've selected it) */
  best_dag = NULL;
  for(dag = &instance->dag_table[0], end = dag + RPL_MAX_DAG_PER_INSTANCE; dag < end; ++dag) {
    if(dag->used && dag->preferred_parent != NULL && rpl_rank_via_dag(dag, NULL) != RPL_INFINITE_RANK) {
      if(best_dag == NULL) {
        best_dag = dag;
      } else {
        best_dag = instance->of->best_dag(best_dag, dag);
      }
    }
  }

  if(best_dag == NULL) {
    /* This could happen if we have not yet added any dag in the supplied instance or
       if we have done so but none of the added dags has a preferred parent or if we
       advertise RPL_INFINITE_RANK in all added dags of the supplied instance. The most
       likely scenario is that none of the dags has a preferred parent. In any case,
       the calling function should handle this problem and we simply return NULL. */
    return NULL;
  }

  /* The best dag for the supplied instance, as determined by the OF, does not equal
     the dag of the supplied instance we're currently part of and so we must join
     best_dag instead. */
  if(instance->current_dag != best_dag) {
    /* Remove routes installed by DAOs. */
    if(RPL_IS_STORING(instance)) {
      rpl_remove_routes(instance->current_dag);
    }

    LOG_INFO("New preferred DAG: ");
    LOG_INFO_6ADDR(&best_dag->dag_id);
    LOG_INFO_("\n");

    if(best_dag->prefix_info.flags & UIP_ND6_RA_FLAG_AUTONOMOUS) {
      check_prefix(&instance->current_dag->prefix_info, &best_dag->prefix_info);
    } else if(instance->current_dag->prefix_info.flags & UIP_ND6_RA_FLAG_AUTONOMOUS) {
      check_prefix(&instance->current_dag->prefix_info, NULL);
    }

    best_dag->joined = 1;
    instance->current_dag->joined = 0;
    instance->current_dag = best_dag;
  } else {
    LOG_DBG("DAG ");
    LOG_DBG_6ADDR(&instance->current_dag->dag_id);
    LOG_DBG_(" remains preferred\n");
  }

  instance->of->update_metric_container(instance);
  /* Update the DAG rank. */
  /* Previously best_dag->rank was simply set to rpl_rank_via_parent(best_dag->preferred_parent).
     However, this was not accurate for all OFs. Hence, rpl rpl_rank_via_dag() and a per-OF
     rank_via_dag() function were introduced to allow each OF to return any rank calculated
     from the parents that are part of the supplied dag. For OF0-based OFs, this doesn't change
     much, as they simply return the rank computed for the path through the preferred parent
     of the supplied dag (which is identical to previous behavior). However, for MRHOF-based
     OFs this means that we can now comply with RFC 6719 Section 3.3. */
  linkaddr_t blame;
  best_dag->rank = rpl_rank_via_dag(best_dag, &blame);
  if(last_parent == NULL || best_dag->rank < best_dag->min_rank) {
    /* This is a slight departure from RFC6550: if we had no preferred parent before,
     * reset min_rank. This helps recovering from temporary bad link conditions. */
    best_dag->min_rank = best_dag->rank;
  }

  if(!rpl_acceptable_rank(best_dag, best_dag->rank)) {
    LOG_WARN("New rank (%u) unacceptable!\n", best_dag->rank);
    /* Prior to my RFC compliant implementation of MRHOF, the advertised rank depended
       solely on the rank computed for the path through the preferred parent and thus if
       the rank of a parent was not acceptable, it was sufficient to make sure that it was
       no longer preferred in order to prevent oneself from advertising an unacceptable
       rank. However, if (with the current implementation) we keep a parent with too high
       a rank in the parent set (whilst still making sure it's not preferred) it could
       happen that our advertised rank becomes too high */
    rpl_parent_t *to_nullify = rpl_get_parent((uip_lladdr_t *)&blame);
    if(to_nullify != NULL) {
      rpl_nullify_parent(to_nullify);
    }
    return NULL;
  }

  if(best_dag->preferred_parent != last_parent) {
    /* Our preferred parent could only have changed if we changed dags or if we didn't because
       p->dag was already best_dag but rpl_select_parent(p->dag) did result in a new parent
       becoming preferred. Remember that best_dag always has a preferred parent because otherwise
       we would have already returned NULL */
    rpl_set_default_route(instance, rpl_parent_get_ipaddr(best_dag->preferred_parent));
    LOG_INFO("RPL: Changed preferred parent, rank changed from %u to %u\n",
           (unsigned)old_rank, best_dag->rank);
    RPL_STAT(rpl_stats.parent_switch++);
    if(RPL_IS_STORING(instance)) {
      if(last_parent != NULL) {
        /* Send a No-Path DAO to the removed preferred parent. */
        dao_output(last_parent, RPL_ZERO_LIFETIME);
      }
      /* Trigger DAO transmission from immediate children.
       * Only for storing mode, see RFC6550 section 9.6. */
      RPL_LOLLIPOP_INCREMENT(instance->dtsn_out);
    }

    /* Since our rank has probably changed we must check that the rank we will
       hereafter advertise is greater than the rank advertised by any eligible parent.
       If not the case, the parents that violate this rule should be marked ineligible. */
    nullify_parents(best_dag, best_dag->rank);

    /* The DAO parent set changed - schedule a DAO transmission. */
    rpl_schedule_dao(instance);
    rpl_reset_dio_timer(instance);
    if(LOG_DBG_ENABLED) {
      rpl_print_neighbor_list();
    }
  } else if(best_dag->rank != old_rank) {
    LOG_DBG("RPL: Eligible parent update, rank changed from %u to %u\n",
           (unsigned)old_rank, best_dag->rank);

    /* Since our rank has probably changed we must check that the rank we will
       hereafter advertise is greater than the rank advertised by any eligible parent.
       If not the case, the parents that violate this rule should be marked ineligible. */
    nullify_parents(best_dag, best_dag->rank);

    if(best_dag->rank != RPL_INFINITE_RANK && old_rank != RPL_INFINITE_RANK &&
       ABS((int32_t)best_dag->rank - old_rank) > RPL_SIGNIFICANT_CHANGE_THRESHOLD) {
      LOG_DBG("Significant rank update!\n");
      rpl_reset_dio_timer(instance);
    }
  } else if(best_dag->preferred_parent != NULL) {
    LOG_DBG("RPL: ");
    LOG_DBG_6ADDR(rpl_parent_get_ipaddr(best_dag->preferred_parent));
    LOG_DBG_(" remains preferred, rank unchanged (%u)\n", best_dag->rank);
  }
  return best_dag;
}
/*---------------------------------------------------------------------------*/
static rpl_parent_t *
best_parent(rpl_dag_t *dag, rpl_parent_freshness_t freshness_type)
{
  rpl_parent_t *p;
  rpl_of_t *of;
  rpl_parent_t *best = NULL;

  if(dag == NULL || dag->instance == NULL || dag->instance->of == NULL) {
    return NULL;
  }

  of = dag->instance->of;
  /* Search for the best parent according to the OF */
  for(p = nbr_table_head(rpl_parents); p != NULL; p = nbr_table_next(rpl_parents, p)) {

    /* Exclude parents from other DAGs or announcing an infinite rank */
    if(p->dag != dag || p->rank == RPL_INFINITE_RANK || p->rank < ROOT_RANK(dag->instance) ||
       (p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE)) {
      if(p->rank < ROOT_RANK(dag->instance)) {
        LOG_WARN("Parent has invalid rank\n");
      }
      continue;
    }

    switch(freshness_type) {
    case RPL_PARENT_FRESHNESS_ALL_INTERFACES:
      if(!rpl_parent_is_fresh(p)) {
        /* We only want parents with all-fresh interfaces so we filter out
           all parents of which at least 1 interface is stale */
        continue;
      }
      break;
    case RPL_PARENT_FRESHNESS_ANY_INTERFACE:
      if(rpl_parent_is_stale(p)) {
        /* We want all parents for which at least 1 interface is fresh
           so we filter out all parents with all-stale interfaces */
        continue;
      }
      break;
    default:
      break;
    }

#if UIP_ND6_SEND_NS
    /* Exclude links to a neighbor that is not reachable at a NUD level */
    if(rpl_get_nbr(p) == NULL) {
      continue;
    }
#endif /* UIP_ND6_SEND_NS */

    /* Now we have an acceptable parent, check if it is the new best */
    best = of->best_parent(best, p);
  }

  return best;
}
/*---------------------------------------------------------------------------*/
rpl_parent_t *
rpl_select_parent(rpl_dag_t *dag)
{
  /* Look for best parent (regardless of freshness) */
  rpl_parent_t *best = best_parent(dag, RPL_PARENT_FRESHNESS_UNSPECIFIED);

  if(best != NULL) {
#if RPL_WITH_PROBING
    /* If all interfaces of best are fresh we can immediately set it
       as the preferred parent and unschedule any urgent probings */
    if(rpl_parent_is_fresh(best)) {
      rpl_set_preferred_parent(dag, best);
      /* Unschedule any already scheduled urgent probing */
      dag->instance->urgent_probing_target = NULL;
    } else {
      /* Not all interfaces of best are fresh, so we look for the best
         parent with all-fresh interfaces */
      rpl_parent_t *best_all_fresh = best_parent(dag, RPL_PARENT_FRESHNESS_ALL_INTERFACES);
      if(best_all_fresh == NULL) {
        /* We didn't find any parent with all-fresh interfaces, so we
           look for the best parent of which at least 1 interface is
           fresh */
        rpl_parent_t *best_part_fresh = best_parent(dag, RPL_PARENT_FRESHNESS_ANY_INTERFACE);
        if(best_part_fresh == NULL) {
          /* We didn't find any parent with at least 1 fresh interface,
             so we simply set the best (stale) parent as preferred */
          rpl_set_preferred_parent(dag, best);
        } else {
          /* We found the best parent with at least 1 fresh interface,
             so we set it as the preferred parent */
          rpl_set_preferred_parent(dag, best_part_fresh);
        }
      } else {
        /* Use best parent with all-fresh interfaces */
        rpl_set_preferred_parent(dag, best_all_fresh);
      }
      /* Probe the best parent shortly in order to get a fresh estimate
         for all of its non-fresh interfaces */
      dag->instance->urgent_probing_target = best;
      rpl_schedule_probing_now(dag->instance);
    }
#else /* RPL_WITH_PROBING */
    rpl_set_preferred_parent(dag, best);
    dag->rank = rpl_rank_via_dag(dag, NULL);
#endif /* RPL_WITH_PROBING */
  } else {
    rpl_set_preferred_parent(dag, NULL);
  }
  dag->rank = rpl_rank_via_dag(dag, NULL);
  return dag->preferred_parent;
}
/*---------------------------------------------------------------------------*/
void
rpl_remove_parent(rpl_parent_t *parent)
{
  LOG_INFO("Removing parent ");
  LOG_INFO_6ADDR(rpl_parent_get_ipaddr(parent));
  LOG_INFO_("\n");

  rpl_nullify_parent(parent);

  nbr_table_remove(rpl_parents, parent);
}
/*---------------------------------------------------------------------------*/
void
rpl_nullify_parent(rpl_parent_t *parent)
{
  rpl_dag_t *dag = parent->dag;
  /* Indicate that the parent in question is no longer eligible for preferred
     parent selection and should not be taken into account when calculating
     the rank to advertise in the DAG it belongs to. */
  if(!(parent->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE)) {
    parent->flags |= RPL_PARENT_FLAG_NOT_ELIGIBLE;
    parent->flags |= RPL_PARENT_FLAG_WAS_KICKED;
  }

#if RPL_WEIGHTED_INTERFACES
  /* In any case, when a parent is nullified, it is not longer eligible and thus
     effectively no longer part of our logical parent set. Hence, we can simply
     reset its weighted interface selection flag */
  const linkaddr_t *lladdr = rpl_get_parent_lladdr(parent);
  if(lladdr != NULL) {
    link_stats_modify_wifsel_flag(lladdr, LINK_STATS_WIFSEL_FLAG_FALSE);
  }
#endif

  /* This function can be called when the preferred parent is NULL, so we
     need to handle this condition in order to trigger uip_ds6_defrt_rm. */
  if(parent == dag->preferred_parent || dag->preferred_parent == NULL) {
    dag->rank = RPL_INFINITE_RANK;
    if(dag->joined) {
      if(dag->instance->def_route != NULL) {
        LOG_DBG("Removing default route ");
        LOG_DBG_6ADDR(rpl_parent_get_ipaddr(parent));
        LOG_DBG_("\n");
        uip_ds6_defrt_rm(dag->instance->def_route);
        dag->instance->def_route = NULL;
      }
      /* Send No-Path DAO only when nullifying preferred parent */
      if(parent == dag->preferred_parent) {
        if(RPL_IS_STORING(dag->instance)) {
          dao_output(parent, RPL_ZERO_LIFETIME);
        }
        rpl_set_preferred_parent(dag, NULL);
      }
    }
  }

  LOG_INFO("Nullifying parent ");
  LOG_INFO_6ADDR(rpl_parent_get_ipaddr(parent));
  LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
void
rpl_move_parent(rpl_dag_t *dag_src, rpl_dag_t *dag_dst, rpl_parent_t *parent)
{
  if(parent == dag_src->preferred_parent) {
      rpl_set_preferred_parent(dag_src, NULL);
      dag_src->rank = RPL_INFINITE_RANK;
    if(dag_src->joined && dag_src->instance->def_route != NULL) {
      LOG_DBG("Removing default route ");
      LOG_DBG_6ADDR(rpl_parent_get_ipaddr(parent));
      LOG_DBG_("\n");
      LOG_DBG("rpl_move_parent\n");
      uip_ds6_defrt_rm(dag_src->instance->def_route);
      dag_src->instance->def_route = NULL;
    }
  } else if(dag_src->joined) {
    if(RPL_IS_STORING(dag_src->instance)) {
      /* Remove uIPv6 routes that have this parent as the next hop. */
      rpl_remove_routes_by_nexthop(rpl_parent_get_ipaddr(parent), dag_src);
    }
  }

  LOG_INFO("Moving parent ");
  LOG_INFO_6ADDR(rpl_parent_get_ipaddr(parent));
  LOG_INFO_("\n");

  parent->dag = dag_dst;

  const linkaddr_t *lladdr = rpl_get_parent_lladdr(parent);
  if(lladdr != NULL) {
    /* REVIEW check if this branch needs more strict requirements. */
    link_stats_reset_defer_flags(lladdr);
    link_stats_update_norm_metric(lladdr);
  }
}
/*---------------------------------------------------------------------------*/
static rpl_dag_t *
rpl_get_any_dag_with_parent(bool requires_parent)
{
  int i;

  for(i = 0; i < RPL_MAX_INSTANCES; ++i) {
    if(instance_table[i].used
         && instance_table[i].current_dag->joined
         && (!requires_parent || instance_table[i].current_dag->preferred_parent != NULL)) {
      return instance_table[i].current_dag;
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
int
rpl_has_joined(void)
{
  if(rpl_dag_root_is_root()) {
    return 1;
  }
  return rpl_get_any_dag_with_parent(true) != NULL;
}
/*---------------------------------------------------------------------------*/
int
rpl_has_downward_route(void)
{
  int i;
  if(rpl_dag_root_is_root()) {
    return 1; /* We are the root, and know the route to ourself */
  }
  for(i = 0; i < RPL_MAX_INSTANCES; ++i) {
    if(instance_table[i].used && instance_table[i].has_downward_route) {
      return 1;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
rpl_dag_t *
rpl_get_dag(const uip_ipaddr_t *addr)
{
  int i, j;

  for(i = 0; i < RPL_MAX_INSTANCES; ++i) {
    if(instance_table[i].used) {
      for(j = 0; j < RPL_MAX_DAG_PER_INSTANCE; ++j) {
        if(instance_table[i].dag_table[j].joined
            && uip_ipaddr_prefixcmp(&instance_table[i].dag_table[j].dag_id, addr,
                instance_table[i].dag_table[j].prefix_info.length)) {
          return &instance_table[i].dag_table[j];
        }
      }
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
rpl_dag_t *
rpl_get_any_dag(void)
{
  return rpl_get_any_dag_with_parent(false);
}
/*---------------------------------------------------------------------------*/
rpl_instance_t *
rpl_get_instance(uint8_t instance_id)
{
  int i;

  for(i = 0; i < RPL_MAX_INSTANCES; ++i) {
    if(instance_table[i].used && instance_table[i].instance_id == instance_id) {
      return &instance_table[i];
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
rpl_of_t *
rpl_find_of(rpl_ocp_t ocp)
{
  unsigned int i;

  for(i = 0;
      i < sizeof(objective_functions) / sizeof(objective_functions[0]);
      i++) {
    if(objective_functions[i]->ocp == ocp) {
      return objective_functions[i];
    }
  }

  return NULL;
}
/*---------------------------------------------------------------------------*/
void
rpl_join_instance(uip_ipaddr_t *from, rpl_dio_t *dio)
{
  rpl_instance_t *instance;
  rpl_dag_t *dag;
  rpl_parent_t *p;
  rpl_of_t *of;

  if((!RPL_WITH_NON_STORING && dio->mop == RPL_MOP_NON_STORING)
      || (!RPL_WITH_STORING && (dio->mop == RPL_MOP_STORING_NO_MULTICAST
          || dio->mop == RPL_MOP_STORING_MULTICAST))) {
    LOG_WARN("DIO advertising a non-supported MOP %u\n", dio->mop);
    return;
  }

  /* Determine the objective function by using the
     objective code point of the DIO. */
  of = rpl_find_of(dio->ocp);
  if(of == NULL) {
    LOG_WARN("DIO for DAG instance %u does not specify a supported OF: %u\n",
           dio->instance_id, dio->ocp);
    return;
  }

  dag = rpl_alloc_dag(dio->instance_id, &dio->dag_id);
  if(dag == NULL) {
    LOG_ERR("Failed to allocate a DAG object!\n");
    return;
  }

  instance = dag->instance;

  p = rpl_add_parent(dag, dio, from);
  LOG_DBG("Adding ");
  LOG_DBG_6ADDR(from);
  LOG_DBG_(" as a parent: ");
  if(p == NULL) {
    LOG_DBG_("failed\n");
    instance->used = 0;
    return;
  }
  p->dtsn = dio->dtsn;
  LOG_DBG_("succeeded\n");
  /* Init parent as eligible in given DAG */
  p->flags &= ~RPL_PARENT_FLAG_NOT_ELIGIBLE;
  p->flags &= ~RPL_PARENT_FLAG_WAS_KICKED;

  /* Autoconfigure an address if this node does not already have an address
     with this prefix. */
  if(dio->prefix_info.flags & UIP_ND6_RA_FLAG_AUTONOMOUS) {
    check_prefix(NULL, &dio->prefix_info);
  }

  dag->joined = 1;
  dag->preference = dio->preference;
  dag->grounded = dio->grounded;
  dag->version = dio->version;

  instance->of = of;
  instance->mop = dio->mop;
  instance->mc.type = dio->mc.type;
  instance->mc.flags = dio->mc.flags;
  instance->mc.aggr = dio->mc.aggr;
  instance->mc.prec = dio->mc.prec;
  instance->current_dag = dag;
  instance->dtsn_out = RPL_LOLLIPOP_INIT;

  instance->max_rankinc = dio->dag_max_rankinc;
  instance->min_hoprankinc = dio->dag_min_hoprankinc;
  instance->dio_intdoubl = dio->dag_intdoubl;
  instance->dio_intmin = dio->dag_intmin;
  instance->dio_intcurrent = instance->dio_intmin + instance->dio_intdoubl;
  instance->dio_redundancy = dio->dag_redund;
  instance->default_lifetime = dio->default_lifetime;
  instance->lifetime_unit = dio->lifetime_unit;

  memcpy(&dag->dag_id, &dio->dag_id, sizeof(dio->dag_id));

  /* Copy prefix information from the DIO into the DAG object. */
  memcpy(&dag->prefix_info, &dio->prefix_info, sizeof(rpl_prefix_t));

  rpl_set_preferred_parent(dag, p);
  instance->of->update_metric_container(instance);
  /* Since at this point there is only one parent in the parent set of the given
     DAG, the following is correct behavior for MRHOF (RFC6719, Section 3.3.) */
  dag->rank = rpl_rank_via_parent(p);
  /* So far this is the lowest rank we are aware of. */
  dag->min_rank = dag->rank;

  if(default_instance == NULL) {
    default_instance = instance;
  }

#if RPL_WEIGHTED_INTERFACES
  const linkaddr_t *lladdr = rpl_get_parent_lladdr(p);
  if(lladdr != NULL) {
    /* Since the parent is our only parent and it is in fact in our parent set
       because we've marked it eligible and set it as our preferred parent we
       can be certain that preferred interface selection must be weighted for
       said parent. */
    link_stats_modify_wifsel_flag(lladdr, LINK_STATS_WIFSEL_FLAG_TRUE);
  }
#endif

  LOG_INFO("Joined DAG with instance ID %u, rank %hu, DAG ID ",
         dio->instance_id, dag->rank);
  LOG_INFO_6ADDR(&dag->dag_id);
  LOG_INFO_("\n");

  LOG_ANNOTATE("#A join=%u\n", dag->dag_id.u8[sizeof(dag->dag_id) - 1]);

  rpl_reset_dio_timer(instance);
  rpl_set_default_route(instance, from);

  if(instance->mop != RPL_MOP_NO_DOWNWARD_ROUTES) {
    rpl_schedule_dao(instance);
  } else {
    LOG_WARN("The DIO does not meet the prerequisites for sending a DAO\n");
  }

  instance->of->reset(dag);
}

#if RPL_MAX_DAG_PER_INSTANCE > 1
/*---------------------------------------------------------------------------*/
rpl_dag_t *
rpl_add_dag(uip_ipaddr_t *from, rpl_dio_t *dio)
{
  rpl_instance_t *instance;
  rpl_dag_t *dag, *previous_dag;
  rpl_parent_t *p;
  rpl_of_t *of;

  dag = rpl_alloc_dag(dio->instance_id, &dio->dag_id);
  if(dag == NULL) {
    LOG_ERR("Failed to allocate a DAG object!\n");
    return NULL;
  }

  instance = dag->instance;

  previous_dag = find_parent_dag(instance, from);
  if(previous_dag == NULL) {
    LOG_DBG("Adding ");
    LOG_DBG_6ADDR(from);
    LOG_DBG_(" as a parent: ");
    p = rpl_add_parent(dag, dio, from);
    if(p == NULL) {
      LOG_DBG_("failed\n");
      dag->used = 0;
      return NULL;
    }
    LOG_DBG_("succeeded\n");
  } else {
    p = rpl_find_parent(previous_dag, from);
    rpl_move_parent(previous_dag, dag, p);
  }
  p->rank = dio->rank;
  /* Init parent as eligible in given DAG */
  p->flags &= ~RPL_PARENT_FLAG_NOT_ELIGIBLE;
  p->flags &= ~RPL_PARENT_FLAG_WAS_KICKED;

  /* Determine the objective function by using the
     objective code point of the DIO. */
  of = rpl_find_of(dio->ocp);
  if(of != instance->of ||
     instance->mop != dio->mop ||
     instance->max_rankinc != dio->dag_max_rankinc ||
     instance->min_hoprankinc != dio->dag_min_hoprankinc ||
     instance->dio_intdoubl != dio->dag_intdoubl ||
     instance->dio_intmin != dio->dag_intmin ||
     instance->dio_redundancy != dio->dag_redund ||
     instance->default_lifetime != dio->default_lifetime ||
     instance->lifetime_unit != dio->lifetime_unit) {
    LOG_WARN("DIO for DAG instance %u incompatible with previous DIO\n",
	   dio->instance_id);
    rpl_remove_parent(p);
    dag->used = 0;
    return NULL;
  }

  dag->used = 1;
  dag->grounded = dio->grounded;
  dag->preference = dio->preference;
  dag->version = dio->version;

  memcpy(&dag->dag_id, &dio->dag_id, sizeof(dio->dag_id));

  /* copy prefix information into the dag */
  memcpy(&dag->prefix_info, &dio->prefix_info, sizeof(rpl_prefix_t));

  rpl_set_preferred_parent(dag, p);
  /* We don't need to update the parent's normalized metric and defer flags
     because at this point that has already been handled by rpl_add_parent()
     or rpl_move_parent() */
  /* Since at this point there is only one parent in the parent set of the given
     DAG, the following is correct behavior for MRHOF (RFC6719, Section 3.3.) */
  dag->rank = rpl_rank_via_parent(p);
  dag->min_rank = dag->rank; /* So far this is the lowest rank we know of. */

  LOG_INFO("Joined DAG with instance ID %u, rank %hu, DAG ID ",
         dio->instance_id, dag->rank);
  LOG_INFO_6ADDR(&dag->dag_id);
  LOG_INFO_("\n");

  LOG_ANNOTATE("#A join=%u\n", dag->dag_id.u8[sizeof(dag->dag_id) - 1]);

  /* This call also ensures that the wifsel flag will be set appropriately
     so there's no reason to set it manually here. */
  rpl_process_parent_event(instance, p);
  p->dtsn = dio->dtsn;

  return dag;
}
#endif /* RPL_MAX_DAG_PER_INSTANCE > 1 */

/*---------------------------------------------------------------------------*/
static void
global_repair(uip_ipaddr_t *from, rpl_dag_t *dag, rpl_dio_t *dio)
{
  rpl_parent_t *p;

  remove_parents(dag, 0);
  dag->version = dio->version;

  /* copy parts of the configuration so that it propagates in the network */
  dag->instance->dio_intdoubl = dio->dag_intdoubl;
  dag->instance->dio_intmin = dio->dag_intmin;
  dag->instance->dio_redundancy = dio->dag_redund;
  dag->instance->default_lifetime = dio->default_lifetime;
  dag->instance->lifetime_unit = dio->lifetime_unit;

  dag->instance->of->reset(dag);
  dag->min_rank = RPL_INFINITE_RANK;
  RPL_LOLLIPOP_INCREMENT(dag->instance->dtsn_out);

  p = rpl_add_parent(dag, dio, from);
  if(p == NULL) {
    LOG_ERR("Failed to add a parent during the global repair\n");
    dag->rank = RPL_INFINITE_RANK;
  } else {
    /* We don't need to update the parent's normalized metric and defer flags
       because at this point that has already been handled by rpl_add_parent() */
    /* Since at this point there is only one parent in the parent set of the given
       DAG, the following is correct behavior for MRHOF (RFC6719, Section 3.3.) */
    dag->rank = rpl_rank_via_parent(p);
    dag->min_rank = dag->rank;
    LOG_DBG("rpl_process_parent_event global repair\n");
    rpl_process_parent_event(dag->instance, p);
  }

  LOG_DBG("Participating in a global repair (version=%u, rank=%hu)\n",
         dag->version, dag->rank);

  RPL_STAT(rpl_stats.global_repairs++);
}

/*---------------------------------------------------------------------------*/
void
rpl_local_repair(rpl_instance_t *instance)
{
  int i;

  if(instance == NULL) {
    LOG_WARN("local repair requested for instance NULL\n");
    return;
  }

  /* According to RFC 8036 Section 7.1.5., a local repair consists of a node detaching
     from a DAG and then reattaching to the same or to a different DAG at a later time.
     A node becomes detached from a DAG when it has an empty parent set. While detached,
     a node must advertise RPL_INFINITE_RANK such that its children may select a new
     preferred parent (this process is called "poisoning"). After the detached node has
     made sufficient effort to send a notification to its children that it is detached,
     the node can rejoin the same DAG with a higher rank value. */
  LOG_INFO("Starting a local instance repair\n");
  for(i = 0; i < RPL_MAX_DAG_PER_INSTANCE; i++) {
    if(instance->dag_table[i].used) {
      /* Poison our children by advertising RPL_INFINITE_RANK */
      instance->dag_table[i].rank = RPL_INFINITE_RANK;
      /* Detach from the DAG by removing all parents from the parent set */
      nullify_parents(&instance->dag_table[i], 0);
    }
  }

  /* no downward route anymore */
  instance->has_downward_route = 0;
#if RPL_WITH_DAO_ACK
  ctimer_stop(&instance->dao_retransmit_timer);
#endif /* RPL_WITH_DAO_ACK */

  /* Start poisoning ASAP */
  rpl_reset_dio_timer(instance);
  /* The poisoning DIOs must be sent before we start accepting new DIOs ourselves */
  rpl_reset_poison_timer(instance);
  if(RPL_IS_STORING(instance)) {
    /* Request refresh of DAO registrations next DIO. Only for storing mode. In
     * non-storing mode, non-root nodes increment DTSN only on when their parent do,
     * or on global repair (see RFC6550 section 9.6.) */
    RPL_LOLLIPOP_INCREMENT(instance->dtsn_out);
  }

  RPL_STAT(rpl_stats.local_repairs++);
}
/*---------------------------------------------------------------------------*/
/* Function that is supposed to be called periodically to recalculate ranks
   asynchronously by setting the RPL_PARENT_FLAG_UPDATED flag of a parent,
   and thereby causing selection of a new DAG and preferred parent. */
void
rpl_recalculate_ranks(void)
{
  rpl_parent_t *p;

  /*
   * We recalculate ranks when we receive feedback from the system rather
   * than RPL protocol messages. This periodical recalculation is called
   * from a timer in order to keep the stack depth reasonably low.
   */
  p = nbr_table_head(rpl_parents);
  while(p != NULL) {
    if(p->dag != NULL && p->dag->instance && (p->flags & RPL_PARENT_FLAG_UPDATED)) {
      p->flags &= ~RPL_PARENT_FLAG_UPDATED;
      LOG_DBG("rpl_process_parent_event recalculate_ranks because ");
      LOG_DBG_LLADDR(rpl_get_parent_lladdr(p));
      LOG_DBG_(" was updated\n");
      /* We don't need to update the parent's normalized metric and defer flags
         because at this point that should have already been handled prior to
         setting the parent's RPL_PARENT_FLAG_UPDATED flag */
      if(!rpl_process_parent_event(p->dag->instance, p)) {
        LOG_DBG("A parent was dropped\n");
      }
    }
    p = nbr_table_next(rpl_parents, p);
  }
}
/*---------------------------------------------------------------------------*/
int
rpl_process_parent_event(rpl_instance_t *instance, rpl_parent_t *p)
{
  int return_value;
  rpl_parent_t *last_parent = instance->current_dag->preferred_parent;

#if LOG_DBG_ENABLED
    rpl_rank_t old_rank;
    old_rank = instance->current_dag->rank;
#endif /* LOG_DBG_ENABLED */

  return_value = 1;

  if(RPL_IS_STORING(instance)
      && uip_ds6_route_is_nexthop(rpl_parent_get_ipaddr(p))
      && !rpl_parent_is_reachable(p) && instance->mop > RPL_MOP_NON_STORING) {
    LOG_WARN("Unacceptable link %u, removing routes via: ", rpl_get_parent_link_metric(p));
    LOG_WARN_6ADDR(rpl_parent_get_ipaddr(p));
    LOG_WARN_("\n");
    rpl_remove_routes_by_nexthop(rpl_parent_get_ipaddr(p), p->dag);
  }

  if(!rpl_acceptable_rank(p->dag, rpl_rank_via_parent(p)) && !(p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE)) {
    /* The candidate parent is no longer valid: the max possible rank increase
       resulting from the choice of it as a parent (meaning it may thereafter
       become the preferred parent too) would be too high according to rule
       3 of RFC6550 Section 8.2.2.4. For OF0-based OFs the rank via the preferred
       parent is also the advertised rank and so if the rank via p is not acceptable
       it should not be in the parent set because it may become preferred at some
       point. For MRHOF-based OFs, the rank via p is the highest rank we may ever
       advertise for which p is to blame and so if it is not acceptable, it is
       probably a safe bet to not have p as a member of the parent set. */
    LOG_WARN("Stored rank %u of ",(unsigned)p->rank);
    if(p == p->dag->preferred_parent) {
      LOG_WARN_("preferred ");
    }
    LOG_WARN_("parent ");
    LOG_WARN_6ADDR(rpl_parent_get_ipaddr(p));
    LOG_WARN_(" may cause unacceptable advertised rank %u in worst case (Current min %u, MaxRankInc %u)\n",
              (unsigned)rpl_rank_via_parent(p), p->dag->min_rank, p->dag->instance->max_rankinc);
    rpl_nullify_parent(p);
  }

  if((p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE) && p == p->dag->preferred_parent) {
    /* This may happen when the preferred parent is marked ineligible outside of
       a call to rpl_nullify_parent. */
    rpl_nullify_parent(p);
  }

  return_value = !(p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE);

#if RPL_WEIGHTED_INTERFACES
  /* Even though the wifsel flag is reset in rpl_nullify_parent, we still reset it here
     if the parent is marked ineligible because it may have been marked that way outside
     of rpl_nullify_parent, e.g., after receiving a DIO with an innapropriate rank from
     a new parent, a parent moved from a different DAG, or from a parent that was not our
     preferred parent. */
  link_stats_wifsel_flag_t wifsel_flag;
  wifsel_flag = return_value ? LINK_STATS_WIFSEL_FLAG_TRUE : LINK_STATS_WIFSEL_FLAG_FALSE;
  link_stats_modify_wifsel_flag(rpl_get_parent_lladdr(p), wifsel_flag);
#endif

  if(return_value || (p->flags & RPL_PARENT_FLAG_WAS_KICKED)) {
    p->flags &= ~RPL_PARENT_FLAG_WAS_KICKED;
    if(rpl_select_dag(instance, p) == NULL) {
      if(last_parent != NULL) {
        /* No suitable parent anymore; trigger a local repair. */
        LOG_ERR("No parents found in any DAG\n");
        rpl_local_repair(instance);
        return 0;
      }
    }
  }

#if LOG_DBG_ENABLED
  if(DAG_RANK(old_rank, instance) != DAG_RANK(instance->current_dag->rank, instance)) {
    LOG_INFO("Moving in the instance from rank %hu to %hu\n",
	   DAG_RANK(old_rank, instance), DAG_RANK(instance->current_dag->rank, instance));
    if(instance->current_dag->rank != RPL_INFINITE_RANK) {
      LOG_DBG("The preferred parent is ");
      LOG_DBG_6ADDR(rpl_parent_get_ipaddr(instance->current_dag->preferred_parent));
      LOG_DBG_(" (rank %u)\n",
           (unsigned)DAG_RANK(instance->current_dag->preferred_parent->rank, instance));
    } else {
      LOG_WARN("We don't have any parent");
    }
  }
#endif /* LOG_DBG_ENABLED */

  return return_value;
}
/*---------------------------------------------------------------------------*/
static int
add_nbr_from_dio(uip_ipaddr_t *from, rpl_dio_t *dio)
{
  /* add this to the neighbor cache if not already there */
  if(rpl_icmp6_update_nbr_table(from, NBR_TABLE_REASON_RPL_DIO, dio) == NULL) {
    LOG_ERR("Out of memory, dropping DIO from ");
    LOG_ERR_6ADDR(from);
    LOG_ERR_("\n");
    return 0;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
void
rpl_process_dio(uip_ipaddr_t *from, rpl_dio_t *dio)
{
  rpl_instance_t *instance;
  rpl_dag_t *dag, *previous_dag;
  rpl_parent_t *p;

#if RPL_WITH_MULTICAST
  /* If the root is advertising MOP 2 but we support MOP 3 we can still join
   * In that scenario, we suppress DAOs for multicast targets */
  if(dio->mop < RPL_MOP_STORING_NO_MULTICAST) {
#else
  if(dio->mop != RPL_MOP_DEFAULT) {
#endif
    LOG_ERR("Ignoring a DIO with an unsupported MOP: %d\n", dio->mop);
    return;
  }

  /* Retrieve both the DAG and the instance to which the DIO belongs */
  dag = get_dag(dio->instance_id, &dio->dag_id);
  instance = rpl_get_instance(dio->instance_id);

  /* FIXME it's definitely not appropriate to check this here but for testing purposes
     we make sure we don't process any DIOs for an instance if we're currently poisoning it. */
  if(instance != NULL && poisoning_instance == instance && dio->rank != RPL_INFINITE_RANK) {
    LOG_DBG("Not processing DIO from ");
    LOG_DBG_6ADDR(from);
    LOG_DBG_(", currently poisoning instance %u\n", instance->instance_id);
    return;
  }

  /* If both DAG and instance are != NULL we know that the DIO corresponds
     to a given DAG in a given instance of which we've at least already been
     a part in the past (however, we might currently have joined another DAG) */
  if(dag != NULL && instance != NULL) {
    if(lollipop_greater_than(dio->version, dag->version)) {
      if(dag->rank == ROOT_RANK(instance)) {
        /* We are root (not Groot, you Marvel addict) and we somehow receive a
           DIO (presumably from our sub-DAG) which advertises an innapropriately
           high DAG version. Hence, we should probably start advertising an even
           higher DAG version (assuming we are part of the DAG to which the DIO
           pertains that is). */
        LOG_WARN("Root received inconsistent DIO version number (current: %u, received: %u)\n", dag->version, dio->version);
        dag->version = dio->version;
        RPL_LOLLIPOP_INCREMENT(dag->version);
      } else {
        /* Migrate to the new DODAG version */
        /* We can perform a global repair here regardless of the fact we have currently
           joined the given DAG. That's because a global repair operation operates on the
           parents of the given DAG only and so it should (theoretically) not affect
           correct operation in another DAG. Remember that a global repair is initiated
           by the root by incrementing the DAG version (see RFC 6550 Section 3.2.2.). */
        LOG_DBG("Global repair\n");
        if(dio->prefix_info.length != 0) {
          if(dio->prefix_info.flags & UIP_ND6_RA_FLAG_AUTONOMOUS) {
            LOG_DBG("Prefix announced in DIO\n");
            rpl_set_prefix(dag, &dio->prefix_info.prefix, dio->prefix_info.length);
          }
        }
        global_repair(from, dag, dio);
      }
      if(dag->joined) {
        /* We advertise the new DODAG version ASAP */
        rpl_reset_dio_timer(instance);
      }
      return;
    }

    if(lollipop_greater_than(dag->version, dio->version)) {
      /* The DIO advertises a smaller DODAG version than the version of
         said DODAG which we're currently part of, i.e., the DIO sender
         is part of an older version of the DAG. */
      LOG_WARN("Old DAG version received => inconsistency detected\n");
      if(dag->joined) {
        /* We advertise the new DODAG version ASAP */
        rpl_reset_dio_timer(instance);
      }
      return;
    }
  }

  if(instance == NULL) {
    LOG_INFO("New instance detected (ID=%u): Joining...\n", dio->instance_id);
    if(add_nbr_from_dio(from, dio)) {
      rpl_join_instance(from, dio);
    } else {
      LOG_WARN("Not joining instance since could not add neighbor ");
      LOG_WARN_6ADDR(from);
      LOG_WARN_("\n");
    }
    return;
  }

  if(instance->current_dag->rank == ROOT_RANK(instance) && instance->current_dag != dag) {
    LOG_WARN("Root ignored DIO for different DAG\n");
    return;
  }

  if(dag == NULL) {
#if RPL_MAX_DAG_PER_INSTANCE > 1
    LOG_INFO("Adding new DAG to known instance.\n");
    if(!add_nbr_from_dio(from, dio)) {
      LOG_WARN("Not adding DAG ");
      LOG_WARN_6ADDR(&dio->dag_id);
      LOG_WARN_(" since could not add neighbor ");
      LOG_WARN_6ADDR(from);
      LOG_WARN_("\n");
      return;
    }
    dag = rpl_add_dag(from, dio);
    if(dag == NULL) {
      LOG_WARN("Failed to add DAG.\n");
      return;
    }
#else /* RPL_MAX_DAG_PER_INSTANCE > 1 */
    LOG_WARN("Only one DAG per instance supported.\n");
    return;
#endif /* RPL_MAX_DAG_PER_INSTANCE > 1 */
  }


  if(dio->rank < ROOT_RANK(instance)) {
    LOG_INFO("Ignoring DIO with rank (%u) < root rank (%u)\n",
             (unsigned)dio->rank, (unsigned)ROOT_RANK(instance));
    return;
  }

  /* Prefix Information Option treated to add new prefix */
  if(dio->prefix_info.length != 0) {
    if(dio->prefix_info.flags & UIP_ND6_RA_FLAG_AUTONOMOUS) {
      LOG_DBG("Prefix announced in DIO\n");
      rpl_set_prefix(dag, &dio->prefix_info.prefix, dio->prefix_info.length);
    }
  }

  if(!add_nbr_from_dio(from, dio)) {
    LOG_WARN("Could not add neighbor ");
    LOG_WARN_6ADDR(from);
    LOG_WARN_(" based on DIO\n");
    return;
  }

  if(dag->rank == ROOT_RANK(instance)) {
    if(dio->rank != RPL_INFINITE_RANK) {
      instance->dio_counter++;
    }
    LOG_DBG("DIO processing terminated because we are root\n");
    return;
  }

  /* The DIO comes from a valid DAG, we can refresh its lifetime */
  dag->lifetime = (1UL << (instance->dio_intmin + instance->dio_intdoubl)) * RPL_DAG_LIFETIME / 1000;
  LOG_INFO("Set DAG ");
  LOG_INFO_6ADDR(&dag->dag_id);
  LOG_INFO_(" lifetime to %ld\n", (long int) dag->lifetime);

  /* If we have reached this point, we know that the version of the DAG advertised
     in the DIO and our last stored version of that same DAG are identical because
     otherwise we'd have returned already. */
  p = rpl_find_parent(dag, from);
  if(p == NULL) {
    LOG_DBG("No parent with address ");
    LOG_DBG_6ADDR(from);
    LOG_DBG_(" present in DAG ");
    LOG_DBG_6ADDR(&dag->dag_id);
    LOG_DBG_("\n");
  } else {
    LOG_DBG("Parent ");
    LOG_DBG_6ADDR(from);
    LOG_DBG_(" found in DAG ");
    LOG_DBG_6ADDR(&dag->dag_id);
    LOG_DBG_("\n");
  }
  /* REVIEW maybe we should compare with DAG_RANK instead */
  if(dio->rank < dag->rank) {
    LOG_DBG("DIO advertises a rank (%u) < DAG rank (%u)\n",
            (unsigned)dio->rank, (unsigned)dag->rank);
    if(p == NULL) {
      previous_dag = find_parent_dag(instance, from);
      if(previous_dag == NULL) {
        p = rpl_add_parent(dag, dio, from);
        if(p == NULL) {
          LOG_WARN("Failed to add a new parent (");
          LOG_WARN_6ADDR(from);
          LOG_WARN_(")\n");
          return;
        }
        LOG_INFO("New candidate parent with rank %u: ", (unsigned)p->rank);
        LOG_INFO_6ADDR(from);
        LOG_INFO_("\n");
      } else {
        p = rpl_find_parent(previous_dag, from);
        rpl_move_parent(previous_dag, dag, p);
      }
      /* Init parent as eligible in given DAG */
      p->flags &= ~RPL_PARENT_FLAG_NOT_ELIGIBLE;
      p->flags &= ~RPL_PARENT_FLAG_WAS_KICKED;
    } else {
      if(p->rank == dio->rank) {
        LOG_INFO("Received consistent DIO\n");
        if(dag->joined) {
          instance->dio_counter++;
        }
      }
    }
    p->rank = dio->rank;
    if(p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE) {
      LOG_DBG("Originator of DIO is currently ineligible\n");
      link_stats_update_norm_metric(rpl_get_parent_lladdr(p));
      if(rpl_acceptable_rank(dag, rpl_rank_via_parent(p))) {
        LOG_DBG("Originator of DIO will be marked eligible\n");
        p->flags &= ~RPL_PARENT_FLAG_NOT_ELIGIBLE;
        rpl_exec_norm_metric_logic(RPL_RESET_DEFER_TRUE);
      } else {
        LOG_DBG("Parent ");
        LOG_DBG_6ADDR(rpl_parent_get_ipaddr(p));
        LOG_DBG_(" may cause unacceptable advertised rank %u in worst case (Current min %u, MaxRankInc %u)\n",
                  (unsigned)rpl_rank_via_parent(p), p->dag->min_rank, p->dag->instance->max_rankinc);
        link_stats_reset_defer_flags(rpl_get_parent_lladdr(p));
      }
    } else {
      LOG_DBG("Originator of DIO is currently eligible\n");
      rpl_exec_norm_metric_logic(RPL_RESET_DEFER_TRUE);
    }
  } else {
    LOG_DBG("DIO advertises a rank (%u) >= DAG rank (%u)\n",
            (unsigned)dio->rank, (unsigned)dag->rank);
    if(p == NULL) {
      previous_dag = find_parent_dag(instance, from);
      if(previous_dag == NULL) {
        p = rpl_add_parent(dag, dio, from);
        if(p == NULL) {
          LOG_DBG("Failed to add a new parent (");
          LOG_DBG_6ADDR(from);
          LOG_DBG_(")\n");
          return;
        }
        LOG_DBG("New candidate parent ");
        LOG_DBG_6ADDR(from);
        LOG_DBG_(" wasn't part of any DAG and will be marked ineligible\n");
      } else {
        p = rpl_find_parent(previous_dag, from);
        rpl_move_parent(previous_dag, dag, p);
        LOG_DBG("Candidate parent ");
        LOG_DBG_6ADDR(from);
        LOG_DBG_(" was part of another DAG and will be marked ineligible\n");
      }
      p->flags |= RPL_PARENT_FLAG_NOT_ELIGIBLE;
      p->rank = dio->rank;
      return;
    }
    p->rank = dio->rank;
    if(p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE) {
      LOG_DBG("Candidate parent ");
      LOG_DBG_6ADDR(from);
      LOG_DBG_(" was part of same DAG but is already marked ineligible\n");
      return;
    }
    LOG_DBG("Candidate parent ");
    LOG_DBG_6ADDR(from);
    LOG_DBG_(" was part of parent set and will be marked ineligible\n");
    p->flags |= RPL_PARENT_FLAG_NOT_ELIGIBLE;
    p->flags |= RPL_PARENT_FLAG_WAS_KICKED;
    /* Make sure the normalized metric of the parent is updated even if it is
       currently preferred because it will no longer be preferred soon enough */
    /* TODO move away from using default instance */
    if(default_instance != NULL && default_instance->current_dag != NULL &&
       p == default_instance->current_dag->preferred_parent) {
      link_stats_reset_defer_flags(rpl_get_parent_lladdr(p));
    }
    rpl_exec_norm_metric_logic(RPL_RESET_DEFER_TRUE);
  }

  if(dio->rank == RPL_INFINITE_RANK && p == dag->preferred_parent) {
    /* Our preferred parent advertised an infinite rank, reset DIO timer */
    LOG_DBG("Preferred parent ");
    LOG_DBG_6ADDR(rpl_parent_get_ipaddr(p));
    LOG_DBG_(" advertises RPL_INFINITE_RANK\n");
    rpl_reset_dio_timer(instance);
  }

#if RPL_WITH_MC
  memcpy(&p->mc, &dio->mc, sizeof(p->mc));
#endif /* RPL_WITH_MC */

  /* Parent info has been updated, trigger rank recalculation */
  // p->flags |= RPL_PARENT_FLAG_UPDATED;
  
  if(rpl_process_parent_event(instance, p) == 0) {
    LOG_WARN("The candidate parent is rejected\n");
    return;
  }

  /* We don't use route control, so we can have only one official parent. */
  if(dag->joined && p == dag->preferred_parent) {
    if(should_refresh_routes(instance, dio, p)) {
      /* Our parent is requesting a new DAO. Increment DTSN in turn,
       * in both storing and non-storing mode (see RFC6550 section 9.6.) */
      RPL_LOLLIPOP_INCREMENT(instance->dtsn_out);
      rpl_schedule_dao(instance);
    }
    /* We received a new DIO from our preferred parent.
     * Call uip_ds6_defrt_add to set a fresh value for the lifetime counter */
    uip_ds6_defrt_add(from, RPL_DEFAULT_ROUTE_INFINITE_LIFETIME ? 0 : RPL_LIFETIME(instance, instance->default_lifetime));
  }
  p->dtsn = dio->dtsn;
}
/*---------------------------------------------------------------------------*/
/** @} */
