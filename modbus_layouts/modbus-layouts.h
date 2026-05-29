#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

constexpr uint16_t modbus_swap(uint16_t v) { return (v >> 8) | ((v & 0xff) << 8) ; }

/**
* Definition of the fronius sunspec-meter model with all registers needed for operation.
*/
#pragma pack(push, 1)
struct halfs_sunspec {
	constexpr static int OFFSET = 40000;
	std::array<char, 4> sid{'S','u','n','S'};
	uint16_t id = modbus_swap(1);
	uint16_t l_header = modbus_swap(65);
	std::array<char,32> manufacturer{"Lachei"};
	std::array<char,32> device_model{"100.100.100"};
	std::array<char,16> options{""};
	std::array<char,16> sw_meter_version{"1.0"};
	std::array<char,32> sn{"meter1"};
	uint16_t modbus_device_address = modbus_swap(1); // NOTE: has to be adopted
	/* meter data*/
	uint16_t modbus_map = modbus_swap(213);  // three phase map id
	uint16_t l_data = modbus_swap(124);      // register length
	float a{};
	float apha{};
	float aphb{};
	float aphc{};
	float phv{};
	float phvpha{};
	float phvphb{};
	float phvphc{};
	float ppv{};
	float ppvphab{};
	float ppvphbc{};
	float ppvphca{};
	float hz{};
	float w{};
	float wpha{};
	float wphb{};
	float wphc{};
	float va{};
	float vapha{};
	float vaphb{};
	float vaphc{};
	float var{};
	float varpha{};
	float varphb{};
	float varphc{};
	float pf{};
	float pfpha{};
	float pfphb{};
	float pfphc{};
	float totwhexp{};
	float totwhexppha{};
	float totwhexpphb{};
	float totwhexpphc{};
	float totwhimp{};
	float totwhimppha{};
	float totwhimpphb{};
	float totwhimpphc{};
	float totvahexp{};
	float totvahexppha{};
	float totvahexpphb{};
	float totvahexpphc{};
	float totvahimp{};
	float totvahimppha{};
	float totvahimpphb{};
	float totvahimpphc{};
	/* unsupported registers*/
	float totvarhimpq1{};
	float totvarhimpq1pha{};
	float totvarhimpq1phb{};
	float totvarhimpq1phc{};
	float totvarhimpq2{};
	float totvarhimpq2pha{};
	float totvarhimpq2phb{};
	float totvarhimpq2phc{};
	float totvarhimpq3{};
	float totvarhimpq3pha{};
	float totvarhimpq3phb{};
	float totvarhimpq3phc{};
	float totvarhimpq4{};
	float totvarhimpq4pha{};
	float totvarhimpq4phb{};
	float totvarhimpq4phc{};
	/* unsupported end*/
	uint32_t events{0};
	/* end block*/
	uint16_t end_id{0xffff};
	uint16_t l_end{0};
};

struct halfs_eastron {
	constexpr static int OFFSET = 0;
	float phase_1_neutral_volts;
	float phase_2_neutral_volts;
	float phase_3_neutral_volts;
	float phase_1_current;
	float phase_2_current;
	float phase_3_current;
	float phase_1_active_power;
	float phase_2_active_power;
	float phase_3_active_power;
	float phase_1_apparent_power;
	float phase_2_apparent_power;
	float phase_3_apparent_power;
	float phase_1_reactive_power;
	float phase_2_reactive_power;
	float phase_3_reactive_power;
	float phase_1_power_factor;
	float phase_2_power_factor;
	float phase_3_power_factor;
	float phase_1_filler;
	float phase_2_filler;
	float phase_3_filler;
	float average_line_to_neutral_volts;
	float average_filler;
	float average_line_current;
	float sum_of_line_currents;
	float sum_filler;
	float total_system_power;
	float total_system_filler;
	float total_system_volt_amps;
	float total_system_f_iller;
	float total_system_VAr;
	float total_system_power_factor;
	float fill0;
	float fill1;
	float filler2;
	float frequency_of_supply_voltage;
	float import_active_energy;
	float export_active_energy;
	// offset 60

	uint16_t space0[124];
	// offset 200
	float line_1_to_line_2_volts;
	float line_2_to_line_3_volts;
	float line_3_to_line_1_volts;
	float average_line_to_line_volts;
	// offset 208

	// unused, as not really mapped
	// uint16_t space1[16];
	// // start offset 224
	// float neutral_current;
	// // offset 226
	// 
	// uint16_t space2[116];
	// // start offset 342
	// float total_active_energy;
	// float total_reactive_energy;
	// // offset 346

	// uint16_t space3[934];
	// // start offset 1280
	// float total_import_active_power;
	// float total_export_active_power;
};
static_assert(offsetof(halfs_eastron, line_1_to_line_2_volts) / 2 == 200);
// static_assert(offsetof(halfs_eastron, neutral_current) / 2 == 224);
// static_assert(offsetof(halfs_eastron, total_active_energy) / 2 == 342);
// static_assert(offsetof(halfs_eastron, total_import_active_power) / 2 == 1280);
#pragma pack(pop)

struct sunspec_layout {
	halfs_sunspec halfs_registers{};
};
struct eastron_layout {
	halfs_eastron halfs_registers{};
};


