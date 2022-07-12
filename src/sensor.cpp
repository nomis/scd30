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
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <functional>
#include <string>
#include <vector>

#include <uuid/common.h>
#include <uuid/log.h>

#include "app/config.h"
#include "scd30/report.h"

using Config = ::app::Config;

static const char __pstr__logger_name[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = "sensor";

namespace scd30 {

uuid::log::Logger Sensor::logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::DAEMON};
std::bitset<sizeof(uint32_t) * 8> Sensor::config_operations_;

static inline float convert_f(const uint16_t *data) {
	union {
		uint32_t u32;
		float f;
	} temp;

	temp.u32 = (data[0] << 16) | data[1];
	return temp.f;
}

Sensor::Sensor(::HardwareSerial &device, int ready_pin, Report &report)
		: client_(device), ready_pin_(ready_pin), report_(report) {
	pinMode(ready_pin_, INPUT);

	config_operations_.set(static_cast<size_t>(Operation::CONFIG_AUTOMATIC_CALIBRATION));
	config_operations_.set(static_cast<size_t>(Operation::CONFIG_TEMPERATURE_OFFSET));
	config_operations_.set(static_cast<size_t>(Operation::CONFIG_ALTITUDE_COMPENSATION));
	config_operations_.set(static_cast<size_t>(Operation::CONFIG_CONTINUOUS_MEASUREMENT));
	config_operations_.set(static_cast<size_t>(Operation::CONFIG_AMBIENT_PRESSURE));

	client_.default_unicast_timeout_ms(MODBUS_TIMEOUT_MS);
}

void Sensor::start() {
	pending_operations_.set(static_cast<size_t>(Operation::READ_FIRMWARE_VERSION));
	config();
}

void Sensor::config(std::initializer_list<Operation> operations) {
	Config config;

	if (operations.size() == 0) {
		pending_operations_ |= config_operations_;
	} else {
		for (auto operation : operations) {
			if (config_operations_[static_cast<size_t>(operation)]) {
				pending_operations_.set(static_cast<size_t>(operation));
			}
		}
	}

	interval_ = std::max(0UL, std::min(static_cast<unsigned long>(UINT8_MAX), config.take_measurement_interval()));
}

void Sensor::reset(uint32_t wait_ms) {
	pending_operations_.reset();
	pending_operations_.set(static_cast<size_t>(Operation::SOFT_RESET));
	current_operation_ = Operation::NONE;
	response_.reset();
	start();
	reset_start_ms_ = ::millis();
	reset_wait_ms_ = wait_ms;
	last_reading_s_ = 0;
	measurement_status_ = Measurement::PENDING;
}

uint32_t Sensor::current_time() {
	struct timeval tv;

	if (gettimeofday(&tv, nullptr) == 0) {
		return tv.tv_sec;
	} else {
		return 0;
	}
}

void Sensor::loop() {
	client_.loop();

	if (measurement_status_ == Measurement::IDLE && interval_ > 0) {
		uint32_t now = current_time();

		if (now > last_reading_s_ && now % interval_ == 0) {
			logger_.trace(F("Take measurement"));
			pending_operations_.set(static_cast<size_t>(Operation::TAKE_MEASUREMENT));
			measurement_status_ = Measurement::PENDING;
		}
	}

retry:
	switch (current_operation_) {
	case Operation::NONE:
		if (pending_operations_.any()) {
			int bit = ffs(pending_operations_.to_ulong());
			if (bit != 0) {
				current_operation_ = static_cast<Operation>(bit - 1);
				pending_operations_.reset(static_cast<size_t>(current_operation_));
			}
			goto retry;
		}
		break;

	case Operation::SOFT_RESET:
		if (!response_) {
			if (::millis() - reset_start_ms_ >= reset_wait_ms_) {
				logger_.debug(F("Restarting sensor"));
				response_ = client_.write_holding_register(DEVICE_ADDRESS, SOFT_RESET_ADDRESS, 0x0001);
				reset_complete_ = false;
			}
		} else if (response_->done()) {
			auto write_response = std::static_pointer_cast<const uuid::modbus::RegisterWriteResponse>(response_);

			if (write_response->data().size() < 1 || write_response->data()[0] != 0x0001) {
				logger_.emerg(F("Failed to restart sensor"));
				reset();
				return;
			} else if (!reset_complete_) {
				logger_.info(F("Restarted sensor"));
				reset_start_ms_ = ::millis();
				reset_complete_ = true;
			} else {
				if (::millis() - reset_start_ms_ >= RESET_POST_DELAY_MS) {
					response_.reset();
					current_operation_ = Operation::NONE;
					measurement_status_ = Measurement::IDLE;
				}
			}
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
				return;
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
			false, &automatic_calibration, bool_value_str, bool_set_value_str);
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
			false, &temperature_offset, temp_value_str);
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
			false, &altitude_compensation, alt_value_str);
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
			false, &measurement_interval, secs_value_str);
		break;

	case Operation::CONFIG_AMBIENT_PRESSURE:
		static const auto pressure_value_str = [] (uint16_t value) -> std::string {
				std::vector<char> text(10 + 1);
				if (snprintf_P(text.data(), text.size(), PSTR("%u mbar"), value) <= 0) {
					return uuid::read_flash_string(F("?"));
				}
				return text.data();
			};

		update_config_register(F("continuous measurement with ambient pressure"), AMBIENT_PRESSURE_ADDRESS,
			true, &ambient_pressure, pressure_value_str);
		break;

	case Operation::CALIBRATE:
		if (!response_) {
			logger_.info(F("Writing calibration value of %u ppm"), calibration_ppm_);
			response_ = client_.write_holding_register(DEVICE_ADDRESS, FORCED_RECALIBRATION_ADDRESS, calibration_ppm_);
		} else if (response_->done()) {
			auto response = std::static_pointer_cast<const uuid::modbus::RegisterWriteResponse>(response_);

			if (response->data().size() < 1) {
				logger_.crit(F("Failed to set calibration value"));
				reset();
				return;
			} else {
				logger_.info(F("Calibrated CO₂ ppm: %u"), response->data()[0]);
			}

			response_.reset();
			current_operation_ = Operation::NONE;
		}
		break;

	case Operation::TAKE_MEASUREMENT:
		if (!response_) {
			if (digitalRead(ready_pin_) == HIGH) {
				logger_.trace(F("Read measurement data"));
				response_ = client_.read_holding_registers(DEVICE_ADDRESS, MEASUREMENT_DATA_ADDRESS, 6);
			} else if (measurement_status_ == Measurement::WAITING) {
				if (::millis() - measurement_start_ms_ >= MEASUREMENT_TIMEOUT_MS) {
					logger_.alert(F("Timeout waiting for measurement to be ready"));
					reset();
					return;
				}
			} else {
				measurement_status_ = Measurement::WAITING;
				measurement_start_ms_ = ::millis();
			}
		} else if (response_->done()) {
			auto response = std::static_pointer_cast<const uuid::modbus::RegisterDataResponse>(response_);

			if (response->data().size() < 6) {
				logger_.alert(F("Failed to read measurement data"));
				reset();
				return;
			} else {
				uint32_t now = current_time();
				float co2 = convert_f(&response->data()[0]);
				temperature_c_ = convert_f(&response->data()[2]);
				relative_humidity_pc_ = convert_f(&response->data()[4]);

				logger_.debug(F("Temperature %.2f°C, Relative humidity %.2f%%, CO₂ %.2f ppm"),
					temperature_c_, relative_humidity_pc_, co2);

				if (co2 >= MINIMUM_CO2_PPM) {
					co2_ppm_ = co2;
				} else {
					co2_ppm_ = NAN;
				}

				report_.add(now, temperature_c_, relative_humidity_pc_, co2_ppm_);

				last_reading_s_ = now;
				measurement_status_ = Measurement::IDLE;
			}

			response_.reset();
			current_operation_ = Operation::NONE;
		}
		break;
	}
}

void Sensor::update_config_register(const __FlashStringHelper *name,
		const uint16_t address, const bool always_write,
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
				logger_.crit(F("Failed to write %S configuration"), name);
				reset();
				return;
			} else {
				std::string name_title = uuid::read_flash_string(name);

				name_title[0] = ::toupper(name_title[0]);

				logger_.info(F("%s %s"), name_title.c_str(), func_value_str(read_response->data()[0]).c_str());
			}
		} else if (read_response) {
			if (read_response->data().size() < 1) {
				logger_.crit(F("Failed to read %S configuration"), name);
				reset();
				return;
			} else {
				const uint16_t value = func_cfg_value();

				if (read_response->data()[0] == value && !always_write) {
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
	return std::max(0UL, std::min(static_cast<unsigned long>(UINT16_MAX), Config().sensor_temperature_offset()));
}

uint16_t Sensor::altitude_compensation() {
	return std::max(0UL, std::min(static_cast<unsigned long>(UINT16_MAX), Config().sensor_altitude_compensation()));
}

uint16_t Sensor::measurement_interval() {
	return std::max(2UL, std::min(1800UL, Config().sensor_measurement_interval()));
}

uint16_t Sensor::ambient_pressure() {
	unsigned long value = Config().sensor_ambient_pressure();

	if (value == 0) {
		return value;
	} else {
		return std::max(700UL, std::min(1200UL, value));
	}
}

void Sensor::calibrate(unsigned long ppm) {
	uint16_t value = std::max(MINIMUM_CALIBRATION_PPM, std::min(MAXIMUM_CALIBRATION_PPM, ppm));

	if (ppm == value) {
		calibration_ppm_ = value;
		pending_operations_.set(static_cast<size_t>(Operation::CALIBRATE));
	}
}

} // namespace scd30
