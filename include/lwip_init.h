#pragma once

static int ON = 1;
static int OFF = 0;

void init_lwip();
void board_led_set(int on);

// ----------------------------------------------------------------------------------------------------------------
#if CYW43_LWIP
// ----------------------------------------------------------------------------------------------------------------

#include "pico/cyw43_arch.h"

// cyw43 initialization
inline void lwip_init() {
	if (cyw43_arch_init()) {
		for (;;) {
			vTaskDelay(1000);
			LogError("failed to initialize\n");
			std::cout << "failed to initialize arch (probably ram problem, increase ram size)\n";
		}
	}
	cyw43_arch_enable_sta_mode();
}

inline void lwip_lock() {
	cyw43_arch_lwip_begin();
}

inline void lwip_unlock() {
	cyw43_arch_lwip_end();
}

inline struct netif* get_netif() {
	return &cyw43_state.netif[CYW43_ITF_STA];
}

inline void board_led_set(int on) {
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

// ----------------------------------------------------------------------------------------------------------------
#elif WIZNET_LWIP
// ----------------------------------------------------------------------------------------------------------------

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
extern "C" {
#include "wizchip_conf.h"
#include "socket.h"
#include "w5x00_spi.h"
#include "w5x00_lwip.h"
}

extern uint8_t mac[6];

static std::array<uint8_t, 1500> pack;

inline struct netif* get_netif() {
	static struct netif g_netif;
	return &g_netif;
}
void wiznet_poll_task(void*) {
	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(1));
		uint16_t pack_len{};
		getsockopt(0, SO_RECVBUF, &pack_len);

		if (pack_len == 0)
			continue;
		pack_len = recv_lwip(0, pack.data(), pack_len);
		if (pack_len == 0)
			continue;

		struct pbuf* p = pbuf_alloc(PBUF_RAW, pack_len, PBUF_POOL);
		pbuf_take(p, pack.data(), pack_len);
		if (!p)
			continue;

		LINK_STATS_INC(link.recv);

                if (get_netif()->input(p, get_netif()) != ERR_OK)
                    pbuf_free(p);
	}
}
void init_cb(void* d) { 
	xTaskNotifyGive((TaskHandle_t)d);
}

inline void lwip_init() {
	// system clock init
	constexpr int PLL_SYS_KHZ = 133'000;
	set_sys_clock_khz(PLL_SYS_KHZ, true);

	// configure the specified clock
	clock_configure(
		clk_peri,
		0,                                                // No glitchless mux
		CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
		PLL_SYS_KHZ * 1000,                               // Input frequency
		PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
	);

	// wiznet initialization ------------------------------------------------------------------------------------------------
	wizchip_spi_initialize();
	wizchip_cris_initialize();

	wizchip_reset();
	wizchip_initialize();
	wizchip_check();

	setSHAR(mac);
	ctlwizchip(CW_RESET_PHY, 0);

	// init lwip ------------------------------------------------------------------------------------------------------------
	std::cout << "Initing tcpip\n";
	tcpip_init(init_cb, (void*)xTaskGetCurrentTaskHandle()); // init in os mode
	ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1'000));
	std::cout << "Initing tcpip done\n";
	
	struct netif &g_netif = *get_netif();
	netif_add(&g_netif, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, netif_initialize, tcpip_input);
	g_netif.name[0] = 'e';
	g_netif.name[1] = '0';

	// Assign callbacks for link and status
	netif_set_link_callback(&g_netif, netif_link_callback);
	netif_set_status_callback(&g_netif, netif_status_callback);

	// MACRAW socket open
	err_t retval = socket(0, Sn_MR_MACRAW, 5001, 0x00);

	if (retval < 0)
	{
		std::cout << "Socket open failed: " << retval << std::endl;
		LogError("MACRAW socket open failed");
	}

	// Set the default interface and bring it up
	netif_set_default(&g_netif);
	netif_set_link_up(&g_netif);
	netif_set_up(&g_netif);

	xTaskCreate(wiznet_poll_task, "wiz_poll", 2048, NULL, 1, NULL);

	dhcp_start(&g_netif);
}

inline void lwip_lock() {
}

inline void lwip_unlock() {
}

inline void board_led_set(int on) {
	static bool init = []{
		gpio_init(PICO_DEFAULT_LED_PIN);
		gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
		return true;
	}();
	gpio_put(PICO_DEFAULT_LED_PIN, on);
}

// ----------------------------------------------------------------------------------------------------------------

#endif
