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
 *      The Parent-Oriented Objective Function (POOF)
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

/* Constants from RFC6552. We use the default values. */
#define RANK_STRETCH        0 /* Must be in the range [0;5] */
#define RANK_FACTOR         1 /* Must be in the range [1;4] */

#define MIN_STEP_OF_RANK    1
#define MAX_STEP_OF_RANK    9

#ifdef RPL_POOF_CONF_STEP_OF_RANK
#define STEP_OF_RANK        RPL_POOF_CONF_STEP_OF_RANK
#else /* RPL_POOF_CONF_STEP_OF_RANK */
#if LINK_STATS_INTERFACES_WITH_ETX
#define STEP_OF_RANK(p)     (((3 * parent_link_metric(p)) / LINK_STATS_ETX_DIVISOR) - 2)
#else /* LINK_STATS_INTERFACES_WITH_ETX */
#define STEP_OF_RANK(p)     parent_link_metric(p)
#endif /* LINK_STATS_INTERFACES_WITH_ETX */
#endif /* RPL_POOF_CONF_STEP_OF_RANK */

/*---------------------------------------------------------------------------*/
static void
reset(rpl_dag_t *dag)
{
  LOG_INFO("Reset POOF\n");
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
parent_rank_increase(rpl_parent_t *p)
{
  uint16_t min_hoprankinc;
  if(p == NULL || p->dag == NULL || p->dag->instance == NULL) {
    return RPL_INFINITE_RANK;
  }
  min_hoprankinc = p->dag->instance->min_hoprankinc;
  return (RANK_FACTOR * STEP_OF_RANK(p) + RANK_STRETCH) * min_hoprankinc;
}
/*---------------------------------------------------------------------------*/
static uint16_t
parent_path_cost(rpl_parent_t *p)
{
  if(p == NULL) {
    return 0xffff;
  }
  /* path cost upper bound: 0xffff */
  return MIN((uint32_t)p->rank + parent_link_metric(p), 0xffff);
}
/*---------------------------------------------------------------------------*/
static rpl_rank_t
rank_via_parent(rpl_parent_t *p)
{
  if(p == NULL) {
    return RPL_INFINITE_RANK;
  } else {
    return MIN((uint32_t)p->rank + parent_rank_increase(p), RPL_INFINITE_RANK);
  }
}
/*---------------------------------------------------------------------------*/
static int
parent_is_acceptable(rpl_parent_t *p)
{
  return STEP_OF_RANK(p) >= MIN_STEP_OF_RANK
         && STEP_OF_RANK(p) <= MAX_STEP_OF_RANK;
}
/*---------------------------------------------------------------------------*/
static int
parent_has_usable_link(rpl_parent_t *p)
{
  return parent_is_acceptable(p);
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

  /* Paths costs coarse-grained (multiple of min_hoprankinc), we operate without hysteresis */
  if(p1_cost != p2_cost) {
    /* Pick parent with lowest path cost */
    return p1_cost < p2_cost ? p1 : p2;
  } else {
    /* We have a tie! */
    /* Stik to current preferred parent if possible */
    if(p1 == dag->preferred_parent || p2 == dag->preferred_parent) {
      return dag->preferred_parent;
    }
    /* None of the nodes is the current preferred parent,
     * choose parent with best link metric */
    return parent_link_metric(p1) < parent_link_metric(p2) ? p1 : p2;
  }
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
static void
update_metric_container(rpl_instance_t *instance)
{
  instance->mc.type = RPL_DAG_MC_NONE;
}
/*---------------------------------------------------------------------------*/
static rpl_rank_t
rank_via_dag(rpl_dag_t *dag)
{
  if(dag != NULL && dag->preferred_parent != NULL) {
    return rank_via_parent(dag->preferred_parent);
  }
  return RPL_INFINITE_RANK;
}
/*---------------------------------------------------------------------------*/
rpl_of_t rpl_poof = {
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
  RPL_OCP_POOF
};