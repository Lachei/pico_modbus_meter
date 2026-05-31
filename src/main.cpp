/**
 * Copyright (c) 2026 Josef Stumpfegger
 * MIT License
 */

#include "pico/stdlib.h"
#include "hardware/timer.h"

#include "lwip/ip4_addr.h"
#include "lwip/apps/mdns.h"

#include "FreeRTOS.h"
#include "task.h"

#include "log_storage.h"
#include "access_point.h"
#include "wifi_storage.h"
#include "webserver.h"
#include "usb_interface.h"
#include "settings.h"
#include "measurements.h"
#include "crypto_storage.h"
#include "ntp_client.h"
#include "eastron_modbus.h"
#include "sunspec_modbus.h"
#include "lwip_init.h"

inline uint32_t time_s() { return time_us_64() / 1000000;  }

void usb_comm_task(void *) {
	LogInfo("Usb communication task");
	crypto_storage::Default();

	for (;;) {
		handle_usb_command();
	}
}

void wifi_search_task(void *) {
	LogInfo("Wifi task started");
	if (wifi_storage::Default().ssid_wifi.empty()) // onyl start the access point by default if no normal wifi connection is set
		access_point::Default().init();

	constexpr uint32_t ap_timeout = 10;
	uint32_t cur_time = time_s();
	uint32_t last_conn = cur_time;

	for (;;) {
		cur_time = time_s();
		uint32_t dt = cur_time - last_conn;
		wifi_storage::Default().update_hostname();
		wifi_storage::Default().update_wifi_connection();
		if (wifi_storage::Default().wifi_connected)
			last_conn = cur_time;
		if (dt % 30 == 5) // every 30 seconds enable reconnect try
			wifi_storage::Default().wifi_changed = true;
		if (dt > ap_timeout) {
			access_point::Default().init();
			board_led_set(cur_time & 1);
		} else {
			board_led_set(wifi_storage::Default().wifi_connected);
		}
		wifi_storage::Default().update_scanned();
		if (wifi_storage::Default().wifi_connected)
			ntp_client::Default().update_time();
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void update_meter_task(void *) {
	LogInfo("Update eastron values task started");
	ls::modbus_actor<eastron_layout, rtu_io>& e = g::eastron_modbus();
	ls::modbus_actor<sunspec_layout, tcp_io>& s = g::sunspec_modbus();
	for (;;) {
		int ms_s = time_us_64() / 1000;
		// fetch values
		e.read_remote(1, &halfs_eastron::phase_1_neutral_volts, &halfs_eastron::export_active_energy);
		e.read_remote(1, &halfs_eastron::line_1_to_line_2_volts, &halfs_eastron::average_line_to_line_volts);

		// write to sunspec modbus
		{
			scoped_lock lock{g::sunspec_mutex()};
			s.write(e.read(&halfs_eastron::phase_1_neutral_volts), 	&halfs_sunspec::phvpha);
			s.write(e.read(&halfs_eastron::phase_2_neutral_volts), 	&halfs_sunspec::phvphb);
			s.write(e.read(&halfs_eastron::phase_3_neutral_volts), 	&halfs_sunspec::phvphc);
			s.write(e.read(&halfs_eastron::phase_1_current), 	&halfs_sunspec::apha);
			s.write(e.read(&halfs_eastron::phase_2_current), 	&halfs_sunspec::aphb);
			s.write(e.read(&halfs_eastron::phase_3_current), 	&halfs_sunspec::aphc);
			s.write(e.read(&halfs_eastron::phase_1_active_power), 	&halfs_sunspec::wpha);
			s.write(e.read(&halfs_eastron::phase_2_active_power), 	&halfs_sunspec::wphb);
			s.write(e.read(&halfs_eastron::phase_3_active_power), 	&halfs_sunspec::wphc);
			s.write(e.read(&halfs_eastron::phase_1_apparent_power), &halfs_sunspec::vapha);
			s.write(e.read(&halfs_eastron::phase_2_apparent_power), &halfs_sunspec::vaphb);
			s.write(e.read(&halfs_eastron::phase_3_apparent_power), &halfs_sunspec::vaphc);
			s.write(e.read(&halfs_eastron::phase_1_reactive_power), &halfs_sunspec::varpha);
			s.write(e.read(&halfs_eastron::phase_2_reactive_power), &halfs_sunspec::varphb);
			s.write(e.read(&halfs_eastron::phase_3_reactive_power), &halfs_sunspec::varphc);
			s.write(e.read(&halfs_eastron::phase_1_power_factor), 	&halfs_sunspec::pfpha);
			s.write(e.read(&halfs_eastron::phase_2_power_factor), 	&halfs_sunspec::pfphb);
			s.write(e.read(&halfs_eastron::phase_3_power_factor), 	&halfs_sunspec::pfphc);
			s.write(e.read(&halfs_eastron::line_1_to_line_2_volts), &halfs_sunspec::ppvphab);
			s.write(e.read(&halfs_eastron::line_2_to_line_3_volts), &halfs_sunspec::ppvphbc);
			s.write(e.read(&halfs_eastron::line_3_to_line_1_volts), &halfs_sunspec::ppvphca);

			s.write(e.read(&halfs_eastron::average_line_to_neutral_volts), &halfs_sunspec::phv);
			s.write(e.read(&halfs_eastron::average_line_current), 		&halfs_sunspec::a);
			s.write(e.read(&halfs_eastron::total_system_power), 		&halfs_sunspec::w);
			s.write(e.read(&halfs_eastron::total_system_volt_amps), 	&halfs_sunspec::va);
			s.write(e.read(&halfs_eastron::total_system_VAr), 		&halfs_sunspec::var);
			s.write(e.read(&halfs_eastron::total_system_power_factor), 	&halfs_sunspec::pf);
			s.write(e.read(&halfs_eastron::frequency_of_supply_voltage), 	&halfs_sunspec::hz);
			s.write(e.read(&halfs_eastron::average_line_to_line_volts), 	&halfs_sunspec::ppv);
			s.write(e.read(&halfs_eastron::import_active_energy) * 1e3f, 	&halfs_sunspec::totwhimp);
			s.write(e.read(&halfs_eastron::export_active_energy) * 1e3f, 	&halfs_sunspec::totwhexp);
		}

		// wait remaining 250ms for next iteration
		int ms_e = time_us_64() / 1000;
		vTaskDelay(pdMS_TO_TICKS(std::max(0, 250 - (ms_e - ms_s))));
	}
}

// task to initailize everything and only after initialization startin all other threads
// cyw43 init has to be done in freertos task because it utilizes freertos synchronization variables
void startup_task(void *) {
	LogInfo("Starting initialization");
	std::cout << "Starting initialization\n";
	get_netif();
	lwip_init();
	wifi_storage::Default().update_hostname();
	Webserver().start();
	LogInfo("Initialization done");

	std::cout << "Initialization done, get all further info via the commands shown in 'help'\n";
	board_led_set(ON);
	xTaskCreate(usb_comm_task, "usb_comm", 512, NULL, 1, NULL);	// usb task also has to be started only after cyw43 init as some wifi functions are available
	xTaskCreate(wifi_search_task, "UpdateWifiThread", 512, NULL, 1, NULL);
	board_led_set(OFF);
	update_meter_task(nullptr);
}

int main( void )
{
	stdio_init_all();

	LogInfo("Starting FreeRTOS on all cores.");
	std::cout << "Starting FreeRTOS on all cores\n";

	xTaskCreate(startup_task, "StartupThread", 512, NULL, 1, NULL);

	vTaskStartScheduler();
	return 0;
}
