
#include "contiki.h"
#include "sys/log.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
#include "ranger-net.h"

#ifndef LOG_CONF_LEVEL_RANGER_NET
#define LOG_CONF_LEVEL_RANGER_NET LOG_LEVEL_NONE
#endif

#define LOG_MODULE "RANGER_NET"
#define LOG_LEVEL LOG_CONF_LEVEL_RANGER_NET

uint8_t *ranger_net_buf;
uint16_t ranger_net_len;
static ranger_net_input_callback current_callback = NULL;

static void ranger_net_init(void)
{
    LOG_INFO("init\n");
    current_callback = NULL;
}

static void ranger_net_input(void)
{
    if(current_callback != NULL) 
    {
        LOG_INFO("received %u bytes from ", packetbuf_datalen());
        LOG_INFO_LLADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
        LOG_INFO_("\n");

        current_callback(packetbuf_dataptr(), packetbuf_datalen(),

        packetbuf_addr(PACKETBUF_ADDR_SENDER), packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
    }
}

void ranger_net_set_input_callback(ranger_net_input_callback callback)
{
    current_callback = callback;
}

static uint8_t ranger_net_output(const linkaddr_t *dest)
{
    packetbuf_clear();
    packetbuf_copyfrom(ranger_net_buf, ranger_net_len);
    packetbuf_set_attr(PACKETBUF_ATTR_FRAME_TYPE, FRAME802154_DATAFRAME);

    if(dest != NULL) 
    {
        packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, dest);
    } 
    else 
    {
        packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &linkaddr_null);
    }

    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);

    LOG_INFO("sending %u bytes to ", packetbuf_datalen());
    LOG_INFO_LLADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
    LOG_INFO_("\n");

    NETSTACK_MAC.send(NULL, NULL);

    return 1;
}

const struct network_driver ranger_net_driver = {
    "ranger_net",
    ranger_net_init,
    ranger_net_input,
    ranger_net_output
};

