#pragma once

#include <modbus-layouts.h>
#include <modbus-actor.h>

#include "hardware/uart.h"

#include <log_storage.h>

namespace ls = libmodbus_static;

struct rtu_io {
	static constexpr ls::transport_t TRANSPORT_TYPE{ls::transport_t::RTU};

	const int rx_pin{0};
	const int tx_pin{1};
	const int send_enable_pin{2};
	const int baudrate{9600};
	uart_inst_t *const uart{uart0};
	static_vector<uint8_t, 1024> receive_buffer{};

	void init() {
		uint32_t baud = uart_init(uart, baudrate);
		gpio_set_function(tx_pin, GPIO_FUNC_UART);
		gpio_set_function(rx_pin, GPIO_FUNC_UART);
		gpio_init(send_enable_pin);
		gpio_set_dir(send_enable_pin, GPIO_OUT);
		gpio_put(send_enable_pin, 0); // by default enable receive
		LogInfo("Rtu io enabled on rx {}, tx {}, send_enable {}, baudrate {}(should be)", rx_pin, tx_pin, send_enable_pin, baud, baudrate);
	}
	void deinit() {
	}
	std::span<uint8_t> read_bytes(std::chrono::milliseconds max_timeout) {
		receive_buffer.clear();
		while (uart_is_readable(uart))
			receive_buffer.push(uart_getc(uart)) ;
		return receive_buffer.span();
	}
	void write_bytes(std::span<uint8_t> data) {
		gpio_put(send_enable_pin, 1);
		for (uint8_t d: data)
			uart_putc_raw(uart, d);
		gpio_put(send_enable_pin, 0);
	}
	ls::result get_status() const {
		return ls::OK;
	}
};

namespace g {
inline ls::modbus_actor<eastron_layout, rtu_io>& eastron_modbus() {
	static ls::modbus_actor<eastron_layout, rtu_io> eastron{0, eastron_layout{}, rtu_io{}};
	return eastron;
}
}

