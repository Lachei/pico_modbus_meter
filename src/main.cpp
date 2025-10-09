/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

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
#include "modbus_server_client.h"
#include "http_solarapi.h"

namespace fm = fronius_meter;

#define TEST_TASK_PRIORITY ( tskIDLE_PRIORITY + 1UL )

constexpr UBaseType_t STANDARD_TASK_PRIORITY = tskIDLE_PRIORITY + 1ul;
constexpr UBaseType_t CONTROL_TASK_PRIORITY = tskIDLE_PRIORITY + 10ul;

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

    wifi_storage::Default().update_hostname();

    for (;;) {
        LogInfo("Wifi update loop");
        wifi_storage::Default().update_wifi_connection();
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, wifi_storage::Default().wifi_connected);
        wifi_storage::Default().update_hostname();
        wifi_storage::Default().update_scanned();
        if (wifi_storage::Default().wifi_connected)
            ntp_client::Default().update_time();
        vTaskDelay(wifi_storage::Default().wifi_connected ? 5000: 1000);
    }
}

void update_meter_task(void *) {
    LogInfo("Update meter task started");
    for (;;) {
        LogInfo("Trying");
        if (wifi_storage::Default().wifi_connected) {
            LogInfo("Updating");
            update_meter_values(modbus_server_client::Default());
        }
        vTaskDelay(500);
    }
}

// task to initailize everything and only after initialization startin all other threads
// cyw43 init has to be done in freertos task because it utilizes freertos synchronization variables
void startup_task(void *) {
    LogInfo("Starting initialization");
    std::cout << "Starting initialization\n";
    if (cyw43_arch_init()) {
        for (;;) {
            vTaskDelay(1000);
            LogError("failed to initialize\n");
            std::cout << "failed to initialize arch (probably ram problem, increase ram size)\n";
        }
    }
    wifi_storage::Default();
    cyw43_arch_enable_sta_mode();
    Webserver().start();
    modbus_server_client::Default().start();

    modbus_server_client::Default().fronius_server.write(230.f, &fm::halfs_layout::phv);
    modbus_server_client::Default().fronius_server.write(230.f, &fm::halfs_layout::phvpha);
    modbus_server_client::Default().fronius_server.write(230.f, &fm::halfs_layout::phvphb);
    modbus_server_client::Default().fronius_server.write(230.f, &fm::halfs_layout::phvphc);
    modbus_server_client::Default().fronius_server.write(400.f, &fm::halfs_layout::ppv);
    modbus_server_client::Default().fronius_server.write(400.f, &fm::halfs_layout::ppvphab);
    modbus_server_client::Default().fronius_server.write(400.f, &fm::halfs_layout::ppvphbc);
    modbus_server_client::Default().fronius_server.write(400.f, &fm::halfs_layout::ppvphca);
    modbus_server_client::Default().fronius_server.write(50.f, &fm::halfs_layout::hz);
    modbus_server_client::Default().fronius_server.write(1.f, &fm::halfs_layout::pf);
    modbus_server_client::Default().fronius_server.write(1.f, &fm::halfs_layout::pfpha);
    modbus_server_client::Default().fronius_server.write(1.f, &fm::halfs_layout::pfphb);
    modbus_server_client::Default().fronius_server.write(1.f, &fm::halfs_layout::pfphc);

    LogInfo("Ready, running http at {}", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    LogInfo("Initialization done");
    std::cout << "Initialization done, get all further info via the commands shown in 'help'\n";
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    TaskHandle_t task_usb_comm;
    TaskHandle_t task_update_wifi;
    auto err = xTaskCreate(usb_comm_task, "usb_comm", 512, NULL, 1, &task_usb_comm);	// usb task also has to be started only after cyw43 init as some wifi functions are available
    if (err != pdPASS)
        LogError("Failed to start usb communication task with code {}" ,err);
    err = xTaskCreate(wifi_search_task, "UpdateWifiThread", 512, NULL, 1, &task_update_wifi);
    if (err != pdPASS)
        LogError("Failed to start usb communication task with code {}" ,err);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    update_meter_task(nullptr);
}

int main( void )
{
    stdio_init_all();

    LogInfo("Starting FreeRTOS on all cores.");
    std::cout << "Starting FreeRTOS on all cores\n";

    TaskHandle_t task_startup;
    xTaskCreate(startup_task, "StartupThread", 512, NULL, 1, &task_startup);

    vTaskStartScheduler();
    return 0;
}
