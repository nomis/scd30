/*
 * scd30 - SCD30 Monitor
 * Copyright 2022  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCD30_SENSOR_H_
#define SCD30_SENSOR_H_

#include <Arduino.h>

#include <bitset>
#include <cmath>
#include <initializer_list>
#include <functional>
#include <memory>
#include <string>

#include <uuid/log.h>
#include <uuid/modbus.h>

namespace scd30 {

enum Operation : uint8_t {
	NONE = 32,
	SOFT_RESET = 0,
	READ_FIRMWARE_VERSION,
	CONFIG_AUTOMATIC_CALIBRATION,
	CONFIG_TEMPERATURE_OFFSET,
	CONFIG_ALTITUDE_COMPENSATION,
	CONFIG_CONTINUOUS_MEASUREMENT,
	CONFIG_AMBIENT_PRESSURE,
	TAKE_MEASUREMENT,
};

enum Measurement : uint8_t {
	IDLE,
	PENDING,
	WAITING,
};

class Sensor {
public:
	static constexpr uint16_t MODBUS_TIMEOUT_MS = 100;
	static constexpr uint16_t RESET_PRE_DELAY_MS = 60000;
	static constexpr uint16_t RESET_POST_DELAY_MS = 5000;
	static constexpr uint16_t MEASUREMENT_TIMEOUT_MS = 30000;

	static constexpr uint8_t DEVICE_ADDRESS = 0x61;
	static constexpr uint16_t FIRMWARE_VERSION_ADDRESS = 0x0020;
	static constexpr uint16_t MEASUREMENT_INTERVAL_ADDRESS = 0x0025;
	static constexpr uint16_t MEASUREMENT_DATA_ADDRESS = 0x0028;
	static constexpr uint16_t SOFT_RESET_ADDRESS = 0x0034;
	static constexpr uint16_t AMBIENT_PRESSURE_ADDRESS = 0x0036;
	static constexpr uint16_t ALTITUDE_COMPENSATION_ADDRESS = 0x0038;
	static constexpr uint16_t ASC_CONFIG_ADDRESS = 0x003A;
	static constexpr uint16_t TEMPERATURE_OFFSET_ADDRESS = 0x003B;

	static constexpr float MINIMUM_CO2_PPM = 200;

	Sensor(::HardwareSerial &device, int ready_pin);
	void start();
	void config(std::initializer_list<Operation> operations = {});
	void reset(uint32_t wait_ms = RESET_PRE_DELAY_MS);
	void loop();

	inline std::string firmware_version() const {
		return std::to_string(firmware_major_) + '.' + std::to_string(firmware_minor_);
	}
	inline float temperature_c() const { return temperature_c_; }
	inline float relative_humidity_pc() const { return relative_humidity_pc_; }
	inline float co2_ppm() const { return co2_ppm_; }

private:
	static uint32_t current_time();
	static uint16_t automatic_calibration();
	static uint16_t temperature_offset();
	static uint16_t altitude_compensation();
	static uint16_t measurement_interval();
	static uint16_t ambient_pressure();

	void update_config_register(const __FlashStringHelper *name,
		const uint16_t address, const bool always_write,
		const std::function<uint16_t ()> &func_cfg_value,
		const std::function<std::string (uint16_t)> &func_value_str,
		const std::function<std::string (uint16_t)> &func_bool_value_str = std::function<std::string (uint16_t)>{});

	static uuid::log::Logger logger_;
	static std::bitset<sizeof(uint32_t) * 8> config_operations_;

	uuid::modbus::SerialClient client_;
	int ready_pin_;
	uint8_t interval_ = 0;
	std::bitset<sizeof(uint32_t) * 8> pending_operations_;
	Operation current_operation_ = Operation::NONE;
	std::shared_ptr<const uuid::modbus::Response> response_;

	uint32_t reset_start_ms_;
	uint32_t reset_wait_ms_;
	bool reset_complete_;

	uint32_t last_reading_s_ = 0;
	uint32_t measurement_start_ms_;
	Measurement measurement_status_ = Measurement::IDLE;

	uint8_t firmware_major_ = 0;
	uint8_t firmware_minor_ = 0;
	float temperature_c_ = NAN;
	float relative_humidity_pc_ = NAN;
	float co2_ppm_ = NAN;
};

} // namespace scd30

#endif
