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
 *      Device-specific configuration: the twofaced-rf driver for zoul devices
 * \author
 *      Robbe Elsas <robbe.elsas@ugent.be>
 */

#ifndef TWOFACED_RF_H_
#define TWOFACED_RF_H_

#include "contiki.h"
#include "dev/radio.h"

#ifdef TWOFACED_RF_CONF_AVAILABLE_IFS
#define TWOFACED_RF_AVAILABLE_IFS TWOFACED_RF_CONF_AVAILABLE_IFS
#else /* TWOFACED_RF_CONF_AVAILABLE_IFS */
#define TWOFACED_RF_AVAILABLE_IFS { &cc2538_rf_driver, &cc1200_driver }
#define CC2538_CONF_INTERFACE_ID 0
#define CC1200_CONF_INTERFACE_ID 1
#endif /* TWOFACED_RF_CONF_AVAILABLE_IFS */

/* TODO add documentation for function prototypes */

/*---------------------------------------------------------------------------*/
/* Prototypes for radio driver functions */
/*---------------------------------------------------------------------------*/
static int init(void);
/*---------------------------------------------------------------------------*/
static int prepare(const void *payload, unsigned short payload_len);
/*---------------------------------------------------------------------------*/
static int transmit(unsigned short transmit_len);
/*---------------------------------------------------------------------------*/
static int send(const void *payload, unsigned short payload_len);
/*---------------------------------------------------------------------------*/
static int read(void *buf, unsigned short buf_len);
/*---------------------------------------------------------------------------*/
static int channel_clear(void);
/*---------------------------------------------------------------------------*/
static int receiving_packet(void);
/*---------------------------------------------------------------------------*/
static int pending_packet(void);
/*---------------------------------------------------------------------------*/
static int on(void);
/*---------------------------------------------------------------------------*/
static int off(void);
/*---------------------------------------------------------------------------*/
static radio_result_t get_value(radio_param_t param, radio_value_t *value);
/*---------------------------------------------------------------------------*/
static radio_result_t set_value(radio_param_t param, radio_value_t value);
/*---------------------------------------------------------------------------*/
static radio_result_t get_object(radio_param_t param, void *dest, size_t size);
/*---------------------------------------------------------------------------*/
static radio_result_t set_object(radio_param_t param, const void *src, size_t size);
/*---------------------------------------------------------------------------*/
static int lock_interface(void);
/*---------------------------------------------------------------------------*/
static void unlock_interface(void);
/*---------------------------------------------------------------------------*/
static int prepare_all(const void *payload, unsigned short payload_len);
/*---------------------------------------------------------------------------*/
static int transmit_all(unsigned short transmit_len);
/*---------------------------------------------------------------------------*/
static int send_all(const void *payload, unsigned short payload_len);
/*---------------------------------------------------------------------------*/
static int channel_clear_all(void);
/*---------------------------------------------------------------------------*/
static int receiving_packet_all(void);
/*---------------------------------------------------------------------------*/
static int pending_packet_all(void);

#endif /* TWOFACED_RF_H_ */
