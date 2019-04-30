
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#define NDEBUG
#include <assert.h>

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
#define CONTENT_SIZE            28
#define TX_POWER_DBM            14
#define CHANNEL                 26

#define ENABLE_CFG_HANDSHAKE    1
#define ENABLE_SEND_PIN         0
#define UNIQUE_ID               UINT32_C(0x30695444)
#define RX_RECEIVE_LEDS         LEDS_GREEN
#define TX_SEND_LEDS            LEDS_RED

/*----------------------------------------------------------------------------*/
PROCESS(ranger_process, "Ranger process");
PROCESS(handshake_delay_process, "Handshake delay process");
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
static uint8_t next_rf_cfg_index = 0;
static const cc1200_rf_cfg_t *current_rf_cfg = &CC1200_CONF_RF_CFG;

extern gpio_hal_pin_t send_pin;
static process_event_t send_pin_event;
static gpio_hal_event_handler_t send_pin_event_handler;

typedef enum
{
    DATA,
    CFG_REQ,
    CFG_ACK,
    CFG_ERQ,
} ranger_message_t;

static enum 
{
    RX,
    TX,
    MODE_AMOUNT,
} current_mode;

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

static const message empty_message;

static struct etimer message_send_tmr;
static struct ctimer handshake_delay_tmr;

static process_event_t handshake_delay_event = 0;

static uint32_t package_nr_to_send = 0;
static int message_counter = 0;

static const linkaddr_t empty_linkaddr;
// node labelled "gateway" has link-addr: 0012.4b00.09df.4dee
// static const linkaddr_t src_linkaddr = {{0x00, 0x12, 0x4b, 0x00, 0x09, 0xdf, 0x4d, 0xee}};

/*----------------------------------------------------------------------------*/

static void print_buffer(const char* buffer, int size, const char* specifier);
static void send_message(const linkaddr_t* dest_addr, ranger_message_t message_type, ...);
static void received_ranger_net_message_callback(const void* data, uint16_t datalen,
                                                 const linkaddr_t* src,
                                                 const linkaddr_t* dest);
static void toggle_mode(void);
static void set_tx_power(int tx_power);
static void set_channel(int channel);
static void set_rf_cfg(int rf_cfg_index);
static void send_handler(gpio_hal_pin_mask_t pin_mask);
static void init_send_pin(void);
static void print_diagnostics(void);
static void handshake_delay_callback(void *ptr);

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

static void send_message(const linkaddr_t* dest_addr, ranger_message_t message_type, ...)
{
    va_list argptr;
    va_start(argptr, message_type);

    leds_on(TX_SEND_LEDS);

    LOG_INFO("Sending message to ");
    LOG_INFO_LLADDR(dest_addr);
    print_buffer(NULL, 0, NULL); // abuse of print_buffer() to print newline

    message new_message = empty_message;
    new_message.unique_id = UNIQUE_ID;
    new_message.message_type = message_type;

    switch (message_type)
    {
        case DATA:
            {
                memset(new_message.content, 0, CONTENT_SIZE);
                strncpy(new_message.content, "hello world!", CONTENT_SIZE);
                new_message.package_nr = package_nr_to_send;
                package_nr_to_send++;

                LOG_INFO("Data message with payload length %d\n", sizeof(new_message));
                LOG_INFO("|-- Content (hex)  : ");
                print_buffer(new_message.content, CONTENT_SIZE, "%02X ");
                LOG_INFO("|-- Content (ascii): ");
                print_buffer(new_message.content, CONTENT_SIZE, "%2c ");
                LOG_INFO("\\-- Package number: %" PRIu32 "\n", new_message.package_nr);
            }
            break;
        case CFG_REQ:
            {
                new_message.rf_cfg_index = va_arg(argptr, int);
                new_message.request_id = va_arg(argptr, unsigned int);

                LOG_INFO("Configuration request with payload length %d\n", sizeof(new_message));
                LOG_INFO("|-- Current configuration index: %" PRIu8 "\n", current_rf_cfg_index);
                LOG_INFO("|-- Requested configuration index: %" PRIu8 "\n", new_message.rf_cfg_index);
                LOG_INFO("\\-- ID of request: %" PRIu32 "\n", new_message.request_id);

            }
            break;
        case CFG_ACK:
            {
                new_message.rf_cfg_index = va_arg(argptr, int);
                new_message.request_id = va_arg(argptr, unsigned int);

                LOG_INFO("Configuration acknowledgement with payload length %d\n", sizeof(new_message));
                LOG_INFO("|-- Current configuration index: %" PRIu8 "\n", current_rf_cfg_index);
                LOG_INFO("|-- Acknowledged configuration index: %" PRIu8 "\n", new_message.rf_cfg_index);
                LOG_INFO("\\-- ID of request: %" PRIu32 "\n", new_message.request_id);
            }
            break;
        case CFG_ERQ:
            {
                new_message.rf_cfg_index = va_arg(argptr, int);
                new_message.request_id = va_arg(argptr, unsigned int);

                LOG_INFO("Configuration end of request with payload length %d\n", sizeof(new_message));
                LOG_INFO("\\-- ID of request: %" PRIu32 "\n", new_message.request_id);
            }
            break;
        default:
            break;
    }

    ranger_net_buf = (uint8_t*) &new_message;
    ranger_net_len = sizeof(new_message);

    NETSTACK_NETWORK.output(dest_addr);

    LOG_INFO("Message sent\n");

    leds_off(LEDS_ALL);

    va_end(argptr);
}

static void received_ranger_net_message_callback(const void* data,
                                              uint16_t datalen,
                                              const linkaddr_t* src,
                                              const linkaddr_t* dest)
{
    linkaddr_t src_addr = empty_linkaddr;
    linkaddr_t dest_addr = empty_linkaddr;

    linkaddr_copy(&src_addr, src);
    linkaddr_copy(&dest_addr,dest);
    
    LOG_INFO("Received message from ");
    LOG_INFO_LLADDR(&src_addr);
    LOG_INFO_(" to ");
    LOG_INFO_LLADDR(&dest_addr);
    LOG_INFO_(" with payload length %" PRIu16 "\n", datalen);

    message current_message = empty_message;
    memcpy(&current_message, data, sizeof(message));

    if (current_message.unique_id != UNIQUE_ID)
    {
        LOG_WARN("Received message with wrong unique id: got %" PRIx32 ", but expected %" PRIx32 ". Message ignored.\n",
                 current_message.unique_id, 
                 UNIQUE_ID);
        return;
    }

    leds_on(RX_RECEIVE_LEDS);

    message_counter++;
    
    switch (current_message.message_type)
    {
        case DATA:
            {
                LOG_INFO("Data message\n");
                LOG_INFO("|-- Content (hex)  : ");
                print_buffer(current_message.content, CONTENT_SIZE, "%02X ");
                LOG_INFO("|-- Content (ascii): ");
                print_buffer(current_message.content, CONTENT_SIZE, "%2c ");
                LOG_INFO("|-- Package number: %" PRIu32 "\n", current_message.package_nr);

                int8_t rssi = (int8_t) packetbuf_attr(PACKETBUF_ATTR_RSSI);
                uint16_t lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);

                LOG_INFO("|-- RSSI: %" PRIi8 "\n", rssi);
                LOG_INFO("\\-- LQI: %" PRIu16 "\n", lqi);

                // Is it a good idea to call the radio here? Takes valuable time!
                // radio_result_t result = NETSTACK_RADIO.get_object(RADIO_PARAM_RF_CFG, current_rf_cfg, 
                //                                                   sizeof(cc1200_rf_cfg_t));
                // assert(result == RADIO_RESULT_OK);
                // LOG_INFO("Current RF config descriptor: %s\n", current_rf_cfg->cfg_descriptor);

                printf("csv-log: %" PRIu32 ", %" PRIu16 ", %" PRIi8 "\n", current_message.package_nr, datalen, rssi);
            }
            break;
        case CFG_REQ:
            {
                LOG_INFO("Configuration request\n");
                LOG_INFO("|-- Current configuration index: %" PRIu8 "\n", current_rf_cfg_index);
                LOG_INFO("|-- Requested configuration index: %" PRIu8 "\n", current_message.rf_cfg_index);
                LOG_INFO("\\-- ID of request: %" PRIu32 "\n", current_message.request_id);

                send_message(&src_addr, CFG_ACK, (int)current_message.rf_cfg_index, 
                            (unsigned int)current_message.request_id);
            }
            break;
        case CFG_ACK:
            {
                LOG_INFO("Configuration acknowledgement\n");
                LOG_INFO("|-- Acknowledged configuration index: %" PRIu8 "\n", current_message.rf_cfg_index);
                LOG_INFO("\\-- ID of request: %" PRIu32 "\n", current_message.request_id);

                send_message(&src_addr, CFG_ERQ, (int)current_message.rf_cfg_index, 
                            (unsigned int)current_message.request_id);
            }
            break;
        case CFG_ERQ:
            {
                LOG_INFO("Configuration end of request\n");
                LOG_INFO("\\-- ID of request: %" PRIu32 "\n", current_message.request_id);

                next_rf_cfg_index = current_message.rf_cfg_index;
                LOG_INFO("Huh what?\n");
                process_post(&handshake_delay_process, handshake_delay_event, 
                             &next_rf_cfg_index);
            }
            break;
        default:
            break;
    }

    LOG_INFO("Total messages received: %d\n", message_counter);

    leds_off(LEDS_ALL);
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

    if(rf_cfg_index >= RF_CFG_AMOUNT)
    {
        LOG_WARN("Requested RF config index %d is larger than maximum RF config index %d.\n", 
                 rf_cfg_index, RF_CFG_AMOUNT - 1);
        
        result = NETSTACK_RADIO.set_object(RADIO_PARAM_RF_CFG, rf_cfg_ptrs[RF_CFG_AMOUNT - 1],
                                           sizeof(cc1200_rf_cfg_t));
        assert(result == RADIO_RESULT_OK);
        current_rf_cfg_index = RF_CFG_AMOUNT - 1;
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
        current_rf_cfg_index = 0;
        LOG_INFO("RF config index changed to minimum RF config index %d.\n", 0);
        LOG_INFO("New RF config has descriptor \"%s\".\n", rf_cfg_ptrs[0]->cfg_descriptor);
    }
    else
    {
        result = NETSTACK_RADIO.set_object(RADIO_PARAM_RF_CFG, rf_cfg_ptrs[rf_cfg_index],
                                           sizeof(cc1200_rf_cfg_t));
        assert(result == RADIO_RESULT_OK);
        current_rf_cfg_index = rf_cfg_index;
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
    LOG_INFO("Current RF config index: %" PRIu8 "\n", current_rf_cfg_index);
}

static void handshake_delay_callback(void *ptr)
{
    set_rf_cfg(*(uint8_t *)ptr);
    set_tx_power(TX_POWER_DBM);
    set_channel(CHANNEL);
}

/*----------------------------------------------------------------------------*/
PROCESS_THREAD(ranger_process, ev, data)
{
    button_hal_button_t *btn;
    static bool long_press_flag = false; // static storage class => retain value between yielding

    PROCESS_BEGIN();

    #if ENABLE_SEND_PIN
    init_send_pin();
    #endif
    
    bool found_rf_cfg_index = false;
    for(size_t i = 0; i < RF_CFG_AMOUNT && !found_rf_cfg_index; i++)
    {
        if (current_rf_cfg->cfg_descriptor == rf_cfg_ptrs[i]->cfg_descriptor
            && current_rf_cfg->register_settings == rf_cfg_ptrs[i]->register_settings) 
        {
            current_rf_cfg_index = i;
            found_rf_cfg_index = true;
        }
    }
    
    set_tx_power(TX_POWER_DBM);
    set_channel(CHANNEL);

    print_diagnostics();
    LOG_INFO("Started ranger process\n");

    handshake_delay_event = process_alloc_event();
    process_start(&handshake_delay_process, NULL);

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

                    #if ENABLE_CFG_HANDSHAKE
                    send_message(&linkaddr_null, CFG_REQ, 
                                 (int)((current_rf_cfg_index + 1) % RF_CFG_AMOUNT), 
                                 (unsigned int)UNIQUE_ID);
                                 
                    next_rf_cfg_index = (current_rf_cfg_index + 1) % RF_CFG_AMOUNT;
                    process_post(&handshake_delay_process, handshake_delay_event, 
                                 &next_rf_cfg_index);
                    #else
                    set_rf_cfg((current_rf_cfg_index + 1) % RF_CFG_AMOUNT);
                    set_tx_power(TX_POWER_DBM);
                    set_channel(CHANNEL);
                    #endif
                }
            }
            else
            {
                long_press_flag = false;
            }
        }
        #if ENABLE_SEND_PIN
        else if (ev == send_pin_event) 
        {
            send_message(&linkaddr_null, DATA);
        }
        #endif
        else if (etimer_expired(&message_send_tmr))
        {
            if (current_mode == TX)
            {
                send_message(&linkaddr_null, DATA);
            }
            
            etimer_reset(&message_send_tmr);
        }
    }

    LOG_INFO("Ranger process done...\n");

    PROCESS_END();
}

PROCESS_THREAD(handshake_delay_process, ev, data)
{
    PROCESS_BEGIN();

    LOG_INFO("Started handshake delay process\n");

    while(1) 
    {
        PROCESS_YIELD();
        if(ev == handshake_delay_event)
        {
            LOG_INFO("Handshake delay event was triggered!\n");
            ctimer_set(&handshake_delay_tmr, CLOCK_SECOND, 
                       handshake_delay_callback, data);
        }
    }

    LOG_INFO("Handshake delay process done...\n");

    PROCESS_END();
}
