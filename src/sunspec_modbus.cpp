#include <sunspec_modbus.h>

err_t tcp_server_result(void *arg, int status, struct tcp_pcb *&client);
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb);
void tcp_server_err(void *arg, err_t err);

err_t tcp_server_accept (void *arg, struct tcp_pcb *client_pcb, err_t err) {
	if (err != ERR_OK || client_pcb == NULL || arg == NULL) {
		LogError("Failure in accept");
		return ERR_VAL;
	}

	tcp_io &io = *(tcp_io*)arg;

	if (!io.conns.push({.client_socket = client_pcb})) {
		LogError("No free client spot");
		return tcp_server_result(nullptr, ERR_ABRT, client_pcb);
	}
	
	tcp_arg(client_pcb, arg);
	tcp_sent(client_pcb, tcp_server_sent);
	tcp_recv(client_pcb, tcp_server_recv);
	tcp_poll(client_pcb, tcp_server_poll, 5 * 2);
	tcp_err(client_pcb, tcp_server_err);

	LogInfo("Sunspec modbus client connected");
	return ERR_OK;
}


err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
	if (!p || !arg) {
		LogError("tcp_server_recv() failed");
		return tcp_server_result(arg, -1, tpcb);
	}
	tcp_io &io = *(tcp_io*)arg;
	tcp_io::connection *c = io.conns | find{&tcp_io::connection::client_socket, tpcb};
	if (!c) {
		LogError("Socket not found in conns");
	} else if (p->tot_len > 0) {
		for (uint16_t i: std::ranges::iota_view{uint16_t(0), p->tot_len})
			c->buffer.push(pbuf_get_at(p, i));
	}
	pbuf_free(p);
	xTaskNotifyGive(io.data_retrieve_wait);
	return ERR_OK;
}

err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
	return ERR_OK;
}

err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb) {
	tcp_io &io = *(tcp_io*)arg;
	tcp_io::connection *c = io.conns | find{&tcp_io::connection::client_socket, tpcb};
	err_t err = tcp_server_result(arg, -1, tpcb);
	if (!c) {
		LogError("Couldnt find connection with client socket");
		return err;
	}
	*c = *io.conns.pop();
	return err;
}

void tcp_server_err(void *arg, err_t err) {
	LogError("sunspec modbus tcp_server_err {}", err);
	if (err != ERR_ABRT) {
		LogError("tcp_client_err_fn {}", err);
		struct tcp_pcb *t{};
		tcp_server_result(arg, err, t);
	}
}

err_t tcp_server_result(void *arg, int status, struct tcp_pcb *&client) {
	if (status == 0) {
		LogInfo("Server success");
		return ERR_OK;
	}
	LogWarning("Server failed {}, deinitializing {}", status, client ? "one client": "no client");

	tcp_arg(client, NULL);
	tcp_poll(client, NULL, 0);
	tcp_sent(client, NULL);
	tcp_recv(client, NULL);
	tcp_err(client, NULL);
	return close_socket(client);
}

err_t close_socket(struct tcp_pcb *& socket) {
	if (!socket)
		return ERR_OK;
	tcp_arg(socket, NULL);
	err_t err{ERR_OK};
	err = tcp_close(socket);
	if (err != ERR_OK) {
		LogError("close failed calling abort: {}", err);
		tcp_abort(socket);
		err = ERR_ABRT;
	}
	socket = NULL;
	return err;
}
