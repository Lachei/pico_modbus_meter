/**
* @brief File only containing lwip tcp server setup stuff
*/

#include "modbus_server_client.h"
#include "log_storage.h"

#include "lwip/pbuf.h"

#include "http_solarapi.h"

using namespace libmodbus_static;

// defining the callback methods needed for lwip
err_t tcp_server_accept (void *arg, struct tcp_pcb *client_pcb, err_t err);
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb);
void tcp_server_err(void *arg, err_t err);
err_t tcp_server_result(void *arg, int status, struct tcp_pcb *client);

err_t modbus_server_client::start() {
	fronius_server.write(uint16_t(fronius_server.addr), &fronius_meter::halfs_layout::modbus_device_address);
	LogInfo("Starting modbus lwip tcp server");
	struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		LogError("failed to create pcb");
		return ERR_ABRT;
	}
	
	tcp_setprio(pcb, 10);
	err_t err = tcp_bind(pcb, IP_ANY_TYPE, port);
	if (err) {
		LogError("failed to bind to port {}", port);
		return ERR_ABRT;
	}
	
	tcp_pcb = tcp_listen_with_backlog(pcb, /*message_buffers*/ 4);
	if (!tcp_pcb) {
		LogError("failed to listen");
		if (pcb) {
			tcp_close(pcb);
		}
		return ERR_ABRT;
	}
	
	tcp_arg(tcp_pcb, this);
	tcp_accept(tcp_pcb, tcp_server_accept);

	LogInfo("Modbus tcp server started");
	
	return ERR_OK;
}

err_t modbus_server_client::stop() {
	LogInfo("Stopping modbus lwip tcp server");

	if (tcp_pcb) {
		tcp_arg(tcp_pcb, NULL);
		tcp_close(tcp_pcb);
		tcp_pcb = NULL;
	}

	return ERR_OK;
}

// ------------------------------------------------------------
// non member functions of modbus_server_client
// ------------------------------------------------------------
err_t tcp_server_accept (void *arg, struct tcp_pcb *client_pcb, err_t err) {
	if (err != ERR_OK || client_pcb == NULL || arg == NULL) {
		LogError("Failure in accept");
		tcp_server_result(arg, err, nullptr);
		return ERR_VAL;
	}

	modbus_server_client& server = *reinterpret_cast<modbus_server_client*>(arg);
	
	LogInfo("Modbus tcp server client connected");
	
	tcp_arg(client_pcb, arg);
	tcp_sent(client_pcb, tcp_server_sent);
	tcp_recv(client_pcb, tcp_server_recv);
	tcp_poll(client_pcb, tcp_server_poll, server.tcp_poll_time_s * 2);
	tcp_err(client_pcb, tcp_server_err);

	LogInfo("Client connected, setup done");
	return ERR_OK;
}

err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
	if (!p || !arg) {
		LogError("tcp_server_recv() failed");
		return tcp_server_result(arg, -1, tpcb);
	}
	modbus_server_client& server = *reinterpret_cast<modbus_server_client*>(arg);
	if (p->tot_len > 0) {
		server.fronius_server.switch_to_request();
		for (uint16_t i: std::ranges::iota_view{uint16_t(0), p->tot_len}) {
			std::string_view r = server.fronius_server.process_tcp(pbuf_get_at(p, i)).err;
			if (r == IN_PROGRESS)
				continue;
			if (r != OK) {
				LogError("Modbus parsing failed with {}", r);
				continue;
			}
			LogInfo("Got request for addr {:x} length{:x}", server.fronius_server.lc.i1, server.fronius_server.lc.i2);
			auto [res, err] = server.fronius_server.get_frame_response();
			if (err != OK) {
				LogError("Modbus generating response failed with {}", err);
				r_tie{res, err} = server.fronius_server.get_frame_error_response(err);
				if (err != OK) {
					LogError("Modbus generating error response failed with {}", err);
					continue;
				}
			}
			LogInfo("Sending response with size {}", res.size());
			err_t tcp_err = tcp_write(tpcb, res.data(), res.size(), 0);
			if (tcp_err != ERR_OK)
				LogWarning("Failed to write data with err {}", tcp_err);
			server.fronius_server.switch_to_request();
		}
	}
	pbuf_free(p);
	return ERR_OK;
}

err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
	return ERR_OK;
}

err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb) {
	// remove connections that are not anymore valid
	LogInfo("tcp_server_poll_fn");
	return tcp_server_result(arg, -1, tpcb); // on no response remove the client to free up space
}

void tcp_server_err(void *arg, err_t err) {
	LogError("tcp_server_err {}", err);
	if (err != ERR_ABRT) {
		LogError("tcp_client_err_fn {}", err);
		tcp_server_result(arg, err, nullptr);
	}
}

err_t tcp_server_result(void *arg, int status, struct tcp_pcb *client) {
	if (status == 0) {
		LogInfo("Server success");
		return ERR_OK;
	}
	LogWarning("Server failed {}, deinitializing {}", status, client ? "one client": "no client");

	err_t err{ERR_OK};
	tcp_arg(client, NULL);
	tcp_poll(client, NULL, 0);
	tcp_sent(client, NULL);
	tcp_recv(client, NULL);
	tcp_err(client, NULL);
	err = tcp_close(client);
	if (err != ERR_OK) {
		LogError("close failed calling abort: {}", err);
		tcp_abort(client);
		err = ERR_ABRT;
	}
	return err;
}

