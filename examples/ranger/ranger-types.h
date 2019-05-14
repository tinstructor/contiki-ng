#ifndef RANGER_TYPES_H_
#define RANGER_TYPES_H_

#include <inttypes.h>
#include "contiki.h"

#ifndef CONTENT_SIZE
#define CONTENT_SIZE 28
#endif

typedef enum
{
    DATA,
    CFG_REQ,
} ranger_message_t;

typedef struct
{
    uint8_t preamble_nibbles;
    uint16_t preamble_word;
} cc1200_preamble_t;

typedef uint32_t cc1200_symbol_rate_t;
typedef uint32_t cc1200_rx_filt_bw_t;

/**
 * @brief Representation of a crc polynomial and its initialisation vector.
 * 
 * A bit at a certain position in the crc_polynomial field is set to one if 
 * and only if the actual polynomial contains a term with an exponent equal
 * to the position of said bit plus 1. For example, consider the following
 * representation of a polynomial:
 * 
 * x^16 + x^15 + x^2 + 1
 * 
 * Accordingly, the crc_polynomial field would have the value:
 * 
 * 0b1100000000000010 = 0xC002
 * 
 * NOTE: positions in the crc_polynomial field start from 0.
 */
typedef struct
{
    uint16_t crc_polynomial;
    uint16_t init_vector;
} cc1200_crc_cfg_t;

typedef struct
{
    uint32_t unique_id;
    ranger_message_t message_type;
    union
    {
        struct
        {
            char content[CONTENT_SIZE];
            uint32_t package_nr;
        };
        struct
        {
            uint8_t rf_cfg_index;
            uint32_t request_id;
        };
    };
} message;

typedef struct
{
    uint8_t next_rf_cfg_index;
    clock_time_t rf_cfg_delay;
} rf_cfg_delay_t;

typedef enum
{
    RX,
    TX,
    MODE_AMOUNT,
} transceiver_mode_t;

#endif /* RANGER_TYPES_H_ */