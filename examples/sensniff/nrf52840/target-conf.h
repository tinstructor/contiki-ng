/*
 * Copyright (c) 2016, George Oikonomou - http://www.spd.gr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
#ifndef TARGET_CONF_H_
#define TARGET_CONF_H_
/*---------------------------------------------------------------------------*/
/*
 * Selection of Sensniff I/O Interface.
 * Defaults to UART0 on the DK and USB on the dongle.
 * Set NRF52840_NATIVE_USB=1 when building to use USB as sensniff's interface.
 *
 * Don't forget to also set a correct baud rate (460800 or higher) by defining
 * the corresponding UART0_CONF_BAUD_RATE 
 */
#define NRF52840_IO_CONF_USB       NRF52840_NATIVE_USB
/*---------------------------------------------------------------------------*/
#if NRF52840_IO_CONF_USB == 0
#define UART0_CONF_BAUD_RATE       NRF_UART_BAUDRATE_460800
#endif
/*---------------------------------------------------------------------------*/
#define SENSNIFF_IO_DRIVER_H "pool/nrf52840-io.h"
/*---------------------------------------------------------------------------*/
#endif /* TARGET_CONF_H_ */
/*---------------------------------------------------------------------------*/
