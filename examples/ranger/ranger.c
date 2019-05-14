#include <stdio.h>
#include <math.h>
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
#include "net/linkaddr.h"
#include "random.h"
#include "dev/uart.h"
#include "dev/tmp102.h"

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
PROCESS(rf_cfg_delay_process, "RF config delay process");
PROCESS(mode_delay_process, "Mode delay process");
PROCESS(led_process, "Led process");
AUTOSTART_PROCESSES(&ranger_process);

/*----------------------------------------------------------------------------*/

static uint8_t current_rf_cfg_led;
static uint8_t current_rf_cfg_index;
static rf_cfg_delay_t current_rf_cfg_delay;
static const cc1200_rf_cfg_t *current_rf_cfg = &CC1200_CONF_RF_CFG; //NOTE: pointer to const != constant

extern gpio_hal_pin_t send_pin;
static process_event_t send_pin_event;
static gpio_hal_event_handler_t send_pin_event_handler;

static transceiver_mode_t current_mode;

static struct etimer message_send_tmr;
static struct etimer rf_cfg_delay_tmr;
static struct etimer mode_delay_tmr;
static struct etimer led_on_tmr;

static process_event_t rf_cfg_delay_event;
static process_event_t mode_delay_event;
static process_event_t reset_mode_event;
static process_event_t rf_cfg_led_event;

static uint32_t package_nr_to_send;
static int message_counter;

static bool reset_mode_flag = false;

static uint32_t current_request_id;

#if ENABLE_UART_INPUT
static int16_t temperature;
static process_event_t read_temp_event;
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

static void print_line(void) 
{
    if (LOG_LEVEL >= LOG_LEVEL_INFO)
    {
        LOG_OUTPUT("\n");
    }
}

static void print_node_addr(linkaddr_t node_addr)
{
    for(size_t i = 0; i < LINKADDR_SIZE; i++) 
    {
        if(i > 0 && i % 2 == 0) 
        {
            printf(".");
        }
        printf("%02x", node_addr.u8[i]);
    }
}

//TODO: return an error when arguments are invalid
static void send_message(const linkaddr_t* dest_addr, ranger_message_t message_type, ...)
{
    va_list argptr;
    va_start(argptr, message_type);

    rgb_led_set(TX_LED);

    LOG_INFO("Sending message to ");
    LOG_INFO_LLADDR(dest_addr);
    print_line();

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
        default:
            break;
    }

    ranger_net_buf = (uint8_t*) &new_message;
    ranger_net_len = sizeof(new_message);

    NETSTACK_NETWORK.output(dest_addr);

    LOG_INFO("Message sent\n");

    rgb_led_off();

    va_end(argptr);
}

//TODO: return error values instead of using assert statements
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

    rgb_led_set(RX_LED);

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

                radio_value_t tx_power = 0;
                radio_value_t channel = 0;
                radio_result_t result = RADIO_RESULT_ERROR;

                result = NETSTACK_RADIO.get_value(RADIO_PARAM_TXPOWER, &tx_power);
                assert(result == RADIO_RESULT_OK);
                result = NETSTACK_RADIO.get_value(RADIO_PARAM_CHANNEL, &channel);
                assert(result == RADIO_RESULT_OK);

                cc1200_preamble_t preamble = get_cc1200_preamble();
                cc1200_symbol_rate_t symbol_rate = get_cc1200_symbol_rate();
                cc1200_rx_filt_bw_t rx_filt_bw = get_cc1200_rx_filt_bw();

                uint32_t chan_center_freq = current_rf_cfg->chan_center_freq0 * 1000 + (channel * current_rf_cfg->chan_spacing);

                //TODO: adapt analyzer.py to work with new log format
                //FIXME: float format specifier doesn't work because "-u_printf_float" option is not passed to linker
                //NOTE: temporary fix is to count preamble words in amount of nibbles instead of bytes (1 byte = 2 nibbles)
                printf("csv-log: %s, %"PRIu32", %"PRIu16", %"PRIi16", %"PRIi8", %"PRIu16", %d, %d, %"PRIu32", %"PRIu32", %"PRIu32", %"PRIu32", %"PRIu32", %"PRIu32", %"PRIu8", 0x%02X, ",
                       current_rf_cfg->cfg_descriptor,
                       current_message.package_nr,
                       datalen,
                       rssi,
                       current_rf_cfg->rssi_offset,
                       lqi,
                       tx_power,
                       channel,
                       current_rf_cfg->chan_center_freq0,
                       current_rf_cfg->chan_spacing,
                       chan_center_freq,
                       current_rf_cfg->bitrate,
                       symbol_rate,
                       rx_filt_bw,
                       preamble.preamble_nibbles,
                       preamble.preamble_word);
                print_node_addr(linkaddr_node_addr);
                printf(", ");
                print_node_addr(src_addr);
                printf("\n");
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
                    current_rf_cfg_delay = empty_rf_cfg_delay;
                    current_rf_cfg_delay.next_rf_cfg_index = current_message.rf_cfg_index;
                    current_rf_cfg_delay.rf_cfg_delay = CFG_REQ_DELAY;
                    process_post(&rf_cfg_delay_process, rf_cfg_delay_event, 
                                 &current_rf_cfg_delay);
                }

                current_request_id = current_message.request_id;
            }
            break;
        default:
            break;
    }

    LOG_INFO("Total messages received: %d\n", message_counter);

    rgb_led_off();
}

#if ENABLE_UART_INPUT
static int uart_byte_input_callback(unsigned char input)
{
    switch (input)
    {
        case 'l':
            {
                memcpy(&fake_button_press, button_hal_get_by_id(BUTTON_HAL_ID_USER_BUTTON), 
                       sizeof(button_hal_button_t));
                fake_button_press.press_duration_seconds = 5;
                LOG_INFO("Fake button press triggered by pressing l key.\n");
                process_post(&ranger_process, button_hal_periodic_event, &fake_button_press);
            }
            break;
        case 's':
            {
                memcpy(&fake_button_press, button_hal_get_by_id(BUTTON_HAL_ID_USER_BUTTON), 
                       sizeof(button_hal_button_t));
                LOG_INFO("Fake button press triggered by pressing s key.\n");
                process_post(&ranger_process, button_hal_release_event, &fake_button_press);
            }
            break;
        case 'r':
            {
                LOG_INFO("Reboot triggered by pressing r key.\n");
                watchdog_reboot();
            }
        case 't':
            {
                LOG_INFO("Temperature reading triggered by pressing t key.\n");
                process_post(&ranger_process, read_temp_event, NULL);
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
    current_rf_cfg_led = rf_cfg_leds[current_rf_cfg_index];
    process_post(&led_process, rf_cfg_led_event, &current_rf_cfg_led);
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

static cc1200_preamble_t get_cc1200_preamble(void)
{
    cc1200_preamble_t preamble = {};
    registerSetting_t cc1200_preamble_cfg1 = {};
    radio_result_t result = NETSTACK_RADIO.get_object(RADIO_PARAM_PREAMBLE_CFG1, 
                                                      &cc1200_preamble_cfg1, 
                                                      sizeof(registerSetting_t));
    assert(result == RADIO_RESULT_OK);
    LOG_INFO("PREAMBLE_CFG1: 0x%02X\n", cc1200_preamble_cfg1.val);

    preamble.preamble_nibbles = num_preamble_nibbles[(cc1200_preamble_cfg1.val & ~0xC3) >> 2];
    preamble.preamble_word = preamble_words[cc1200_preamble_cfg1.val & ~0xFC];

    return preamble;
}

static cc1200_symbol_rate_t get_cc1200_symbol_rate(void)
{
    cc1200_symbol_rate_t symbol_rate = 0;
    registerSetting_t cc1200_symbol_rate[3] = {{}};
    radio_result_t result = NETSTACK_RADIO.get_object(RADIO_PARAM_SYMBOL_RATE,
                                                      cc1200_symbol_rate,
                                                      sizeof(registerSetting_t)*3);
    assert(result == RADIO_RESULT_OK);
    LOG_INFO("SYMBOL_RATE0: 0x%02X\n",cc1200_symbol_rate[0].val);
    LOG_INFO("SYMBOL_RATE1: 0x%02X\n",cc1200_symbol_rate[1].val);
    LOG_INFO("SYMBOL_RATE2: 0x%02X\n",cc1200_symbol_rate[2].val);

    uint8_t exponent = (cc1200_symbol_rate[2].val & ~0x0F) >> 4;
    uint32_t mantissa = cc1200_symbol_rate[0].val + (cc1200_symbol_rate[1].val << 8) + 
                        ((cc1200_symbol_rate[2].val & ~0xF0) << 16);

    if(exponent != 0)
    {
        symbol_rate = ((pow(2.0, 20.0) + mantissa) * pow(2.0, (double)exponent) * 
                      XTAL_FREQ_KHZ * 1000 / pow(2.0, 39.0)) + 0.5;
    }
    else
    {
        symbol_rate = (mantissa * XTAL_FREQ_KHZ * 1000 / pow(2.0, 38.0)) + 0.5;
    }
    
    return symbol_rate;
}

static cc1200_rx_filt_bw_t get_cc1200_rx_filt_bw(void)
{
    cc1200_rx_filt_bw_t rx_filt_bw = 0;
    registerSetting_t cc1200_chan_bw = {};
    radio_result_t result = NETSTACK_RADIO.get_object(RADIO_PARAM_CHAN_BW,
                                                      &cc1200_chan_bw,
                                                      sizeof(registerSetting_t));
    assert(result == RADIO_RESULT_OK);
    LOG_INFO("CHAN_BW: 0x%02X\n", cc1200_chan_bw.val);

    rx_filt_bw = (XTAL_FREQ_KHZ * 1000 / (decimation_factors[(cc1200_chan_bw.val & ~0x3F) >> 6] * 
                 (cc1200_chan_bw.val & ~0xC0) * 2.0)) + 0.5;

    return rx_filt_bw;
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
    rf_cfg_delay_event = process_alloc_event();
    mode_delay_event = process_alloc_event();
    rf_cfg_led_event = process_alloc_event();
    process_start(&rf_cfg_delay_process, NULL);
    process_start(&mode_delay_process, NULL);
    process_start(&led_process, NULL);

    current_rf_cfg_led = rf_cfg_leds[current_rf_cfg_index];
    process_post(&led_process, rf_cfg_led_event, &current_rf_cfg_led);

    #if ENABLE_UART_INPUT
    tmp102_init();
    read_temp_event = process_alloc_event();
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

            #if ENABLE_UART_INPUT && BUTTON_HAL_CONF_WITH_DESCRIPTION
            if (btn->description == button_hal_get_by_id(BUTTON_HAL_ID_USER_BUTTON)->description)
            #else
            if (btn == button_hal_get_by_id(BUTTON_HAL_ID_USER_BUTTON))
            #endif
            {
                if (btn->press_duration_seconds == 5)
                {
                    LOG_INFO("Pressed user button for 5 seconds\n");
                    long_press_flag = true;
                    if (current_mode == RX) 
                    {
                        toggle_mode();
                        reset_mode_flag = true;
                        process_post(&mode_delay_process, mode_delay_event, NULL);
                    }
                }
            }
        }
        else if (ev == button_hal_release_event)
        {
            if (!long_press_flag)
            {
                btn = (button_hal_button_t *)data;
                #if ENABLE_UART_INPUT && BUTTON_HAL_CONF_WITH_DESCRIPTION 
                if (btn->description == button_hal_get_by_id(BUTTON_HAL_ID_USER_BUTTON)->description)
                #else
                if (btn == button_hal_get_by_id(BUTTON_HAL_ID_USER_BUTTON))
                #endif
                {
                    LOG_INFO("Released user button\n");

                    #if ENABLE_CFG_REQ
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

                    current_rf_cfg_delay = empty_rf_cfg_delay;         
                    current_rf_cfg_delay.next_rf_cfg_index = (current_rf_cfg_index + 1) % RF_CFG_AMOUNT;
                    current_rf_cfg_delay.rf_cfg_delay = CFG_REQ_DELAY;
                    process_post(&rf_cfg_delay_process, rf_cfg_delay_event, 
                                 &current_rf_cfg_delay);
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
        else if (ev == reset_mode_event)
        {
            if (reset_mode_flag)
            {
                toggle_mode();
                reset_mode_flag = false;
            }
        }
        #if ENABLE_UART_INPUT
        else if (ev == read_temp_event)
        {
            uint8_t i2c_error = I2C_MASTER_ERR_NONE;
            i2c_error = tmp102_read(&temperature);
            if (i2c_error == I2C_MASTER_ERR_NONE)
            {
                printf("The temperature at this location equals %d °C\n", temperature);
            }
            else
            {
                LOG_INFO("I2C error: %X \n", i2c_error);
            }
        }
        #endif
    }

    LOG_INFO("Ranger process done...\n");

    PROCESS_END();
}

/*----------------------------------------------------------------------------*/

PROCESS_THREAD(rf_cfg_delay_process, ev, data)
{
    static uint8_t rf_cfg_index = 0;
    static clock_time_t rf_cfg_delay = 0;
    PROCESS_BEGIN();

    LOG_INFO("Started RF config delay process\n");

    while(1) 
    {
        PROCESS_YIELD();
        if(ev == rf_cfg_delay_event)
        {
            rf_cfg_index = ((rf_cfg_delay_t *)data)->next_rf_cfg_index;
            rf_cfg_delay = ((rf_cfg_delay_t *)data)->rf_cfg_delay;
            LOG_INFO("RF config delay event was triggered!\n");
            etimer_set(&rf_cfg_delay_tmr, rf_cfg_delay);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rf_cfg_delay_tmr));
            set_rf_cfg(rf_cfg_index);
            set_tx_power(TX_POWER_DBM);
            set_channel(CHANNEL);
            //REVIEW: reason for triggering an event instead of toggling here?
            process_post(&ranger_process, reset_mode_event, NULL);
        }
    }

    LOG_INFO("RF config delay process done...\n");

    PROCESS_END();
}

/*----------------------------------------------------------------------------*/

PROCESS_THREAD(mode_delay_process, ev, data)
{
    PROCESS_BEGIN();

    LOG_INFO("Started mode delay process\n");

    while(1) 
    {
        PROCESS_YIELD();
        if(ev == mode_delay_event)
        {
            etimer_set(&mode_delay_tmr, TX_DURATION);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&mode_delay_tmr));
            //REVIEW: reason for triggering an event instead of toggling here?
            process_post(&ranger_process, reset_mode_event, NULL);
        }
    }

    LOG_INFO("Mode delay process done...\n");

    PROCESS_END();
}

/*----------------------------------------------------------------------------*/

PROCESS_THREAD(led_process, ev, data)
{
    static uint8_t rf_cfg_led = 0;
    PROCESS_BEGIN();

    LOG_INFO("Started led process\n");

    while(1) 
    {
        PROCESS_YIELD();
        if(ev == rf_cfg_led_event)
        {
            rf_cfg_led = *(uint8_t *)data;
            rgb_led_off();
            rgb_led_set(rf_cfg_led);
            etimer_set(&led_on_tmr, CLOCK_SECOND / 2);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&led_on_tmr));
            rgb_led_off();
        }
    }

    LOG_INFO("Led process done...\n");

    PROCESS_END();
}
