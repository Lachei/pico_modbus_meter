#pragma once

#include <fronius-sunspec.h>
#include <modbus-register.h>

#include "lwip/tcp.h"

namespace ls = libmodbus_static;

struct modbus_server_client {

	static modbus_server_client& Default() {
		static modbus_server_client msr{};
		return msr;
	}

	ls::modbus_register<fronius_meter::layout> &fronius_server{ls::modbus_register<fronius_meter::layout>::Default(1)};

	uint16_t port{502}; // default modbus tcp port is 502
	int tcp_poll_time_s{5};
	struct tcp_pcb *tcp_pcb{};

	/** @brief Starts the lwip tcp server on specified port */
	err_t start();
	err_t stop();
};

