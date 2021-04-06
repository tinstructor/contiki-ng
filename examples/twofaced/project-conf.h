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

#define NETSTACK_CONF_RADIO twofaced_rf_driver

#ifndef NETSTACK_CONF_RADIO
#define ZOUL_CONF_USE_CC1200_RADIO 1
#else /* NETSTACK_CONF_RADIO */
#if MAC_CONF_WITH_CSMA
#define CSMA_CONF_ACK_WAIT_TIME                 (RTIMER_SECOND / 200)
#define CSMA_CONF_AFTER_ACK_DETECTED_WAIT_TIME  (RTIMER_SECOND / 1500)
#endif /* MAC_CONF_WITH_CSMA */
#define REMOTE_DUAL_RF_ENABLED 1
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
#define RPL_CONF_OF_OCP RPL_OCP_MRHOF

/*
 * The RPL_CONF_SUPPORTED_OFS parameter configures the OFs supported by
 * any node (root or non-root) at runtime. A node may only join a RPL
 * instance (advertised in a DIO) which is based on one of the OFs in
 * this set as indicated by the OCP in the DODAG Configuration option
 * attached to a DIO.
 */
#define RPL_CONF_SUPPORTED_OFS { &rpl_mrhof, &rpl_poof, &rpl_driplof }

#define LOG_CONF_LEVEL_APP      LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RPL      LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_TCPIP    LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_IPV6     LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_6LOWPAN  LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_MAC      LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_FRAMER   LOG_LEVEL_NONE

#endif /* PROJECT_CONF_H_ */
