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
    CFG_ACK,
    CFG_ERQ,
} ranger_message_t;

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
    clock_time_t handshake_delay;
} handshake_delay_t;

typedef enum
{
    RX,
    TX,
    MODE_AMOUNT,
} transceiver_mode_t;

#endif /* RANGER_TYPES_H_ */