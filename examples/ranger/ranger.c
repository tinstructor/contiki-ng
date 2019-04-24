
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>

#include "contiki.h"
#include "sys/process.h"
#include "sys/etimer.h"
#include "sys/log.h"
#include "dev/gpio-hal.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "dev/radio.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "ranger-net.h"

#include "arch/dev/cc1200/cc1200-conf.h"
#include "arch/dev/cc1200/cc1200-rf-cfg.h"

#ifndef LOG_CONF_LEVEL_RANGER
#define LOG_CONF_LEVEL_RANGER LOG_LEVEL_NONE
#endif

#define LOG_MODULE "RANGER"
#define LOG_LEVEL LOG_CONF_LEVEL_RANGER

/*----------------------------------------------------------------------------*/
// Change these for your own use case
// Make sure that the interval is long enough to send and receive your messages

#define MAIN_INTERVAL_SECONDS   1
#define MAIN_INTERVAL           (CLOCK_SECOND * MAIN_INTERVAL_SECONDS)
#define CONTENT_SIZE            32
#define TX_POWER_DBM            14
#define CHANNEL                 26

#define ENABLE_SEND_PIN         0
#define UNIQUE_ID               UINT32_C(0x931de41a)
#define RX_RECEIVE_LEDS         LEDS_GREEN
#define TX_SEND_LEDS            LEDS_RED

/*----------------------------------------------------------------------------*/
PROCESS(ranger_process, "Ranger process");
AUTOSTART_PROCESSES(&ranger_process);

/*----------------------------------------------------------------------------*/
extern const cc1200_rf_cfg_t cc1200_868_fsk_1_2kbps;
extern const cc1200_rf_cfg_t cc1200_802154g_863_870_fsk_50kbps;
extern const cc1200_rf_cfg_t cc1200_868_4gfsk_1000kbps;

static const cc1200_rf_cfg_t *rf_cfg_ptrs[] = {&cc1200_868_fsk_1_2kbps, 
                                               &cc1200_802154g_863_870_fsk_50kbps,
                                               &cc1200_868_4gfsk_1000kbps};

enum {RF_CFG_AMOUNT = sizeof(rf_cfg_ptrs)/sizeof(rf_cfg_ptrs[0]),};

static uint8_t current_rf_cfg_index = 0;
static cc1200_rf_cfg_t current_rf_cfg;

extern gpio_hal_pin_t send_pin;
static process_event_t send_pin_event;
static gpio_hal_event_handler_t send_pin_event_handler;

static enum 
{
    RX,
    TX,
    MODE_AMOUNT,
} current_mode;

typedef struct
{
    char content[CONTENT_SIZE];
    uint32_t package_nr;
    uint32_t unique_id;
} message;

static struct etimer message_send_tmr;

static uint32_t package_nr_to_send = 0;
static int message_counter = 0;

// node labelled "gateway" has link-addr: 0012.4b00.09df.4dee
// static const linkaddr_t src_linkaddr = {{0x00, 0x12, 0x4b, 0x00, 0x09, 0xdf, 0x4d, 0xee}};

/*----------------------------------------------------------------------------*/

static void print_buffer(const char* buffer, int size, const char* specifier)
{
    if (LOG_LEVEL >= LOG_LEVEL_INFO)
    {
        for (int i = 0; i < size; i++)
        {
            LOG_OUTPUT(specifier, (unsigned char) buffer[i]);
        }
        LOG_OUTPUT("\n");
    }
}

static void send_message(const linkaddr_t* dest_addr)
{
    leds_on(TX_SEND_LEDS);

    LOG_INFO("Sending message to ");
    LOG_INFO_LLADDR(dest_addr);
    LOG_INFO("\n");

    message new_message;
    memset(new_message.content, 0, CONTENT_SIZE);
    strncpy(new_message.content, "hello world!", CONTENT_SIZE);
    new_message.package_nr = package_nr_to_send;
    new_message.unique_id = UNIQUE_ID;
    package_nr_to_send++;

    LOG_INFO("Message with payload length %d\n", sizeof(new_message));
    LOG_INFO("|-- Content (hex)  : ");
    print_buffer(new_message.content, CONTENT_SIZE, "%02X ");
    LOG_INFO("|-- Content (ascii): ");
    print_buffer(new_message.content, CONTENT_SIZE, "%2c ");
    LOG_INFO("\\-- Package number: %" PRIu32 "\n", new_message.package_nr);

    ranger_net_buf = (uint8_t*) &new_message;
    ranger_net_len = sizeof(new_message);

    NETSTACK_NETWORK.output(dest_addr);

    LOG_INFO("Message sent\n");

    leds_off(LEDS_ALL);
}

static void received_message(const message* current_message, uint16_t datalen)
{
    if (current_message->unique_id != UNIQUE_ID)
    {
        LOG_WARN("Received message with wrong unique id: got %" PRIx32 ", but expected %" PRIx32 ". Message ignored.\n",
                 current_message->unique_id, 
                 UNIQUE_ID);
        return;
    }

    leds_on(RX_RECEIVE_LEDS);

    message_counter++;

    LOG_INFO("Message\n");
    LOG_INFO("|-- Content (hex)  : ");
    print_buffer(current_message->content, CONTENT_SIZE, "%02X ");
    LOG_INFO("|-- Content (ascii): ");
    print_buffer(current_message->content, CONTENT_SIZE, "%2c ");
    LOG_INFO("|-- Package number: %" PRIu32 "\n", current_message->package_nr);

    int8_t rssi = (int8_t) packetbuf_attr(PACKETBUF_ATTR_RSSI);
    uint16_t lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);

    LOG_INFO("|-- RSSI: %" PRIi8 "\n", rssi);
    LOG_INFO("\\-- LQI: %" PRIu16 "\n", lqi);

    radio_result_t result = NETSTACK_RADIO.get_object(RADIO_PARAM_RF_CFG, &current_rf_cfg, 
                                                      sizeof(cc1200_rf_cfg_t));
    assert(result == RADIO_RESULT_OK);
    LOG_INFO("Current RF config descriptor: %s\n", current_rf_cfg.cfg_descriptor);

    printf("csv-log: %" PRIu32 ", %" PRIu16 ", %" PRIi8 "\n", current_message->package_nr, datalen, rssi);

    LOG_INFO("Total messages received: %d\n", message_counter);

    leds_off(LEDS_ALL);
}

static void received_ranger_net_message_callback(const void* data,
                                              uint16_t datalen,
                                              const linkaddr_t* src,
                                              const linkaddr_t* dest)
{
    LOG_INFO("Received message from ");
    LOG_INFO_LLADDR(src);
    LOG_INFO_(" to ");
    LOG_INFO_LLADDR(dest);
    LOG_INFO_(" with payload length %" PRIu16 "\n", datalen);

    received_message((message*) data, datalen);
}

static void toggle_mode(void)
{
    current_mode = (current_mode + 1) % MODE_AMOUNT;
    LOG_INFO("Switched to %s Mode\n", current_mode == RX ? "RX" : "TX");
    leds_off(LEDS_ALL);
}

static void set_tx_power(int tx_power)
{
    LOG_INFO("Setting TX power to %d dBm\n", tx_power);

    radio_value_t min_value = 0;
    radio_value_t max_value = 0;

    radio_result_t result = NETSTACK_RADIO.get_value(RADIO_CONST_TXPOWER_MAX, &max_value);
    assert(result == RADIO_RESULT_OK);

    result = NETSTACK_RADIO.get_value(RADIO_CONST_TXPOWER_MIN, &min_value);
    assert(result == RADIO_RESULT_OK);

    if (tx_power > max_value)
    {
        LOG_WARN("Requested TX power %d dBm is larger than maximum allowed TX power %d dBm.\n",
                 tx_power,
                 max_value);

        result = NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, max_value);
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("TX power set to maximum of %d dBm\n", max_value);
    }
    else if (tx_power < min_value)
    {
        LOG_WARN("Requested TX power %d dBm is less than minimum allowed TX power %d dBm.\n",
                 tx_power,
                 min_value);

        result = NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, min_value);
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("TX power set to minimum of %d dBm\n", min_value);
    }
    else
    {
        result = NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, tx_power);
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("TX power set to %d dBm\n", tx_power);
    }
}

static void set_channel(int channel)
{
    LOG_INFO("Changing channel to nr %d\n", channel);

    radio_value_t min_value = 0;
    radio_value_t max_value = 0;

    radio_result_t result = NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MAX, &max_value);
    assert(result == RADIO_RESULT_OK);

    result = NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MIN, &min_value);
    assert(result == RADIO_RESULT_OK);

    if (channel > max_value)
    {
        LOG_WARN("Requested channel nr %d is larger than maximum channel nr %d.\n", channel, max_value);

        result = NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, max_value);
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("Channel changed to maximum channel nr %d\n", max_value);
    }
    else if (channel < min_value)
    {
        LOG_WARN("Requested channel nr %d is less than minimum minimal channel nr %d.\n", channel, min_value);

        result = NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, min_value);
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("Channel changed to minimum channel nr %d\n", min_value);
    }
    else
    {
        result = NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel);
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("Channel changed to nr %d\n", channel);
    }
}

static void set_rf_cfg(int rf_cfg_index)
{
    radio_result_t result;

    if(rf_cfg_index > RF_CFG_AMOUNT)
    {
        LOG_WARN("Requested RF config index %d is larger than maximum RF config index %d.\n", 
                 rf_cfg_index, RF_CFG_AMOUNT);
        
        result = NETSTACK_RADIO.set_object(RADIO_PARAM_RF_CFG, rf_cfg_ptrs[RF_CFG_AMOUNT - 1],
                                           sizeof(cc1200_rf_cfg_t));
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("RF config index changed to maximum RF config index %d.\n", RF_CFG_AMOUNT - 1);
        LOG_INFO("New RF config has descriptor \"%s\".\n", rf_cfg_ptrs[RF_CFG_AMOUNT - 1]->cfg_descriptor);
    }
    else if(rf_cfg_index < 0)
    {
        LOG_WARN("Requested RF config index %d is lower than minimum RF config index %d.\n", 
                 rf_cfg_index, 0);
        
        result = NETSTACK_RADIO.set_object(RADIO_PARAM_RF_CFG, rf_cfg_ptrs[0],
                                           sizeof(cc1200_rf_cfg_t));
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("RF config index changed to minimum RF config index %d.\n", 0);
        LOG_INFO("New RF config has descriptor \"%s\".\n", rf_cfg_ptrs[0]->cfg_descriptor);
    }
    else
    {
        result = NETSTACK_RADIO.set_object(RADIO_PARAM_RF_CFG, rf_cfg_ptrs[rf_cfg_index],
                                           sizeof(cc1200_rf_cfg_t));
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("RF config index changed to %d.\n", rf_cfg_index);
        LOG_INFO("New RF config has descriptor \"%s\".\n", rf_cfg_ptrs[rf_cfg_index]->cfg_descriptor);
    }

    package_nr_to_send = 0;
    LOG_INFO("Package number of TX messages reset to 0 after RF config change.\n");
}

static void send_handler(gpio_hal_pin_mask_t pin_mask)
{
    process_post(&ranger_process, send_pin_event, NULL);
}

static void init_send_pin(void)
{ 
    gpio_hal_pin_cfg_t send_pin_cfg = GPIO_HAL_PIN_CFG_EDGE_RISING | GPIO_HAL_PIN_CFG_INT_ENABLE |
        GPIO_HAL_PIN_CFG_PULL_UP;

    send_pin_event = process_alloc_event();

    send_pin_event_handler.pin_mask = 0;
    send_pin_event_handler.handler = send_handler;

    gpio_hal_arch_pin_set_input(send_pin);
    gpio_hal_arch_pin_cfg_set(send_pin, send_pin_cfg);
    gpio_hal_arch_interrupt_enable(send_pin);
    send_pin_event_handler.pin_mask |= gpio_hal_pin_to_mask(send_pin);
    gpio_hal_register_handler(&send_pin_event_handler);
}

static void print_diagnostics(void)
{
    LOG_INFO("Device: %s\n", BOARD_STRING);
    LOG_INFO("Payload size: %zu byte(s)\n", sizeof(message));
    LOG_INFO("Transmission power: %d dBm\n", TX_POWER_DBM);
    LOG_INFO("Channel: %d\n", CHANNEL);
    LOG_INFO("Timer period: %d s\n", MAIN_INTERVAL_SECONDS);
}

/*----------------------------------------------------------------------------*/
PROCESS_THREAD(ranger_process, ev, data)
{
    button_hal_button_t *btn;
    static bool long_press_flag = false; // static storage class => retain value between yielding

    PROCESS_BEGIN();

    if (ENABLE_SEND_PIN)
    {
        init_send_pin();
    }
    
    //TODO: set current_rf_cfg_index to index of active rf_cfg at startup

    set_tx_power(TX_POWER_DBM);
    set_channel(CHANNEL);

    print_diagnostics();
    LOG_INFO("Started ranger process\n");

    leds_off(LEDS_ALL);

    ranger_net_set_input_callback(received_ranger_net_message_callback);

    current_mode = RX;
    LOG_INFO("Booted in RX Mode\n");

    etimer_set(&message_send_tmr, MAIN_INTERVAL);

    while (1)
    {
        PROCESS_YIELD();

        if (ev == button_hal_periodic_event)
        {
            LOG_INFO("Periodic button event\n");
            btn = (button_hal_button_t *)data;
            if (btn->press_duration_seconds == 5)
            {
                LOG_INFO("Pressed user button for 5 seconds\n");
                long_press_flag = true;
                toggle_mode();
            }
        }
        else if (ev == button_hal_release_event)
        {
            if (!long_press_flag)
            {
                btn = (button_hal_button_t *)data;
                if (btn == button_hal_get_by_id(BUTTON_HAL_ID_USER_BUTTON))
                {
                    LOG_INFO("Released user button\n");
                    set_rf_cfg(current_rf_cfg_index++ % RF_CFG_AMOUNT);
                    set_tx_power(TX_POWER_DBM);
                    set_channel(CHANNEL);
                }
            }
            else
            {
                long_press_flag = false;
            }
        }
        else if (ENABLE_SEND_PIN && ev == send_pin_event) 
        {
            send_message(&linkaddr_null);
        }
        else if (etimer_expired(&message_send_tmr))
        {
            if (current_mode == TX)
            {
                send_message(&linkaddr_null);
            }
            
            etimer_reset(&message_send_tmr);
        }
    }

    LOG_INFO("Done...\n");

    PROCESS_END();
}
