#pragma once

#include <iostream>

#include "log_storage.h"
#include "static_types.h"
#include "persistent_storage.h"
#include "ranges_util.h"

struct wifi_storage {
	static constexpr uint32_t DISCOVER_TIMEOUT_US = 6e6; // 6 seconds

	struct wifi_info {
		static_string<256> ssid{};
		int rssi{};
		uint64_t last_seen_us{};
	};
	static_vector<wifi_info, 8> wifis{};
	static wifi_storage& Default() {
		static wifi_storage storage{};
		[[maybe_unused]] static bool inited = [](){ storage.load_from_persistent_storage(); return true; }();
		return storage;
	}
	bool wifi_changed{true};
	bool wifi_connected{false};
	static_string<64> ssid_wifi{};
	static_string<64> pwd_wifi{};
	bool hostname_inited{false};
	bool hostname_changed{true};
	static_string<64> hostname{"DcDcConverter"};
	static_string<64> mdns_service_name{"lachei_tcp_server"};

	void update_hostname() {
		if (!hostname_changed)
			return;

		LogInfo("Hostname change detected, adopting hostname");
		netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], hostname.data());
		if (!hostname_inited) {
			mdns_resp_init(); 
			mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], hostname.data());
			mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], mdns_service_name.data(), "_http", DNSSD_PROTO_TCP, 80, _mdns_response_callback, NULL);
		} else {
			mdns_resp_rename_netif(&cyw43_state.netif[CYW43_ITF_STA], hostname.data());
		}
		
		hostname_inited = true;
		hostname_changed = false;
	}

	void update_wifi_connection() {
		wifi_connected = CYW43_LINK_UP == cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
		bool wifi_available = wifis | contains{ssid_wifi.sv(), [](const wifi_info &w) { return w.ssid.sv(); }};
		if ((wifi_connected && !wifi_changed) || ssid_wifi.cur_size == 0 || pwd_wifi.cur_size < 8 || !wifi_available)
			return;

		if (wifi_changed) {
			cyw43_arch_disable_sta_mode();
			cyw43_arch_enable_sta_mode();
		}

		LogInfo("Connecting to wifi");
		if (PICO_OK != cyw43_arch_wifi_connect_timeout_ms(ssid_wifi.data(), pwd_wifi.data(), CYW43_AUTH_WPA2_AES_PSK, 5000)) {
			LogWarning("failed to connect, retry next update_wifi_connection_call()");
		}

		wifi_changed = false;
	}
	
	void update_scanned() {
		cyw43_wifi_scan_options_t scan_options = {0};
		if (0 != cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, _scan_result)) {
			LogError("Failed wifi scan");
			return;
		}

		// remove old wifis
		uint64_t cur_us = time_us_64();
		wifis.remove_if([cur_us](const auto &e){ return cur_us - e.last_seen_us > DISCOVER_TIMEOUT_US; });
	}

	void write_to_persistent_storage() {
		if (PICO_OK != persistent_storage_t::Default().write(hostname, &persistent_storage_layout::hostname))
			LogError("Failed to store hostname");
		if (PICO_OK != persistent_storage_t::Default().write(ssid_wifi, &persistent_storage_layout::ssid_wifi))
			LogError("Failed to store ssid_wifi");
		if (PICO_OK != persistent_storage_t::Default().write(pwd_wifi, &persistent_storage_layout::pwd_wifi))
			LogError("Failed to store pwd_wifi");
	}

	void load_from_persistent_storage() {
		persistent_storage_t::Default().read(&persistent_storage_layout::hostname, hostname);
		persistent_storage_t::Default().read(&persistent_storage_layout::ssid_wifi, ssid_wifi);
		persistent_storage_t::Default().read(&persistent_storage_layout::pwd_wifi, pwd_wifi);
		hostname.sanitize();
		hostname.make_c_str_safe();
		ssid_wifi.sanitize();
		ssid_wifi.make_c_str_safe();
		pwd_wifi.sanitize();
		pwd_wifi.make_c_str_safe();
		wifi_changed = true;
		hostname_changed = true;
		LogInfo("Loaded hostanme size: {}", hostname.size());
		LogInfo("Loaded ssid size: {}", ssid_wifi.size());
		LogInfo("Loaded pwd siz: {}", pwd_wifi.size());
	}

	/*INTERNAL*/ static int _scan_result(void *, const cyw43_ev_scan_result_t *result) {
		if (!result || result->ssid_len == 0)
			return 0;
		// check if already added
		std::string_view result_ssid{reinterpret_cast<const char *>(result->ssid), result->ssid_len};
		if (result_ssid.size() == 0)
			return 0;
		for (auto &wifi: wifi_storage::Default().wifis) {
			if (wifi.ssid.sv() == result_ssid) {
				wifi.rssi = (.8 * wifi.rssi) + (.2 * result->rssi);
				wifi.last_seen_us = time_us_64();
				return 0;
			}
		} 

		auto* wifi = wifi_storage::Default().wifis.push();
		if (!wifi) {
			LogError("Wifi storage overflow");
			return 0;
		}
		wifi->ssid.fill(result_ssid);
		wifi->rssi = result->rssi;
		wifi->last_seen_us = time_us_64();
		return 0;
	}
	/*INTERNAL*/ static void _mdns_response_callback(struct mdns_service *service, void*)
	{
		err_t res = mdns_resp_add_service_txtitem(service, "path=/", 6);
		if (res != ERR_OK)
			LogError("mdns add service txt failed");
	}
};

std::ostream& operator<<(std::ostream &os, const wifi_storage &w) {
	os << "Wifi connected: " << (w.wifi_connected ? "true": "false") << '\n';
	os << "Stored wifi ssid: " << w.ssid_wifi.sv() << '\n';
	os << "hostname: " << w.hostname.sv() << '\n';
	os << "mdns_service_name: " << w.mdns_service_name.sv() << '\n';
	os << "Amount of discovered wifis: " << w.wifis.size() << '\n';
	return os;
}


