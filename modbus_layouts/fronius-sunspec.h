#pragma once
#include <array>
#include <cstdint>

constexpr uint16_t modbus_swap(uint16_t v) { return (v >> 8) | ((v & 0xff) << 8) ; }

namespace fronius_meter {

/**
* Definition of the fronius sunspec-meter model with all registers needed for operation.
*/
#pragma pack(push, 1)
struct halfs_layout {
	constexpr static int OFFSET = 40000;
	std::array<char, 4> sid{'S','u','n','S'};
	uint16_t id = modbus_swap(1);
	uint16_t l_header = modbus_swap(65);
	std::array<char,32> manufacturer{"Lachei"};
	std::array<char,32> device_model{"100.100.100"};
	std::array<char,16> options{""};
	std::array<char,16> sw_meter_version{"1.0"};
	std::array<char,32> sn{"1234"};
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
#pragma pack(pop)

struct layout {
	halfs_layout halfs_registers{};
};

}

