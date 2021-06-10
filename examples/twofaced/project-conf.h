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
 *      Project-specific configuration for the twofaced DRiPL example
 * \author
 *      Robbe Elsas <robbe.elsas@ugent.be>
 */

#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#if CONTIKI_TARGET_COOJA == 0

#define NETSTACK_CONF_RADIO twofaced_rf_driver

#ifndef NETSTACK_CONF_RADIO
#define ZOUL_CONF_USE_CC1200_RADIO 1
#else /* NETSTACK_CONF_RADIO */
#if MAC_CONF_WITH_CSMA
#define CSMA_CONF_ACK_WAIT_TIME                 (RTIMER_SECOND / 200)
#define CSMA_CONF_AFTER_ACK_DETECTED_WAIT_TIME  (RTIMER_SECOND / 1500)
#endif /* MAC_CONF_WITH_CSMA */
#define REMOTE_DUAL_RF_ENABLED 1
#define LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR 2U
#define LINK_STATS_CONF_INTERFACES_WITH_ETX 1
#define LINK_STATS_CONF_WITH_WEIGHTS 1
#endif /* NETSTACK_CONF_RADIO */

#if MAC_CONF_WITH_TWOFACED
#define TWOFACED_MAC_CONF_ACK_WAIT_TIME                 (RTIMER_SECOND / 200)
#define TWOFACED_MAC_CONF_AFTER_ACK_DETECTED_WAIT_TIME  (RTIMER_SECOND / 1500)
#define NETSTACK_CONF_MAC twofaced_mac_driver
#define CC2538_CONF_INTERFACE_ID 1
#define CC2538_CONF_DEFAULT_CHANNEL 11
#define CC1200_CONF_INTERFACE_ID 2
#define CC1200_CONF_DEFAULT_CHANNEL 5
#endif /* MAC_CONF_WITH_TWOFACED */

#if MAC_CONF_WITH_TSCH
#error "The twofaced project currently doesn't support TSCH"
#endif /* MAC_CONF_WITH_TSCH */

/*
 * The RPL_CONF_OF_OCP parameter configures the OF used and disseminated
 * by the root in a DODAG Configuration option's OCP field. The OCP setting
 * has no meaning to non-root nodes, as they'll run any OF advertised by
 * the root as long as they recognize the corresponding OCP value and thus
 * support a given OF. The OFs a node (both root and non-root) supports
 * are configured through the RPL_CONF_SUPPORTED_OFS parameter.
 */
#define RPL_CONF_OF_OCP RPL_OCP_DRIPLOF

/*
 * The RPL_CONF_SUPPORTED_OFS parameter configures the OFs supported by
 * any node (root or non-root) at runtime. A node may only join a RPL
 * instance (advertised in a DIO) which is based on one of the OFs in
 * this set as indicated by the OCP in the DODAG Configuration option
 * attached to a DIO.
 */
#define RPL_CONF_SUPPORTED_OFS { &rpl_driplof, &rpl_poof }

/*
 * Although the PO OF and DRiPL OF can both operate with multiple RPL
 * instances, the metric normalization logic is based on the current
 * DAG of the default instance only. Hence, it makes sense to allow
 * only a single instance. However, one could also change which instance
 * is default at runtime.
 */
#define RPL_CONF_MAX_INSTANCES  1

/*
 * When the per-interface inferred metric is only updated upon transmitting
 * a unicast packet towards a neighboring interface (as is the case with ETX),
 * we must force the use of DIS probes instead of DIO probes and make sure that
 * we send a DIS probe to all interfaces of a probing target, regardless of the
 * fact they're stale or not. This ensures that when a child probes a parent,
 * not only is IT able to update the inferred metric for all interfaces of said
 * parent, said parent may then also obtain an inferred metric update for all
 * interfaces towards said child. If, on the other hand, the child would only
 * probe the stale interfaces of its parent with a tx-only inferred metric,
 * then a situation could (and is likely to) occur wherein a child and parent
 * consider a different (but mutually available) interface type to be preferred.
 * This may result in the child probing a parent's interface which the parent
 * itself considers as preferred for the child and presumably (is likely to)
 * have up to date inferred metric information for, while another non-preferred
 * interface from the perspective of the parent (i.e. towards its child) is never
 * updated because it is not used for unicast transmission by the parent towards
 * the given child, nor is it probed because the child considers said interface
 * as preferred for unicast transmissions towards its parent and it is not likely
 * to ever become stale from the child's perspective. When not using a tx-only
 * inferred metric such as LQL, it is not required to send probes (of any kind)
 * to non-stale interfaces because the fact that they're not stale means that
 * the parent has sufficiently recently acknowledged a unicast transmission
 * from its child and thus both the child and its parent have an up-to-date
 * inferred metric for the given interface type. Even when a child and parent
 * consider a different (but mutually available) interface type to be preferred,
 * this still works. What's more it's not required to use DIS probes for 
 * non-tx-only metrics either, since they're also updated upon reception of a
 * packet. As opposed to a single-interface scenario, a parent is required to
 * have up to date metrics on all the interfaces of its children such that
 * it may choose a preferred interface for transmissions to said children.
 */
#if LINK_STATS_CONF_INTERFACES_WITH_ETX && (LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR > 1)
#define RPL_CONF_PROBING_SEND_FUNC(instance, addr)  dis_output((addr))
#define RPL_CONF_PROBING_STALE_INTERFACES_ONLY      0
#elif LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR == 1
#define RPL_CONF_PROBING_STALE_INTERFACES_ONLY      0
#endif

#else /* CONTIKI_TARGET_COOJA == 0 */
#if MAC_CONF_WITH_TWOFACED
#define LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR 2U
#define LINK_STATS_CONF_INTERFACES_WITH_ETX 1
#define LINK_STATS_CONF_WITH_WEIGHTS 1
#define TWOFACED_MAC_CONF_ACK_WAIT_TIME (RTIMER_SECOND / 500)
#define TWOFACED_MAC_CONF_AFTER_ACK_DETECTED_WAIT_TIME (RTIMER_SECOND / 2500)
#define NETSTACK_CONF_MAC twofaced_mac_driver
#else /* MAC_CONF_WITH_TWOFACED */
#define LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR 1U
#endif /* MAC_CONF_WITH_TWOFACED */

#define RPL_CONF_OF_OCP RPL_OCP_DRIPLOF
#define RPL_CONF_SUPPORTED_OFS { &rpl_driplof }
#define RPL_CONF_MAX_INSTANCES 1

#if LINK_STATS_CONF_INTERFACES_WITH_ETX && (LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR > 1)
#define RPL_CONF_PROBING_SEND_FUNC(instance, addr) dis_output((addr))
#define RPL_CONF_PROBING_STALE_INTERFACES_ONLY 0
#elif LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR == 1
#define RPL_CONF_PROBING_STALE_INTERFACES_ONLY 0
#endif /* LINK_STATS_CONF_NUM_INTERFACES_PER_NEIGHBOR == 1 */
#endif /* CONTIKI_TARGET_COOJA == 0 */

#define LOG_CONF_LEVEL_APP      LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RPL      LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_TCPIP    LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_IPV6     LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_6LOWPAN  LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_MAC      LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_FRAMER   LOG_LEVEL_NONE

#endif /* PROJECT_CONF_H_ */
