#pragma once

constexpr int ON = 1;
constexpr int OFF = 0;

void init_lwip();
void board_led_set(int on);

// ----------------------------------------------------------------------------------------------------------------
#if CYW43_LWIP
// ----------------------------------------------------------------------------------------------------------------

#include "pico/cyw43_arch.h"

// cyw43 initialization
inline void init_lwip() {
	if (cyw43_arch_init()) {
		for (;;) {
			vTaskDelay(1000);
			LogError("failed to initialize\n");
			std::cout << "failed to initialize arch (probably ram problem, increase ram size)\n";
		}
	}
	cyw43_arch_enable_sta_mode();
}

inline lwip_lock() {

}

inline lwip_unlock() {

}

inline void board_led_set(int on) {
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

// ----------------------------------------------------------------------------------------------------------------
#elif WIZNET_LWIP
// ----------------------------------------------------------------------------------------------------------------

#include "pico/stdlib.h"

inline void init_lwip() {

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
#else
// ----------------------------------------------------------------------------------------------------------------

inline void init_lwip() {
	static_assert(false, "missing implementation for init_lwip");
}

#endif
