#pragma once

#include <modbus-layouts.h>
#include <modbus-actor.h>

#include "lwip/tcp.h"

#include "mutex.h"

#include <log_storage.h>
#include <ranges_util.h>

namespace ls = libmodbus_static;

err_t close_socket(struct tcp_pcb *& socket);
err_t tcp_server_accept (void *arg, struct tcp_pcb *client_pcb, err_t err);

struct tcp_io {
	static constexpr ls::transport_t TRANSPORT_TYPE{ls::transport_t::TCP};

	const uint16_t port{502};

	struct connection {
		struct tcp_pcb* client_socket{};
		static_vector<uint8_t, 1024> buffer{};
	};
	using connections = static_vector<connection, 8>;

	struct tcp_pcb* server_socket{};
	connections conns{};
	int cur_client{-1};
	TaskHandle_t data_retrieve_wait{};

	void init() {
		LogInfo("Starting modbus lwip tcp server");
		server_socket = tcp_new_ip_type(IPADDR_TYPE_ANY);
		if (!server_socket) {
			LogError("failed to create modbus server pcb");
			return;
		}

		tcp_setprio(server_socket, 10);
		err_t err = tcp_bind(server_socket, IP_ANY_TYPE, port);
		if (err) {
			LogError("failed to bind to port {}", port);
			return;
		}

		server_socket = tcp_listen_with_backlog(server_socket, 16);
		if (!server_socket) {
			LogError("failed to listen");
			if (server_socket) {
				tcp_close(server_socket);
			}
			return;
		}

		tcp_arg(server_socket, this);
		tcp_accept(server_socket, tcp_server_accept);

		LogInfo("Modbus tcp server started");
	}
	void deinit() {
		for (connection &c: conns)
			close_socket(c.client_socket);
		conns.clear();
		close_socket(server_socket);
	}
	std::span<uint8_t> read_bytes(std::chrono::milliseconds max_timeout) {
		for (int i: range(conns.size())) {
			if (conns[i].buffer.size()) {
				std::cout << "got bytes\n";
				std::span<uint8_t> data = conns[i].buffer.span();
				conns[i].buffer.clear();
				cur_client = i;
				return data;
			}
		}

		data_retrieve_wait = xTaskGetCurrentTaskHandle();
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(max_timeout.count()));

		for (int i: range(conns.size())) {
			if (conns[i].buffer.size()) {
				std::cout << "got bytes\n";
				std::span<uint8_t> data = conns[i].buffer.span();
				conns[i].buffer.clear();
				cur_client = i;
				return data;
			}
		}
		return {};
	}
	void write_bytes(std::span<uint8_t> data) {
		std::cout << "write bytes\n" << std::endl;
		if (cur_client == -1)
			LogError("Missing previous read, no idae where to send to");
		if (ERR_OK != tcp_write(conns[cur_client].client_socket, data.data(), data.size(), 0))
			LogError("Failed to write tcp data");
		cur_client = -1;
	}
	ls::result get_status() const {
		if (!server_socket)
			return "Bad server socket";
		return ls::OK;
	}
};

namespace g {
inline mutex& sunspec_mutex() {
	static mutex m{};
	return m;
}
inline ls::modbus_actor<sunspec_layout, tcp_io>& sunspec_modbus() {
	static ls::modbus_actor<sunspec_layout, tcp_io> sunspec{1, []{ std::cout << "server locking\n"; g::sunspec_mutex().lock(); }, []{ std::cout << "server locking\n"; g::sunspec_mutex().unlock(); } };
	return sunspec;
}
}

