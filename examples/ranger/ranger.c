#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "contiki.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "dev/radio.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "sys/etimer.h"

#include "net/nullnet/nullnet.h"

#include "sys/log.h"
#define LOG_MODULE "Ranger"
#define LOG_LEVEL LOG_LEVEL_INFO

/*----------------------------------------------------------------------------*/

#define UNIQUE_ID UINT32_C(0x931de41a)

#define RX_RECEIVE_LEDS LEDS_GREEN
#define TX_SEND_LEDS LEDS_RED

#define SEND_GPIO_PORT GPIO_A_NUM
#define SEND_GPIO_PIN 7
#define SEND_GPIO_PORT_BASE GPIO_PORT_TO_BASE(SEND_GPIO_PORT)
#define SEND_GPIO_PIN_MASK GPIO_PIN_MASK(SEND_GPIO_PIN)

/*----------------------------------------------------------------------------*/

PROCESS(ranger_process, "Ranger process");
AUTOSTART_PROCESSES(&ranger_process);

/*----------------------------------------------------------------------------*/

enum mode
{
    RX,
    TX,
};

enum generated_message_type
{
    USE_PRESET_MESSAGE,
    USE_RANDOM_MESSAGE,
    USE_RANDOM_ASCII_MESSAGE
};

typedef struct
{
    char content[CONTENT_SIZE];
    uint32_t package_nr;
    uint32_t unique_id;
} message;

static enum mode current_mode;
static struct etimer message_send_tmr;
static int message_counter = 0;

// Remote ZOL-RM02-B0-240000169 with battery pint has lladress 0012.4b00.09df.4c73
static const linkaddr_t sender_linkaddr = {{0x00, 0x12, 0x4b, 0x00, 0x09, 0xdf, 0x4c, 0x73}};

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

static void fill_message_content(char* buffer, int size)
{
    if (SEND_MESSAGE_TYPE == USE_PRESET_MESSAGE)
    {
        memset(buffer, 0, size);
        strncpy(buffer, "martijn.saelens@ugent.be", size);
    }
    else if (SEND_MESSAGE_TYPE == USE_RANDOM_MESSAGE)
    {
        for (int i = 0; i < size; i++)
        {
            buffer[i] = rand() % 256;
        }
    }
    else if (SEND_MESSAGE_TYPE == USE_RANDOM_ASCII_MESSAGE)
    {
        for (int i = 0; i < size; i++)
        {
            buffer[i] = (32 + rand() % 95);
        }
    }
}

static void send_message(void)
{
    leds_on(TX_SEND_LEDS);

    static uint32_t package_nr_to_send = 0;

    LOG_INFO("Sending message...\n");

    message new_message;
    fill_message_content(new_message.content, CONTENT_SIZE);
    new_message.package_nr = package_nr_to_send;
    new_message.unique_id = UNIQUE_ID;
    package_nr_to_send++;

    LOG_INFO("Message with payload length %d\n", sizeof(new_message));
    LOG_INFO("|-- Content (hex)  : ");
    print_buffer(new_message.content, CONTENT_SIZE, "%02X ");
    LOG_INFO("|-- Content (ascii): ");
    print_buffer(new_message.content, CONTENT_SIZE, "%2c ");
    LOG_INFO("\\-- Package number: %" PRIu32 "\n", new_message.package_nr);

    nullnet_buf = (uint8_t*) &new_message;
    nullnet_len = sizeof(new_message);

    NETSTACK_NETWORK.output(NULL);

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

    printf("csv-log: %" PRIu32 ", %" PRIu16 ", %" PRIi8 "\n", current_message->package_nr, datalen, rssi);

    LOG_INFO("Total messages received: %d\n", message_counter);

    leds_off(LEDS_ALL);
}

static void received_nullnet_message_callback(const void* data,
                                              uint16_t datalen,
                                              const linkaddr_t* src,
                                              const linkaddr_t* dest)
{
    LOG_INFO("Received message by NULLNET from ");
    LOG_INFO_LLADDR(src);
    LOG_INFO_(" to ");
    LOG_INFO_LLADDR(dest);
    LOG_INFO_(" with payload length %" PRIu16 "\n", datalen);

    if (!linkaddr_cmp(src, &sender_linkaddr))
    {
        LOG_WARN("Received message from wrong sender: from ");
        LOG_INFO_LLADDR(src);
        LOG_WARN_(" but expected ");
        LOG_INFO_LLADDR(&sender_linkaddr);
        LOG_WARN_(". Message ignored.\n");
        return;
    }

    received_message((message*) data, datalen);
}

static void toggle_mode(void)
{
    leds_on(LEDS_ALL);

    if (current_mode == TX)
    {
        current_mode = RX;
        LOG_INFO("Switched to RX Mode\n");
    }
    else
    {
        current_mode = TX;
        LOG_INFO("Switched to TX Mode\n");
    }

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
        LOG_INFO("WARNING: Requested TX power %d dBm is larger than maximum allowed TX power %d dBm.\n",
                 tx_power,
                 max_value);

        result = NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, max_value);
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("TX power set to maximum of %d dBm\n", max_value);
    }
    else if (tx_power < min_value)
    {
        LOG_INFO("WARNING: Requested TX power %d dBm is less than minimum allowed TX power %d dBm.\n",
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
        LOG_INFO("WARNING: Requested channel nr %d is larger than maximum channel nr %d.\n", channel, max_value);

        result = NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, max_value);
        assert(result == RADIO_RESULT_OK);
        LOG_INFO("Channel changed to maximum channel nr %d\n", max_value);
    }
    else if (channel < min_value)
    {
        LOG_INFO("WARNING: Requested channel nr %d is less than minimum minimal channel nr %d.\n", channel, min_value);

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

/* static void send_pin_callback(uint8_t port, uint8_t pin) */
/* { */
/*     LOG_INFO("Interrupt callback on port %" PRIu8 " pin %" PRIu8 "\n", port, pin); */
/*     send_message(); */
/* } */

static void init_send_pin(void)
{
    /* // At reset all GPIO pads are set as inputs and are pulled up */
    /* GPIO_SOFTWARE_CONTROL(SEND_GPIO_PORT_BASE, SEND_GPIO_PIN_MASK); */
    /* GPIO_SET_INPUT(SEND_GPIO_PORT_BASE, SEND_GPIO_PIN_MASK); */
    /* GPIO_DETECT_EDGE(SEND_GPIO_PORT_BASE, SEND_GPIO_PIN_MASK); */
    /* GPIO_TRIGGER_SINGLE_EDGE(SEND_GPIO_PORT_BASE, SEND_GPIO_PIN_MASK); */
    /* GPIO_DETECT_RISING(SEND_GPIO_PORT_BASE, SEND_GPIO_PIN_MASK); */
    /*  */
    /* gpio_register_callback(send_pin_callback, SEND_GPIO_PORT, SEND_GPIO_PIN); */
    /* // typedef void (* gpio_callback_t)(uint8_t port, uint8_t pin); */
    /* // TODO(martijn) Geeft warning, maar de signaturen komen wel overeen? */
    /*  */
    /* GPIO_ENABLE_INTERRUPT(SEND_GPIO_PORT_BASE, SEND_GPIO_PIN_MASK); */
}

static void print_diagnostics(void)
{
    printf("\n");
    printf("Device: %s\n", BOARD_STRING);
    printf("Message size: %d byte(s)\n", sizeof(message));
    printf("Message content size: %d byte(s)\n", CONTENT_SIZE);
    printf("Transmission power: %d dBm\n", TX_POWER_DBM);
    printf("Channel: %d\n", CHANNEL);
    printf("Timer period: %d.%02d s\n", ((int) MAIN_INTERVAL_SECONDS), (int) (MAIN_INTERVAL_SECONDS * 100) % 100);
    printf("\n");
}

/*----------------------------------------------------------------------------*/

PROCESS_THREAD(ranger_process, ev, data)
{
    PROCESS_BEGIN();

    if (ENABLE_SEND_PIN)
    {
        init_send_pin();
    }

    set_tx_power(TX_POWER_DBM);
    set_channel(CHANNEL);

    print_diagnostics();
    LOG_INFO("Started ranger process\n");

    leds_off(LEDS_ALL);

    nullnet_set_input_callback(received_nullnet_message_callback);

    current_mode = RX;
    LOG_INFO("Booted in RX Mode\n");

    etimer_set(&message_send_tmr, MAIN_INTERVAL);

    while (1)
    {
        PROCESS_YIELD();

        if (ev == button_hal_release_event)
        {
            toggle_mode();
        }

        if (etimer_expired(&message_send_tmr))
        {
            if (current_mode == TX)
            {
                send_message();
            }

            etimer_reset(&message_send_tmr);
        }
    }

    LOG_INFO("Done...\n");

    PROCESS_END();
}
