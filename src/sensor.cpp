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

#include "scd30/sensor.h"

#include <Arduino.h>
#include <sys/time.h>
#include <strings.h>

#include <algorithm>
#include <bitset>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <functional>
#include <string>
#include <vector>

#include <uuid/common.h>
#include <uuid/log.h>

#include <scd30/config.h>

static const char __pstr__logger_name[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = "sensor";

namespace scd30 {

uuid::log::Logger Sensor::logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::DAEMON};
std::bitset<sizeof(Operation) * 8> Sensor::config_operations_;

Sensor::Sensor(::HardwareSerial &device, int ready_pin) : client_(device), ready_pin_(ready_pin) {
	config_operations_.set(Operation::CONFIG_AUTOMATIC_CALIBRATION);
	config_operations_.set(Operation::CONFIG_TEMPERATURE_OFFSET);
	config_operations_.set(Operation::CONFIG_ALTITUDE_COMPENSATION);
	config_operations_.set(Operation::CONFIG_CONTINUOUS_MEASUREMENT);
	client_.default_unicast_timeout_ms(TIMEOUT_MS);
}

void Sensor::start() {
	pending_operations_.set(Operation::READ_FIRMWARE_VERSION);
	config();
}

void Sensor::config(std::initializer_list<Operation> operations) {
	Config config;

	if (operations.size() == 0) {
		pending_operations_ |= config_operations_;
	} else {
		for (auto operation : operations) {
			if (config_operations_[operation]) {
				pending_operations_.set(operation);
			}
		}
	}

	interval_ = std::max(0UL, std::min((unsigned long)UINT8_MAX, config.take_measurement_interval()));
}

void Sensor::reset(uint32_t wait_ms) {
	pending_operations_.reset();
	pending_operations_.set(Operation::SOFT_RESET);
	start();
	reset_start_ms_ = ::millis();
	reset_wait_ms_ = wait_ms;
}

void Sensor::loop() {
	client_.loop();

retry:
	switch (current_operation_) {
	case Operation::NONE:
		if (pending_operations_.any()) {
			int bit = ffs(pending_operations_.to_ulong());
			if (bit != 0) {
				current_operation_ = static_cast<Operation>(bit - 1);
				pending_operations_.reset(current_operation_);
			}
			goto retry;
		}
		break;

	case Operation::SOFT_RESET:
		if (!response_) {
			if (::millis() - reset_start_ms_ >= reset_wait_ms_) {
				logger_.debug(F("Restarting sensor"));
				response_ = client_.write_holding_register(DEVICE_ADDRESS, SOFT_RESET_ADDRESS, 0x0001);
			}
		} else if (response_->done()) {
			auto write_response = std::static_pointer_cast<const uuid::modbus::RegisterWriteResponse>(response_);

			if (write_response->data().size() < 1 || write_response->data()[0] != 0x0001) {
				logger_.warning(F("Failed to restart sensor"));
				reset();
			} else {
				logger_.info(F("Restarted sensor"));
			}

			response_.reset();
			current_operation_ = Operation::NONE;
		}
		break;

	case Operation::READ_FIRMWARE_VERSION:
		if (!response_) {
			logger_.debug(F("Reading firmware version"));
			response_ = client_.read_holding_registers(DEVICE_ADDRESS, FIRMWARE_VERSION_ADDRESS, 1);
		} else if (response_->done()) {
			auto response = std::static_pointer_cast<const uuid::modbus::RegisterDataResponse>(response_);

			if (response->data().size() < 1) {
				logger_.warning(F("Failed to read firmware version"));
				reset();
			} else {
				firmware_major_ = response->data()[0] >> 8;
				firmware_minor_ = response->data()[0] & 0xFF;
				logger_.debug(F("Firmware version: %u.%u"), firmware_major_, firmware_minor_);
			}

			response_.reset();
			current_operation_ = Operation::NONE;
		}
		break;

	case Operation::CONFIG_AUTOMATIC_CALIBRATION:
		static const auto bool_value_str = [] (uint16_t value) -> std::string {
				return uuid::read_flash_string(value ? F("enabled") : F("disabled"));
			};
		static const auto bool_set_value_str = [] (uint16_t value) -> std::string {
				return uuid::read_flash_string(value ? F("Enabling") : F("Disabling"));
			};

		update_config_register(F("automatic calibration"), ASC_CONFIG_ADDRESS,
			&automatic_calibration, bool_value_str, bool_set_value_str);
		break;

	case Operation::CONFIG_TEMPERATURE_OFFSET:
		static const auto temp_value_str = [] (uint16_t value) -> std::string {
				std::vector<char> text(9 + 1);
				if (snprintf_P(text.data(), text.size(), PSTR("%u.%02u°C"), value / 100, value % 100) <= 0) {
					return uuid::read_flash_string(F("?"));
				}
				return text.data();
			};

		update_config_register(F("temperature offset"), TEMPERATURE_OFFSET_ADDRESS,
			&temperature_offset, temp_value_str);
		break;

	case Operation::CONFIG_ALTITUDE_COMPENSATION:
		static const auto alt_value_str = [] (uint16_t value) -> std::string {
				std::vector<char> text(6 + 1);
				if (snprintf_P(text.data(), text.size(), PSTR("%um"), value) <= 0) {
					return uuid::read_flash_string(F("?"));
				}
				return text.data();
			};

		update_config_register(F("altitude compensation"), ALTITUDE_COMPENSATION_ADDRESS,
			&altitude_compensation, alt_value_str);
		break;

	case Operation::CONFIG_CONTINUOUS_MEASUREMENT:
		static const auto secs_value_str = [] (uint16_t value) -> std::string {
				std::vector<char> text(6 + 1);
				if (snprintf_P(text.data(), text.size(), PSTR("%us"), value) <= 0) {
					return uuid::read_flash_string(F("?"));
				}
				return text.data();
			};

		update_config_register(F("measurement interval"), MEASUREMENT_INTERVAL_ADDRESS,
			&measurement_interval, secs_value_str);
		break;
	}
}

void Sensor::update_config_register(const __FlashStringHelper *name,
		const uint16_t address,
		const std::function<uint16_t ()> &func_cfg_value,
		const std::function<std::string (uint16_t)> &func_value_str,
		const std::function<std::string (uint16_t)> &func_bool_cfg_str) {
	if (!response_) {
		logger_.debug(F("Reading %S configuration"), name);
		response_ = client_.read_holding_registers(DEVICE_ADDRESS, address, 1);
	} else if (response_->done()) {
		auto read_response = std::dynamic_pointer_cast<const uuid::modbus::RegisterDataResponse>(response_);
		auto write_response = std::dynamic_pointer_cast<const uuid::modbus::RegisterWriteResponse>(response_);

		if (write_response) {
			if (write_response->data().size() < 1) {
				logger_.err(F("Failed to write %S configuration"), name);
				reset();
			} else {
				std::string name_title = uuid::read_flash_string(name);

				name_title[0] = ::toupper(name_title[0]);

				logger_.info(F("%s %s"), name_title.c_str(), func_value_str(read_response->data()[0]).c_str());
			}
		} else if (read_response) {
			if (read_response->data().size() < 1) {
				logger_.err(F("Failed to read %S configuration"), name);
				reset();
			} else {
				const uint16_t value = func_cfg_value();

				if (read_response->data()[0] == value) {
					std::string name_title = uuid::read_flash_string(name);

					name_title[0] = ::toupper(name_title[0]);

					logger_.debug(F("%s %s"), name_title.c_str(), func_value_str(read_response->data()[0]).c_str());
				} else {
					if (func_bool_cfg_str) {
						logger_.info(F("%S %s"), name, func_bool_cfg_str(value).c_str());
					} else {
						logger_.info(F("Setting %S to %s"), name, func_value_str(value).c_str());
					}
					response_ = client_.write_holding_register(DEVICE_ADDRESS, address, value);
					return;
				}
			}
		}

		response_.reset();
		current_operation_ = Operation::NONE;
	}
}

uint16_t Sensor::automatic_calibration() {
	return Config().sensor_automatic_calibration() ? 0x0001 : 0x0000;
}

uint16_t Sensor::temperature_offset() {
	return std::max(0UL, std::min((unsigned long)UINT16_MAX, Config().sensor_temperature_offset()));
}

uint16_t Sensor::altitude_compensation() {
	return std::max(0UL, std::min((unsigned long)UINT16_MAX, Config().sensor_altitude_compensation()));
}

uint16_t Sensor::measurement_interval() {
	return std::max(2UL, std::min(1800UL, Config().sensor_measurement_interval()));
}

} // namespace scd30
