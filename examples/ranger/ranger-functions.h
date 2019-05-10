#ifndef RANGER_FUNCTIONS_H_
#define RANGER_FUNCTIONS_H_

#include "dev/gpio-hal.h"
#include "os/net/linkaddr.h"
#include "ranger-types.h"

static void print_buffer(const char* buffer, int size, const char* specifier);
static void print_line(void);

/**
 * @brief Send a 802.15.4 data frame to a specified destination.
 * 
 * @param dest_addr The link address of the destination, use NULL or &linkaddr_null
 *                  to broadcast.
 * @param message_type The type of the data frame to send.
 * @param ... Variable argument list, accepts either nothing or a config index (int)
 *            and a unique request ID (unsigned int) in that order and with those types.
 */
static void send_message(const linkaddr_t* dest_addr, ranger_message_t message_type, ...);
static void received_ranger_net_message_callback(const void* data, uint16_t datalen,
                                                 const linkaddr_t* src,
                                                 const linkaddr_t* dest);
#if ENABLE_UART_INPUT
static int uart_byte_input_callback(unsigned char input);
#endif
static void toggle_mode(void);
static void set_mode(int mode);
static void set_tx_power(int tx_power);
static void set_channel(int channel);
static void set_rf_cfg(int rf_cfg_index);
static void send_handler(gpio_hal_pin_mask_t pin_mask);
static void init_send_pin(void);
static void print_diagnostics(void);

#endif /* RANGER_FUNCTIONS_H_ */