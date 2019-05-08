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
#include "dev/rgb-led/rgb-led.h"
#include "dev/radio.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "random.h"
#include "dev/uart.h"

#include "arch/dev/cc1200/cc1200-conf.h"
#include "arch/dev/cc1200/cc1200-rf-cfg.h"

#include "ranger-net.h"
#include "ranger-constants.h"
#include "ranger-types.h"
#include "ranger-functions.h"

#ifndef LOG_CONF_LEVEL_RANGER
#define LOG_CONF_LEVEL_RANGER LOG_LEVEL_NONE
#endif

#define LOG_MODULE "RANGER"
#define LOG_LEVEL LOG_CONF_LEVEL_RANGER

/*----------------------------------------------------------------------------*/

PROCESS(ranger_process, "Ranger process");
PROCESS(handshake_delay_process, "Handshake delay process");
PROCESS(auto_measure_process, "Automated measurement process");
// PROCESS(led_process, "Led process");
AUTOSTART_PROCESSES(&ranger_process);

/*----------------------------------------------------------------------------*/

// static uint8_t current_rf_cfg_led_color;
// static uint8_t current_rx_tx_led_color;
static uint8_t current_rf_cfg_index;
static handshake_delay_t current_handshake_delay;
static const cc1200_rf_cfg_t *current_rf_cfg = &CC1200_CONF_RF_CFG; //NOTE: pointer to const != constant

extern gpio_hal_pin_t send_pin;
static process_event_t send_pin_event;
static gpio_hal_event_handler_t send_pin_event_handler;

static transceiver_mode_t current_mode;

static struct etimer message_send_tmr;
static struct etimer handshake_delay_tmr;
static struct etimer auto_measure_tmr;
// static struct etimer led_off_tmr;

static process_event_t handshake_delay_event;
static process_event_t reset_mode_event;
static process_event_t auto_measure_event;
// static process_event_t led_event;

static uint32_t package_nr_to_send;
static int message_counter;

static bool reset_mode_flag = false;

static uint32_t current_request_id;

#if ENABLE_UART_INPUT
static button_hal_button_t fake_button_press;
#endif

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

//TODO: return an error when arguments are invalid
static void send_message(const linkaddr_t* dest_addr, ranger_message_t message_type, ...)
{
    va_list argptr;
    va_start(argptr, message_type);

    // current_rx_tx_led_color = TX_SEND_LED;
    // process_post(&led_process, led_event, &current_rx_tx_led_color);
    rgb_led_set(TX_SEND_LED);

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

    // process_post(&led_process, led_event, &current_rf_cfg_led_color);
    rgb_led_off();

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

    //REVIEW: see if this operation has much influence on performance
    message current_message = empty_message;
    memcpy(&current_message, data, sizeof(message));

    if (current_message.unique_id != UNIQUE_ID)
    {
        LOG_WARN("Received message with wrong unique id: got %" PRIx32 ", but expected %" PRIx32 ". Message ignored.\n",
                 current_message.unique_id, 
                 UNIQUE_ID);
        return;
    }

    // current_rx_tx_led_color = RX_RECEIVE_LED;
    // process_post(&led_process, led_event, &current_rx_tx_led_color);
    rgb_led_set(RX_RECEIVE_LED);

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

                int16_t rssi = (int16_t) packetbuf_attr(PACKETBUF_ATTR_RSSI);
                uint16_t lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);

                LOG_INFO("|-- RSSI: %" PRIi16 "\n", rssi);
                LOG_INFO("\\-- LQI: %" PRIu16 "\n", lqi);

                printf("Current RF config descriptor: %s\n", current_rf_cfg->cfg_descriptor);

                //TODO: add info to csv-log and adapt analyzer.py to work with new log
                printf("csv-log: %" PRIu32 ", %" PRIu16 ", %" PRIi16 "\n", current_message.package_nr, datalen, rssi);
            }
            break;
        case CFG_REQ:
            {
                LOG_INFO("Configuration request\n");
                LOG_INFO("|-- Current configuration index: %" PRIu8 "\n", current_rf_cfg_index);
                LOG_INFO("|-- Requested configuration index: %" PRIu8 "\n", current_message.rf_cfg_index);
                LOG_INFO("\\-- ID of request: %" PRIu32 "\n", current_message.request_id);

                if (current_message.request_id != current_request_id) 
                {
                    //REVIEW: do we keep the ACK messages in case of async?
                    //FIXME: trying to send when messages are incoming may trip the watchdog
                    // send_message(&src_addr, CFG_ACK, (int)current_message.rf_cfg_index, 
                    //              (unsigned int)current_message.request_id);

                    //REVIEW: do we put the rf cfg here and make everything async?
                    current_handshake_delay = empty_handshake_delay;
                    current_handshake_delay.next_rf_cfg_index = current_message.rf_cfg_index;
                    current_handshake_delay.handshake_delay = HANDSHAKE_RX_DELAY;
                    process_post(&handshake_delay_process, handshake_delay_event, 
                                 &current_handshake_delay);
                }

                current_request_id = current_message.request_id;
            }
            break;
        case CFG_ACK:
            {
                LOG_INFO("Configuration acknowledgement\n");
                LOG_INFO("|-- Acknowledged configuration index: %" PRIu8 "\n", current_message.rf_cfg_index);
                LOG_INFO("\\-- ID of request: %" PRIu32 "\n", current_message.request_id);

                //REVIEW: do we keep the ERQ messages in case of async?
                // if (current_message.request_id == current_request_id)
                // {
                //     send_message(&src_addr, CFG_ERQ, (int)current_message.rf_cfg_index, 
                //                  (unsigned int)current_message.request_id);
                // }
            }
            break;
        case CFG_ERQ:
            {
                LOG_INFO("Configuration end of request\n");
                LOG_INFO("\\-- ID of request: %" PRIu32 "\n", current_message.request_id);
            }
            break;
        default:
            break;
    }

    LOG_INFO("Total messages received: %d\n", message_counter);

    // process_post(&led_process, led_event, &current_rf_cfg_led_color);
    rgb_led_off();
}

#if ENABLE_UART_INPUT
static int uart_byte_input_callback(unsigned char input)
{
    switch (input)
    {
        case 't':
            {
                memcpy(&fake_button_press, button_hal_get_by_id(BUTTON_HAL_ID_USER_BUTTON), 
                       sizeof(button_hal_button_t));
                fake_button_press.press_duration_seconds = 5;
                LOG_INFO("Fake button press triggered by pressing t key.\n");
                process_post(&ranger_process, button_hal_periodic_event, &fake_button_press);
            }
            break;
        case 'p':
            {
                memcpy(&fake_button_press, button_hal_get_by_id(BUTTON_HAL_ID_USER_BUTTON), 
                       sizeof(button_hal_button_t));
                LOG_INFO("Fake button press triggered by pressing p key.\n");
                process_post(&ranger_process, button_hal_release_event, &fake_button_press);
            }
            break;
        case 'r':
            {
                LOG_INFO("Reboot triggered by pressing r key.\n");
                watchdog_reboot();
            }
            break;
        default:
            break;
    }
    return 1;
}
#endif

static void toggle_mode(void)
{
    set_mode((current_mode + 1) % MODE_AMOUNT);
}

static void set_mode(int mode)
{
    if (mode >= MODE_AMOUNT)
    {
        LOG_WARN("Requested mode exceeds maximum!\n");
        current_mode = MODE_AMOUNT - 1;
    }
    else if (mode < 0)
    {
        LOG_WARN("Requested mode lesser than minimum!\n");
        current_mode = 0;
    }
    else
    {
        current_mode = mode;
    }
    LOG_INFO("Mode set to %d.\n", current_mode);
    rgb_led_off();
}

//TODO: return error values instead of using assert statements
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

//TODO: return error values instead of using assert statements
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

//TODO: return error values instead of using assert statements
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

    current_rf_cfg = rf_cfg_ptrs[current_rf_cfg_index];
    // current_rf_cfg_led_color = rf_cfg_leds[current_rf_cfg_index];
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
    LOG_INFO("Payload size: %d byte(s)\n", sizeof(message));
    LOG_INFO("Transmission power: %d dBm\n", TX_POWER_DBM);
    LOG_INFO("Channel: %d\n", CHANNEL);
    LOG_INFO("Current RF config index: %" PRIu8 "\n", current_rf_cfg_index);
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

    current_request_id = random_rand();
    
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

    reset_mode_event = process_alloc_event();
    handshake_delay_event = process_alloc_event();
    auto_measure_event = process_alloc_event();
    // led_event = process_alloc_event();
    process_start(&handshake_delay_process, NULL);
    process_start(&auto_measure_process, NULL);
    // process_start(&led_process, NULL);

    // current_rf_cfg_led_color = rf_cfg_leds[current_rf_cfg_index];
    // rgb_led_set(current_rf_cfg_led_color);
    rgb_led_off();

    #if ENABLE_UART_INPUT
    uart_set_input(0, uart_byte_input_callback);
    #endif
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
                #if ENABLE_AUTO_MEASURE && (!ENABLE_CFG_HANDSHAKE)
                process_post(&auto_measure_process, auto_measure_event, NULL);
                #endif
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
                    if (current_mode == TX) 
                    {
                        toggle_mode();
                        reset_mode_flag = true;
                    }

                    current_request_id = random_rand();
                    for(size_t i = 0; i < BURST_AMOUNT; i++)
                    {
                        send_message(&linkaddr_null, CFG_REQ, 
                                     (int)((current_rf_cfg_index + 1) % RF_CFG_AMOUNT), 
                                     (unsigned int)current_request_id);
                    }

                    current_handshake_delay = empty_handshake_delay;         
                    current_handshake_delay.next_rf_cfg_index = (current_rf_cfg_index + 1) % RF_CFG_AMOUNT;
                    current_handshake_delay.handshake_delay = HANDSHAKE_TX_DELAY;
                    process_post(&handshake_delay_process, handshake_delay_event, 
                                 &current_handshake_delay);
                    #elif !ENABLE_AUTO_MEASURE
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
        else if (ev == reset_mode_event)
        {
            if (reset_mode_flag)
            {
                toggle_mode();
                reset_mode_flag = false;
            }
        }
    }

    LOG_INFO("Ranger process done...\n");

    PROCESS_END();
}

/*----------------------------------------------------------------------------*/

PROCESS_THREAD(handshake_delay_process, ev, data)
{
    static uint8_t rf_cfg_to_set = 0;
    static clock_time_t delay_to_set = 0;
    PROCESS_BEGIN();

    LOG_INFO("Started handshake delay process\n");

    while(1) 
    {
        PROCESS_YIELD();
        if(ev == handshake_delay_event)
        {
            rf_cfg_to_set = ((handshake_delay_t *)data)->next_rf_cfg_index;
            delay_to_set = ((handshake_delay_t *)data)->handshake_delay;
            LOG_INFO("Handshake delay event was triggered!\n");
            etimer_set(&handshake_delay_tmr, delay_to_set);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&handshake_delay_tmr));
            set_rf_cfg(rf_cfg_to_set);
            set_tx_power(TX_POWER_DBM);
            set_channel(CHANNEL);
            //REVIEW: reason for triggering an event instead of toggling here?
            process_post(&ranger_process, reset_mode_event, NULL);
        }
    }

    LOG_INFO("Handshake delay process done...\n");

    PROCESS_END();
}

/*----------------------------------------------------------------------------*/

PROCESS_THREAD(auto_measure_process, ev, data)
{
    static bool end_of_measurement = false;
    static size_t auto_measure_index = 0;
    PROCESS_BEGIN();

    LOG_INFO("Started auto measure process\n");

    while(1) 
    {
        PROCESS_YIELD();
        if(ev == auto_measure_event)
        {
            while (!end_of_measurement)
            {
                etimer_set(&auto_measure_tmr, AUTO_MEASURE_INTERVAL);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&auto_measure_tmr));

                auto_measure_index++;
                if (auto_measure_index >= RF_CFG_AMOUNT)
                {
                    toggle_mode();
                    package_nr_to_send = 0;
                    auto_measure_index = 0;
                    end_of_measurement = true;
                }
                else
                {
                    if (current_mode == TX) 
                    {
                        toggle_mode();
                        reset_mode_flag = true;
                    }

                    current_request_id = random_rand();
                    for(size_t i = 0; i < BURST_AMOUNT; i++)
                    {
                        send_message(&linkaddr_null, CFG_REQ, 
                                     (int)((current_rf_cfg_index + 1) % RF_CFG_AMOUNT), 
                                     (unsigned int)current_request_id);
                    }
                    current_handshake_delay = empty_handshake_delay;         
                    current_handshake_delay.next_rf_cfg_index = (current_rf_cfg_index + 1) % RF_CFG_AMOUNT;
                    current_handshake_delay.handshake_delay = HANDSHAKE_TX_DELAY;
                    process_post(&handshake_delay_process, handshake_delay_event, 
                                 &current_handshake_delay);
                }
                
            }
            end_of_measurement = false;
        }
    }

    LOG_INFO("Auto measure process done...\n");

    PROCESS_END();
}

/*----------------------------------------------------------------------------*/

// PROCESS_THREAD(led_process, ev, data)
// {
//     static uint8_t led_color;
//     PROCESS_BEGIN();

//     LOG_INFO("Started led process\n");

//     while(1) 
//     {
//         PROCESS_YIELD();
//         if(ev == led_event)
//         {
//             led_color = *(uint8_t *)data;
//             rgb_led_off();
//             etimer_set(&led_off_tmr, 80);
//             PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&led_off_tmr));
//             rgb_led_set(led_color);
//         }
//     }

//     LOG_INFO("Led process done...\n");

//     PROCESS_END();
// }
