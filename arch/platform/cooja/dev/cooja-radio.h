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

#ifndef COOJA_RADIO_H_
#define COOJA_RADIO_H_

#include "contiki.h"
#include "dev/radio.h"

#ifdef COOJA_CONF_SIMULATE_TURNAROUND
#define COOJA_SIMULATE_TURNAROUND COOJA_CONF_SIMULATE_TURNAROUND
#else
#define COOJA_SIMULATE_TURNAROUND (!(MAC_CONF_WITH_TSCH))
#endif

#ifdef COOJA_CONF_TRANSMIT_ON_CCA
#define COOJA_TRANSMIT_ON_CCA COOJA_CONF_TRANSMIT_ON_CCA
#else
#define COOJA_TRANSMIT_ON_CCA 1
#endif

#ifdef COOJA_CONF_WITH_TWOFACED
#define COOJA_WITH_TWOFACED COOJA_CONF_WITH_TWOFACED
#else
#define COOJA_WITH_TWOFACED MAC_CONF_WITH_TWOFACED
// #define COOJA_WITH_TWOFACED 1
#endif

#if COOJA_WITH_TWOFACED
#ifdef COOJA_CONF_PRIMARY_IF_ID
#define COOJA_PRIMARY_IF_ID   COOJA_CONF_PRIMARY_IF_ID
#else
#define COOJA_PRIMARY_IF_ID   1
#endif

#ifdef COOJA_CONF_SECONDARY_IF_ID
#define COOJA_SECONDARY_IF_ID COOJA_CONF_SECONDARY_IF_ID
#else
#define COOJA_SECONDARY_IF_ID 2
#endif

#ifdef COOJA_CONF_PRIMARY_IF_DR
#define COOJA_PRIMARY_IF_DR   COOJA_CONF_PRIMARY_IF_DR
#else
#define COOJA_PRIMARY_IF_DR   250
#endif

#ifdef COOJA_CONF_SECONDARY_IF_DR
#define COOJA_SECONDARY_IF_DR COOJA_CONF_SECONDARY_IF_DR
#else
#define COOJA_SECONDARY_IF_DR 100
#endif
#endif

extern const struct radio_driver cooja_radio_driver;

/**
 * Set radio channel.
 */
void
radio_set_channel(int channel);

/**
 * Set transmission power of transceiver.
 *
 * \param p The power of the transceiver, between 1 and 100.
 */
void
radio_set_txpower(unsigned char p);

/**
 * The signal strength of the last received packet
 */
int
radio_signal_strength_last(void);

/**
 * This current signal strength.
 */
int
radio_signal_strength_current(void);

/**
 * Link quality indicator of last received packet.
 */
int
radio_LQI(void);

#if COOJA_WITH_TWOFACED
/**
 * Set twofaced radio channel.
 */
void
radio_set_channel_twofaced(int channel);

/**
 * Set transmission power of twofaced transceiver.
 *
 * \param p The power of the twofaced transceiver, between 1 and 100.
 */
void
radio_set_txpower_twofaced(unsigned char p);

/**
 * The signal strength of the last received packet on the twofaced transceiver.
 */
int
radio_signal_strength_last_twofaced(void);

/**
 * This current signal strength on the twofaced transceiver.
 */
int
radio_signal_strength_current_twofaced(void);

/**
 * Link quality indicator of last received packet on the twofaced transceiver.
 */
int
radio_LQI_twofaced(void);
#endif

#endif /* COOJA_RADIO_H_ */
