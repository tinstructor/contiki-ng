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
 *      The DRiPL Objective Function (DRiPLOF)
 * \author
 *      Robbe Elsas <robbe.elsas@ugent.be>
 */

#include "net/routing/rpl-classic/rpl.h"
#include "net/routing/rpl-classic/rpl-private.h"
#include "net/nbr-table.h"
#include "net/link-stats.h"

#include "sys/log.h"

#define LOG_MODULE "RPL"
#define LOG_LEVEL LOG_LEVEL_RPL

#ifdef DRIPL_CONF_MAX_LINK_METRIC
#define DRIPL_MAX_LINK_METRIC DRIPL_CONF_MAX_LINK_METRIC
#else /* DRIPL_CONF_MAX_LINK_METRIC */
#define DRIPL_MAX_LINK_METRIC (8U * LINK_STATS_ETX_DIVISOR)
#endif /* DRIPL_MAX_LINK_METRIC */

#ifdef DRIPL_CONF_PARENT_SWITCH_THRESHOLD
#define DRIPL_PARENT_SWITCH_THRESHOLD DRIPL_CONF_PARENT_SWITCH_THRESHOLD
#else /* DRIPL_CONF_PARENT_SWITCH_THRESHOLD */
#define DRIPL_PARENT_SWITCH_THRESHOLD (.75F * LINK_STATS_ETX_DIVISOR)
#endif /* DRIPL_PARENT_SWITCH_THRESHOLD */

#ifdef DRIPL_CONF_MAX_PATH_COST
#define DRIPL_MAX_PATH_COST DRIPL_CONF_MAX_PATH_COST
#else /* DRIPL_CONF_MAX_PATH_COST */
#define DRIPL_MAX_PATH_COST (256U * LINK_STATS_ETX_DIVISOR)
#endif /* DRIPL_MAX_PATH_COST */

/*---------------------------------------------------------------------------*/
static void
reset(rpl_dag_t *dag)
{
  LOG_INFO("Reset DRiPLOF\n");
}
/*---------------------------------------------------------------------------*/
#if RPL_WITH_DAO_ACK
static void
dao_ack_callback(rpl_parent_t *p, int status)
{
  return;
}
#endif /* RPL_WITH_DAO_ACK */
/*---------------------------------------------------------------------------*/
static uint16_t
parent_link_metric(rpl_parent_t *p)
{
  if(p == NULL) {
    return 0xffff;
  }

  const struct link_stats *stats = rpl_get_parent_link_stats(p);
  return stats != NULL ? stats->normalized_metric : 0xffff;
}
/*---------------------------------------------------------------------------*/
static uint16_t
parent_path_cost(rpl_parent_t *p)
{
  uint16_t base;

  if(p == NULL || p->dag == NULL || p->dag->instance == NULL) {
    return 0xffff;
  }

#if RPL_WITH_MC
  /* Handle the different MC types */
  switch(p->dag->instance->mc.type) {
    /* TODO add case for yet to be defined metric container */
    default:
      base = p->rank;
      break;
  }
#else /* RPL_WITH_MC */
  base = p->rank;
#endif /* RPL_WITH_MC */

  /* path cost upper bound: 0xffff */
  return MIN((uint32_t)base + parent_link_metric(p), 0xffff);
}
/*---------------------------------------------------------------------------*/
#if RPL_WITH_MC
static uint16_t
dag_path_cost(rpl_dag_t *dag)
{
  /* A node must advertise the highest cost path from the node to the
     root through any member of the parent set, and not simply the path
     cost associated with the preferred parent! */
  if(dag == NULL) {
    return 0xffff;
  }
  uint16_t path_cost = 0;
  rpl_parent_t *p;
  p = nbr_table_head(rpl_parents);
  while(p != NULL) {
    if(p->dag != NULL && p->dag == dag) {
      uint16_t parent_cost = parent_path_cost(p);
      path_cost = (parent_cost > path_cost) ? parent_cost : path_cost;
    }
    p = nbr_table_next(rpl_parents, p);
  }
  return (path_cost != 0) ? path_cost : 0xffff;
}
#endif /* RPL_WITH_MC */
/*---------------------------------------------------------------------------*/
static rpl_rank_t
rank_via_parent(rpl_parent_t *p)
{
  uint16_t min_hoprankinc;
  uint16_t path_cost;

  if(p == NULL || p->dag == NULL || p->dag->instance == NULL) {
    return RPL_INFINITE_RANK;
  }

  min_hoprankinc = p->dag->instance->min_hoprankinc;
  path_cost = parent_path_cost(p);

  /* Rank lower-bound: parent rank + min_hoprankinc */
  return MAX(MIN((uint32_t)p->rank + min_hoprankinc, 0xffff), path_cost);
}
/*---------------------------------------------------------------------------*/
static int
parent_is_acceptable(rpl_parent_t *p)
{
  uint16_t link_metric = parent_link_metric(p);
  uint16_t path_cost = parent_path_cost(p);
  /* Exclude links with too high link metrics or path cost (RFC6719, 3.2.2) */
  return link_metric <= DRIPL_MAX_LINK_METRIC && path_cost <= DRIPL_MAX_PATH_COST;
}
/*---------------------------------------------------------------------------*/
static int
parent_has_usable_link(rpl_parent_t *p)
{
  uint16_t link_metric = parent_link_metric(p);
  /* Exclude links with too high link metrics  */
  return link_metric <= DRIPL_MAX_LINK_METRIC;
}
/*---------------------------------------------------------------------------*/
static rpl_parent_t *
best_parent(rpl_parent_t *p1, rpl_parent_t *p2)
{
  rpl_dag_t *dag;
  uint16_t p1_cost;
  uint16_t p2_cost;
  int p1_is_acceptable;
  int p2_is_acceptable;

  p1_is_acceptable = p1 != NULL && parent_is_acceptable(p1);
  p2_is_acceptable = p2 != NULL && parent_is_acceptable(p2);

  if(!p1_is_acceptable) {
    return p2_is_acceptable ? p2 : NULL;
  }
  if(!p2_is_acceptable) {
    return p1_is_acceptable ? p1 : NULL;
  }

  dag = p1->dag; /* Both parents are in the same DAG. */
  p1_cost = parent_path_cost(p1);
  p2_cost = parent_path_cost(p2);

  /* Maintain stability of the preferred parent in case of similar ranks. */
  if(p1 == dag->preferred_parent || p2 == dag->preferred_parent) {
    if(p1_cost < p2_cost + DRIPL_PARENT_SWITCH_THRESHOLD &&
       p1_cost > p2_cost - DRIPL_PARENT_SWITCH_THRESHOLD) {
      return dag->preferred_parent;
    }
  }

  return p1_cost < p2_cost ? p1 : p2;
}
/*---------------------------------------------------------------------------*/
static rpl_dag_t *
best_dag(rpl_dag_t *d1, rpl_dag_t *d2)
{
  if(d1->grounded != d2->grounded) {
    return d1->grounded ? d1 : d2;
  }

  if(d1->preference != d2->preference) {
    return d1->preference > d2->preference ? d1 : d2;
  }

  return d1->rank < d2->rank ? d1 : d2;
}
/*---------------------------------------------------------------------------*/
#if !RPL_WITH_MC
static void
update_metric_container(rpl_instance_t *instance)
{
  instance->mc.type = RPL_DAG_MC_NONE;
}
#else /* RPL_WITH_MC */
static void
update_metric_container(rpl_instance_t *instance)
{
  rpl_dag_t *dag;
  uint16_t path_cost;
  uint8_t type;

  dag = instance->current_dag;
  if(dag == NULL || !dag->joined) {
    LOG_WARN("Cannot update the metric container when not joined\n");
    return;
  }

  if(dag->rank == ROOT_RANK(instance)) {
    /* Configure MC at root only, other nodes are auto-configured when joining */
    instance->mc.type = RPL_DAG_MC;
    instance->mc.flags = 0;
    instance->mc.aggr = RPL_DAG_MC_AGGR_ADDITIVE;
    instance->mc.prec = 0;
    path_cost = dag->rank;
  } else {
    path_cost = dag_path_cost(dag);
  }

  /* Handle the different MC types */
  switch(instance->mc.type) {
    case RPL_DAG_MC_NONE:
      break;
    /* TODO add case for yet to be defined metric container */
    default:
      LOG_WARN("DRiPLOF, non-supported MC %u\n", instance->mc.type);
      break;
  }
}
#endif /* RPL_WITH_MC */
/*---------------------------------------------------------------------------*/
static rpl_rank_t
rank_via_dag(rpl_dag_t *dag, linkaddr_t *blame)
{
  /* The rank must be set to the max of 3 following values:
     1. The rank computed for the path through the preferred parent.
     2. The highest rank advertised by any of its parent set members
        (this is NOT the same as the computed rank for the path 
        through said node), rounded to the next higher integral rank.
     3. The largest computed rank among paths through the parent set,
        minus MaxRankIncrease. */
  if(dag == NULL || dag->preferred_parent == NULL || dag->instance == NULL || !parent_is_acceptable(dag->preferred_parent)) {
    return RPL_INFINITE_RANK;
  }
  uint32_t min_hoprankinc = dag->instance->min_hoprankinc;
  uint32_t max_rankinc = dag->instance->max_rankinc;
  uint32_t rank = rank_via_parent(dag->preferred_parent);
  const linkaddr_t *lladdr = rpl_get_parent_lladdr(dag->preferred_parent);
  rpl_parent_t *p;
  p = nbr_table_head(rpl_parents);
  while(p != NULL) {
    if(p->dag != NULL && p->dag == dag && !(p->flags & RPL_PARENT_FLAG_NOT_ELIGIBLE) && parent_is_acceptable(p)) {
      uint32_t next_higher_rank = min_hoprankinc * (1 + ((uint32_t)p->rank / min_hoprankinc));
      if(next_higher_rank > rank) {
        rank = next_higher_rank;
        lladdr = rpl_get_parent_lladdr(p);
      }
      uint32_t parent_rank = rank_via_parent(p);
      if(max_rankinc <= parent_rank && (parent_rank - max_rankinc) > rank) {
        rank = parent_rank - max_rankinc;
        lladdr = rpl_get_parent_lladdr(p);
      }
    }
    p = nbr_table_next(rpl_parents, p);
  }
  if(blame != NULL) {
    *blame = *lladdr;
  }
  return MIN(rank, RPL_INFINITE_RANK);
}
/*---------------------------------------------------------------------------*/
rpl_of_t rpl_driplof = {
  reset,
#if RPL_WITH_DAO_ACK
  dao_ack_callback,
#endif
  parent_link_metric,
  parent_has_usable_link,
  parent_path_cost,
  rank_via_parent,
  best_parent,
  best_dag,
  update_metric_container,
  rank_via_dag,
  RPL_OCP_DRIPLOF
};
