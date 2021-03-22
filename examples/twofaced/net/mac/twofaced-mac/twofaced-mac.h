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
 *      A MAC protocol configuration that works together with DRiPL and PO
 * \author
 *      Robbe Elsas <robbe.elsas@ugent.be>
 */

#ifndef TWOFACED_MAC_H_
#define TWOFACED_MAC_H_

#include "contiki.h"
#include "net/mac/mac.h"
#include "dev/radio.h"

/* TODO add documentation for function prototypes */

/*---------------------------------------------------------------------------*/
/* Prototypes for mac driver functions */
/*---------------------------------------------------------------------------*/
static void init(void);
/*---------------------------------------------------------------------------*/
static void send(mac_callback_t sent_callback, void *ptr);
/*---------------------------------------------------------------------------*/
static void input(void);
/*---------------------------------------------------------------------------*/
static int on(void);
/*---------------------------------------------------------------------------*/
static int off(void);
/*---------------------------------------------------------------------------*/
static int max_payload(void);
/*---------------------------------------------------------------------------*/
/* Prototypes for internal mac driver functions */
/*---------------------------------------------------------------------------*/
/* NOTE add internal radio driver function prototypes here as required */

#endif /* TWOFACED_MAC_H_ */