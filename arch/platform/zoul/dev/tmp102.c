/*
 * Copyright (c) 2015, Zolertia - http://www.zolertia.com
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
 */
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup zoul-tmp102-sensor
 * @{
 *
 * \file
 *  Driver for the TMP102 temperature sensor
 */
/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdbool.h>
#include "contiki.h"
#include "dev/i2c.h"
#include "tmp102.h"

void
tmp102_init(void)
{
  i2c_init(I2C_SDA_PORT, I2C_SDA_PIN, I2C_SCL_PORT, I2C_SCL_PIN, I2C_SCL_NORMAL_BUS_SPEED);
}
/*---------------------------------------------------------------------------*/

uint8_t
tmp102_read(int16_t *data)
{
  uint8_t buf[2] = {0,0};
  uint16_t MSB = 0;
  uint16_t LSB = 0;
  uint16_t u_temp = 0;
  bool is_negative = false;
  int16_t s_temp = 0;

  /* Write to the temperature register to trigger a reading */
  if(i2c_single_send(TMP102_ADDR, TMP102_TEMP) == I2C_MASTER_ERR_NONE) {
    /* Read two bytes only */
    if(i2c_burst_receive(TMP102_ADDR, buf, 2) == I2C_MASTER_ERR_NONE) {
      /* 12-bit value, TMP102 SBOS397F Table 8-9 */
      /* NOTE: Before the actual bit-shift, integer promotions are performed 
         on both operands. The value of the right operand musn't be negative, 
         and must be less than the width of the left operand after integer
         promotion. Otherwise, the program’s behavior is undefined. */
      MSB = buf[0] << 4;
      LSB = buf[1] >> 4;
      u_temp = MSB + LSB;

      is_negative = (u_temp & (1 << 12)) != 0;
      if (is_negative) {
        s_temp = u_temp | ~((1 << 12) - 1);
        MSB = s_temp & ~0x000F;
        LSB = s_temp & ~0xFFF0;
      } else {
        s_temp = u_temp;
      }

      if (LSB != 0) {
        s_temp *= (0.0625/LSB);
      } else {
        s_temp *= 0.0625;
      }

      *data = ((-1)*is_negative)*s_temp;
      return I2C_MASTER_ERR_NONE;
    }
  }
  return i2c_master_error();
}
/*---------------------------------------------------------------------------*/
/** @} */
