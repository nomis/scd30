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
#include <cstdint>
#include <initializer_list>

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
		if (!response_) {
			logger_.debug(F("Reading automatic calibration configuration"));
			response_ = client_.read_holding_registers(DEVICE_ADDRESS, ASC_CONFIG_ADDRESS, 1);
		} else if (response_->done()) {
			auto read_response = std::dynamic_pointer_cast<const uuid::modbus::RegisterDataResponse>(response_);
			auto write_response = std::dynamic_pointer_cast<const uuid::modbus::RegisterWriteResponse>(response_);

			if (write_response) {
				if (write_response->data().size() < 1) {
					logger_.err(F("Failed to write automatic calibration configuration"));
					reset();
				} else {
					logger_.info(F("Automatic calibration %S"), write_response->data()[0] ? F("enabled") : F("disabled"));
				}
			} else if (read_response) {
				if (read_response->data().size() < 1) {
					logger_.err(F("Failed to read automatic calibration configuration"));
					reset();
				} else {
					const uint16_t value = automatic_calibration();

					if (read_response->data()[0] == value) {
						logger_.debug(F("Automatic calibration %S"), value ? F("enabled") : F("disabled"));
					} else {
						logger_.info(F("%S automatic calibration"), value ? F("Enabling") : F("Disabling"));
						response_ = client_.write_holding_register(DEVICE_ADDRESS, ASC_CONFIG_ADDRESS, value);
						return;
					}
				}
			}

			response_.reset();
			current_operation_ = Operation::NONE;
		}
		break;

	case Operation::CONFIG_TEMPERATURE_OFFSET:
		if (!response_) {
			logger_.debug(F("Reading temperature offset configuration"));
			response_ = client_.read_holding_registers(DEVICE_ADDRESS, TEMPERATURE_OFFSET_ADDRESS, 1);
		} else if (response_->done()) {
			auto read_response = std::dynamic_pointer_cast<const uuid::modbus::RegisterDataResponse>(response_);
			auto write_response = std::dynamic_pointer_cast<const uuid::modbus::RegisterWriteResponse>(response_);

			if (write_response) {
				if (write_response->data().size() < 1) {
					logger_.err(F("Failed to write temperature offset configuration"));
					reset();
				} else {
					uint16_t major = write_response->data()[0] / 100;
					uint8_t minor = write_response->data()[0] % 100;

					logger_.info(F("Temperature offset %u.%02u°C"), major, minor);
				}
			} else if (read_response) {
				if (read_response->data().size() < 1) {
					logger_.err(F("Failed to read temperature offset configuration"));
					reset();
				} else {
					const uint16_t value = temperature_offset();

					if (read_response->data()[0] == value) {
						uint16_t major = read_response->data()[0] / 100;
						uint8_t minor = read_response->data()[0] % 100;

						logger_.debug(F("Temperature offset %u.%02u°C"), major, minor);
					} else {
						uint16_t major = value / 100;
						uint8_t minor = value % 100;

						logger_.info(F("Setting temperature offset to %u.%02u°C"), major, minor);
						response_ = client_.write_holding_register(DEVICE_ADDRESS, TEMPERATURE_OFFSET_ADDRESS, value);
						return;
					}
				}
			}

			response_.reset();
			current_operation_ = Operation::NONE;
		}
		break;

	case Operation::CONFIG_ALTITUDE_COMPENSATION:
		if (!response_) {
			logger_.debug(F("Reading altitude compensation configuration"));
			response_ = client_.read_holding_registers(DEVICE_ADDRESS, ALTITUDE_COMPENSATION_ADDRESS, 1);
		} else if (response_->done()) {
			auto read_response = std::dynamic_pointer_cast<const uuid::modbus::RegisterDataResponse>(response_);
			auto write_response = std::dynamic_pointer_cast<const uuid::modbus::RegisterWriteResponse>(response_);

			if (write_response) {
				if (write_response->data().size() < 1) {
					logger_.err(F("Failed to write altitude compensation configuration"));
					reset();
				} else {
					logger_.info(F("Altitude compensation %um"), write_response->data()[0]);
				}
			} else if (read_response) {
				if (read_response->data().size() < 1) {
					logger_.err(F("Failed to read altitude compensation configuration"));
					reset();
				} else {
					const uint16_t value = automatic_calibration();

					if (read_response->data()[0] == value) {
						logger_.debug(F("Altitude compensation %um"), read_response->data()[0]);
					} else {
						logger_.info(F("Setting altitude compensation to %um"), value);
						response_ = client_.write_holding_register(DEVICE_ADDRESS, ALTITUDE_COMPENSATION_ADDRESS, value);
						return;
					}
				}
			}

			response_.reset();
			current_operation_ = Operation::NONE;
		}
		break;

	case Operation::CONFIG_CONTINUOUS_MEASUREMENT:
		if (!response_) {
			logger_.debug(F("Reading measurement interval configuration"));
			response_ = client_.read_holding_registers(DEVICE_ADDRESS, MEASUREMENT_INTERVAL_ADDRESS, 1);
		} else if (response_->done()) {
			auto read_response = std::dynamic_pointer_cast<const uuid::modbus::RegisterDataResponse>(response_);
			auto write_response = std::dynamic_pointer_cast<const uuid::modbus::RegisterWriteResponse>(response_);

			if (write_response) {
				if (write_response->data().size() < 1) {
					logger_.err(F("Failed to write measurement interval configuration"));
					reset();
				} else {
					logger_.info(F("Measurement interval %us"), write_response->data()[0]);
				}
			} else if (read_response) {
				if (read_response->data().size() < 1) {
					logger_.err(F("Failed to read measurement interval configuration"));
					reset();
				} else {
					const uint16_t value = measurement_interval();

					if (read_response->data()[0] == value) {
						logger_.debug(F("Measurement interval %us"), read_response->data()[0]);
					} else {
						logger_.info(F("Setting measurement interval to %us"), value);
						response_ = client_.write_holding_register(DEVICE_ADDRESS, ALTITUDE_COMPENSATION_ADDRESS, value);
						return;
					}
				}
			}

			response_.reset();
			current_operation_ = Operation::NONE;
		}
		break;
		response_.reset();
		current_operation_ = Operation::NONE;
		break;
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
