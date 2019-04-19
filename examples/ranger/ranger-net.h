#ifndef RANGER_NET_H_
#define RANGER_NET_H_

#include "contiki.h"
#include "net/linkaddr.h"

extern uint8_t *ranger_net_buf;
extern uint16_t ranger_net_len;

typedef void (* ranger_net_input_callback)(const void *data, uint16_t len, 
  const linkaddr_t *src, const linkaddr_t *dest);

void ranger_net_set_input_callback(ranger_net_input_callback callback);

#endif /* RANGER_NET_H_ */