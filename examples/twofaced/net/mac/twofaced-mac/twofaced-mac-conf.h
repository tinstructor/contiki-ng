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

#ifndef TWOFACED_MAC_CONF_H_
#define TWOFACED_MAC_CONF_H_

#include "contiki.h"
#include "net/mac/mac.h"
#include "dev/radio.h"
#include "net/queuebuf.h"

#ifdef TWOFACED_MAC_CONF_ACK_WAIT_TIME
#define TWOFACED_MAC_ACK_WAIT_TIME TWOFACED_MAC_CONF_ACK_WAIT_TIME
#else /* TWOFACED_MAC_CONF_ACK_WAIT_TIME */
#define TWOFACED_MAC_ACK_WAIT_TIME RTIMER_SECOND / 2500
#endif /* TWOFACED_MAC_CONF_ACK_WAIT_TIME */

#ifdef TWOFACED_MAC_CONF_AFTER_ACK_DETECTED_WAIT_TIME
#define TWOFACED_MAC_AFTER_ACK_DETECTED_WAIT_TIME TWOFACED_MAC_CONF_AFTER_ACK_DETECTED_WAIT_TIME
#else /* TWOFACED_MAC_CONF_AFTER_ACK_DETECTED_WAIT_TIME */
#define TWOFACED_MAC_AFTER_ACK_DETECTED_WAIT_TIME RTIMER_SECOND / 1500
#endif /* TWOFACED_MAC_CONF_AFTER_ACK_DETECTED_WAIT_TIME */

#define TWOFACED_MAC_ACK_LEN 3

/* macMinBE */
#ifdef TWOFACED_MAC_CONF_MIN_BE
#define TWOFACED_MAC_MIN_BE TWOFACED_MAC_CONF_MIN_BE
#else /* TWOFACED_MAC_CONF_MIN_BE */
#define TWOFACED_MAC_MIN_BE 3
#endif /* TWOFACED_MAC_CONF_MIN_BE */

/* macMaxBE */
#ifdef TWOFACED_MAC_CONF_MAX_BE
#define TWOFACED_MAC_MAX_BE TWOFACED_MAC_CONF_MAX_BE
#else /* TWOFACED_MAC_CONF_MAX_BE */
#define TWOFACED_MAC_MAX_BE 5
#endif /* TWOFACED_MAC_CONF_MAX_BE */

/* macMaxTWOFACEDBackoffs */
#ifdef TWOFACED_MAC_CONF_MAX_BACKOFF
#define TWOFACED_MAC_MAX_BACKOFF TWOFACED_MAC_CONF_MAX_BACKOFF
#else /* TWOFACED_MAC_CONF_MAX_BACKOFF */
#define TWOFACED_MAC_MAX_BACKOFF 5
#endif /* TWOFACED_MAC_CONF_MAX_BACKOFF */

/* macMaxFrameRetries */
#ifdef TWOFACED_MAC_CONF_MAX_FRAME_RETRIES
#define TWOFACED_MAC_MAX_FRAME_RETRIES TWOFACED_MAC_CONF_MAX_FRAME_RETRIES
#else /* TWOFACED_MAC_CONF_MAX_FRAME_RETRIES */
#define TWOFACED_MAC_MAX_FRAME_RETRIES 7
#endif /* TWOFACED_MAC_CONF_MAX_FRAME_RETRIES */

/* The maximum number of co-existing neighbor queues */
#ifdef TWOFACED_MAC_CONF_MAX_NEIGHBOR_QUEUES
#define TWOFACED_MAC_MAX_NEIGHBOR_QUEUES TWOFACED_MAC_CONF_MAX_NEIGHBOR_QUEUES
#else /* TWOFACED_MAC_CONF_MAX_NEIGHBOR_QUEUES */
#define TWOFACED_MAC_MAX_NEIGHBOR_QUEUES 2
#endif /* TWOFACED_MAC_CONF_MAX_NEIGHBOR_QUEUES */

/* The maximum number of pending packets per neighbor */
#ifdef TWOFACED_MAC_CONF_MAX_PACKET_PER_NEIGHBOR
#define TWOFACED_MAC_MAX_PACKET_PER_NEIGHBOR TWOFACED_MAC_CONF_MAX_PACKET_PER_NEIGHBOR
#else /* TWOFACED_MAC_CONF_MAX_PACKET_PER_NEIGHBOR */
#define TWOFACED_MAC_MAX_PACKET_PER_NEIGHBOR MAX_QUEUED_PACKETS
#endif /* TWOFACED_MAC_CONF_MAX_PACKET_PER_NEIGHBOR */

#ifdef TWOFACED_CONF_SEND_SOFT_ACK
#define TWOFACED_SEND_SOFT_ACK TWOFACED_CONF_SEND_SOFT_ACK
#else /* TWOFACED_CONF_SEND_SOFT_ACK */
#define TWOFACED_SEND_SOFT_ACK 0
#endif /* TWOFACED_CONF_SEND_SOFT_ACK */

#endif /* TWOFACED_MAC_CONF_H_ */