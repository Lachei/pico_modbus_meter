#include "http_solarapi.h"
#include "log_storage.h"

#include "pico/cyw43_arch.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "static_types.h"
#include "string_util.h"
#include "modbus_server_client.h"

#include <atomic>

constexpr std::string_view CL{"Content-Length:"};
struct tcp_pcb *client_pcb{};
std::atomic<bool> connected{};
TaskHandle_t task_handle{};
modbus_server_client *modbus{};
static_vector<char, 2 * 4096> storage{};
int open_bracket_count{};

void tcp_err_cb(void *arg, err_t err);
err_t tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
err_t tcp_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len);
err_t tcp_connect_cb(void *arg, struct tcp_pcb *tpcb, err_t err);
err_t send_request();
err_t tcp_client_close();

void update_meter_values(modbus_server_client &m) {
	LogInfo("Entering update meter vals, {}ms", time_us_64() / 1000);
	modbus = &m;
	task_handle = xTaskGetCurrentTaskHandle();
	cyw43_arch_lwip_begin();
	ip_addr_t ip;
	IP4_ADDR(&ip, 192, 168, 178, 181);
	if (!client_pcb) {
		client_pcb = tcp_new();
		if (!client_pcb) {
			LogError("Failed to create client_pcb");
			return;
		}

		tcp_arg(client_pcb, nullptr);
		tcp_err(client_pcb, tcp_err_cb);
		tcp_recv(client_pcb, tcp_recv_cb);
		tcp_sent(client_pcb, tcp_sent_cb);
	}

	if (!connected)
		tcp_connect(client_pcb, &ip, 80, tcp_connect_cb);
	else
		send_request();
	cyw43_arch_lwip_end();

	uint32_t count = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(4000));
	if (count == 0) { // avoid notifying the task if we ran into a timeout
		LogInfo("Ran into wait timeout");
		task_handle = nullptr;
		cyw43_arch_lwip_begin();
		tcp_client_close();
		cyw43_arch_lwip_end();
	}
	LogInfo("Quitting update meter vals, {}ms", time_us_64() / 1000);
}

void wake_up_update_task() {
	if (!task_handle)
		return;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR(task_handle, &xHigherPriorityTaskWoken);
	task_handle = nullptr;
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

err_t send_request() {
	LogInfo("Sending request");
	constexpr std::string_view frame = "GET /solar_api/v1/GetMeterRealtimeData.cgi HTTP/1.0\r\nHost: lachei_request.com\r\nAccept: */*\r\n\r\n ";

	err_t error = tcp_write(client_pcb, frame.data(), frame.size(), 0);

	if (error) {
		LogError("ERROR: Code: {} (tcp_send_packet :: tcp_write)", error);
		return 1;
	}

	error = tcp_output(client_pcb);
	if (error) {
		LogError("ERROR: Code: {} (tcp_send_packet :: tcp_output)", error);
		return 1;
	}
	return 0;
}

err_t tcp_connect_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
	LogInfo("Connected");
	connected = true;
	return send_request();
}

void tcp_err_cb(void *arg, err_t err) {
	LogError("Error: {}", err);
	if (err != ERR_ABRT)
		tcp_client_close();
}
#define TRY_EXTRACT(key, variable) if (cur.find(key) != std::string_view::npos && modbus) modbus->fronius_server.write(extract_val(cur), &halfs_sunspec::variable)
#define TRY_EXTRACTE(key, variable) else if (cur.find(key) != std::string_view::npos && modbus) modbus->fronius_server.write(extract_val(cur), &halfs_sunspec::variable)
float extract_val(std::string_view line) {
	size_t p = line.find(':');
	if (p == std::string_view::npos)
		return 0;
	float v = std::strtof(line.data() + p + 1, nullptr);
	return v;
}
err_t tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
	if (!p) {
		LogInfo("Done with a frame");
		storage.clear();
		open_bracket_count = 0;
		return tcp_client_close();
	}

	bool finished{};
	for (struct pbuf *t = p; t; t = t->next) {
		for (char v: std::string_view{(char *)t->payload, t->len}) {
			storage.push(v);
			finished |= open_bracket_count == 1 && v == '}';
			if (v == '{')
				++open_bracket_count;
			if (v == '}')
				--open_bracket_count;
		}
	}

	tcp_recved(tpcb, p->tot_len);
	pbuf_free(p);

	LogInfo("Bracket count {}, storage size {}", open_bracket_count, storage.size());
	if (finished || storage.size() > 4096) {
		std::string_view content{storage.begin(), storage.end()};
		int i{};
		for (std::string_view cur = extract_line(content); cur.size() && i < 400; cur = extract_line(content)) {
			if (!modbus)
				break;
			TRY_EXTRACT("Current_AC_Sum", a);
			TRY_EXTRACTE("Current_AC_Phase_1", apha);
			TRY_EXTRACTE("Current_AC_Phase_2", aphb);
			TRY_EXTRACTE("Current_AC_Phase_3", aphc);
			TRY_EXTRACTE("EnergyReal_WAC_Sum_Consumed", totwhimp);
			TRY_EXTRACTE("EnergyReal_WAC_Sum_Produced", totwhexp);
			TRY_EXTRACTE("Frequency_Phase_Average", hz);
			TRY_EXTRACTE("PowerReal_P_Sum", w);
			TRY_EXTRACTE("PowerReal_P_Phase_1", wpha);
			TRY_EXTRACTE("PowerReal_P_Phase_2", wphb);
			TRY_EXTRACTE("PowerReal_P_Phase_3", wphc);
			TRY_EXTRACTE("PowerFactor_Sum", pf);
			TRY_EXTRACTE("PowerFactor_Phase_1", pfpha);
			TRY_EXTRACTE("PowerFactor_Phase_2", pfphb);
			TRY_EXTRACTE("PowerFactor_Phase_3", pfphc);
			TRY_EXTRACTE("Voltage_AC_Phase_Average", phv);
			TRY_EXTRACTE("Voltage_AC_Phase_1", phvpha);
			TRY_EXTRACTE("Voltage_AC_Phase_2", phvphb);
			TRY_EXTRACTE("Voltage_AC_Phase_3", phvphc);
			TRY_EXTRACTE("Voltage_AC_PhaseToPhase_12", ppvphab);
			TRY_EXTRACTE("Voltage_AC_PhaseToPhase_23", ppvphbc);
			TRY_EXTRACTE("Voltage_AC_PhaseToPhase_31", ppvphca);
			++i;
		}
		storage.clear();
		// calculate corerct per phase values
		float wh_exp = modbus->fronius_server.read(&halfs_sunspec::totwhexp);
		float wh_imp = modbus->fronius_server.read(&halfs_sunspec::totwhimp);
		modbus->fronius_server.write(wh_exp / 3.f, &halfs_sunspec::totwhexppha);
		modbus->fronius_server.write(wh_exp / 3.f, &halfs_sunspec::totwhexpphb);
		modbus->fronius_server.write(wh_exp / 3.f, &halfs_sunspec::totwhexpphc);
		modbus->fronius_server.write(wh_imp / 3.f, &halfs_sunspec::totwhimppha);
		modbus->fronius_server.write(wh_imp / 3.f, &halfs_sunspec::totwhimpphb);
		modbus->fronius_server.write(wh_imp / 3.f, &halfs_sunspec::totwhimpphc);
		modbus->fronius_server.write(wh_exp, &halfs_sunspec::totvahexp);
		modbus->fronius_server.write(wh_exp / 3.f, &halfs_sunspec::totvahexppha);
		modbus->fronius_server.write(wh_exp / 3.f, &halfs_sunspec::totvahexpphb);
		modbus->fronius_server.write(wh_exp / 3.f, &halfs_sunspec::totvahexpphc);
		modbus->fronius_server.write(wh_imp, &halfs_sunspec::totvahimp);
		modbus->fronius_server.write(wh_imp / 3.f, &halfs_sunspec::totvahimppha);
		modbus->fronius_server.write(wh_imp / 3.f, &halfs_sunspec::totvahimpphb);
		modbus->fronius_server.write(wh_imp / 3.f, &halfs_sunspec::totvahimpphc);
		open_bracket_count = 0;
		LogInfo("########  Updated meter values ##########");
		wake_up_update_task();
	}

	return 0;
}
#undef TRY_EXTRACT
#undef TRY_EXTRACTE

err_t tcp_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len) {
	return 0;
}

err_t tcp_client_close() { 
	err_t err = ERR_OK;
	if (client_pcb) {
		tcp_arg(client_pcb, NULL);
		tcp_poll(client_pcb, NULL, 0);
		tcp_sent(client_pcb, NULL);
		tcp_recv(client_pcb, NULL);
		tcp_err(client_pcb, NULL);
		if (tcp_close(client_pcb) != ERR_OK) {
			LogError("Close failed on pcb, calling abort");
			tcp_abort(client_pcb);
			err = ERR_ABRT;
		}
		client_pcb = {};
		connected = false;
	}; 
	wake_up_update_task();
	return err;
}

