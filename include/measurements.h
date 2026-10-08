#pragma once

#include <iostream>

#include "static_types.h"
#include "eastron_modbus.h"

struct measurements {
	static measurements& Default() {
		static measurements m{};
		return m;
	}
};

/** @brief prints formatted for monospace output, eg. usb */
std::ostream& operator<<(std::ostream &os, const measurements &m) {
	auto &e = g::eastron_modbus();
	os << "Watt usage: " << e.read(&halfs_eastron::total_system_power) << '\n';
	os << "P1 V      : " << e.read(&halfs_eastron::phase_1_neutral_volts) << '\n';
	os << "Hz        : " << e.read(&halfs_eastron::frequency_of_supply_voltage) << '\n';
	return os;
}

